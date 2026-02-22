#include <cstdio>
#include <vector>
#include <cstdint>
#include <fstream>
#include <algorithm>
#include <random>
#include <array>
#include <cfloat>

struct Vec2 {
    float x;
    float y;
};

struct UVec2 {
    uint32_t x;
    uint32_t y;
};

struct BitArray2D {
    uint32_t width;
    uint32_t height;
    std::vector<uint64_t> data;

    uint32_t width_in_chunks() const {
        return (width + 63) / 64;
    }

    uint64_t get_chunk_const(uint32_t chunk_x, uint32_t y) const {
        return data[y * width_in_chunks() + chunk_x];
    }

    uint64_t& get_chunk(uint32_t chunk_x, uint32_t y) {
        return data[y * width_in_chunks() + chunk_x];
    }

    bool get(uint32_t x, uint32_t y) const {
        auto mask = uint64_t(1) << (x % 64);
        return get_chunk_const(x / 64, y) & mask;
    }

    void set(uint32_t x, uint32_t y) {
        auto mask = uint64_t(1) << (x % 64);
        get_chunk(x / 64, y) |= mask;
    }
};

// The equiv of cross2d(x - y, z - w)
// where cross2d is a.x * b.y - a.y * b.x.
float cross2d_points(Vec2 x, Vec2 y, Vec2 z, Vec2 w) {
    const Vec2 a = {x.x - y.x, x.y - y.y};
    const Vec2 b = {z.x - w.x, z.y - w.y};
    return a.x * b.y - a.y * b.x;
}

static bool point_in_tri(Vec2 p, Vec2 a, Vec2 b, Vec2 c) {
    const auto d0 = cross2d_points(b, a, p, a);
    const auto d1 = cross2d_points(c, b, p, b);
    const auto d2 = cross2d_points(a, c, p, c);
    const auto has_neg = (d0 < 0) || (d1 < 0) || (d2 < 0);
    const auto has_pos = (d0 > 0) || (d1 > 0) || (d2 > 0);
    return !(has_neg && has_pos);
}

static bool segments_intersect(Vec2 a, Vec2 b, Vec2 c, Vec2 d) {
    const auto d0 = cross2d_points(b, a, c, a);
    const auto d1 = cross2d_points(b, a, d, a);
    const auto d2 = cross2d_points(d, c, a, c);
    const auto d3 = cross2d_points(d, c, b, c);
    return (((d0 > 0 && d1 < 0) || (d0 < 0 && d1 > 0)) &&
            ((d2 > 0 && d3 < 0) || (d2 < 0 && d3 > 0)));
}

bool tri_intersects_box(const std::array<Vec2, 3>& tri, Vec2 box_center, Vec2 half_size) {
    const Vec2 b_min = {box_center.x - half_size.x, box_center.y - half_size.y};
    const Vec2 b_max = {box_center.x + half_size.x, box_center.y + half_size.y};

    const std::array<Vec2, 4> box_corners = {
        b_min,
        Vec2{b_min.x, b_max.y},
        b_max,
        Vec2{b_max.x, b_min.y},
    };

    // Any triangle vertex inside the box?
    for (const auto& v : tri) {
        if (v.x > b_min.x && v.x < b_max.x && v.y > b_min.y && v.y < b_max.y)
            return true;
    }

    // Any box corner inside the triangle?
    for (const auto& corner : box_corners) {
        if (point_in_tri(corner, tri[0], tri[1], tri[2]))
            return true;
    }

    // Any triangle edge crosses any box edge?
    const std::array<std::pair<Vec2, Vec2>, 3> tri_edges = {{{tri[0], tri[1]}, {tri[1], tri[2]}, {tri[2], tri[0]}}};
    for (const auto& [ta, tb] : tri_edges) {
        for (size_t i = 0; i < 4; i++) {
            if (segments_intersect(ta, tb, box_corners[i], box_corners[(i + 1) % 4]))
                return true;
        }
    }

    return false;
}

BitArray2D raster_island(const std::vector<std::array<Vec2, 3>>& tris) {
    auto x_min = FLT_MAX;
    auto y_min = FLT_MAX;
    auto x_max = -FLT_MAX;
    auto y_max = -FLT_MAX;

    for (const auto& tri: tris) {
        for (const auto& v: tri) {
            x_min = std::min(x_min, v.x);
            y_min = std::min(y_min, v.y);
            x_max = std::max(x_max, v.x);
            y_max = std::max(y_max, v.y);
        }
    }

    const auto x_min_i = int(round(x_min)) - 1;
    const auto y_min_i = int(round(y_min)) - 1;
    const auto x_max_i = int(round(x_max)) + 1;
    const auto y_max_i = int(round(y_max)) + 1;

    BitArray2D mask;
    mask.width  = uint32_t(x_max_i - x_min_i);
    mask.height = uint32_t(y_max_i - y_min_i);
    mask.data   = std::vector<uint64_t>(mask.width_in_chunks() * mask.height);

    for (const auto& t : tris) {
        const auto tri_x_min = int(round(std::min({t[0].x, t[1].x, t[2].x}))) - 1;
        const auto tri_x_max = int(round(std::max({t[0].x, t[1].x, t[2].x}))) + 1;
        const auto tri_y_min = int(round(std::min({t[0].y, t[1].y, t[2].y}))) - 1;
        const auto tri_y_max = int(round(std::max({t[0].y, t[1].y, t[2].y}))) + 1;

        for (int y = tri_y_min; y < tri_y_max; y++) {
            for (int x = tri_x_min; x < tri_x_max; x++) {
                const auto mask_x = uint32_t(x - x_min_i);
                const auto mask_y = uint32_t(y - y_min_i);

                if (!mask.get(mask_x, mask_y)) {
                    if (tri_intersects_box(t, {x + 0.5f, y + 0.5f}, {1.0f, 1.0f})) {
                        mask.set(mask_x, mask_y);
                    }
                }
            }
        }
    }

    return mask;
}

bool check_placement(const BitArray2D& atlas, const BitArray2D& mask, uint32_t x, uint32_t y) {
    uint32_t chunk_offset = x / 64;
    uint32_t bit_offset   = x % 64;

    // 'y + mask_y < atlas.height' means that we don't check rows that go off the bottom edge.
    for (uint32_t mask_y = 0; mask_y < mask.height && y + mask_y < atlas.height; mask_y++) {
        for (uint32_t mask_x = 0; mask_x < mask.width_in_chunks(); mask_x++) {
            auto mask_chunk = mask.get_chunk_const(mask_x, mask_y);

            // Low chunk: mask bits shifted into position
            if (atlas.get_chunk_const(chunk_offset + mask_x, y + mask_y) & (mask_chunk << bit_offset)) {
                return false;
            }

            // High chunk: bits that spilled over the 64-bit boundary
            if (bit_offset > 0 && atlas.get_chunk_const(chunk_offset + mask_x + 1, y + mask_y) & (mask_chunk >> (64 - bit_offset))) {
                return false;
            }
        }
    }
    return true;
}

UVec2 find_placement(const BitArray2D& atlas, const BitArray2D& mask, uint32_t last_y) {
    // check every row, even if it goes off the bottom edge (we can just resize)
    for (uint32_t y = last_y; y < atlas.height; y++) {
        for (uint32_t x = 0; x <= atlas.width - mask.width; x++) {
            if (check_placement(atlas, mask, x, y)) {
                return { x, y };
            }
        }
    }
    return {0, atlas.height};
}

void copy_mask(BitArray2D& atlas, const BitArray2D& mask, UVec2 location) {
    auto chunk_offset = location.x / 64;
    auto bit_offset = location.x % 64;

    for (uint32_t y = 0; y < mask.height; y++) {
        for (uint32_t x = 0; x < mask.width_in_chunks(); x++) {
            auto mask_chunk = mask.get_chunk_const(x, y);

            atlas.get_chunk(
                chunk_offset + x,
                location.y + y
            ) |= mask_chunk << bit_offset;
            if (bit_offset > 0) {
                atlas.get_chunk(
                    chunk_offset + x + 1,
                    location.y + y
                ) |= mask_chunk >> (64 - bit_offset);
            }
        }
    }
}

void sort_masks(std::vector<BitArray2D>& masks) {
    std::sort(masks.begin(), masks.end(), [](const auto& a, const auto& b) {
        return a.width + a.height > b.width + b.height;
    });
}

std::vector<UVec2> place_masks(BitArray2D& atlas, const std::vector<BitArray2D>& masks) {
    std::vector<UVec2> locations;
    uint32_t last_perim = 0;
    uint32_t last_y = 0;

    for (size_t i = 0; i < masks.size(); i++) {
        const auto& mask = masks[i];

        if (mask.width + mask.height != last_perim) {
            last_y      = 0;
            last_perim = mask.width + mask.height;
        }

        auto location = find_placement(atlas, mask, last_y);
        auto end_height = location.y + mask.height;
        if (end_height > atlas.height) {
            atlas.data.resize(atlas.width_in_chunks() * end_height);
            atlas.height = end_height;
        }

        last_y = location.y;

        printf("%zu/%zu - %ux%u @ %ux%u\n", i, masks.size(), mask.width, mask.height, location.x, location.y);
        copy_mask(atlas, mask, location);
        locations.push_back(location);
    }

    return locations;
}

uint32_t max_mask_width(const std::vector<BitArray2D>& masks) {
    uint width = 0;
    for (const auto& mask: masks) {
        width = std::max(width, mask.width);
    }
    return width;
}


void write_colored_ppm(
    const char* path,
    const BitArray2D& atlas,
    const std::vector<BitArray2D>& masks,
    const std::vector<UVec2>& locations)
{
    std::vector<uint8_t> img(atlas.width * atlas.height * 3, 0);

    std::mt19937 rng(42);
    std::uniform_int_distribution<uint32_t> dist(80, 255);

    for (size_t i = 0; i < masks.size(); i++) {
        auto r = uint8_t(dist(rng));
        auto g = uint8_t(dist(rng));
        auto b = uint8_t(dist(rng));
        const auto& mask = masks[i];
        const auto& loc  = locations[i];
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
    std::vector<BitArray2D> masks;


    for (int i = 0; i < 1000; i++) {
        for (int j = 0; j < 3; j++) {
            masks.push_back(raster_island({
                {{
                    {50, 50},
                    {4, 50},
                    {50, 4}
                }},
                {{
                    {4, 4},
                    {4, 50},
                    {50, 4}
                }}
                
            }));
        }

        for (int j = 0; j < 10; j++) {
        masks.push_back(raster_island({
            {{
                {50, 50},
                {40, 50},
                {50, 40}
            }}
        }));
        masks.push_back(raster_island({
            {{
                {500, 500},
                {495, 500},
                {500, 495}
            }}
        }));
        }

    }




    for (auto i = 1; i < argc; i++) {
        const auto filepath = argv[i];

        std::ifstream stream(filepath, std::ios::binary);
        if (!stream) {
            printf("bad filepath: %s\n", filepath);
            return 1;
        }

        BitArray2D mask;
        stream.read(reinterpret_cast<char*>(&mask.height), 4);
        stream.read(reinterpret_cast<char*>(&mask.width),  4);
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

        masks.push_back(mask);
        masks.push_back(mask);
        masks.push_back(mask);
        masks.push_back(mask);
    }

    const auto max_width = max_mask_width(masks);
    const uint32_t atlas_width = 2048 << 2;

    if (max_width > atlas_width) {
        printf("%u > %u\n", max_width, atlas_width);
        return 1;
    }

    sort_masks(masks);

    BitArray2D atlas;
    atlas.width = atlas_width;
    atlas.height = 1;
    atlas.data = std::vector<uint64_t>(atlas.width_in_chunks());

    const auto locations = place_masks(atlas, masks);

    write_colored_ppm("output.ppm", atlas, masks, locations);
    return 0;
}
