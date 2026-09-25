// Pixel encoding (PNG / JPEG via stb_image_write) — the one place every host encodes a surface.
#pragma once

#include "spec.h"

#include <cstddef>
#include <cstdint>

namespace anycanvas {

// Straight RGBA8, top row first, w × h → malloc'd bytes (free with std::free). Null on failure.
// `quality` is JPEG only (1..100).
uint8_t* encodeImage(const uint8_t* rgba, int32_t w, int32_t h, ImageFormat format, int32_t quality, size_t* outLen);

}  // namespace anycanvas
