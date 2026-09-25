// The CSS subset a canvas resolves: colors and the font shorthand. Parsed ONCE here, so painters
// never see a CSS string.
#pragma once

#include "anycanvas/css_color.h"
#include "drawlist.h"

namespace anycanvas {

// A CSS color → a Color, through the one parser of anycanvas/css_color.h (its grammar: #rgb #rgba
// #rrggbb #rrggbbaa · rgb()/rgba() · hsl()/hsla() · the CSS named colors · transparent · clear).
// False (and `out` untouched) for anything else — a canvas keeps its previous style then, like the
// browser.
static inline bool parseCssColor(const char* s, Color& out) {
  css::Rgba c;
  if (!css::parseColor(s, c)) return false;
  out.r = c.r;
  out.g = c.g;
  out.b = c.b;
  out.a = c.a;
  return true;
}

// `[italic|oblique] [normal|bold|bolder|lighter|100..900] <size>(px|pt|em|rem|%)[/line-height] <family>[, …]`.
// The first family, unquoted; a missing family is "sans-serif", a missing size 10px (the Canvas2D
// default). Never fails: an unparsable string yields the defaults.
FontData parseCssFont(const char* s);

}  // namespace anycanvas
