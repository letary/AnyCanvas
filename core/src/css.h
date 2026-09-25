// The CSS subset a canvas resolves: colors and the font shorthand. Parsed ONCE here, so painters
// never see a CSS string.
#pragma once

#include "drawlist.h"

namespace anycanvas {

// #rgb #rgba #rrggbb #rrggbbaa · rgb()/rgba() with commas or spaces, numbers or percentages, an
// alpha as a number or a percentage · hsl()/hsla() · the CSS named colors · transparent.
// Case-insensitive, surrounding spaces ignored. False (and `out` untouched) for anything else — a
// canvas keeps its previous style then, like the browser.
bool parseCssColor(const char* s, Color& out);

// `[italic|oblique] [normal|bold|bolder|lighter|100..900] <size>(px|pt|em|rem|%)[/line-height] <family>[, …]`.
// The first family, unquoted; a missing family is "sans-serif", a missing size 10px (the Canvas2D
// default). Never fails: an unparsable string yields the defaults.
FontData parseCssFont(const char* s);

}  // namespace anycanvas
