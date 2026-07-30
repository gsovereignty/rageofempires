#include "aoe/legacy_assets.hpp"
#include "aoe/player_color_palette.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <fstream>
#include <limits>
#include <sstream>
#include <string_view>

namespace aoe {
namespace {

constexpr std::uint64_t drs_header_size = 64;
constexpr std::uint64_t drs_table_size = 12;
constexpr std::uint64_t drs_entry_size = 12;
constexpr std::uint64_t slp_header_size = 32;
constexpr std::uint64_t slp_frame_info_size = 32;
constexpr std::uint32_t maximum_dimension = 16384;
constexpr std::uint32_t maximum_frame_count = 100000;
constexpr std::uint32_t maximum_table_count = 4096;
constexpr std::uint32_t maximum_entry_count = 1000000;

void require(bool condition, std::string_view message) {
    if (!condition) {
        throw LegacyAssetError{std::string{message}};
    }
}

std::uint8_t byte_at(std::span<const std::byte> data, std::size_t offset) {
    require(offset < data.size(), "legacy asset is truncated");
    return std::to_integer<std::uint8_t>(data[offset]);
}

std::uint16_t u16(std::span<const std::byte> data, std::size_t offset) {
    return static_cast<std::uint16_t>(
        byte_at(data, offset) |
        (static_cast<std::uint16_t>(byte_at(data, offset + 1)) << 8)
    );
}

std::uint32_t u32(std::span<const std::byte> data, std::size_t offset) {
    return static_cast<std::uint32_t>(
        byte_at(data, offset) |
        (static_cast<std::uint32_t>(byte_at(data, offset + 1)) << 8) |
        (static_cast<std::uint32_t>(byte_at(data, offset + 2)) << 16) |
        (static_cast<std::uint32_t>(byte_at(data, offset + 3)) << 24)
    );
}

std::int32_t i32(std::span<const std::byte> data, std::size_t offset) {
    return static_cast<std::int32_t>(u32(data, offset));
}

void checked_range(
    std::uint64_t offset,
    std::uint64_t length,
    std::uint64_t size,
    std::string_view message
) {
    require(offset <= size && length <= size - offset, message);
}

std::string normalized_extension(std::string extension) {
    std::transform(
        extension.begin(),
        extension.end(),
        extension.begin(),
        [](unsigned char character) {
            return static_cast<char>(std::tolower(character));
        }
    );
    while (!extension.empty() && extension.back() == ' ') {
        extension.pop_back();
    }
    require(
        !extension.empty() && extension.size() <= 4,
        "invalid DRS extension"
    );
    return extension;
}

std::vector<std::byte> read_file(const std::filesystem::path& path) {
    std::ifstream input{path, std::ios::binary | std::ios::ate};
    require(input.is_open(), "cannot open legacy asset archive");
    const std::streamoff length = input.tellg();
    require(length >= 0, "cannot determine legacy asset size");
    std::vector<std::byte> bytes(static_cast<std::size_t>(length));
    input.seekg(0);
    if (!bytes.empty()) {
        input.read(
            reinterpret_cast<char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size())
        );
    }
    require(
        input.good() || input.eof(),
        "cannot read legacy asset archive"
    );
    return bytes;
}

std::array<std::uint8_t, 4> palette_color(
    const LegacyPalette& palette,
    std::size_t index,
    std::uint8_t alpha = 255
) {
    require(index < palette.colors.size(), "SLP palette index is invalid");
    const auto& color = palette.colors[index];
    return {color[0], color[1], color[2], alpha};
}

void set_pixel(
    RgbaFrame& frame,
    std::size_t row,
    std::size_t column,
    const std::array<std::uint8_t, 4>& color
) {
    require(
        row < static_cast<std::size_t>(frame.height) &&
            column < static_cast<std::size_t>(frame.width),
        "SLP command draws outside frame"
    );
    const std::size_t target =
        (row * static_cast<std::size_t>(frame.width) + column) * 4;
    std::copy(color.begin(), color.end(), frame.rgba.begin() + target);
}

struct FrameInfo {
    std::uint32_t commands{};
    std::uint32_t outline{};
    std::uint32_t properties{};
    int width{};
    int height{};
    int hotspot_x{};
    int hotspot_y{};
};

FrameInfo frame_info(
    std::span<const std::byte> slp,
    std::size_t frame_index
) {
    const std::size_t offset =
        slp_header_size + frame_index * slp_frame_info_size;
    checked_range(offset, slp_frame_info_size, slp.size(), "SLP frame table is truncated");
    FrameInfo info{
        u32(slp, offset),
        u32(slp, offset + 4),
        u32(slp, offset + 12),
        i32(slp, offset + 16),
        i32(slp, offset + 20),
        i32(slp, offset + 24),
        i32(slp, offset + 28)
    };
    require(
        info.width > 0 && info.height > 0 &&
            info.width <= static_cast<int>(maximum_dimension) &&
            info.height <= static_cast<int>(maximum_dimension),
        "SLP frame dimensions are invalid"
    );
    checked_range(
        info.outline,
        static_cast<std::uint64_t>(info.height) * 4,
        slp.size(),
        "SLP outline table is truncated"
    );
    checked_range(
        info.commands,
        static_cast<std::uint64_t>(info.height) * 4,
        slp.size(),
        "SLP command table is truncated"
    );
    return info;
}

}  // namespace

LegacyPalette LegacyPalette::from_jasc(
    std::span<const std::byte> bytes
) {
    std::string text;
    text.resize(bytes.size());
    std::transform(
        bytes.begin(),
        bytes.end(),
        text.begin(),
        [](std::byte value) {
            return static_cast<char>(std::to_integer<unsigned char>(value));
        }
    );
    std::istringstream input{text};
    std::string signature;
    std::string version;
    std::size_t count{};
    require(
        static_cast<bool>(std::getline(input, signature)) &&
            static_cast<bool>(std::getline(input, version)) &&
            static_cast<bool>(input >> count),
        "palette header is truncated"
    );
    if (!signature.empty() && signature.back() == '\r') {
        signature.pop_back();
    }
    if (!version.empty() && version.back() == '\r') {
        version.pop_back();
    }
    require(
        signature == "JASC-PAL" && version == "0100",
        "unsupported palette format"
    );
    require(count > 0 && count <= 65536, "palette color count is invalid");

    LegacyPalette palette;
    palette.colors.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        int red{};
        int green{};
        int blue{};
        require(
            static_cast<bool>(input >> red >> green >> blue),
            "palette color table is truncated"
        );
        require(
            red >= 0 && red <= 255 && green >= 0 && green <= 255 &&
                blue >= 0 && blue <= 255,
            "palette channel is invalid"
        );
        palette.colors.push_back({
            static_cast<std::uint8_t>(red),
            static_cast<std::uint8_t>(green),
            static_cast<std::uint8_t>(blue)
        });
    }
    return palette;
}

DrsArchive::DrsArchive(const std::filesystem::path& path)
    : path_{path} {
    const std::vector<std::byte> bytes = read_file(path);
    file_size_ = bytes.size();
    require(bytes.size() >= drs_header_size, "DRS header is truncated");
    require(
        std::string_view{
            reinterpret_cast<const char*>(bytes.data() + 40),
            4
        }.starts_with("1."),
        "unsupported DRS version"
    );
    const std::int32_t raw_table_count = i32(bytes, 56);
    require(
        raw_table_count >= 0 &&
            raw_table_count <= static_cast<std::int32_t>(maximum_table_count),
        "DRS table count is invalid"
    );
    const std::uint32_t table_count =
        static_cast<std::uint32_t>(raw_table_count);
    checked_range(
        drs_header_size,
        static_cast<std::uint64_t>(table_count) * drs_table_size,
        file_size_,
        "DRS table directory is truncated"
    );

    for (std::uint32_t table = 0; table < table_count; ++table) {
        const std::size_t table_offset =
            drs_header_size + table * drs_table_size;
        std::string extension;
        for (int index = 3; index >= 0; --index) {
            extension.push_back(
                static_cast<char>(byte_at(bytes, table_offset + index))
            );
        }
        extension = normalized_extension(extension);
        const std::int32_t raw_info_offset = i32(bytes, table_offset + 4);
        const std::int32_t raw_entry_count = i32(bytes, table_offset + 8);
        require(
            raw_info_offset >= 0 && raw_entry_count >= 0 &&
                raw_entry_count <=
                    static_cast<std::int32_t>(maximum_entry_count),
            "DRS table metadata is invalid"
        );
        const std::uint64_t info_offset =
            static_cast<std::uint32_t>(raw_info_offset);
        const std::uint32_t entry_count =
            static_cast<std::uint32_t>(raw_entry_count);
        checked_range(
            info_offset,
            static_cast<std::uint64_t>(entry_count) * drs_entry_size,
            file_size_,
            "DRS file index is truncated"
        );
        for (std::uint32_t entry = 0; entry < entry_count; ++entry) {
            const std::size_t offset =
                static_cast<std::size_t>(info_offset) +
                entry * drs_entry_size;
            const std::int32_t id = i32(bytes, offset);
            const std::int32_t raw_data_offset = i32(bytes, offset + 4);
            const std::int32_t raw_data_size = i32(bytes, offset + 8);
            require(
                id >= 0 && raw_data_offset >= 0 && raw_data_size >= 0,
                "DRS file entry is invalid"
            );
            const Entry value{
                static_cast<std::uint32_t>(raw_data_offset),
                static_cast<std::uint32_t>(raw_data_size)
            };
            checked_range(
                value.offset,
                value.size,
                file_size_,
                "DRS file entry points outside archive"
            );
            require(
                entries_.emplace(
                    std::make_pair(extension, id),
                    value
                ).second,
                "DRS contains duplicate resource identifier"
            );
        }
    }
}

bool DrsArchive::contains(
    std::string extension,
    std::int32_t resource_id
) const {
    return entries_.contains({
        normalized_extension(std::move(extension)),
        resource_id
    });
}

std::vector<std::byte> DrsArchive::read(
    std::string extension,
    std::int32_t resource_id
) const {
    const auto found = entries_.find({
        normalized_extension(std::move(extension)),
        resource_id
    });
    require(found != entries_.end(), "DRS resource does not exist");
    std::ifstream input{path_, std::ios::binary};
    require(input.is_open(), "cannot reopen legacy asset archive");
    input.seekg(static_cast<std::streamoff>(found->second.offset));
    std::vector<std::byte> bytes(
        static_cast<std::size_t>(found->second.size)
    );
    if (!bytes.empty()) {
        input.read(
            reinterpret_cast<char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size())
        );
    }
    require(
        input.gcount() == static_cast<std::streamsize>(bytes.size()),
        "DRS resource read is truncated"
    );
    return bytes;
}

std::vector<std::int32_t> DrsArchive::resource_ids(
    std::string extension
) const {
    extension = normalized_extension(std::move(extension));
    std::vector<std::int32_t> ids;
    for (const auto& [key, entry] : entries_) {
        static_cast<void>(entry);
        if (key.first == extension) {
            ids.push_back(key.second);
        }
    }
    return ids;
}

std::size_t DrsArchive::entry_count() const noexcept {
    return entries_.size();
}

LegacyWavResources::LegacyWavResources(
    const std::filesystem::path& installation_root
) {
    std::filesystem::path root = installation_root;
    if (std::filesystem::is_directory(root / "app" / "Data")) {
        root /= "app";
    }
    const std::array candidates{
        root / "Data" / "sounds.drs",
        root / "Data" / "sounds_x1.drs",
        root / "Data" / "sounds_x2.drs"
    };
    for (const auto& candidate : candidates) {
        if (!std::filesystem::is_regular_file(candidate)) {
            continue;
        }
        try {
            archives_.emplace_back(candidate);
        } catch (const std::exception&) {
            continue;
        }
        const std::size_t archive_index = archives_.size() - 1;
        for (const std::int32_t id :
             archives_.back().resource_ids("wav")) {
            resources_[id] = archive_index;
        }
    }
}

bool LegacyWavResources::contains(std::int32_t resource_id) const noexcept {
    return resources_.contains(resource_id);
}

std::vector<std::byte> LegacyWavResources::read(
    std::int32_t resource_id
) const {
    const auto found = resources_.find(resource_id);
    require(
        found != resources_.end(),
        "legacy WAV resource does not exist"
    );
    std::vector<std::byte> bytes =
        archives_[found->second].read("wav", resource_id);
    require(
        bytes.size() >= 12 &&
            std::string_view{
                reinterpret_cast<const char*>(bytes.data()), 4
            } == "RIFF" &&
            std::string_view{
                reinterpret_cast<const char*>(bytes.data() + 8), 4
            } == "WAVE",
        "DRS WAV resource has invalid RIFF header"
    );
    return bytes;
}

std::vector<std::int32_t> LegacyWavResources::resource_ids() const {
    std::vector<std::int32_t> ids;
    ids.reserve(resources_.size());
    for (const auto& [id, archive] : resources_) {
        static_cast<void>(archive);
        ids.push_back(id);
    }
    return ids;
}

std::size_t LegacyWavResources::archive_count() const noexcept {
    return archives_.size();
}

std::vector<std::filesystem::path> discover_legacy_music_tracks(
    const std::filesystem::path& installation_root
) {
    std::filesystem::path root = installation_root;
    if (std::filesystem::is_directory(root / "app" / "Sound")) {
        root /= "app";
    }
    const std::filesystem::path stream = root / "Sound" / "stream";
    std::vector<std::filesystem::path> tracks;
    std::error_code error;
    std::filesystem::directory_iterator iterator{stream, error};
    const std::filesystem::directory_iterator end;
    while (!error && iterator != end) {
        const auto& entry = *iterator;
        if (entry.is_regular_file(error)) {
            std::string extension = entry.path().extension().string();
            std::ranges::transform(
                extension, extension.begin(),
                [](unsigned char character) {
                    return static_cast<char>(std::tolower(character));
                }
            );
            if (extension == ".mp3" || extension == ".wav") {
                tracks.push_back(entry.path());
            }
        }
        iterator.increment(error);
    }
    std::ranges::sort(
        tracks,
        [](const std::filesystem::path& left,
           const std::filesystem::path& right) {
            std::string left_name = left.filename().string();
            std::string right_name = right.filename().string();
            std::ranges::transform(
                left_name, left_name.begin(),
                [](unsigned char character) {
                    return static_cast<char>(std::tolower(character));
                }
            );
            std::ranges::transform(
                right_name, right_name.begin(),
                [](unsigned char character) {
                    return static_cast<char>(std::tolower(character));
                }
            );
            if (left_name != right_name) {
                return left_name < right_name;
            }
            return left.filename().string() <
                right.filename().string();
        }
    );
    return tracks;
}

std::size_t slp_frame_count(std::span<const std::byte> slp) {
    require(slp.size() >= slp_header_size, "SLP header is truncated");
    const std::string_view version{
        reinterpret_cast<const char*>(slp.data()),
        4
    };
    require(
        version.starts_with("2.0"),
        "only classic SLP 2.0 is supported"
    );
    const std::int32_t raw_count = i32(slp, 4);
    require(
        raw_count >= 0 &&
            raw_count <= static_cast<std::int32_t>(maximum_frame_count),
        "SLP frame count is invalid"
    );
    checked_range(
        slp_header_size,
        static_cast<std::uint64_t>(raw_count) * slp_frame_info_size,
        slp.size(),
        "SLP frame directory is truncated"
    );
    return static_cast<std::size_t>(raw_count);
}

RgbaFrame decode_slp_frame(
    std::span<const std::byte> slp,
    const LegacyPalette& palette,
    std::size_t frame_index,
    PlayerSlotId player
) {
    const std::size_t count = slp_frame_count(slp);
    require(frame_index < count, "SLP frame index is invalid");
    const FrameInfo info = frame_info(slp, frame_index);
    const bool direct_rgba = (info.properties & 0x0F) == 7;
    const auto player_color = [&](std::uint8_t source) {
        const auto resolved =
            resolve_player_color_palette_index(player, source);
        require(
            resolved.has_value(),
            "SLP player color source index is unsupported"
        );
        return palette_color(palette, *resolved);
    };

    RgbaFrame frame{
        info.width,
        info.height,
        info.hotspot_x,
        info.hotspot_y,
        std::vector<std::uint8_t>(
            static_cast<std::size_t>(info.width) *
                static_cast<std::size_t>(info.height) * 4,
            0
        )
    };

    for (std::size_t row = 0;
         row < static_cast<std::size_t>(info.height);
         ++row) {
        const std::uint16_t left = u16(slp, info.outline + row * 4);
        const std::uint16_t right = u16(slp, info.outline + row * 4 + 2);
        if (left == 0x8000 || right == 0x8000) {
            continue;
        }
        require(
            static_cast<std::uint32_t>(left) +
                    static_cast<std::uint32_t>(right) <=
                static_cast<std::uint32_t>(info.width),
            "SLP row outline is invalid"
        );
        std::size_t column = left;
        const std::size_t row_end =
            static_cast<std::size_t>(info.width) - right;
        std::size_t position = u32(slp, info.commands + row * 4);
        bool ended = false;
        while (!ended) {
            require(position < slp.size(), "SLP command stream is truncated");
            const std::uint8_t command = byte_at(slp, position++);
            const std::uint8_t low = command & 0x0F;
            const std::uint8_t high = command & 0xF0;
            const std::uint8_t crumb = command & 0x03;

            auto next_count = [&](unsigned shift) {
                std::size_t amount = command >> shift;
                if (amount == 0) {
                    amount = byte_at(slp, position++);
                }
                return amount;
            };
            auto draw = [&](const std::array<std::uint8_t, 4>& color) {
                require(column < row_end, "SLP row draws too many pixels");
                set_pixel(frame, row, column++, color);
            };
            auto ordinary_color = [&]() {
                if (!direct_rgba) {
                    return palette_color(
                        palette, byte_at(slp, position++)
                    );
                }
                checked_range(
                    position, 4, slp.size(),
                    "SLP RGBA color is truncated"
                );
                const std::array<std::uint8_t, 4> color{
                    byte_at(slp, position + 2),
                    byte_at(slp, position + 1),
                    byte_at(slp, position),
                    byte_at(slp, position + 3),
                };
                position += 4;
                return color;
            };
            if (low == 0x0F) {
                ended = true;
            } else if (crumb == 0) {
                const std::size_t amount = command >> 2;
                checked_range(
                    position,
                    amount * (direct_rgba ? 4U : 1U),
                    slp.size(),
                    "SLP color list is truncated"
                );
                for (std::size_t index = 0; index < amount; ++index) {
                    draw(ordinary_color());
                }
            } else if (crumb == 1) {
                const std::size_t amount = next_count(2);
                require(amount <= row_end - column, "SLP skip exceeds row");
                column += amount;
            } else if (low == 0x02 || low == 0x03) {
                const std::size_t amount =
                    (static_cast<std::size_t>(high) << 4) +
                    byte_at(slp, position++);
                require(amount <= row_end - column, "SLP long command exceeds row");
                if (low == 0x02) {
                    checked_range(
                        position,
                        amount * (direct_rgba ? 4U : 1U),
                        slp.size(),
                        "SLP long color list is truncated"
                    );
                    for (std::size_t index = 0; index < amount; ++index) {
                        draw(ordinary_color());
                    }
                } else {
                    column += amount;
                }
            } else if (low == 0x06) {
                const std::size_t amount = next_count(4);
                checked_range(position, amount, slp.size(), "SLP player color list is truncated");
                for (std::size_t index = 0; index < amount; ++index) {
                    draw(player_color(byte_at(slp, position++)));
                }
            } else if (low == 0x07 || low == 0x0A) {
                const std::size_t amount = next_count(4);
                if (low == 0x0A) {
                    const auto color =
                        player_color(byte_at(slp, position++));
                    for (std::size_t index = 0; index < amount; ++index) {
                        draw(color);
                    }
                } else {
                    const auto color = ordinary_color();
                    for (std::size_t index = 0; index < amount; ++index) {
                        draw(color);
                    }
                }
            } else if (low == 0x0B) {
                const std::size_t amount = next_count(4);
                for (std::size_t index = 0; index < amount; ++index) {
                    draw({0, 0, 0, 100});
                }
            } else if (low == 0x0E) {
                std::size_t amount{};
                std::array<std::uint8_t, 4> color{};
                if (high == 0x40 || high == 0x60) {
                    amount = 1;
                } else if (high == 0x50 || high == 0x70) {
                    amount = byte_at(slp, position++);
                } else if (high <= 0x30) {
                    continue;
                } else if (direct_rgba && high == 0x90) {
                    const std::size_t amount =
                        byte_at(slp, position++);
                    checked_range(
                        position, amount * 4, slp.size(),
                        "SLP premultiplied-alpha colors are truncated"
                    );
                    for (std::size_t index = 0;
                         index < amount;
                         ++index) {
                        const std::array<std::uint8_t, 4> color{
                            byte_at(slp, position + 2),
                            byte_at(slp, position + 1),
                            byte_at(slp, position),
                            static_cast<std::uint8_t>(
                                255 - byte_at(slp, position + 3)
                            ),
                        };
                        position += 4;
                        draw(color);
                    }
                    continue;
                } else {
                    throw LegacyAssetError{
                        "unsupported classic SLP extended command"
                    };
                }
                color = high == 0x40 || high == 0x50
                    ? player_color(0)
                    : std::array<std::uint8_t, 4>{0, 0, 0, 255};
                for (std::size_t index = 0; index < amount; ++index) {
                    draw(color);
                }
            } else {
                throw LegacyAssetError{"unknown classic SLP command"};
            }
        }
        require(column == row_end, "SLP row has wrong pixel count");
    }
    return frame;
}

RgbaFrame decode_slp_frame(
    std::span<const std::byte> slp,
    const LegacyPalette& palette,
    std::size_t frame_index,
    unsigned player
) {
    const auto slot = legacy_slp_player_slot(player);
    require(slot.has_value(), "SLP player number is invalid");
    return decode_slp_frame(slp, palette, frame_index, *slot);
}

}  // namespace aoe
