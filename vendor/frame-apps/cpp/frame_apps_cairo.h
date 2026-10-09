// SPDX-License-Identifier: MIT — part of frame-apps by sasaken1102r, shipped under the host app's MIT license
// The apps' icons as cairo surfaces, for panels that draw with cairo. A fetched icon whose SHA-256 matched
// (Entry::iconPath) is used first, then the one built into the program (cpp/frame_apps_icons.cpp).
//
//   frame_apps::drawIcon(cr, entry, x, y, 52);   // scaled into a 52×52 square; false → draw a stand-in
#pragma once

#include "frame_apps.h"

#include <cairo.h>

#include <string>

namespace frame_apps {

/**
 * The app's built-in icon (128×128 ARGB), decoded once and kept for the life of the program. Don't destroy it.
 * @param name app name
 * @return the surface, or nullptr if there is no icon or it can't be decoded
 */
cairo_surface_t* iconSurface(const std::string& name);

/**
 * The icon for a line of the list: the fetched one (Entry::iconPath, checked again here: PNG 128×128 with the
 * list's SHA-256), else the built-in one of the same name. Kept for the life of the program. Don't destroy it.
 * @param entry the line
 * @return the surface, or nullptr
 */
cairo_surface_t* iconSurface(const Entry& entry);

/**
 * Draws the built-in icon scaled into a square (it has its own rounded tile, so draw nothing under it).
 * @return true if drawn (false: no icon; draw a stand-in)
 */
bool drawIcon(cairo_t* cr, const std::string& name, double x, double y, double size);

/**
 * Draws the line's icon (fetched or built-in) scaled into a square.
 * @param cr where to draw
 * @param entry the line
 * @param x left
 * @param y top
 * @param size side in px
 * @return true if drawn (false: no icon; draw entry.app.mono instead)
 */
bool drawIcon(cairo_t* cr, const Entry& entry, double x, double y, double size);

}  // namespace frame_apps
