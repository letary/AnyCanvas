// AnyCanvas spec — the OPCODE STREAM: what a recorder writes and the core interpreter reads.
//
// A stream is a Float32Array of words plus a string table (`refs`). Every op is its id followed by
// its operands, each one word:
//   f  a float
//   i  an integer (an enum value, a count, a surface id) stored as a float — exact below 2^24
//   r  a string: the index into `refs` (a CSS color, a CSS font shorthand, a text)
// An op with a REPEAT group ends with a count `i` followed by that many groups.
//
// Semantics are the HTML Canvas2D ones (state stack, transforms, path building, fill / stroke /
// clip, text, images) unless a comment says otherwise. The interpreter stops at the first unknown or
// truncated op, so a newer stream degrades to a shorter drawing rather than a crash.
//
// X(NAME, ID, ARGS, REPEAT, DOC). ids are wire values: append, never renumber (bump the version).
#pragma once

#define ANYCANVAS_SPEC_VERSION 1

#define ANYCANVAS_OPS(X) \
  /* state stack */ \
  X(SAVE,            0,  "",         "",   "push the drawing state (transform, clip, styles)") \
  X(RESTORE,         1,  "",         "",   "pop it; ignored when the stack is empty") \
  /* transforms (multiply the CTM, Canvas2D order) */ \
  X(TRANSLATE,       2,  "ff",       "",   "x y") \
  X(SCALE,           3,  "ff",       "",   "sx sy") \
  X(ROTATE,          4,  "f",        "",   "radians, clockwise") \
  X(TRANSFORM,       5,  "ffffff",   "",   "a b c d e f: CTM = CTM * M") \
  X(SET_TRANSFORM,   6,  "ffffff",   "",   "a b c d e f: CTM = M") \
  X(RESET_TRANSFORM, 7,  "",         "",   "CTM = identity") \
  /* styles */ \
  X(GLOBAL_ALPHA,    8,  "f",        "",   "0..1, multiplies every later paint") \
  X(FILL_STYLE,      9,  "r",        "",   "a CSS color (#rgb #rgba #rrggbb #rrggbbaa rgb() rgba() hsl() hsla() the CSS names transparent clear)") \
  X(STROKE_STYLE,    10, "r",        "",   "a CSS color") \
  X(FILL_GRADIENT,   11, "iffffffi", "fr", "kind x0 y0 x1 y1 r0 r1 nstops (offset color)*: LINEAR uses x0 y0 x1 y1; RADIAL = createRadialGradient(x0 y0 r0 x1 y1 r1)") \
  X(STROKE_GRADIENT, 12, "iffffffi", "fr", "same layout as FILL_GRADIENT") \
  X(LINE_WIDTH,      13, "f",        "",   "") \
  X(LINE_JOIN,       14, "i",        "",   "LineJoin") \
  X(LINE_CAP,        15, "i",        "",   "LineCap") \
  X(MITER_LIMIT,     16, "f",        "",   "") \
  X(LINE_DASH,       17, "i",        "f",  "n (dash)*: an empty list = solid; an odd list is repeated (Canvas2D)") \
  X(LINE_DASH_OFFSET,18, "f",        "",   "") \
  X(FONT,            19, "r",        "",   "a CSS font shorthand: [italic] [weight] <size>px <family>[, ...]") \
  X(TEXT_ALIGN,      20, "i",        "",   "TextAlign") \
  X(TEXT_BASELINE,   21, "i",        "",   "TextBaseline") \
  X(LETTER_SPACING,  22, "f",        "",   "px, added after every glyph") \
  /* path building (user space at the time of the call, like Canvas2D) */ \
  X(PATH_BEGIN,      23, "",         "",   "beginPath (named PATH_* because wingdi.h defines BEGIN_PATH)") \
  X(PATH_CLOSE,      24, "",         "",   "") \
  X(MOVE_TO,         25, "ff",       "",   "x y") \
  X(LINE_TO,         26, "ff",       "",   "x y") \
  X(QUADRATIC_TO,    27, "ffff",     "",   "cx cy x y") \
  X(BEZIER_TO,       28, "ffffff",   "",   "c1x c1y c2x c2y x y") \
  X(ARC,             29, "fffffi",   "",   "x y r a0 a1 ccw — connects from the current point; ends at the arc end") \
  X(ARC_TO,          30, "fffff",    "",   "x1 y1 x2 y2 r") \
  X(ELLIPSE,         31, "fffffffi", "",   "x y rx ry rotation a0 a1 ccw") \
  X(RECT,            32, "ffff",     "",   "x y w h: a closed subpath") \
  X(ROUND_RECT,      33, "fffff",    "",   "x y w h r: r clamped to half the shorter side") \
  /* drawing the current path */ \
  X(FILL,            34, "i",        "",   "FillRule") \
  X(STROKE,          35, "",         "",   "") \
  X(CLIP,            36, "i",        "",   "FillRule: intersect the clip with the current path (the path stays)") \
  /* rectangles and text (do not touch the current path) */ \
  X(FILL_RECT,       37, "ffff",     "",   "x y w h") \
  X(STROKE_RECT,     38, "ffff",     "",   "x y w h") \
  X(CLEAR_RECT,      39, "ffff",     "",   "x y w h: pixels become transparent black") \
  X(FILL_TEXT,       40, "rfff",     "",   "text x y maxWidth (0 = none)") \
  X(STROKE_TEXT,     41, "rfff",     "",   "text x y maxWidth") \
  /* images: a surface id the host resolves (a snapshot, a decoded image) */ \
  X(DRAW_IMAGE,      42, "iffffffff","",   "surface sx sy sw sh dx dy dw dh: src in the surface's device px, dst in user space")
