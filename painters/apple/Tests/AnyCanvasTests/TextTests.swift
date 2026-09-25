// Text at the level of ink, never glyph pixels: alignment and baseline move the ink where Canvas2D
// says, letterSpacing widens the measured and the painted line alike, maxWidth condenses, a stroke
// is an outline, the font hook sees the resolved font. The default font resolution on top.
import CoreGraphics
import CoreText
import XCTest
import AnyCanvas

final class TextTests: XCTestCase {
    static let black = PaintData.solid(alpha: 1, color: Color(r: 0, g: 0, b: 0, a: 1))
    static let sans = FontData(family: "sans-serif", size: 40, weight: 400, italic: false)

    static func text(_ s: String, _ x: Float, _ y: Float, align: TextAlign = .left, baseline: TextBaseline = .alphabetic, letterSpacing: Float = 0, maxWidth: Float = 0, font: FontData = sans) -> TextData {
        TextData(text: s, x: x, y: y, maxWidth: maxWidth, font: font, align: align, baseline: baseline, letterSpacing: letterSpacing)
    }

    /// The bounding box of the inked pixels (alpha > 40): minX, minY, maxX, maxY; nil when empty.
    static func ink(_ ctx: CGContext) -> (Int, Int, Int, Int)? {
        var minX = Int.max, minY = Int.max, maxX = -1, maxY = -1
        for y in 0..<ctx.height { for x in 0..<ctx.width where Surface.pixel(ctx, x, y)[3] > 40 {
            minX = min(minX, x); maxX = max(maxX, x); minY = min(minY, y); maxY = max(maxY, y)
        } }
        return maxX < 0 ? nil : (minX, minY, maxX, maxY)
    }

    func testAlignmentMovesTheInk() throws {
        let left = try XCTUnwrap(Self.ink(try PainterTests.paint(300, 80, [.fillText(Self.text("MMMM", 150, 60, align: .left), paint: Self.black)])))
        XCTAssertGreaterThanOrEqual(left.0, 149)
        let right = try XCTUnwrap(Self.ink(try PainterTests.paint(300, 80, [.fillText(Self.text("MMMM", 150, 60, align: .right), paint: Self.black)])))
        XCTAssertLessThanOrEqual(right.2, 151)
        let center = try XCTUnwrap(Self.ink(try PainterTests.paint(300, 80, [.fillText(Self.text("MMMM", 150, 60, align: .center), paint: Self.black)])))
        XCTAssertLessThanOrEqual(abs((center.0 + center.2) / 2 - 150), 3)
        let end = try XCTUnwrap(Self.ink(try PainterTests.paint(300, 80, [.fillText(Self.text("MMMM", 150, 60, align: .end), paint: Self.black)])))
        XCTAssertEqual(end.2, right.2, "end reads as right")
    }

    func testBaselineMovesTheInk() throws {
        let alphabetic = try XCTUnwrap(Self.ink(try PainterTests.paint(200, 120, [.fillText(Self.text("MMM", 10, 60), paint: Self.black)])))
        XCTAssertLessThanOrEqual(abs(alphabetic.3 - 59), 2, "an M sits on the alphabetic baseline")
        let top = try XCTUnwrap(Self.ink(try PainterTests.paint(200, 120, [.fillText(Self.text("MMM", 10, 60, baseline: .top), paint: Self.black)])))
        XCTAssertGreaterThanOrEqual(top.1, 59, "top: all ink below y")
        let bottom = try XCTUnwrap(Self.ink(try PainterTests.paint(200, 120, [.fillText(Self.text("MMM", 10, 60, baseline: .bottom), paint: Self.black)])))
        XCTAssertLessThanOrEqual(bottom.3, 61, "bottom: all ink above y")
        let middle = try XCTUnwrap(Self.ink(try PainterTests.paint(200, 120, [.fillText(Self.text("MMM", 10, 60, baseline: .middle), paint: Self.black)])))
        XCTAssertLessThan(middle.1, 60); XCTAssertGreaterThan(middle.3, 60)
        let ideographic = try XCTUnwrap(Self.ink(try PainterTests.paint(200, 120, [.fillText(Self.text("MMM", 10, 60, baseline: .ideographic), paint: Self.black)])))
        XCTAssertEqual(ideographic.3, bottom.3, "ideographic reads as bottom, as on every painter")
        let hanging = try XCTUnwrap(Self.ink(try PainterTests.paint(200, 120, [.fillText(Self.text("MMM", 10, 60, baseline: .hanging), paint: Self.black)])))
        XCTAssertGreaterThan(hanging.1, alphabetic.1); XCTAssertLessThan(hanging.1, top.1, "hanging sits between alphabetic and top")
    }

    func testLetterSpacingWidensTheMeasureAndThePaint() throws {
        let font = Fonts.resolve(Self.sans)
        let plain = TextLine("iiii", font: font).width
        let spaced = TextLine("iiii", font: font, letterSpacing: 10).width
        XCTAssertEqual(spaced - plain, 40, accuracy: 0.5, "a gap after EVERY glyph")
        let a = try XCTUnwrap(Self.ink(try PainterTests.paint(300, 80, [.fillText(Self.text("iiii", 10, 60), paint: Self.black)])))
        let b = try XCTUnwrap(Self.ink(try PainterTests.paint(300, 80, [.fillText(Self.text("iiii", 10, 60, letterSpacing: 10), paint: Self.black)])))
        XCTAssertGreaterThanOrEqual(b.2 - a.2, 28, "the last glyph moved right by three gaps")
        XCTAssertEqual(a.0, b.0, "the first glyph stays")
    }

    func testMaxWidthCondensesTheLine() throws {
        let wide = try XCTUnwrap(Self.ink(try PainterTests.paint(400, 80, [.fillText(Self.text("MMMMMMMM", 10, 60), paint: Self.black)])))
        XCTAssertGreaterThan(wide.2, 120)
        let narrow = try XCTUnwrap(Self.ink(try PainterTests.paint(400, 80, [.fillText(Self.text("MMMMMMMM", 10, 60, maxWidth: 100), paint: Self.black)])))
        XCTAssertLessThanOrEqual(narrow.2, 111)
        XCTAssertGreaterThanOrEqual(narrow.0, 9)
        XCTAssertGreaterThan(narrow.2, 80, "condensed, not clipped")
        let centered = try XCTUnwrap(Self.ink(try PainterTests.paint(400, 80, [.fillText(Self.text("MMMMMMMM", 200, 60, align: .center, maxWidth: 100), paint: Self.black)])))
        XCTAssertLessThanOrEqual(abs((centered.0 + centered.2) / 2 - 200), 3, "condensed around the aligned origin")
    }

    func testStrokeTextIsAnOutline() throws {
        let stroke = StrokeData(width: 2, join: .miter, cap: .butt, miterLimit: 10, dashOffset: 0, dash: [])
        let big = FontData(family: "sans-serif", size: 200, weight: 400, italic: false)
        let ctx = try PainterTests.paint(240, 240, [.strokeText(Self.text("O", 20, 200, font: big), stroke: stroke, paint: Self.black)])
        let box = try XCTUnwrap(Self.ink(ctx))
        XCTAssertEqual(Surface.pixel(ctx, (box.0 + box.2) / 2, (box.1 + box.3) / 2)[3], 0, "the counter is empty")
        let fill = try PainterTests.paint(240, 240, [.fillText(Self.text("O", 20, 200, font: big), paint: Self.black)])
        let fbox = try XCTUnwrap(Self.ink(fill))
        XCTAssertLessThanOrEqual(abs(fbox.0 - box.0), 2, "the outline sits on the fill's edge (centered stroke)")
        var strokeInk = 0, fillInk = 0
        for y in 0..<240 { for x in 0..<240 { if Surface.pixel(ctx, x, y)[3] > 40 { strokeInk += 1 }; if Surface.pixel(fill, x, y)[3] > 40 { fillInk += 1 } } }
        XCTAssertLessThan(strokeInk * 3, fillInk, "an outline is much lighter than a fill")
    }

    func testAGradientFillsTheGlyphs() throws {
        let red = Color(r: 1, g: 0, b: 0, a: 1), blue = Color(r: 0, g: 0, b: 1, a: 1)
        let paint = PaintData.linear(alpha: 1, matrix: Matrix(a: 200, b: 0, c: 0, d: 200, e: 10, f: 0), spread: .pad, stops: [Stop(offset: 0, color: red), Stop(offset: 1, color: blue)])
        let ctx = try PainterTests.paint(240, 80, [.fillText(Self.text("MMMMMMMMMM", 10, 60), paint: paint)])
        let box = try XCTUnwrap(Self.ink(ctx))
        var leftRed = 0, rightBlue = 0
        for y in box.1...box.3 { for x in box.0...box.2 {
            let p = Surface.pixel(ctx, x, y)
            if p[3] > 200 && x < box.0 + 20 && p[0] > p[2] { leftRed += 1 }
            if p[3] > 200 && x > box.2 - 20 && p[2] > p[0] { rightBlue += 1 }
        } }
        XCTAssertGreaterThan(leftRed, 20); XCTAssertGreaterThan(rightBlue, 20)
        XCTAssertEqual(Surface.pixel(ctx, 5, 5), [0, 0, 0, 0], "the gradient is clipped to the glyphs")
    }

    func testEmptyTextDrawsNothingAndTheHookSeesTheFont() throws {
        final class Recording: PainterHooks {
            var seen: [FontData] = []
            func font(_ font: FontData) -> CTFont { seen.append(font); return Fonts.resolve(font) }
        }
        let hooks = Recording()
        let inter = FontData(family: "Inter", size: 24, weight: 700, italic: true)
        let ctx = try PainterTests.paint(100, 50, [.fillText(Self.text("", 10, 30, font: inter), paint: Self.black), .fillText(Self.text("x", 10, 30, font: inter), paint: Self.black)], hooks: hooks)
        XCTAssertEqual(hooks.seen, [inter], "called once, for the non-empty text, with the resolved font")
        XCTAssertNotNil(Self.ink(ctx))
    }

    func testDefaultFontResolution() {
        let mono = Fonts.resolve(FontData(family: "monospace", size: 12, weight: 400, italic: false))
        XCTAssertEqual(CTFontCopyFamilyName(mono) as String, "Menlo")
        let named = Fonts.resolve(FontData(family: "Times New Roman", size: 12, weight: 400, italic: false))
        XCTAssertEqual(CTFontCopyFamilyName(named) as String, "Times New Roman")
        let unknown = Fonts.resolve(FontData(family: "NoSuchFamily-XYZ", size: 12, weight: 400, italic: false))
        let system = Fonts.resolve(FontData(family: "sans-serif", size: 12, weight: 400, italic: false))
        XCTAssertEqual(CTFontCopyFamilyName(unknown) as String, CTFontCopyFamilyName(system) as String, "an unknown family is the system font, not Helvetica")
        XCTAssertNotEqual(CTFontCopyFamilyName(unknown) as String, "Helvetica")
        let bold = Fonts.resolve(FontData(family: "sans-serif", size: 12, weight: 700, italic: true))
        XCTAssertTrue(CTFontGetSymbolicTraits(bold).contains(.traitBold))
        XCTAssertTrue(CTFontGetSymbolicTraits(bold).contains(.traitItalic))
        XCTAssertFalse(CTFontGetSymbolicTraits(system).contains(.traitBold))
        XCTAssertEqual(CTFontGetSize(bold), 12)
        XCTAssertEqual(Fonts.ctWeight(400), 0); XCTAssertEqual(Fonts.ctWeight(700), 0.4); XCTAssertEqual(Fonts.ctWeight(100), -0.8); XCTAssertEqual(Fonts.ctWeight(900), 0.62)
        XCTAssertEqual(Fonts.ctWeight(650), 0.35, accuracy: 0.001)
    }

    func testTheMeasureHelperMatchesTheFontMetrics() {
        let font = Fonts.resolve(Self.sans)
        let line = TextLine("Hello", font: font)
        XCTAssertGreaterThan(line.width, 0)
        XCTAssertEqual(line.ascent, CTFontGetAscent(font)); XCTAssertEqual(line.descent, CTFontGetDescent(font))
        XCTAssertEqual(TextLine("", font: font).width, 0)
    }
}
