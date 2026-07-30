#include "aoe/legacy_campaign.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <limits>
#include <stdexcept>

namespace aoe {
namespace {

constexpr std::size_t max_campaign_size = 256U * 1024U * 1024U;
constexpr std::size_t max_scenarios = 256;

std::uint32_t u32(std::span<const std::byte> bytes, std::size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 4) {
        throw std::runtime_error("truncated campaign integer");
    }
    const auto* data = reinterpret_cast<const unsigned char*>(
        bytes.data() + offset
    );
    return static_cast<std::uint32_t>(data[0]) |
        (static_cast<std::uint32_t>(data[1]) << 8U) |
        (static_cast<std::uint32_t>(data[2]) << 16U) |
        (static_cast<std::uint32_t>(data[3]) << 24U);
}

std::string fixed_string(
    std::span<const std::byte> bytes,
    std::size_t offset,
    std::size_t length
) {
    if (offset > bytes.size() || length > bytes.size() - offset) {
        throw std::runtime_error("truncated campaign string");
    }
    const auto* begin = reinterpret_cast<const char*>(bytes.data() + offset);
    const auto* end = begin + length;
    const auto* nul = std::find(begin, end, '\0');
    if (nul == begin) throw std::runtime_error("empty campaign string");
    return std::string(begin, nul);
}

std::vector<std::byte> read_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) throw std::runtime_error("cannot open campaign file");
    const auto end = input.tellg();
    if (end < 0 || static_cast<std::uint64_t>(end) > max_campaign_size) {
        throw std::runtime_error("campaign file exceeds 256 MiB limit");
    }
    std::vector<std::byte> bytes(static_cast<std::size_t>(end));
    input.seekg(0);
    if (!bytes.empty()) {
        input.read(
            reinterpret_cast<char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size())
        );
    }
    if (!input) throw std::runtime_error("cannot read complete campaign file");
    return bytes;
}

}  // namespace

LegacyCampaignImportResult inspect_legacy_campaign_bytes(
    std::span<const std::byte> bytes
) {
    LegacyCampaignImportResult result;
    try {
        if (bytes.size() < 264) {
            throw std::runtime_error("truncated classic campaign header");
        }
        result.version.assign(
            reinterpret_cast<const char*>(bytes.data()),
            4
        );
        if (result.version != "1.00") {
            result.status = LegacyCampaignImportStatus::unsupported_version;
            result.diagnostic =
                "unsupported campaign version '" + result.version + "'";
            return result;
        }
        result.name = fixed_string(bytes, 4, 256);
        const auto count = u32(bytes, 260);
        if (count == 0 || count > max_scenarios) {
            throw std::runtime_error("invalid campaign scenario count");
        }
        constexpr std::size_t entry_size = 520;
        if (count > (bytes.size() - 264) / entry_size) {
            throw std::runtime_error("truncated campaign index");
        }
        const std::size_t index_end = 264 + count * entry_size;
        struct Range {
            std::size_t begin{};
            std::size_t end{};
        };
        std::vector<Range> ranges;
        result.entries.reserve(count);
        for (std::uint32_t index = 0; index < count; ++index) {
            const std::size_t cursor = 264 + index * entry_size;
            const auto signed_size =
                static_cast<std::int32_t>(u32(bytes, cursor));
            const auto signed_offset =
                static_cast<std::int32_t>(u32(bytes, cursor + 4));
            if (signed_size <= 0 || signed_offset < 0) {
                throw std::runtime_error("invalid campaign entry range");
            }
            const std::size_t size = static_cast<std::size_t>(signed_size);
            const std::size_t offset =
                static_cast<std::size_t>(signed_offset);
            if (offset < index_end || offset > bytes.size() ||
                size > bytes.size() - offset) {
                throw std::runtime_error("campaign entry lies outside file");
            }
            if (bytes[cursor + 518] != std::byte{} ||
                bytes[cursor + 519] != std::byte{}) {
                throw std::runtime_error("invalid campaign index padding");
            }
            LegacyCampaignEntry entry;
            entry.name = fixed_string(bytes, cursor + 8, 255);
            entry.filename = fixed_string(bytes, cursor + 263, 255);
            entry.offset = offset;
            entry.raw_payload.assign(
                bytes.begin() + offset,
                bytes.begin() + offset + size
            );
            entry.scenario = inspect_legacy_scenario_bytes(entry.raw_payload);
            if (entry.scenario.status ==
                LegacyScenarioImportStatus::metadata_only) {
                ++result.decoded_scenarios;
            } else {
                ++result.unsupported_scenarios;
            }
            result.entries.push_back(std::move(entry));
            ranges.push_back({offset, offset + size});
        }
        std::sort(ranges.begin(), ranges.end(), [](auto left, auto right) {
            return left.begin < right.begin;
        });
        for (std::size_t index = 1; index < ranges.size(); ++index) {
            if (ranges[index].begin < ranges[index - 1].end) {
                throw std::runtime_error("overlapping campaign entries");
            }
        }
        std::size_t cursor = index_end;
        for (const auto& range : ranges) {
            if (cursor < range.begin) {
                result.unindexed_payload.push_back({
                    cursor,
                    std::vector<std::byte>(
                        bytes.begin() + cursor,
                        bytes.begin() + range.begin
                    ),
                });
            }
            cursor = range.end;
        }
        if (cursor < bytes.size()) {
            result.unindexed_payload.push_back({
                cursor,
                std::vector<std::byte>(
                    bytes.begin() + cursor,
                    bytes.end()
                ),
            });
        }
        result.status = LegacyCampaignImportStatus::inspected;
        result.diagnostic =
            "classic campaign index and embedded payloads inspected; "
            "branching, cinematics, and runtime progression are not inferred";
        return result;
    } catch (const std::exception& error) {
        result.status = LegacyCampaignImportStatus::malformed;
        result.diagnostic = error.what();
        return result;
    }
}

LegacyCampaignImportResult inspect_legacy_campaign(
    const std::filesystem::path& path
) {
    try {
        const auto bytes = read_file(path);
        return inspect_legacy_campaign_bytes(bytes);
    } catch (const std::exception& error) {
        LegacyCampaignImportResult result;
        result.status = LegacyCampaignImportStatus::io_error;
        result.diagnostic = error.what();
        return result;
    }
}

}  // namespace aoe
