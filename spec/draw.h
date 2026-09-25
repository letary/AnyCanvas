// AnyCanvas spec — the DRAW LIST: what core emits and every painter replays.
//
// A draw list is a Float32Array of words plus a string table, the same physical shape as the opcode
// stream, so it crosses JNI / wasm / Swift as one buffer. Everything is RESOLVED: CSS colors are
// RGBA floats, CSS fonts are (family size weight italic), arcs / rects / ellipses are path verbs, the
// transform is absolute, alpha is a number on the paint. A painter is a loop with one case per
// command and NO state of its own beyond the platform canvas' save / restore stack.
//
// Every command is  [id, len, payload...]  where len = the number of payload words, so a painter can
// skip a command it does not know. Coordinates are in the space set by the latest SET_TRANSFORM
// (the painter's canvas transform); core emits SET_TRANSFORM lazily, only when it changes, and the
// painter must start each replay with the identity transform and an empty save stack. SAVE /
// RESTORE are balanced by core (a list never ends inside a save).
//
// Composite payloads (word counts in brackets):
//   color   [4]   r g b a — straight (non-premultiplied) 0..1
//   paint   [var] kind alpha ...   kind = Paint enum; alpha multiplies the whole paint (globalAlpha, SVG opacity)
//             COLOR:  color
//             LINEAR: a b c d e f spread nstops (offset color)*   the gradient runs from (0,0) to (1,0)
//                     in GRADIENT space; M = [a b c d e f] maps gradient space to the command's user space
//             RADIAL: a b c d e f fx fy r0 spread nstops (offset color)*   the end circle is the UNIT
//                     circle at the origin of gradient space, the start circle is centered at (fx fy)
//                     with radius r0 (Canvas2D two-circle semantics; SVG focal = r0 0)
//             Stops are normalized by core: sorted, offsets clamped to 0..1, at least two.
//   stroke  [var] width join cap miterLimit dashOffset ndash (dash)*
//   font    [4]   family(string index) size weight italic(0|1)
//   path    [var] nwords (verb coords...)*   verb = PathVerb: MOVE x y · LINE x y · QUAD cx cy x y ·
//             CUBIC c1x c1y c2x c2y x y · CLOSE. User space; the transform applies at replay, so
//             native strokes stay crisp under scale.
//
// X(NAME, ID, PAYLOAD). ids are wire values: append, never renumber (bump ANYCANVAS_SPEC_VERSION).
#pragma once

#define ANYCANVAS_DRAW(X) \
  X(SET_TRANSFORM, 0, "a b c d e f — replace the canvas transform") \
  X(SAVE,          1, "— push the transform and the clip") \
  X(RESTORE,       2, "— pop them") \
  X(CLIP,          3, "rule path — intersect the clip with the path") \
  X(FILL_PATH,     4, "rule paint path") \
  X(STROKE_PATH,   5, "stroke paint path") \
  X(FILL_TEXT,     6, "text(string) x y maxWidth font align baseline letterSpacing paint — one line, platform-shaped; maxWidth 0 = none") \
  X(STROKE_TEXT,   7, "text(string) x y maxWidth font align baseline letterSpacing stroke paint") \
  X(DRAW_IMAGE,    8, "surface sx sy sw sh dx dy dw dh alpha — src in the surface's device px, dst in user space") \
  X(CLEAR_RECT,    9, "x y w h — pixels become transparent black")
