// The Apple painter in real pixels, on a bitmap surface (Surface.makeContext: sRGB, the y-flip as the
// base CTM). The transform cases are the Android CanvasPainterTest's; the shapes probes are the web
// painter test's, same coordinates and expectations as Skia.
import CoreGraphics
import XCTest
import AnyCanvas

final class PainterTests: XCTestCase {
    private let red = PaintData.solid(alpha: 1, color: Color(r: 1, g: 0, b: 0, a: 1))
    private let blue = PaintData.solid(alpha: 1, color: Color(r: 0, g: 0, b: 1, a: 1))

    static func rect(_ x: Float, _ y: Float, _ w: Float, _ h: Float) -> PathData {
        let m = Float(PathVerb.move.rawValue), l = Float(PathVerb.line.rawValue), z = Float(PathVerb.close.rawValue)
        return PathData(words: [m, x, y, l, x + w, y, l, x + w, y + h, l, x, y + h, z])
    }
    static func fill(_ paint: PaintData, _ x: Float, _ y: Float, _ w: Float, _ h: Float) -> DrawCommand {
        .fillPath(rule: .nonzero, paint: paint, path: rect(x, y, w, h))
    }
    static func matrix(_ a: Float, _ b: Float, _ c: Float, _ d: Float, _ e: Float, _ f: Float) -> DrawCommand {
        .setTransform(Matrix(a: a, b: b, c: c, d: d, e: e, f: f))
    }

    static func paint(_ w: Int, _ h: Int, _ commands: [DrawCommand], hooks: PainterHooks = DefaultHooks()) throws -> CGContext {
        let ctx = try XCTUnwrap(Surface.makeContext(width: w, height: h))
        Painter(hooks: hooks).paint(ctx, commands)
        return ctx
    }
    static func px(_ ctx: CGContext, _ x: Int, _ y: Int) -> [UInt8] { Surface.pixel(ctx, x, y) }

    // ---- transforms (CanvasPainterTest.kt) ------------------------------------------------------

    func testATranslateAfterTheFirstDrawLandsAtDeviceScaleOnA2xCanvas() throws {
        // c.fillRect(0,0,10,10); c.save(); c.translate(50,0); c.fillRect(0,0,10,10); c.restore() at pixelRatio 2.
        let ctx = try Self.paint(240, 40, [
            Self.matrix(2, 0, 0, 2, 0, 0), Self.fill(red, 0, 0, 10, 10),
            .save, Self.matrix(2, 0, 0, 2, 100, 0), Self.fill(blue, 0, 0, 10, 10), .restore,
        ])
        XCTAssertEqual(Self.px(ctx, 10, 10), [255, 0, 0, 255])
        XCTAssertEqual(Self.px(ctx, 110, 10), [0, 0, 255, 255])   // device 100..120, not 200..220
        XCTAssertEqual(Self.px(ctx, 210, 10), [0, 0, 0, 0])
    }

    func testScaleThenTranslateComposesLikeTheListNotTwice() throws {
        let ctx = try Self.paint(60, 20, [
            Self.matrix(2, 0, 0, 2, 0, 0), Self.fill(red, 0, 0, 5, 5),
            Self.matrix(2, 0, 0, 2, 20, 0), Self.fill(blue, 0, 0, 5, 5),
        ])
        XCTAssertEqual(Self.px(ctx, 5, 5), [255, 0, 0, 255])
        XCTAssertEqual(Self.px(ctx, 25, 5), [0, 0, 255, 255])     // device 20..30, not 40..50
        XCTAssertEqual(Self.px(ctx, 45, 5), [0, 0, 0, 0])
    }

    func testASingularTransformDrawsNothingAndTheNextOneLandsWhereTheListSays() throws {
        let ctx = try Self.paint(40, 20, [
            Self.matrix(0, 0, 0, 0, 0, 0), Self.fill(red, 0, 0, 40, 20),
            Self.matrix(1, 0, 0, 1, 20, 0), Self.fill(blue, 0, 0, 10, 10),
        ])
        XCTAssertEqual(Self.px(ctx, 5, 5), [0, 0, 0, 0])
        XCTAssertEqual(Self.px(ctx, 25, 5), [0, 0, 255, 255])
    }

    func testAClipUnderASingularTransformIsEmpty() throws {
        let ctx = try Self.paint(40, 20, [
            .save, Self.matrix(0, 0, 0, 0, 0, 0), .clip(rule: .nonzero, path: Self.rect(0, 0, 40, 20)),
            Self.matrix(1, 0, 0, 1, 0, 0), Self.fill(red, 0, 0, 40, 20), .restore,
            Self.fill(blue, 30, 0, 10, 10),
        ])
        XCTAssertEqual(Self.px(ctx, 5, 5), [0, 0, 0, 0])
        XCTAssertEqual(Self.px(ctx, 35, 5), [0, 0, 255, 255])
    }

    func testTheContextsOwnTransformStaysTheBaseAsOnAViewsContext() throws {
        let ctx = try XCTUnwrap(Surface.makeContext(width: 60, height: 20))
        ctx.translateBy(x: 30, y: 0)   // the node's offset in its container
        Painter().paint(ctx, [
            Self.matrix(2, 0, 0, 2, 0, 0), Self.fill(red, 0, 0, 2, 2),
            Self.matrix(2, 0, 0, 2, 10, 0), Self.fill(blue, 0, 0, 2, 2),
        ])
        XCTAssertEqual(Self.px(ctx, 32, 2), [255, 0, 0, 255])      // 30 + 0..4
        XCTAssertEqual(Self.px(ctx, 42, 2), [0, 0, 255, 255])      // 30 + 10..14
        XCTAssertEqual(Self.px(ctx, 12, 2), [0, 0, 0, 0])
    }

    func testYIsDownOnTheSurface() throws {
        let ctx = try Self.paint(10, 30, [Self.fill(red, 0, 0, 10, 10)])
        XCTAssertEqual(Self.px(ctx, 5, 5), [255, 0, 0, 255])
        XCTAssertEqual(Self.px(ctx, 5, 25), [0, 0, 0, 0])
    }

    // ---- the goldens ----------------------------------------------------------------------------

    /// A 64×32 magenta stub for DRAW_IMAGE, as in the web painter test.
    struct StubHooks: PainterHooks {
        let image: CGImage? = {
            guard let c = Surface.makeContext(width: 64, height: 32) else { return nil }
            c.setFillColor(Painter.cgColor(Color(r: 1, g: 0, b: 1, a: 1)))
            c.fill(CGRect(x: 0, y: 0, width: 64, height: 32))
            return c.makeImage()
        }()
        func image(_ surface: Int32) -> CGImage? { image }
    }

    static func render(_ name: String, _ w: Int, _ h: Int) throws -> CGContext {
        let ctx = try Self.paint(w, h, try Golden.commands(name), hooks: StubHooks())
        try FileManager.default.createDirectory(at: Golden.outDir, withIntermediateDirectories: true)
        if let png = AnyCanvas.encode(rgba: Surface.straightRGBA(ctx), width: w, height: h) {
            try png.write(to: Golden.outDir.appendingPathComponent("\(name).png"))
        }
        return ctx
    }

    func testEveryGoldenReplays() throws {
        for name in try Golden.names() { _ = try Self.render(name, 640, 560) }
    }

    func testShapesTheColorsLandWhereTheDrawingSays2x() throws {
        let ctx = try Self.render("stream-shapes", 700, 600)
        // The blue rect at (10,10)-(110,70) logical → (20,20)-(220,140) device.
        XCTAssertEqual(Self.px(ctx, 100, 60), [30, 144, 255, 255])
        // Outside everything: transparent.
        XCTAssertEqual(Self.px(ctx, 690, 590), [0, 0, 0, 0])
        // The orange pie: its center area is filled (the 270° sweep covers the upper-left quadrant).
        XCTAssertEqual(Self.px(ctx, 380, 100), [255, 165, 0, 255])
        // The clipped green circle: inside the clip rect it is painted, just outside it is not.
        XCTAssertGreaterThan(Self.px(ctx, 560, 100)[1], 100)
        XCTAssertEqual(Self.px(ctx, 470, 100), [0, 0, 0, 0])
        // The gradient card: white-ish at the top, grey at the bottom.
        let top = Self.px(ctx, 200, 250), bottom = Self.px(ctx, 200, 390)
        XCTAssertGreaterThan(Int(top[0]), Int(bottom[0]) + 60)
    }

    func testAListLevelClipDoesNotOutliveThePaint() throws {
        let ctx = try XCTUnwrap(Surface.makeContext(width: 8, height: 8))
        let painter = Painter()
        painter.paint(ctx, [.clip(rule: .nonzero, path: Self.rect(0, 0, 4, 8)), Self.fill(red, 0, 0, 8, 8)])
        XCTAssertEqual(Self.px(ctx, 6, 4), [0, 0, 0, 0])
        Surface.clear(ctx)
        painter.paint(ctx, [Self.fill(blue, 0, 0, 8, 8)])
        XCTAssertEqual(Self.px(ctx, 6, 4), [0, 0, 255, 255])
        XCTAssertEqual(Self.px(ctx, 1, 4), [0, 0, 255, 255])
    }

    func testPaintingTwiceIsDeterministic() throws {
        let commands = try Golden.commands("stream-all-ops")
        let a = try Self.paint(300, 300, commands, hooks: StubHooks())
        let b = try Self.paint(300, 300, commands, hooks: StubHooks())
        XCTAssertEqual(Surface.straightRGBA(a), Surface.straightRGBA(b))
    }

    func testAnUnbalancedRestoreNeverPopsTheWrapper() throws {
        let ctx = try Self.paint(8, 8, [.restore, .restore, Self.fill(red, 0, 0, 8, 8), .restore])
        XCTAssertEqual(Self.px(ctx, 4, 4), [255, 0, 0, 255])
    }

    func testClearRectMakesPixelsTransparent() throws {
        let ctx = try Self.paint(20, 10, [Self.fill(red, 0, 0, 20, 10), .clearRect(Rect4(x: 5, y: 0, w: 10, h: 10))])
        XCTAssertEqual(Self.px(ctx, 2, 5), [255, 0, 0, 255])
        XCTAssertEqual(Self.px(ctx, 10, 5), [0, 0, 0, 0])
        XCTAssertEqual(Self.px(ctx, 17, 5), [255, 0, 0, 255])
    }
}
