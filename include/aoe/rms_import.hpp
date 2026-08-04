#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>

#include "aoe/random_map.hpp"

namespace aoe {

struct RmsLineSpan {
    std::size_t first_line{};
    std::size_t last_line{};
};

struct RmsDirective {
    std::string section;
    std::string name;
    std::vector<std::string> arguments;
    RmsLineSpan span;
    std::size_t random_group{};
    std::size_t random_branch{};
    int random_weight{100};
    // Nested classic `if`/`elseif`/`else` predicates. Symbols are stored in
    // lowercase; `expected == false` represents an else/preceding-branch
    // exclusion. Evaluation resolves these after random #define branches.
    std::vector<std::pair<std::string, bool>> conditions;
    // Nonzero only for create_object and directives lexically owned by its
    // brace-delimited body. IDs are document-local and source ordered.
    std::size_t object_block{};
};

struct RmsUnsupported {
    RmsLineSpan span;
    std::string exact_text;
    std::string reason;
    bool affects_map{true};
};

struct RmsDocument {
    std::vector<RmsDirective> directives;
    std::vector<RmsUnsupported> unsupported;
    bool syntactically_valid{};
    std::string error;

    [[nodiscard]] bool playable() const;
};

struct RmsImportLimits {
    std::size_t maximum_bytes{1024 * 1024};
    std::size_t maximum_lines{20000};
    std::size_t maximum_tokens{200000};
    std::size_t maximum_nesting{64};
    std::size_t maximum_include_depth{32};
};

struct RmsMapResult {
    std::optional<Scenario> scenario;
    std::string error;
};

// Symbols supplied by match setup before an RMS is evaluated. Classic scripts
// use these for game-mode and map-size conditionals (for example REGICIDE and
// TINY_MAP). Definitions are case-insensitive.
struct RmsEvaluationContext {
    std::set<std::string> definitions;
    std::optional<RandomMapSize> map_size;
};

[[nodiscard]] RmsDocument parse_rms(
    std::string_view source,
    const RmsImportLimits& limits = {}
);

// Loads a root RMS and expands filesystem includes relative to each including
// file. When installation_root is supplied, #include_drs resources are read
// from its Data/gamedata*.drs archives; no installation is discovered or
// probed implicitly.
[[nodiscard]] RmsDocument parse_rms_file(
    const std::filesystem::path& path,
    const std::optional<std::filesystem::path>& installation_root =
        std::nullopt,
    const RmsImportLimits& limits = {}
);

// Resolves caller-owned include names before parsing. No filesystem lookup is
// performed; built/runtime code therefore cannot escape packaged inputs.
[[nodiscard]] RmsDocument parse_rms(
    std::string_view source,
    const std::unordered_map<std::string, std::string>& includes,
    const RmsImportLimits& limits = {}
);

[[nodiscard]] std::vector<int> msvcrt_rms_random_sequence(
    std::uint32_t seed, std::size_t count
);

[[nodiscard]] std::optional<Scenario> evaluate_rms(
    const RmsDocument& document,
    std::uint64_t seed,
    Civilization blue = Civilization::generic,
    Civilization red = Civilization::generic
);

[[nodiscard]] std::optional<Scenario> evaluate_rms(
    const RmsDocument& document,
    std::uint64_t seed,
    const RmsEvaluationContext& context,
    Civilization blue = Civilization::generic,
    Civilization red = Civilization::generic
);

// Frontend bridge: when source is empty, evaluates reconstruction-owned
// classic RMS definitions for the selected lobby map kind. Supplying source
// evaluates that script instead. Native hard-coded map recipes are never used.
[[nodiscard]] RmsMapResult generate_rms_map(
    const RandomMapSettings& settings,
    std::optional<std::string_view> source = std::nullopt
);

[[nodiscard]] RmsMapResult generate_rms_map_file(
    const RandomMapSettings& settings,
    const std::filesystem::path& path,
    const std::optional<std::filesystem::path>& installation_root =
        std::nullopt
);

}  // namespace aoe
