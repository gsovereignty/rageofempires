#include "aoe/classic_save_import.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cctype>
#include <cstring>
#include <limits>

#include <zlib.h>

namespace aoe {
namespace {

std::uint32_t read_u32(std::span<const std::uint8_t> bytes) {
    return static_cast<std::uint32_t>(bytes[0]) |
        (static_cast<std::uint32_t>(bytes[1]) << 8) |
        (static_cast<std::uint32_t>(bytes[2]) << 16) |
        (static_cast<std::uint32_t>(bytes[3]) << 24);
}

float read_float(std::span<const std::uint8_t> bytes) {
    return std::bit_cast<float>(read_u32(bytes));
}

std::string lower(std::string text) {
    std::ranges::transform(text, text.begin(), [](unsigned char value) {
        return static_cast<char>(std::tolower(value));
    });
    return text;
}

bool known_version(const std::string& version) {
    static constexpr std::array versions{
        "VER 9.3", "VER 9.4", "VER 9.8", "VER 9.9",
        "VER 9.A", "VER 9.B", "VER 9.C", "VER 9.D",
        "VER 9.E", "VER 9.F", "MCP 9.F",
    };
    return std::ranges::find(versions, version) != versions.end();
}

bool inflate_raw(
    std::span<const std::uint8_t> compressed,
    std::vector<std::uint8_t>& output,
    std::size_t limit
) {
    z_stream stream{};
    if (inflateInit2(&stream, -MAX_WBITS) != Z_OK) return false;
    std::array<std::uint8_t, 16384> buffer{};
    std::size_t consumed{};
    int status = Z_OK;
    while (status == Z_OK) {
        if (stream.avail_in == 0 && consumed < compressed.size()) {
            const std::size_t chunk = std::min<std::size_t>(
                compressed.size() - consumed,
                std::numeric_limits<uInt>::max()
            );
            stream.next_in = const_cast<Bytef*>(
                compressed.data() + consumed
            );
            stream.avail_in = static_cast<uInt>(chunk);
            consumed += chunk;
        }
        stream.next_out = buffer.data();
        stream.avail_out = static_cast<uInt>(buffer.size());
        status = inflate(&stream, Z_NO_FLUSH);
        const std::size_t produced = buffer.size() - stream.avail_out;
        if (produced > limit - std::min(limit, output.size())) {
            inflateEnd(&stream);
            return false;
        }
        output.insert(
            output.end(), buffer.begin(), buffer.begin() + produced
        );
        if (status == Z_BUF_ERROR && stream.avail_in == 0 &&
            consumed == compressed.size()) {
            break;
        }
    }
    const bool complete =
        status == Z_STREAM_END && consumed == compressed.size() &&
        stream.avail_in == 0;
    inflateEnd(&stream);
    return complete;
}

}  // namespace

ClassicSaveInspection inspect_classic_save(
    std::span<const std::uint8_t> bytes,
    std::string extension,
    const ClassicSaveLimits& limits
) {
    ClassicSaveInspection result;
    if (bytes.size() <= limits.maximum_file_bytes) {
        result.raw_file.assign(bytes.begin(), bytes.end());
    } else {
        result.diagnostics.push_back({
            ClassicSaveDiagnosticKind::limit_exceeded, 0,
            "classic save exceeds file byte limit",
        });
        return result;
    }
    extension = lower(std::move(extension));
    if (extension != ".gax" && extension != ".ga1" &&
        extension != ".gam" && extension != ".mgx") {
        result.diagnostics.push_back({
            ClassicSaveDiagnosticKind::unsupported, 0,
            "extension is not evidenced as a classic Genie save/record",
        });
    }
    if (bytes.size() < 8) {
        result.diagnostics.push_back({
            ClassicSaveDiagnosticKind::malformed, bytes.size(),
            "truncated eight-byte envelope",
        });
        return result;
    }
    result.header_length = read_u32(bytes.first<4>());
    result.next_position = read_u32(bytes.subspan<4, 4>());
    if (result.header_length < 8 ||
        result.header_length > bytes.size()) {
        result.diagnostics.push_back({
            ClassicSaveDiagnosticKind::malformed, 0,
            "header length lies outside file",
        });
        return result;
    }
    const std::size_t compressed_size = result.header_length - 8;
    if (compressed_size > limits.maximum_compressed_header_bytes) {
        result.diagnostics.push_back({
            ClassicSaveDiagnosticKind::limit_exceeded, 8,
            "compressed header exceeds limit",
        });
        return result;
    }
    result.compressed_header.assign(
        bytes.begin() + 8, bytes.begin() + result.header_length
    );
    if (!inflate_raw(
            result.compressed_header, result.inflated_header,
            limits.maximum_inflated_header_bytes
        )) {
        result.inflated_header.clear();
        result.diagnostics.push_back({
            ClassicSaveDiagnosticKind::malformed, 8,
            "header is not one complete bounded raw-DEFLATE stream",
        });
        return result;
    }
    result.envelope_valid = true;
    if (result.inflated_header.size() < 12) {
        result.diagnostics.push_back({
            ClassicSaveDiagnosticKind::unsupported, 8,
            "inflated header is too short for proved version fields",
        });
        return result;
    }
    std::string game(
        result.inflated_header.begin(),
        result.inflated_header.begin() + 7
    );
    if (known_version(game) && result.inflated_header[7] == 0) {
        result.metadata.game_version = game;
        const float save = read_float(std::span<const std::uint8_t>(
            result.inflated_header.data() + 8, 4
        ));
        if (std::isfinite(save)) result.metadata.save_version = save;
    } else {
        result.diagnostics.push_back({
            ClassicSaveDiagnosticKind::unsupported, 8,
            "inflated header has unknown game-version signature",
        });
    }
    if (bytes.size() >=
        static_cast<std::size_t>(result.header_length) + 4) {
        result.metadata.log_version = read_u32(
            bytes.subspan(result.header_length, 4)
        );
    }
    result.diagnostics.push_back({
        ClassicSaveDiagnosticKind::unsupported, result.header_length,
        "player, map, tick, and complete object state remain unproved",
    });
    return result;
}

std::optional<Scenario> convert_classic_save_losslessly(
    const ClassicSaveInspection&
) {
    return std::nullopt;
}

}  // namespace aoe
