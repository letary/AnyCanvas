// AnyCanvas core — the C API. One vector core for canvas opcodes and SVG: both become ONE draw list
// (spec/draw.h) that a thin platform painter replays. The core is platform-free: no rasterizer, no
// fonts, no image decoding — a painter provides those.
//
// Surfaces: the core keeps the registry (id → size, scale); the PIXELS live with the painter (a GPU
// surface, a Bitmap, an OffscreenCanvas). A host rasterizes by asking the core for a draw list and
// handing it to its painter; it reads pixels back from the painter and hands them to ac_encode.
//
// Memory: every pointer a call returns stays valid until the next call on the same context, unless the
// comment says "malloc'd" — those are freed with ac_free.
#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef AC_API
#define AC_API
#endif

typedef struct ac_context ac_context;
typedef struct ac_svg ac_svg;

// A draw list: f32 words + a string table (spec/draw.h). Owned by the context.
typedef struct ac_drawlist {
  const float* words;
  int32_t wordCount;
  const char* const* strings;
  int32_t stringCount;
} ac_drawlist;

AC_API int32_t ac_spec_version(void);

AC_API ac_context* ac_context_create(void);
AC_API void ac_context_destroy(ac_context* ctx);

// ---- the opcode interpreter --------------------------------------------------------------------------
// `cmd` (cmdLen f32 words) + `refs` (the string table) → the draw list in device px (`scale` is the
// device-pixel multiplier, the base transform). Returns the number of ops consumed (the stream stops
// at the first unknown / truncated op; what came before is drawn).
AC_API int32_t ac_interpret(ac_context* ctx, const float* cmd, int32_t cmdLen, const char* const* refs, int32_t refsCount,
                            float scale, ac_drawlist* out);

// ---- SVG --------------------------------------------------------------------------------------------
AC_API int ac_looks_like_svg(const void* data, size_t len);
// Parses markup (copied). Null on failure. Free with ac_svg_free.
AC_API ac_svg* ac_svg_parse(const char* data, size_t len);
AC_API void ac_svg_size(const ac_svg* svg, float* width, float* height);
AC_API void ac_svg_free(ac_svg* svg);
// The draw list of `svg` aspect-fitted into w × h device px (w or h ≤ 0: natural size). `hasTint` = 1
// replaces every color with tint (RGBA 0..1); shape opacity still applies.
AC_API void ac_svg_draw(ac_context* ctx, const ac_svg* svg, float w, float h, int hasTint, const float* tintRGBA, ac_drawlist* out);

// ---- fonts ------------------------------------------------------------------------------------------
// A CSS font shorthand resolved the way the interpreter resolves it for a text command, so a host's
// measureText sees the same (family, size, weight, italic) as its painter.
typedef struct ac_font {
  char family[64];
  float size;        /* px */
  int32_t weight;    /* 100..900 */
  int32_t italic;    /* 0 / 1 */
} ac_font;
AC_API void ac_font_parse(const char* css, ac_font* out);

// ---- the draw list ----------------------------------------------------------------------------------
// The deterministic JSON text of a draw list (tests, debugging, hashing). Malloc'd; ac_free.
AC_API char* ac_drawlist_json(const ac_drawlist* list);

// ---- surfaces ---------------------------------------------------------------------------------------
// Ensure a surface: `id` 0 or unknown creates one; an existing one is resized when the device size
// changed (`*resized` = 1 then, and also when created). Returns the id (never 0). pw/ph = device px.
AC_API int32_t ac_surface_prepare(ac_context* ctx, int32_t id, float w, float h, float scale, int32_t* pw, int32_t* ph, int* resized);
// A standalone surface of a known device size (a decoded image, a snapshot). Returns the id.
AC_API int32_t ac_surface_add(ac_context* ctx, int32_t pw, int32_t ph, float scale);
// 1 with the sizes filled, 0 for an unknown id.
AC_API int ac_surface_info(const ac_context* ctx, int32_t id, int32_t* pw, int32_t* ph, float* scale);
AC_API void ac_surface_remove(ac_context* ctx, int32_t id);
// Drop every surface (a project switch); the host frees its pixels alongside.
AC_API void ac_surface_clear(ac_context* ctx);

// ---- encoding ---------------------------------------------------------------------------------------
// Straight RGBA8 (top row first) → PNG (format 0) or JPEG (format 1, quality 1..100). Malloc'd; ac_free.
AC_API uint8_t* ac_encode(const uint8_t* rgba, int32_t w, int32_t h, int32_t format, int32_t quality, size_t* outLen);

AC_API void ac_free(void* p);

#ifdef __cplusplus
}
#endif
