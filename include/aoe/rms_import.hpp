#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

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
};

[[nodiscard]] RmsDocument parse_rms(
    std::string_view source,
    const RmsImportLimits& limits = {}
);

[[nodiscard]] std::optional<Scenario> evaluate_rms(
    const RmsDocument& document,
    std::uint64_t seed,
    Civilization blue = Civilization::generic,
    Civilization red = Civilization::generic
);

}  // namespace aoe
