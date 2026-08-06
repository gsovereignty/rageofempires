#pragma once

#include <filesystem>
#include <optional>

namespace aoe {

[[nodiscard]] std::filesystem::path runtime_resources_directory();
[[nodiscard]] std::filesystem::path runtime_game_data_directory();
[[nodiscard]] std::filesystem::path user_data_directory();
[[nodiscard]] std::filesystem::path user_settings_directory();
[[nodiscard]] std::filesystem::path user_autosave_directory();
[[nodiscard]] std::optional<std::filesystem::path> configured_asset_root();

}  // namespace aoe
