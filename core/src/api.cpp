#include "anycanvas/anycanvas.h"

#include "drawlist.h"
#include "encode.h"
#include "interpreter.h"
#include "svg.h"

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <unordered_map>
#include <vector>

using namespace anycanvas;

struct SurfaceInfo { int32_t pw = 0, ph = 0; float scale = 1; };

struct ac_context {
  DrawList list;
  std::vector<const char*> stringPtrs;
  std::unordered_map<int32_t, SurfaceInfo> surfaces;
  int32_t nextSurface = 1;

  void expose(ac_drawlist* out) {
    stringPtrs.clear();
    stringPtrs.reserve(list.strings.size());
    for (const auto& s : list.strings) stringPtrs.push_back(s.c_str());
    if (!out) return;
    out->words = list.words.data();
    out->wordCount = (int32_t)list.words.size();
    out->strings = stringPtrs.data();
    out->stringCount = (int32_t)stringPtrs.size();
  }
};

struct ac_svg { Svg* svg; };

extern "C" {

int32_t ac_spec_version(void) { return ANYCANVAS_SPEC_VERSION; }

ac_context* ac_context_create(void) { return new ac_context(); }
void ac_context_destroy(ac_context* ctx) { delete ctx; }

int32_t ac_interpret(ac_context* ctx, const float* cmd, int32_t cmdLen, const char* const* refs, int32_t refsCount, float scale, ac_drawlist* out) {
  if (!ctx) return 0;
  const int32_t ops = interpret(cmd, cmdLen, refs, refsCount, scale, ctx->list);
  ctx->expose(out);
  return ops;
}

int ac_looks_like_svg(const void* data, size_t len) { return looksLikeSvg(data, len) ? 1 : 0; }

ac_svg* ac_svg_parse(const char* data, size_t len) {
  Svg* s = parseSvg(data, len);
  if (!s) return nullptr;
  return new ac_svg{ s };
}

void ac_svg_size(const ac_svg* svg, float* width, float* height) {
  float w = 0, h = 0;
  if (svg && svg->svg) svgSize(*svg->svg, w, h);
  if (width) *width = w;
  if (height) *height = h;
}

void ac_svg_free(ac_svg* svg) {
  if (!svg) return;
  delete svg->svg;
  delete svg;
}

void ac_svg_draw(ac_context* ctx, const ac_svg* svg, float w, float h, int hasTint, const float* tintRGBA, ac_drawlist* out) {
  if (!ctx) return;
  ctx->list.clear();
  if (svg && svg->svg) {
    Color tint;
    if (hasTint && tintRGBA) { tint.r = tintRGBA[0]; tint.g = tintRGBA[1]; tint.b = tintRGBA[2]; tint.a = tintRGBA[3]; }
    drawSvg(ctx->list, *svg->svg, w, h, hasTint && tintRGBA ? &tint : nullptr);
  }
  ctx->expose(out);
}

char* ac_drawlist_json(const ac_drawlist* list) {
  if (!list) return nullptr;
  DrawList dl;
  dl.words.assign(list->words, list->words + (list->wordCount > 0 ? list->wordCount : 0));
  for (int32_t i = 0; i < list->stringCount; i++) dl.strings.push_back(list->strings[i] ? list->strings[i] : "");
  const std::string json = dl.toJson();
  char* out = (char*)std::malloc(json.size() + 1);
  if (!out) return nullptr;
  std::memcpy(out, json.c_str(), json.size() + 1);
  return out;
}

int32_t ac_surface_prepare(ac_context* ctx, int32_t id, float w, float h, float scale, int32_t* pw, int32_t* ph, int* resized) {
  if (!(scale > 0)) scale = 1;
  const int32_t dw = (int32_t)std::lround(w * scale) < 1 ? 1 : (int32_t)std::lround(w * scale);
  const int32_t dh = (int32_t)std::lround(h * scale) < 1 ? 1 : (int32_t)std::lround(h * scale);
  auto it = id ? ctx->surfaces.find(id) : ctx->surfaces.end();
  bool changed = false;
  if (it == ctx->surfaces.end()) {
    id = ctx->nextSurface++;
    it = ctx->surfaces.emplace(id, SurfaceInfo{ dw, dh, scale }).first;
    changed = true;
  } else if (it->second.pw != dw || it->second.ph != dh) {
    it->second.pw = dw; it->second.ph = dh; changed = true;
  }
  it->second.scale = scale;
  if (pw) *pw = dw;
  if (ph) *ph = dh;
  if (resized) *resized = changed ? 1 : 0;
  return id;
}

int32_t ac_surface_add(ac_context* ctx, int32_t pw, int32_t ph, float scale) {
  const int32_t id = ctx->nextSurface++;
  ctx->surfaces.emplace(id, SurfaceInfo{ pw < 1 ? 1 : pw, ph < 1 ? 1 : ph, scale > 0 ? scale : 1 });
  return id;
}

int ac_surface_info(const ac_context* ctx, int32_t id, int32_t* pw, int32_t* ph, float* scale) {
  auto it = ctx->surfaces.find(id);
  if (it == ctx->surfaces.end()) return 0;
  if (pw) *pw = it->second.pw;
  if (ph) *ph = it->second.ph;
  if (scale) *scale = it->second.scale;
  return 1;
}

void ac_surface_remove(ac_context* ctx, int32_t id) { ctx->surfaces.erase(id); }
void ac_surface_clear(ac_context* ctx) { ctx->surfaces.clear(); ctx->nextSurface = 1; }

uint8_t* ac_encode(const uint8_t* rgba, int32_t w, int32_t h, int32_t format, int32_t quality, size_t* outLen) {
  return encodeImage(rgba, w, h, format == 1 ? ImageFormat::JPEG : ImageFormat::PNG, quality, outLen);
}

void ac_free(void* p) { std::free(p); }

}  // extern "C"
