# AnyCanvas

One vector core for canvas drawings and SVG. An app records a drawing through a Canvas2D-shaped API
(the **opcode stream**) or hands over SVG markup; the platform-free C++ **core** turns either into one
normalized **draw list**; a thin per-platform **painter** replays that list on the native 2D API
(Canvas2D, android.graphics, tgfx, CoreGraphics). No rasterizer, no fonts and no image decoding live
in the core — every platform draws with its own stack, and every platform draws the same list.

```
   recorder (TS / Kotlin / Swift)  ──opcode stream──▶ ┌──────────────┐
                                                      │  core (C++)  │ ──draw list──▶ painter ──▶ pixels
   SVG markup ─────────────────────────────────────▶ └──────────────┘        (web · android · tgfx · apple)
```

Synchronization across the languages is guarded by generated tables and tests, not discipline: the
formats live in three headers under `spec/`, a generator emits the same ids for TypeScript, Kotlin and
Swift, every painter switches over the draw commands exhaustively, and golden draw lists pin the
core's behaviour byte for byte.

## Layout

```
spec/            THE formats. enums.h · ops.h (the opcode stream) · draw.h (the draw list), X-macro tables
                 generate.ts → gen/spec.ts, gen/Spec.kt, gen/Spec.swift (committed; `bun run check:spec` in CI)
core/            C++17, no platform code. include/anycanvas/anycanvas.h = the C API; src/ = the interpreter
                 (opcodes → draw list), the SVG front end (nanosvg → the same draw list), the CSS color / font
                 parsers, the geometry (arcs → cubics), the surface registry, PNG / JPEG encoding, the JSON dump
                 vendor/ = nanosvg (zlib, patched: see "ANYCANVAS" comments) + stb_image_write (public domain)
recorders/ts/    `Recorder`: the Canvas2D-shaped API that writes the opcode stream (npm anycanvas-recorder)
painters/web/    `readDrawList` + `paint(ctx, commands)` on any Canvas2D context (npm anycanvas-web)
painters/tgfx/   the C++ painter over tgfx::Canvas — sources only, compiled by the including build
painters/android/  the Kotlin painter over android.graphics + the JNI binding of core (a Gradle library)
painters/apple/  the Swift painter over CoreGraphics (an SPM package) — later, written on the Mac
tools/acdump     draw lists and reference PNGs from the command line
tests/           core/ (ctest: unit + golden) · ts/ (bun: recorder, reader, web painter) · golden/ (the corpus)
```

## The formats, in one paragraph each

**The opcode stream** ([spec/ops.h](spec/ops.h)) is a `Float32Array` of words plus a string table: an
op id followed by its operands (floats, ints, string indices). It is the HTML Canvas2D model —
state stack, transforms, path building, fill / stroke / clip, text, images — plus gradients, dashes,
the fill rule, the miter limit and letter spacing. An interpreter stops at the first unknown or
truncated op, so a newer stream degrades to a shorter drawing.

**The draw list** ([spec/draw.h](spec/draw.h)) is the same physical shape (words + strings) with
everything resolved: absolute transforms, RGBA colors, `(family size weight italic)` fonts, arcs and
rectangles as path verbs, alpha on the paint, normalized gradient stops, balanced save / restore.
Ten commands. A painter is a loop with one case each and no state of its own.

**Gradients** are a matrix from *gradient space* to user space: a linear gradient runs from (0,0) to
(1,0) there, a radial one is the unit circle with a start circle at (fx, fy) of radius r0. Every
backend expresses that natively (a shader local matrix on Skia / tgfx / Android, a context
transform on Canvas2D and CoreGraphics), and SVG `gradientTransform` needs no special case.

## Build and test

```
./build.ps1                        Windows: configure + build (MSVC, Ninja) + ctest
./build.sh                         Linux / macOS / WSL
./build.ps1 -Update                rewrite tests/golden/expected after an intended change — review the diff
bun install && bun test tests/ts   the TS side (needs the goldens the C++ test wrote)
bun run record                     re-record tests/golden/streams from tests/ts/scenarios.ts
bun run check:spec                 the generated enums are fresh
build/acdump svg file.svg          the draw list of an SVG (JSON); `stream file.json` for an opcode stream
build/acdump png file.svg out.png  a CPU reference raster (nanosvgrast) for the eye
```

Tests, by layer: **unit** (hostile streams never crash, the CSS parsers, the geometry, transform and
path semantics, the reader round trip, the encoder), **golden** (every stream and SVG under
`tests/golden` → its draw list as JSON, byte-equal on the platform that wrote it and numerically equal
(±1e-4) on every other — float trig differs in the last digits between libms — and as the binary the
painters read),
**recorder** (the committed streams match a fresh recording; every opcode is exercised), **reader**
(the TS decoder of the binary equals the core's JSON), **web painter** (every golden replays on
Skia via @napi-rs/canvas; known colors land where the drawing says).

## Using it

**C / C++**: `add_subdirectory(AnyCanvas)` and link `anycanvas` (the C API of
[anycanvas.h](core/include/anycanvas/anycanvas.h)) or `anycanvas-internal` (the C++ types of
`core/src`, for a C++ painter). A host keeps the pixels: it asks `ac_interpret` / `ac_svg_draw` for a
draw list, replays it with its painter, and reads pixels back for `ac_encode`. The surface registry
(`ac_surface_*`) is optional bookkeeping.

**TypeScript**: `new Recorder()` records; `readDrawList(words, strings)` decodes what the core
produced (from wasm memory or a file); `paint(ctx, commands, { image, fontFamily })` draws.

**Kotlin / Swift**: the painters under `painters/` plus the generated `spec/gen/Spec.kt` / `Spec.swift`.

## Platform limits, stated

- Text is shaped and rasterized by the platform. Identical draw lists, different pixels; goldens
  compare lists, never text pixels.
- Canvas2D gradients only pad (reflect / repeat draw as pad); a gradient under a skew or non-uniform
  scale is drawn by transforming the context, which also distorts a *stroke's* pen. Android's
  `RadialGradient` is concentric (the focal point is ignored). `letterSpacing` needs Chrome 99+ /
  Safari 17+ on the web.
- SVG is nanosvg's model: shapes, fills, strokes, gradients, opacity, dashes, transforms, a tint
  override. No text (planned: `<text>` / `<tspan>` in the parser, one `fillText` in the list), no
  filters, masks, patterns or CSS. `objectBoundingBox` gradient coordinates must be percentages
  (a bare `1` is one pixel — a nanosvg trait).
- A note for readers of the earlier Kotlin / Swift SVG libraries: nanosvg's `NSVGgradient::xform` is
  the *inverse* transform (image → gradient space) with the linear axis along gradient Y. The core
  inverts and canonicalizes it; `acdump png` (nanosvgrast) is the reference when in doubt.

## Status

| part | state |
|---|---|
| spec + generator | built; `bun run check:spec` |
| core: interpreter, SVG, CSS, geometry, surfaces, encode, JSON, C API | built; 102 unit checks, 15 goldens |
| recorders/ts | built; tested |
| painters/web | built; tested on Skia (@napi-rs/canvas) |
| painters/tgfx | written against the tgfx API of the LeCodes desktop host; compiles there (its step 3) |
| painters/android | built + run on a phone: `:anycanvas:assembleRelease` → AAR (arm64, armv7, x86_64); `:demo:assembleDebug` = the device check app (all 15 goldens through libanycanvas.so + the Kotlin painter on screen) |
| painters/apple, recorders/kotlin, recorders/swift | not started |
| SVG `<text>` | not started (additive: parser + the existing text command) |

## License

MIT. nanosvg is zlib (core/vendor/nanosvg/LICENSE.txt), stb_image_write public domain.
