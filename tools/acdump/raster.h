// A CPU reference rasterizer for SVG (nanosvgrast) — the owner's-eye PNG and the shapes-only
// reference of the cross-painter tests. Not part of core: core never rasterizes.
#pragma once

#include <cstdint>
#include <vector>

namespace anycanvas { struct Svg; }

// Rasterizes `svg` at `scale` (device px per SVG px) into straight RGBA8; sets w / h.
bool rasterizeSvg(const anycanvas::Svg& svg, float scale, std::vector<uint8_t>& rgba, int32_t& w, int32_t& h);
