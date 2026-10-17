// Pixel-exact Cairo conversion regression test; optional timing comparison.
#include "draw.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace {
#define CHECK(expr) do { if (!(expr)) throw std::runtime_error(#expr); } while (false)

// The previous conversion algorithm, retained here as an independent reference.
void referenceRgba(cairo_surface_t* surface, std::vector<uint8_t>& out) {
    static const auto table = [] {
        std::vector<uint8_t> values(256 * 256);
        for (unsigned a = 0; a < 256; ++a) {
            for (unsigned c = 0; c < 256; ++c) {
                values[a * 256 + c] = a == 0 ? 0 : static_cast<uint8_t>(std::min(255u, (c * 255 + a / 2) / a));
            }
        }
        return values;
    }();
    cairo_surface_flush(surface);
    const int width = cairo_image_surface_get_width(surface), height = cairo_image_surface_get_height(surface);
    const int stride = cairo_image_surface_get_stride(surface);
    const uint8_t* data = cairo_image_surface_get_data(surface);
    out.resize(static_cast<size_t>(width) * height * 4);
    uint8_t* dst = out.data();
    for (int y = 0; y < height; ++y) {
        const auto* row = reinterpret_cast<const uint32_t*>(data + static_cast<size_t>(y) * stride);
        for (int x = 0; x < width; ++x) {
            const uint32_t p = row[x], a = p >> 24;
            const uint8_t* line = table.data() + a * 256;
            *dst++ = line[(p >> 16) & 255];
            *dst++ = line[(p >> 8) & 255];
            *dst++ = line[p & 255];
            *dst++ = static_cast<uint8_t>(a);
        }
    }
}

// Direct integer arithmetic is the accuracy oracle, independently of either
// conversion implementation's lookup table or fast paths.
void expectedRgba(cairo_surface_t* surface, std::vector<uint8_t>& out) {
    cairo_surface_flush(surface);
    const int width = cairo_image_surface_get_width(surface), height = cairo_image_surface_get_height(surface);
    const int stride = cairo_image_surface_get_stride(surface);
    const uint8_t* data = cairo_image_surface_get_data(surface);
    out.resize(static_cast<size_t>(width) * height * 4);
    uint8_t* dst = out.data();
    for (int y = 0; y < height; ++y) {
        const auto* row = reinterpret_cast<const uint32_t*>(data + static_cast<size_t>(y) * stride);
        for (int x = 0; x < width; ++x) {
            const uint32_t p = row[x], a = p >> 24;
            const auto channel = [a](uint32_t c) {
                return a == 0 ? 0 : static_cast<uint8_t>(std::min(255u, (c * 255 + a / 2) / a));
            };
            *dst++ = channel((p >> 16) & 255);
            *dst++ = channel((p >> 8) & 255);
            *dst++ = channel(p & 255);
            *dst++ = static_cast<uint8_t>(a);
        }
    }
}

void pixels() {
    // Every channel/alpha combination, including invalid premultiplied channels
    // (the public helper previously saturated these rather than wrapping them).
    cairo_surface_t* image = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 256, 256);
    CHECK(cairo_surface_status(image) == CAIRO_STATUS_SUCCESS);
    auto* data = reinterpret_cast<uint32_t*>(cairo_image_surface_get_data(image));
    for (unsigned a = 0; a < 256; ++a) {
        for (unsigned c = 0; c < 256; ++c) data[a * 256 + c] = (a << 24) | (c << 16) | ((255 - c) << 8) | ((c + a) & 255);
    }
    cairo_surface_mark_dirty(image);
    std::vector<uint8_t> actual, expected;
    expectedRgba(image, expected);
    surfaceToRgba(image, actual);
    CHECK(actual == expected);
    cairo_surface_destroy(image);

    // Exercise complete blocks, every tail length, poisoned row padding and
    // buffer reuse. Rows cover opaque, zero-alpha with RGB, fractional and mixed blocks.
    constexpr int height = 4;
    const uint32_t fractional[] = {0x01010002, 0x7f7f20ff, 0xfe80fe40, 0x03030201};
    const uint32_t mixed[] = {0xff123456, 0x00123456, 0x807f4020, 0x0180ff02};
    for (int width = 1; width <= 17; ++width) {
        const int stride = (width + 4) * 4;
        std::vector<uint32_t> padded(stride / 4 * height, 0xdeadbeef);
        for (int x = 0; x < width; ++x) {
            padded[x] = 0xff000000 | (0x123456u + x);
            padded[stride / 4 + x] = 0x123456u + x;
            padded[2 * stride / 4 + x] = fractional[x % 4];
            padded[3 * stride / 4 + x] = mixed[x % 4];
        }
        image = cairo_image_surface_create_for_data(reinterpret_cast<uint8_t*>(padded.data()), CAIRO_FORMAT_ARGB32,
                                                    width, height, stride);
        CHECK(cairo_surface_status(image) == CAIRO_STATUS_SUCCESS);
        expectedRgba(image, expected);
        surfaceToRgba(image, actual);
        CHECK(actual == expected && actual.size() == static_cast<size_t>(width) * height * 4);
        cairo_surface_destroy(image);
    }
}

void benchmark() {
    const char* const kinds[] = {"opaque", "translucent", "opaque-with-edges", "transparent", "random-premultiplied"};
    for (const auto& dimensions : {std::pair<int, int>{256, 256}, {1200, 788}}) {
        for (int kind = 0; kind < 5; ++kind) {
            const int width = dimensions.first, height = dimensions.second;
            cairo_surface_t* image = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
            auto* data = reinterpret_cast<uint32_t*>(cairo_image_surface_get_data(image));
            uint32_t random = 0x9e3779b9;
            const auto nextRandom = [&random] {
                random ^= random << 13;
                random ^= random >> 17;
                random ^= random << 5;
                return random;
            };
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    unsigned a = 255;
                    if (kind == 1) a = static_cast<unsigned>((x + y) % 254 + 1);
                    if (kind == 2) a = x > 8 && x < width - 8 && y > 8 && y < height - 8 ? 255 : (x + y) % 256;
                    if (kind == 3) a = 0;
                    if (kind == 4) a = nextRandom() >> 24;
                    const unsigned r = kind == 4 ? nextRandom() % (a + 1) : a;
                    const unsigned g = kind == 4 ? nextRandom() % (a + 1) : a / 2;
                    const unsigned b = kind == 4 ? nextRandom() % (a + 1) : a / 3;
                    data[y * width + x] = (a << 24) | (r << 16) | (g << 8) | b;
                }
            }
            cairo_surface_mark_dirty(image);
            std::vector<uint8_t> actual, expected;
            referenceRgba(image, expected);
            surfaceToRgba(image, actual);
            CHECK(actual == expected);
            constexpr int repeats = 200;
            const auto measure = [&](auto convert) {
                const auto start = std::chrono::steady_clock::now();
                for (int i = 0; i < repeats; ++i) {
                    convert(image, actual);
                    asm volatile("" : : "g"(actual.data()) : "memory");
                }
                return std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - start).count() / repeats;
            };
            // Alternate order across five samples; report the median to reduce noise.
            std::vector<double> oldTimes, newTimes;
            for (int sample = 0; sample < 5; ++sample) {
                if (sample % 2 == 0) { oldTimes.push_back(measure(referenceRgba)); newTimes.push_back(measure(surfaceToRgba)); }
                else { newTimes.push_back(measure(surfaceToRgba)); oldTimes.push_back(measure(referenceRgba)); }
            }
            std::sort(oldTimes.begin(), oldTimes.end()); std::sort(newTimes.begin(), newTimes.end());
            std::printf("RGBA %dx%d %s: reference=%.3f us current=%.3f us ratio=%.4f (%d repeats x5)\n",
                        width, height, kinds[kind],
                        oldTimes[2], newTimes[2], newTimes[2] / oldTimes[2], repeats);
            cairo_surface_destroy(image);
        }
    }
}
} // namespace

int main(int argc, char** argv) {
    try {
        pixels();
        if (argc == 2 && std::strcmp(argv[1], "--benchmark") == 0) benchmark();
        std::puts("Exhaustive RGBA conversion, block tails, padded stride and buffer-reuse tests passed.");
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
