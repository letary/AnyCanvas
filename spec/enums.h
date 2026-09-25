// AnyCanvas spec — the small enums shared by the opcode stream (input) and the draw list (output).
// THE source of truth: spec/generate.ts emits the same values for TypeScript, Kotlin and Swift
// (spec/gen/*), and core reads them as C++ enums (core/src/spec.h). A value here is a wire value:
// never renumber without bumping ANYCANVAS_SPEC_VERSION (spec/ops.h).
//
// Format: ANYCANVAS_ENUM_<NAME>(X) with X(MEMBER, value). Member order = declaration order.
#pragma once

#define ANYCANVAS_ENUM_LINE_JOIN(X) \
  X(MITER, 0) \
  X(ROUND, 1) \
  X(BEVEL, 2)

#define ANYCANVAS_ENUM_LINE_CAP(X) \
  X(BUTT, 0) \
  X(ROUND, 1) \
  X(SQUARE, 2)

#define ANYCANVAS_ENUM_TEXT_ALIGN(X) \
  X(LEFT, 0) \
  X(CENTER, 1) \
  X(RIGHT, 2) \
  X(START, 3) \
  X(END, 4)

#define ANYCANVAS_ENUM_TEXT_BASELINE(X) \
  X(ALPHABETIC, 0) \
  X(TOP, 1) \
  X(MIDDLE, 2) \
  X(BOTTOM, 3) \
  X(HANGING, 4) \
  X(IDEOGRAPHIC, 5)

#define ANYCANVAS_ENUM_FILL_RULE(X) \
  X(NONZERO, 0) \
  X(EVENODD, 1)

// How a gradient continues outside [0, 1].
#define ANYCANVAS_ENUM_SPREAD(X) \
  X(PAD, 0) \
  X(REFLECT, 1) \
  X(REPEAT, 2)

// The opcode stream's gradient kind (FILL_GRADIENT / STROKE_GRADIENT).
#define ANYCANVAS_ENUM_GRADIENT(X) \
  X(LINEAR, 0) \
  X(RADIAL, 1)

// The draw list's paint kind.
#define ANYCANVAS_ENUM_PAINT(X) \
  X(COLOR, 0) \
  X(LINEAR, 1) \
  X(RADIAL, 2)

// The draw list's path verbs (see draw.h "path").
#define ANYCANVAS_ENUM_PATH_VERB(X) \
  X(MOVE, 0) \
  X(LINE, 1) \
  X(QUAD, 2) \
  X(CUBIC, 3) \
  X(CLOSE, 4)

// Encoded image formats (ac_encode).
#define ANYCANVAS_ENUM_IMAGE_FORMAT(X) \
  X(PNG, 0) \
  X(JPEG, 1)

// Every enum, for the generator and the C++ side: E(EnumName, ANYCANVAS_ENUM_MACRO).
#define ANYCANVAS_ENUMS(E) \
  E(LineJoin, ANYCANVAS_ENUM_LINE_JOIN) \
  E(LineCap, ANYCANVAS_ENUM_LINE_CAP) \
  E(TextAlign, ANYCANVAS_ENUM_TEXT_ALIGN) \
  E(TextBaseline, ANYCANVAS_ENUM_TEXT_BASELINE) \
  E(FillRule, ANYCANVAS_ENUM_FILL_RULE) \
  E(Spread, ANYCANVAS_ENUM_SPREAD) \
  E(Gradient, ANYCANVAS_ENUM_GRADIENT) \
  E(Paint, ANYCANVAS_ENUM_PAINT) \
  E(PathVerb, ANYCANVAS_ENUM_PATH_VERB) \
  E(ImageFormat, ANYCANVAS_ENUM_IMAGE_FORMAT)
