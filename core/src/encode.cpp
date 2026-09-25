#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_WRITE_NO_STDIO
#include "stb/stb_image_write.h"

#include "encode.h"

#include <cstdlib>
#include <cstring>
#include <vector>

namespace anycanvas {

namespace {
void collect(void* ctx, void* data, int size) {
  auto* v = static_cast<std::vector<uint8_t>*>(ctx);
  v->insert(v->end(), (const uint8_t*)data, (const uint8_t*)data + size);
}
}  // namespace

uint8_t* encodeImage(const uint8_t* rgba, int32_t w, int32_t h, ImageFormat format, int32_t quality, size_t* outLen) {
  if (outLen) *outLen = 0;
  if (!rgba || w <= 0 || h <= 0) return nullptr;
  std::vector<uint8_t> bytes;
  int ok = 0;
  if (format == ImageFormat::JPEG) {
    if (quality < 1) quality = 1; else if (quality > 100) quality = 100;
    ok = stbi_write_jpg_to_func(collect, &bytes, w, h, 4, rgba, quality);
  } else {
    ok = stbi_write_png_to_func(collect, &bytes, w, h, 4, rgba, w * 4);
  }
  if (!ok || bytes.empty()) return nullptr;
  uint8_t* out = (uint8_t*)std::malloc(bytes.size());
  if (!out) return nullptr;
  std::memcpy(out, bytes.data(), bytes.size());
  if (outLen) *outLen = bytes.size();
  return out;
}

}  // namespace anycanvas
