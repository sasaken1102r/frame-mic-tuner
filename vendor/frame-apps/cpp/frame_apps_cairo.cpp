// SPDX-License-Identifier: MIT — part of frame-apps by sasaken1102r, shipped under the host app's MIT license
// See frame_apps_cairo.h.
#include "frame_apps_cairo.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstring>
#include <map>

namespace frame_apps {

namespace {

/** Reading position in a PNG held in memory. */
struct PngReader {
    const unsigned char* data;
    size_t size;
    size_t pos;
};

/** cairo_read_func_t over PngReader. */
cairo_status_t readPng(void* closure, unsigned char* out, unsigned int length) {
    PngReader* r = static_cast<PngReader*>(closure);
    if (r->pos + length > r->size) return CAIRO_STATUS_READ_ERROR;
    std::memcpy(out, r->data + r->pos, length);
    r->pos += length;
    return CAIRO_STATUS_SUCCESS;
}

/** Decodes a PNG in memory. @return the surface (128×128), or nullptr */
cairo_surface_t* decode(const unsigned char* data, size_t size) {
    PngReader reader {data, size, 0};
    cairo_surface_t* surface = cairo_image_surface_create_from_png_stream(readPng, &reader);
    if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS || cairo_image_surface_get_width(surface) != 128 ||
        cairo_image_surface_get_height(surface) != 128) {
        cairo_surface_destroy(surface);
        return nullptr;
    }
    return surface;
}

/** Draws a 128×128 surface scaled into a square. */
void paintScaled(cairo_t* cr, cairo_surface_t* icon, double x, double y, double size) {
    const double scale = size / cairo_image_surface_get_width(icon);
    cairo_save(cr);
    cairo_translate(cr, x, y);
    cairo_scale(cr, scale, scale);
    cairo_set_source_surface(cr, icon, 0, 0);
    // shrinking 128 → about 50: the "good" filter averages the pixels instead of picking some (no jaggies)
    cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_GOOD);
    cairo_paint(cr);
    cairo_restore(cr);
}

}  // namespace

cairo_surface_t* iconSurface(const std::string& name) {
    static std::map<std::string, cairo_surface_t*> cache;  // kept until the program ends
    const auto it = cache.find(name);
    if (it != cache.end()) return it->second;
    const EmbeddedIcon* icon = embeddedIcon(name);
    cairo_surface_t* surface = icon != nullptr ? decode(icon->data, icon->size) : nullptr;
    cache[name] = surface;
    return surface;
}

cairo_surface_t* iconSurface(const Entry& entry) {
    if (!entry.iconPath.empty()) {
        // keyed by the expected SHA-256: a changed icon is a new key, an icon read once isn't read again
        static std::map<std::string, cairo_surface_t*> cache;
        const std::string key = entry.iconPath + "\n" + entry.app.iconSha256;
        auto it = cache.find(key);
        if (it == cache.end()) {
            cairo_surface_t* surface = nullptr;
            std::string bytes;
            const int fd = ::open(entry.iconPath.c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
            if (fd >= 0) {
                char buf[8192];
                ssize_t n;
                while ((n = ::read(fd, buf, sizeof(buf))) > 0 && bytes.size() <= kMaxIconBytes) bytes.append(buf, static_cast<size_t>(n));
                ::close(fd);
            }
            // check again here: the file may have changed since the list was built
            if (isPng128(bytes) && sha256Hex(bytes) == entry.app.iconSha256) {
                surface = decode(reinterpret_cast<const unsigned char*>(bytes.data()), bytes.size());
            }
            it = cache.emplace(key, surface).first;
        }
        if (it->second != nullptr) return it->second;
    }
    return iconSurface(entry.app.name);
}

bool drawIcon(cairo_t* cr, const std::string& name, double x, double y, double size) {
    cairo_surface_t* icon = iconSurface(name);
    if (icon == nullptr) return false;
    paintScaled(cr, icon, x, y, size);
    return true;
}

bool drawIcon(cairo_t* cr, const Entry& entry, double x, double y, double size) {
    cairo_surface_t* icon = iconSurface(entry);
    if (icon == nullptr) return false;
    paintScaled(cr, icon, x, y, size);
    return true;
}

}  // namespace frame_apps
