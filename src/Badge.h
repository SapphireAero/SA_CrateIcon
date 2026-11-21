#pragma once

#include <algorithm>
#include <cstdint>

namespace badge
{
using Pixel = std::uint16_t;

constexpr Pixel dim(Pixel color)
{
    auto scale = [](unsigned channel) { return (3 * channel + 2) / 4; };
    return static_cast<Pixel>((scale(color >> 11) << 11) |
                             (scale((color >> 5) & 63) << 5) | scale(color & 31));
}

static_assert(dim(0xFFFF) == 0xBDF7 && dim(0) == 0, "Keep 75% of RGB565 brightness");

struct Label { char left, right; Pixel border; };
constexpr Label labels[] = {
    {'$', ' ', 0xFDC0}, {'U', ' ', 0x07FC}, {'+', ' ', 0x67EB}, {'C', 'L', 0xC3FF},
    {'E', 'X', 0xFBE0}, {'B', 'X', 0xFB00}, {'$', ' ', 0xFDC0}, {'S', 'H', 0xABBD},
    {'M', 'P', 0x667F}, {'D', 'F', 0x3CDF}, {'S', 'P', 0x77FF}, {'A', 'T', 0xFF80},
    {'N', ' ', 0xFD44}, {'I', 'V', 0xFFDC}, {'U', 'P', 0x67FF}, {'I', 'S', 0xE47F},
};

struct Letter { char key; unsigned rows[5]; };
constexpr Letter alphabet[] = {
    {'$', {3,6,3,3,6}}, {'+', {0,2,7,2,0}}, {'A', {2,5,7,5,5}},
    {'B', {6,5,6,5,6}}, {'C', {3,4,4,4,3}}, {'D', {6,5,5,5,6}},
    {'E', {7,4,6,4,7}}, {'F', {7,4,6,4,4}}, {'H', {5,5,7,5,5}},
    {'I', {7,2,2,2,7}}, {'L', {4,4,4,4,7}}, {'M', {5,7,7,5,5}},
    {'N', {5,7,7,7,5}}, {'P', {6,5,6,4,4}}, {'S', {3,4,2,1,6}},
    {'T', {7,2,2,2,2}}, {'U', {5,5,5,5,7}}, {'V', {5,5,5,5,2}},
    {'X', {5,5,2,5,5}},
};

inline const unsigned* glyph(char key)
{
    for (const auto& entry : alphabet)
        if (entry.key == key) return entry.rows;
    return nullptr;
}

struct Canvas { unsigned char* pixels; int width, height, pitch; };

inline bool draw(Canvas canvas, int centerX, int bottom, unsigned type, bool shadow)
{
    if (type >= sizeof(labels) / sizeof(labels[0])) return false;
    constexpr int W = 19, H = 14;
    const auto& label = labels[type];
    const auto* first = glyph(label.left);
    const auto* second = glyph(label.right);
    const int left = centerX - W / 2;
    const int top = bottom - H;
    const int x0 = std::max(0, left), x1 = std::min(canvas.width, left + W);
    const int y0 = std::max(0, top), y1 = std::min(canvas.height, top + H);
    if (x0 >= x1 || y0 >= y1) return false;
    Pixel colors[] = {0x2108, label.border, 0xFFFF};
    if (shadow) for (auto& color : colors) color = dim(color);
    const int start = second ? 2 : 6;
    for (int y = y0; y < y1; ++y)
    {
        auto* row = reinterpret_cast<Pixel*>(canvas.pixels + y * canvas.pitch);
        for (int x = x0; x < x1; ++x)
        {
            const int px = x - left, py = y - top;
            const bool edge = px == 0 || px == W - 1 || py == 0 || py == H - 1;
            const int letterX = px - start;
            const unsigned* pattern = letterX < 8 ? first : second;
            const int column = letterX < 8 ? letterX : letterX - 8;
            const bool lit = py >= 2 && py < 12 && pattern && column >= 0 && column < 6 &&
                             (pattern[(py - 2) / 2] & (4u >> (column / 2)));
            row[x] = colors[lit ? 2 : edge ? 1 : 0];
        }
    }
    return true;
}
}
