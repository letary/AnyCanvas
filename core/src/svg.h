// The SVG front end: nanosvg's shape model → the same draw list a canvas produces.
#pragma once

#include "drawlist.h"

#include <cstddef>

struct NSVGimage;

namespace anycanvas {

struct Svg {
  NSVGimage* image = nullptr;
  ~Svg();
};

// Parses SVG markup (copied: nanosvg mutates its input). Null on failure or an empty / sizeless document.
Svg* parseSvg(const char* data, size_t len);

// The document's viewport in px.
void svgSize(const Svg& svg, float& w, float& h);

// True if the bytes look like an SVG document (a "<svg" tag near the start).
bool looksLikeSvg(const void* data, size_t len);

// The draw list of `svg` aspect-fitted and centered into w × h device px (w or h ≤ 0: the natural
// size, scale 1). `tint` replaces every fill and stroke color (gradients included); shape opacity
// still applies. Appends to `out` (call out.clear() first for a fresh list).
void drawSvg(DrawList& out, const Svg& svg, float w, float h, const Color* tint);

}  // namespace anycanvas
