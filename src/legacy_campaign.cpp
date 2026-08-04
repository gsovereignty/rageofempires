#include "aoe/legacy_campaign.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <limits>
#include <iomanip>
#include <sstream>
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

std::uint16_t u16(std::span<const std::byte> bytes, std::size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 2) {
        throw std::runtime_error("truncated campaign integer");
    }
    const auto* data = reinterpret_cast<const unsigned char*>(bytes.data() + offset);
    return static_cast<std::uint16_t>(data[0]) |
        static_cast<std::uint16_t>(data[1] << 8U);
}

std::uint64_t u64(std::span<const std::byte> bytes, std::size_t offset) {
    return static_cast<std::uint64_t>(u32(bytes, offset)) |
        (static_cast<std::uint64_t>(u32(bytes, offset + 4)) << 32U);
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

std::string source_digest(std::span<const std::byte> bytes) {
    std::uint64_t hash = 14695981039346656037ULL;
    for (const auto value : bytes) {
        hash ^= std::to_integer<unsigned char>(value);
        hash *= 1099511628211ULL;
    }
    std::ostringstream output;
    output << std::hex << std::setfill('0') << std::setw(16) << hash;
    return output.str();
}

std::string tagged_string(
    std::span<const std::byte> bytes, std::size_t& cursor
) {
    if (u16(bytes, cursor) != 0x0a60U) {
        throw std::runtime_error("invalid campaign string marker");
    }
    const auto length = u16(bytes, cursor + 2);
    cursor += 4;
    if (length == 0 || cursor > bytes.size() || length > bytes.size() - cursor) {
        throw std::runtime_error("invalid campaign string length");
    }
    const auto* begin = reinterpret_cast<const char*>(bytes.data() + cursor);
    std::string result(begin, begin + length);
    cursor += length;
    return result;
}

struct ParsedIndex {
    std::string name;
    struct Entry { std::uint64_t size{}; std::uint64_t offset{}; std::string name; std::string filename; };
    std::vector<Entry> entries;
    std::size_t end{};
};

ParsedIndex parse_index(std::span<const std::byte> bytes, std::string_view version) {
    ParsedIndex parsed;
    std::size_t cursor = 4;
    std::uint32_t count{};
    if (version == "1.00") {
        parsed.name = fixed_string(bytes, cursor, 256);
        cursor += 256;
        count = u32(bytes, cursor);
        cursor += 4;
        if (count == 0 || count > max_scenarios ||
            count > (bytes.size() - cursor) / 520) {
            throw std::runtime_error("invalid campaign scenario count");
        }
        for (std::uint32_t index = 0; index < count; ++index) {
            ParsedIndex::Entry entry;
            entry.size = static_cast<std::int32_t>(u32(bytes, cursor));
            entry.offset = static_cast<std::int32_t>(u32(bytes, cursor + 4));
            if (static_cast<std::int64_t>(entry.size) <= 0 ||
                static_cast<std::int64_t>(entry.offset) < 0) {
                throw std::runtime_error("invalid campaign entry range");
            }
            entry.name = fixed_string(bytes, cursor + 8, 255);
            entry.filename = fixed_string(bytes, cursor + 263, 255);
            if (bytes[cursor + 518] != std::byte{} ||
                bytes[cursor + 519] != std::byte{}) {
                throw std::runtime_error("invalid campaign index padding");
            }
            parsed.entries.push_back(std::move(entry));
            cursor += 520;
        }
    } else if (version == "1.10") {
        count = u32(bytes, cursor);
        cursor += 4;
        parsed.name = tagged_string(bytes, cursor);
        if (count == 0 || count > max_scenarios) {
            throw std::runtime_error("invalid campaign scenario count");
        }
        for (std::uint32_t index = 0; index < count; ++index) {
            ParsedIndex::Entry entry;
            entry.size = u64(bytes, cursor);
            entry.offset = u64(bytes, cursor + 8);
            cursor += 16;
            entry.name = tagged_string(bytes, cursor);
            entry.filename = tagged_string(bytes, cursor);
            parsed.entries.push_back(std::move(entry));
        }
    } else if (version == "2.00") {
        // HD .cpx2 places an opaque int32 list before classic fixed campaign
        // name, then uses tagged variable-length scenario names.
        const auto opaque_count = u32(bytes, cursor);
        if (opaque_count > 4096 || opaque_count + 1 > (bytes.size() - 8) / 4) {
            throw std::runtime_error("invalid cpx2 header field count");
        }
        cursor += 4 + static_cast<std::size_t>(opaque_count + 1) * 4;
        parsed.name = fixed_string(bytes, cursor, 256);
        cursor += 256;
        count = u32(bytes, cursor);
        cursor += 4;
        if (count == 0 || count > max_scenarios) {
            throw std::runtime_error("invalid campaign scenario count");
        }
        for (std::uint32_t index = 0; index < count; ++index) {
            ParsedIndex::Entry entry;
            entry.size = static_cast<std::int32_t>(u32(bytes, cursor));
            entry.offset = static_cast<std::int32_t>(u32(bytes, cursor + 4));
            cursor += 8;
            const auto name_length = u16(bytes, cursor);
            cursor += 4; // length plus observed 0x0a60 marker
            if (name_length == 0 || name_length > bytes.size() - cursor) {
                throw std::runtime_error("invalid cpx2 scenario name length");
            }
            entry.name.assign(reinterpret_cast<const char*>(bytes.data() + cursor), name_length);
            cursor += name_length;
            const auto file_length = u16(bytes, cursor);
            cursor += 4;
            if (file_length == 0 || file_length > bytes.size() - cursor) {
                throw std::runtime_error("invalid cpx2 filename length");
            }
            entry.filename.assign(reinterpret_cast<const char*>(bytes.data() + cursor), file_length);
            cursor += file_length;
            parsed.entries.push_back(std::move(entry));
        }
    } else {
        throw std::invalid_argument("unsupported");
    }
    parsed.end = cursor;
    return parsed;
}

}  // namespace

LegacyCampaignImportResult inspect_legacy_campaign_bytes(
    std::span<const std::byte> bytes
) {
    LegacyCampaignImportResult result;
    result.original_bytes.assign(bytes.begin(), bytes.end());
    try {
        if (bytes.size() < 8) {
            throw std::runtime_error("truncated campaign header");
        }
        result.version.assign(
            reinterpret_cast<const char*>(bytes.data()),
            4
        );
        if (result.version != "1.00" && result.version != "1.10" &&
            result.version != "2.00") {
            result.status = LegacyCampaignImportStatus::unsupported_version;
            result.diagnostic =
                "unsupported campaign version '" + result.version + "'";
            return result;
        }
        const ParsedIndex parsed = parse_index(bytes, result.version);
        result.name = parsed.name;
        const std::size_t index_end = parsed.end;
        struct Range {
            std::size_t begin{};
            std::size_t end{};
        };
        std::vector<Range> ranges;
        result.entries.reserve(parsed.entries.size());
        for (const auto& source : parsed.entries) {
            if (source.size == 0 || source.offset > std::numeric_limits<std::size_t>::max()) {
                throw std::runtime_error("invalid campaign entry range");
            }
            const std::size_t size = static_cast<std::size_t>(source.size);
            const std::size_t offset = static_cast<std::size_t>(source.offset);
            if (offset < index_end || offset > bytes.size() ||
                size > bytes.size() - offset) {
                throw std::runtime_error("campaign entry lies outside file");
            }
            LegacyCampaignEntry entry;
            entry.name = source.name;
            entry.filename = source.filename;
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

std::vector<std::byte> serialize_legacy_campaign(
    const LegacyCampaignImportResult& campaign
) {
    if (campaign.status != LegacyCampaignImportStatus::inspected ||
        campaign.original_bytes.empty()) {
        throw std::invalid_argument("campaign has no validated source image");
    }
    const auto verified = inspect_legacy_campaign_bytes(campaign.original_bytes);
    if (verified.status != LegacyCampaignImportStatus::inspected) {
        throw std::invalid_argument("campaign source image is no longer valid");
    }
    return campaign.original_bytes;
}

void save_legacy_campaign(
    const LegacyCampaignImportResult& campaign,
    const std::filesystem::path& path
) {
    const auto bytes = serialize_legacy_campaign(campaign);
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) throw std::runtime_error("cannot write campaign file");
    output.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
    if (!output) throw std::runtime_error("cannot write complete campaign file");
}

Campaign import_legacy_campaign(
    const std::filesystem::path& campaign_path,
    const std::filesystem::path& dat_path,
    const std::filesystem::path& install_root
) {
    const LegacyCampaignImportResult source = inspect_legacy_campaign(campaign_path);
    if (source.status != LegacyCampaignImportStatus::inspected) {
        throw std::runtime_error("cannot import campaign: " + source.diagnostic);
    }
    const LegacyDatFile dat = LegacyDatFile::load(dat_path);
    const std::string id = "classic-v3-" + source_digest(source.original_bytes);
    const std::filesystem::path target = install_root / id;
    const std::filesystem::path manifest = target / "campaign.campaign";
    if (std::filesystem::is_regular_file(manifest)) return load_campaign(manifest);
    std::filesystem::create_directories(install_root);
    const std::filesystem::path staging = install_root / (id + ".importing");
    if (std::filesystem::exists(staging)) {
        throw std::runtime_error("campaign import already in progress");
    }
    std::filesystem::create_directory(staging);
    try {
        Campaign campaign;
        campaign.id = id;
        campaign.name = source.name;
        campaign.description = "Imported classic campaign " + source.version;
        campaign.human_player = Player::blue;
        for (std::size_t index = 0; index < source.entries.size(); ++index) {
            const auto& entry = source.entries[index];
            if (entry.scenario.status != LegacyScenarioImportStatus::metadata_only ||
                !entry.scenario.metadata) {
                throw std::runtime_error("scenario " + std::to_string(index + 1) +
                    " cannot be decoded: " + entry.scenario.diagnostic);
            }
            LegacyScenarioConversionReport converted =
                convert_legacy_scenario(*entry.scenario.metadata, dat, true);
            if (!converted.scenario) {
                const std::string reason = converted.diagnostics.empty()
                    ? "unsupported scenario semantics"
                    : converted.diagnostics.back();
                throw std::runtime_error("scenario " + std::to_string(index + 1) +
                    " cannot be played: " + reason);
            }
            // Commercial scenarios may stack decoration/resource records on
            // occupied footprints. Native placement rules are stricter.
            // Retain every independently placeable gameplay entity while
            // source bytes remain exact in original container.
            Scenario playable = *converted.scenario;
            const auto buildings = std::move(playable.buildings);
            const auto units = std::move(playable.units);
            playable.buildings.clear();
            playable.units.clear();
            for (const auto& building : buildings) {
                playable.buildings.push_back(building);
                try { (void)create_simulation(playable); }
                catch (const std::exception&) { playable.buildings.pop_back(); }
            }
            for (const auto& unit : units) {
                playable.units.push_back(unit);
                try { (void)create_simulation(playable); }
                catch (const std::exception&) { playable.units.pop_back(); }
            }
            const std::filesystem::path relative =
                "scenario-" + std::to_string(index + 1) + ".scenario";
            save_scenario(playable, staging / relative);
            campaign.scenarios.push_back({static_cast<int>(index + 1),
                staging / relative, entry.name, {}, {}});
        }
        save_campaign(campaign, staging / "campaign.campaign");
        std::error_code error;
        std::filesystem::rename(staging, target, error);
        if (error) {
            throw std::runtime_error("cannot publish imported campaign: " +
                                     error.message());
        }
        return load_campaign(manifest);
    } catch (...) {
        std::error_code ignored;
        std::filesystem::remove_all(staging, ignored);
        throw;
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
