#include "aoe/legacy_assets.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

std::vector<std::byte> read_bytes(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) throw std::runtime_error("cannot open SLP");
    const auto size = input.tellg();
    input.seekg(0);
    std::vector<std::byte> result(static_cast<std::size_t>(size));
    input.read(
        reinterpret_cast<char*>(result.data()),
        static_cast<std::streamsize>(result.size())
    );
    if (!input) throw std::runtime_error("cannot read SLP");
    return result;
}

void write_u16(std::ofstream& output, std::uint16_t value) {
    output.put(static_cast<char>(value));
    output.put(static_cast<char>(value >> 8U));
}

void write_u32(std::ofstream& output, std::uint32_t value) {
    for (int shift = 0; shift < 32; shift += 8) {
        output.put(static_cast<char>(value >> shift));
    }
}

void write_bmp(
    const std::filesystem::path& path,
    int width,
    int height,
    const std::vector<std::uint8_t>& rgba
) {
    std::ofstream output(path, std::ios::binary);
    const std::uint32_t pixel_bytes =
        static_cast<std::uint32_t>(width * height * 4);
    output.write("BM", 2);
    write_u32(output, 54 + pixel_bytes);
    write_u32(output, 0);
    write_u32(output, 54);
    write_u32(output, 40);
    write_u32(output, static_cast<std::uint32_t>(width));
    write_u32(output, static_cast<std::uint32_t>(height));
    write_u16(output, 1);
    write_u16(output, 32);
    write_u32(output, 0);
    write_u32(output, pixel_bytes);
    write_u32(output, 0);
    write_u32(output, 0);
    write_u32(output, 0);
    write_u32(output, 0);
    for (int y = height - 1; y >= 0; --y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t at =
                static_cast<std::size_t>((y * width + x) * 4);
            output.put(static_cast<char>(rgba[at + 2]));
            output.put(static_cast<char>(rgba[at + 1]));
            output.put(static_cast<char>(rgba[at]));
            output.put(static_cast<char>(rgba[at + 3]));
        }
    }
    if (!output) throw std::runtime_error("cannot write BMP");
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 4 && argc != 5) {
        std::cerr
            << "usage: slp_contact_sheet SLP INTERFAC_DRS OUTPUT\n"
            << "   or: slp_contact_sheet GRAPHICS_DRS INTERFAC_DRS "
               "SLP_ID OUTPUT\n";
        return 2;
    }
    try {
        std::vector<std::byte> bytes;
        std::int32_t palette_id = 50589;
        const std::filesystem::path output = argv[argc - 1];
        if (argc == 5) {
            std::size_t consumed{};
            const std::string text = argv[3];
            const long long parsed = std::stoll(text, &consumed);
            if (consumed != text.size() || parsed < 0 ||
                parsed > std::numeric_limits<std::int32_t>::max()) {
                throw std::runtime_error{"SLP ID must be a non-negative integer"};
            }
            const aoe::DrsArchive graphics{argv[1]};
            bytes = graphics.read("slp", static_cast<std::int32_t>(parsed));
            palette_id = 50500;
        } else {
            bytes = read_bytes(argv[1]);
        }
        const aoe::DrsArchive interface{argv[2]};
        const aoe::LegacyPalette palette = aoe::LegacyPalette::from_jasc(
            interface.read("bina", palette_id)
        );
        std::vector<aoe::RgbaFrame> frames;
        int cell_width{};
        int cell_height{};
        for (std::size_t index = 0;
             index < aoe::slp_frame_count(bytes);
             ++index) {
            frames.push_back(aoe::decode_slp_frame(
                bytes, palette, index
            ));
            cell_width = std::max(cell_width, frames.back().width);
            cell_height = std::max(cell_height, frames.back().height);
            std::cout << index << '\t' << frames.back().width << 'x'
                      << frames.back().height << '\t'
                      << frames.back().hotspot_x << ','
                      << frames.back().hotspot_y << '\n';
        }
        const int columns = std::min(4, static_cast<int>(frames.size()));
        if (columns == 0) {
            throw std::runtime_error{"SLP contains no frames"};
        }
        const int rows =
            (static_cast<int>(frames.size()) + columns - 1) / columns;
        const int width = columns * cell_width;
        const int height = rows * cell_height;
        std::vector<std::uint8_t> sheet(
            static_cast<std::size_t>(width * height * 4)
        );
        for (std::size_t index = 0; index < frames.size(); ++index) {
            const auto& frame = frames[index];
            const int left = static_cast<int>(index % columns) * cell_width;
            const int top = static_cast<int>(index / columns) * cell_height;
            for (int y = 0; y < frame.height; ++y) {
                for (int x = 0; x < frame.width; ++x) {
                    const std::size_t source =
                        static_cast<std::size_t>(
                            (y * frame.width + x) * 4
                        );
                    if (frame.rgba[source + 3] == 0) continue;
                    const std::size_t target =
                        static_cast<std::size_t>(
                            ((top + y) * width + left + x) * 4
                        );
                    std::copy_n(
                        frame.rgba.begin() +
                            static_cast<std::ptrdiff_t>(source),
                        4,
                        sheet.begin() +
                            static_cast<std::ptrdiff_t>(target)
                    );
                }
            }
        }
        write_bmp(output, width, height, sheet);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
