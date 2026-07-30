#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "aoe/scenario.hpp"

namespace aoe {

enum class ClassicSaveDiagnosticKind {
    unsupported,
    malformed,
    limit_exceeded,
};

struct ClassicSaveDiagnostic {
    ClassicSaveDiagnosticKind kind{ClassicSaveDiagnosticKind::malformed};
    std::size_t offset{};
    std::string message;
};

struct ClassicSaveLimits {
    std::size_t maximum_file_bytes{64 * 1024 * 1024};
    std::size_t maximum_compressed_header_bytes{16 * 1024 * 1024};
    std::size_t maximum_inflated_header_bytes{64 * 1024 * 1024};
};

struct ClassicSaveMetadata {
    std::optional<std::string> game_version;
    std::optional<float> save_version;
    std::optional<std::uint32_t> log_version;
    std::optional<std::uint32_t> player_count;
    std::optional<std::uint32_t> map_width;
    std::optional<std::uint32_t> map_height;
    std::optional<std::uint64_t> tick;
};

struct ClassicSaveInspection {
    std::vector<std::uint8_t> raw_file;
    std::vector<std::uint8_t> compressed_header;
    std::vector<std::uint8_t> inflated_header;
    std::uint32_t header_length{};
    std::uint32_t next_position{};
    ClassicSaveMetadata metadata;
    std::vector<ClassicSaveDiagnostic> diagnostics;
    bool envelope_valid{};

    [[nodiscard]] bool lossless_conversion_available() const {
        return false;
    }
};

[[nodiscard]] ClassicSaveInspection inspect_classic_save(
    std::span<const std::uint8_t> bytes,
    std::string extension,
    const ClassicSaveLimits& limits = {}
);

[[nodiscard]] std::optional<Scenario> convert_classic_save_losslessly(
    const ClassicSaveInspection& inspection
);

}  // namespace aoe
