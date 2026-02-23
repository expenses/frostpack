#include "frostpack.h"

#include <fstream>
#include <random>

void write_colored_ppm(
    const char* path,
    const frostpack::BitArray2D& atlas,
    const std::vector<frostpack::BitArray2D>& masks,
    const std::vector<frostpack::UVec2>& locations) {
    std::vector<uint8_t> img(atlas.width * atlas.height * 3, 0);

    std::mt19937 rng(42);
    std::uniform_int_distribution<uint32_t> dist(80, 255);

    for (size_t i = 0; i < masks.size(); i++) {
        auto r = uint8_t(dist(rng));
        auto g = uint8_t(dist(rng));
        auto b = uint8_t(dist(rng));
        const auto& mask = masks[i];
        const auto& loc = locations[i];
        for (uint32_t y = 0; y < mask.height; y++) {
            for (uint32_t x = 0; x < mask.width; x++) {
                if (mask.get(x, y)) {
                    auto idx = ((loc.y + y) * atlas.width + (loc.x + x)) * 3;
                    img[idx + 0] = r;
                    img[idx + 1] = g;
                    img[idx + 2] = b;
                }
            }
        }
    }

    std::ofstream f(path, std::ios::binary);
    f << "P6\n" << atlas.width << " " << atlas.height << "\n255\n";
    f.write(reinterpret_cast<const char*>(img.data()), std::streamsize(img.size()));
}

int main(int argc, char** argv) {
    std::vector<frostpack::BitArray2D> masks;

    for (int i = 0; i < 1000; i++) {
        for (int j = 0; j < 3; j++) {
            masks.push_back(frostpack::raster_island(
                {{{{50, 50}, {4, 50}, {50, 4}}}, {{{4, 4}, {4, 50}, {50, 4}}}

                }));
        }

        for (int j = 0; j < 10; j++) {
            masks.push_back(frostpack::raster_island({{{{50, 50}, {40, 50}, {50, 40}}}}));
            masks.push_back(frostpack::raster_island({{{{500, 500}, {495, 500}, {500, 495}}}}));
        }
    }

    for (auto i = 1; i < argc; i++) {
        const auto filepath = argv[i];

        std::ifstream stream(filepath, std::ios::binary);
        if (!stream) {
            printf("bad filepath: %s\n", filepath);
            return 1;
        }

        frostpack::BitArray2D mask;
        stream.read(reinterpret_cast<char*>(&mask.height), 4);
        stream.read(reinterpret_cast<char*>(&mask.width), 4);
        mask.data = std::vector<uint64_t>(mask.width_in_chunks() * mask.height);

        for (uint32_t y = 0; y < mask.height; y++) {
            for (uint32_t x = 0; x < mask.width; x++) {
                uint8_t value;
                stream.read(reinterpret_cast<char*>(&value), 1);
                if (value) {
                    mask.set(x, y);
                }
            }
        }

        for (int j = 0; j < 4; j++) {
            masks.push_back(mask);
        }
    }

    const uint32_t atlas_width = 4096 << 1;

    uint32_t max_width = 0;
    for (const auto& mask : masks) {
        max_width = std::max(max_width, mask.width);
    }

    if (max_width > atlas_width) {
        printf("%u > %u\n", max_width, atlas_width);
        return 1;
    }

    std::sort(masks.begin(), masks.end(), [](const auto& a, const auto& b) {
        return a.sort_key() > b.sort_key();
    });

    frostpack::Atlas atlas;
    atlas.array.width = atlas_width;
    atlas.array.height = 1;
    atlas.array.data = std::vector<uint64_t>(atlas.array.width_in_chunks());

    std::vector<frostpack::UVec2> locations;

    for (size_t i = 0; i < masks.size(); i++) {
        const auto& mask = masks[i];

        auto location = atlas.place(mask);

        printf(
            "%zu/%zu - %ux%u @ %ux%u\n",
            i,
            masks.size(),
            mask.width,
            mask.height,
            location.x,
            location.y);
        locations.push_back(location);
    }

    write_colored_ppm("output.ppm", atlas.array, masks, locations);
    return 0;
}
