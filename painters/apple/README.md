# painters/apple

The Swift painter over CoreGraphics + CoreText, and the Swift binding of the core, as one SwiftPM
package whose manifest is the repo's `Package.swift` (a target's path must be inside the package, and
`core/` and `spec/gen` are shared with every other painter). iOS 15+ and macOS 12+: the painter uses no
UIKit / AppKit, so the same code paints a `UIView`'s context, a bitmap surface, or a test on the Mac.

```swift
import AnyCanvas

let core = AnyCanvas()                                        // one per thread of use
let list = core.interpret(cmd: stream.cmd, refs: stream.refs, scale: Float(screen.scale))

struct Hooks: PainterHooks {
    func font(_ f: FontData) -> CTFont { FontRegistry.get(f) ?? Fonts.resolve(f) }   // registered fonts first
    func image(_ surface: Int32) -> CGImage? { surfaces[surface] }                    // your bitmaps
}
let painter = Painter(hooks: Hooks())

let ctx = Surface.makeContext(width: pw, height: ph)!         // sRGB, transparent, y-down (the flip is its base CTM)
try painter.paint(ctx, list)                                  // or painter.paint(ctx, commands)
let rgba = Surface.straightRGBA(ctx)                          // straight RGBA8 for ac_encode / a texture
let image = Surface.image(ctx)                                // a CGImage snapshot (copy-on-write)

if let svg = core.parseSvg(markup) { try painter.paint(ctx, svg.draw(width: w, height: h, tint: red)) }

// In a UIView: draw(_ rect:)'s context is already y-down — paint straight into it.
override func draw(_ rect: CGRect) { try? painter.paint(UIGraphicsGetCurrentContext()!, commands) }
```

Two products. **`AnyCanvasPainter`** is Swift only (the reader `DrawList` / `DrawCommand`, `Painter`,
`PainterHooks`, `Fonts`, `TextLine`, `Surface`) — for a host that links the core ITSELF and hands the
painter the words the core produced; LeCodes does that (its runtime carries the core, exactly as
`anycanvas.externalCore` does on Android), so a second core from this package would collide at the
static link. **`AnyCanvas`** adds the C++ core as the Clang module `CAnyCanvas`
(`core/include/module.modulemap` — only `anycanvas.h`, `css_color.h` is C++) and its Swift binding
(`interpret`, `parseSvg` / `draw`, `looksLikeSvg`, `encode`, `json`, `parseFont`): the standalone
library. `acpaint` (an executable, macOS) paints a golden `.bin` or an SVG to a PNG.

The generated enums come from `spec/gen/Spec.swift` (the target `AnyCanvasSpec`, never a copy): the
painter's `switch` over `DrawCommand` has no `default`, so a new draw command fails to compile here
until it is handled.

## How it paints

- **The CTM at entry is the base** (a surface's y-flip, a view's offset). `SET_TRANSFORM` is absolute
  and CG has no setCTM, so the painter concatenates `target · applied⁻¹` (the Android rule); a
  singular matrix marks the state invisible — draws are skipped, a clip empties the clip. One
  `saveGState` wraps the replay so a list-level clip dies with the paint; `RESTORE` never pops it.
- **Colors** in one sRGB space, solids and gradient stops alike; the paint alpha multiplies a solid's
  alpha and is `setAlpha` around a gradient.
- **Gradients**: the path (or `replacePathWithStrokedPath()` for a stroke, with width / caps / dash
  honoured) is the clip, the paint's matrix is concatenated, and the gradient is drawn in GRADIENT
  space — linear from (0,0) to (1,0), radial from the start circle `(fx fy r0)` to the unit circle:
  CoreGraphics has a real focal point (Android / tgfx are concentric). `PAD` extends; CG has no tiling,
  so `REPEAT` / `REFLECT` extend the stop list over the clip's bounds in gradient space.
- **Text** goes through ONE `CTLine` (`TextLine`) for fill, stroke and measure: `kern` carries
  letterSpacing (a gap after every glyph, as CSS), `CTLineGetTypographicBounds` is the width a host's
  `measureText` reports and the painter aligns by, the baseline table is the tgfx / Android one
  (`hanging` = 0.8·ascent, `ideographic` = bottom). `maxWidth` condenses horizontally around the
  ALIGNED origin of the condensed box. `strokeText` is `CTLineDraw` in `.stroke` mode — a centered
  outline like the browser's; a gradient paint uses `.fillClip` / `.strokeClip` and draws the gradient
  through the glyphs.
- **Fonts** (`Fonts.resolve`, the default hook): an installed family by name, the generic families
  (`serif` → Times New Roman, `monospace` → Menlo, `sans-serif` / `system-ui` → the system font), the
  system font for an unknown name (never CoreText's Helvetica fallback); CSS weight → the system
  font's trait stops (100 −0.8 … 400 0 … 700 0.4 … 900 0.62), italic as a symbolic trait.
- **Images**: `cropping(to:)` in device px, drawn y-down by a local flip, `interpolationQuality = .high`.

## Tests

`swift test` on the Mac is the fast check; the same bundle on the phone simulator is the real one:

```
swift test
xcodebuild test -scheme AnyCanvas-Package -destination 'platform=iOS Simulator,name=iPhone 17'
./build.sh                                                  # the CMake build: acdump for the cross-painter test
swift run acpaint tests/golden/expected/svg-23.bin tiger.png --size 494 800 --white
```

The corpus is read from `tests/golden` through `#filePath` (the simulator sees the Mac's files).
`ReaderTests` decode every golden's `.bin` to the very commands its `.json` describes;
`CoreGoldenTests` run every stream and SVG through the C API and compare with the expected JSON
(byte-equal, or numerically equal — Apple's libm moves the last digits of float trig);
`PainterTests` pin the transform deltas in pixels (the Android cases), the shapes golden's colors at
the web painter's coordinates, every golden replayed (PNGs in `tests/out/apple`), determinism, the
clip and restore guards; `GradientTests` the axis, the focal point, `r0`, gradient strokes, the three
spreads; `TextTests` the ink under align / baseline / letterSpacing / maxWidth, the outline, gradient
text, the font hook and the default resolution; `ImageTests` the crop, the orientation, alpha, the
encode round trip through ImageIO; `CrossPainterTests` (macOS) five shapes-only SVGs against
`acdump png` — mean channel error under 1/255 away from edges.

`demo/` is the on-device check (the Android demo's twin): an Xcode project that bundles `tests/golden`,
runs every golden through the core, compares the JSON, and paints each list on screen. For a phone,
put `DEVELOPMENT_TEAM = <your team id>` in `demo/Local.xcconfig` (gitignored; `Signing.xcconfig`
includes it optionally) — the team never lands in the committed project.
