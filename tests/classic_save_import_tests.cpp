#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include <zlib.h>

#include "aoe/classic_save_import.hpp"

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

void append_u32(std::vector<std::uint8_t>& bytes, std::uint32_t value) {
    for (int shift = 0; shift < 32; shift += 8) {
        bytes.push_back(static_cast<std::uint8_t>(value >> shift));
    }
}

std::vector<std::uint8_t> raw_deflate(
    const std::vector<std::uint8_t>& input
) {
    z_stream stream{};
    require(
        deflateInit2(
            &stream, Z_BEST_COMPRESSION, Z_DEFLATED, -MAX_WBITS,
            8, Z_DEFAULT_STRATEGY
        ) == Z_OK,
        "fixture compressor init failed"
    );
    std::vector<std::uint8_t> output(compressBound(input.size()));
    stream.next_in = const_cast<Bytef*>(input.data());
    stream.avail_in = static_cast<uInt>(input.size());
    stream.next_out = output.data();
    stream.avail_out = static_cast<uInt>(output.size());
    const int status = deflate(&stream, Z_FINISH);
    require(status == Z_STREAM_END, "fixture compression failed");
    output.resize(stream.total_out);
    deflateEnd(&stream);
    return output;
}

std::vector<std::uint8_t> fixture(
    std::string version = "VER 9.4",
    float save_version = 11.76F,
    std::uint32_t log_version = 4
) {
    std::vector<std::uint8_t> inflated(version.begin(), version.end());
    inflated.push_back(0);
    const std::uint32_t save_bits =
        std::bit_cast<std::uint32_t>(save_version);
    append_u32(inflated, save_bits);
    inflated.insert(inflated.end(), 128, 0x5a);
    const std::vector<std::uint8_t> compressed = raw_deflate(inflated);
    std::vector<std::uint8_t> bytes;
    append_u32(
        bytes, static_cast<std::uint32_t>(compressed.size() + 8)
    );
    append_u32(bytes, 0x12345678);
    bytes.insert(bytes.end(), compressed.begin(), compressed.end());
    append_u32(bytes, log_version);
    bytes.insert(bytes.end(), {0xde, 0xad, 0xbe, 0xef});
    return bytes;
}

void envelope_and_proved_metadata_are_inspected() {
    const std::vector<std::uint8_t> bytes = fixture();
    const aoe::ClassicSaveInspection inspected =
        aoe::inspect_classic_save(bytes, ".gax");
    require(inspected.envelope_valid, "valid envelope rejected");
    require(inspected.raw_file == bytes, "raw file not preserved");
    require(
        inspected.metadata.game_version == "VER 9.4" &&
        inspected.metadata.save_version.has_value() &&
        *inspected.metadata.save_version == 11.76F &&
        inspected.metadata.log_version == 4,
        "proved version metadata mismatch"
    );
    require(
        !inspected.metadata.player_count &&
        !inspected.metadata.map_width &&
        !inspected.metadata.map_height &&
        !inspected.metadata.tick,
        "unproved metadata was invented"
    );
    require(
        !inspected.lossless_conversion_available() &&
        !aoe::convert_classic_save_losslessly(inspected),
        "lossy classic save conversion was offered"
    );
}

void malformed_and_bounded_streams_fail_structurally() {
    std::vector<std::uint8_t> bad_length = fixture();
    bad_length[0] = 7;
    bad_length[1] = bad_length[2] = bad_length[3] = 0;
    const auto malformed =
        aoe::inspect_classic_save(bad_length, ".gax");
    require(
        !malformed.envelope_valid &&
        !malformed.diagnostics.empty() &&
        malformed.diagnostics.front().kind ==
            aoe::ClassicSaveDiagnosticKind::malformed,
        "invalid header length accepted"
    );

    std::vector<std::uint8_t> truncated = fixture();
    truncated.resize(12);
    const auto cut = aoe::inspect_classic_save(truncated, ".ga1");
    require(!cut.envelope_valid, "truncated stream accepted");

    aoe::ClassicSaveLimits limits;
    limits.maximum_inflated_header_bytes = 32;
    const auto limited =
        aoe::inspect_classic_save(fixture(), ".gax", limits);
    require(
        !limited.envelope_valid &&
        !limited.diagnostics.empty(),
        "inflated-header limit ignored"
    );
}

void unknown_signatures_stay_unsupported() {
    const auto inspected = aoe::inspect_classic_save(
        fixture("BAD 9.4"), ".unknown"
    );
    require(inspected.envelope_valid, "valid compression rejected");
    require(
        !inspected.metadata.game_version &&
        !inspected.metadata.save_version,
        "unknown version signature treated as proved"
    );
    require(
        std::ranges::any_of(
            inspected.diagnostics,
            [](const aoe::ClassicSaveDiagnostic& diagnostic) {
                return diagnostic.kind ==
                    aoe::ClassicSaveDiagnosticKind::unsupported;
            }
        ),
        "unsupported diagnostic missing"
    );
}

}  // namespace

int main() {
    try {
        envelope_and_proved_metadata_are_inspected();
        malformed_and_bounded_streams_fail_structurally();
        unknown_signatures_stay_unsupported();
        std::cout << "All classic save import tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
