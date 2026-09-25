// The tgfx painter: replays a draw list on a tgfx::Canvas. Sources only — compiled by the including
// build against its tgfx (see sources.cmake). Text goes through the host's text pipeline (hooks), so
// registered fonts and the host's shaping apply to canvas text exactly as to UI text.
#pragma once

#include "drawlist.h"   // anycanvas-internal (core/src)

#include <functional>
#include <memory>
#include <string>

namespace tgfx { class Canvas; class Image; class Font; class Paint; }

namespace anycanvas {

struct TgfxHooks {
  // A tgfx::Font for the resolved font (family → typeface via the host's registry, size, weight, italic).
  std::function<tgfx::Font(const FontData&)> font;
  // The advance width of one line of text in that font (the host's measurer), letterSpacing px added
  // after every glyph (CSS / Canvas2D) — the width drawLine then draws.
  std::function<float(const std::string& text, const tgfx::Font&, float letterSpacing)> measure;
  // Draw one line with its baseline origin at (x, y), letterSpacing px after every glyph — the host's
  // shaper / glyph run drawer.
  std::function<void(tgfx::Canvas*, const std::string& text, float x, float y, const tgfx::Font&, const tgfx::Paint&, float letterSpacing)> drawLine;
  // The tgfx::Image of a surface id (a snapshot of the surface the host keeps); null skips the blit.
  std::function<std::shared_ptr<tgfx::Image>(int32_t surface)> image;
  // Font metrics: positive ascent and descent for the baseline mapping (null → tgfx's own metrics).
  std::function<void(const tgfx::Font&, float& ascent, float& descent)> metrics;
};

// Replays `words` / `strings` on `canvas`, which must start with the identity matrix and an empty
// save stack (the painter resets both). The replay runs inside one save, so no clip or matrix it sets
// outlives it. Clearing the surface is the caller's decision.
void paintTgfx(tgfx::Canvas* canvas, const float* words, int32_t wordCount, const char* const* strings, int32_t stringCount, const TgfxHooks& hooks);

}  // namespace anycanvas
