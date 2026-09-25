// Gradients in pixels: the matrix is concatenated into gradient space (the axis of a rotated
// linear gradient, the focal point and start radius of a radial one), gradient strokes, the paint
// alpha, and the three spreads. NanoSVGKit read the axis off the matrix's first column and drew
// reflect / repeat as "nothing outside the ramp" — these pin the opposite.
import CoreGraphics
import XCTest
import AnyCanvas

final class GradientTests: XCTestCase {
    static let red = Color(r: 1, g: 0, b: 0, a: 1), blue = Color(r: 0, g: 0, b: 1, a: 1)
    static let white = Color(r: 1, g: 1, b: 1, a: 1), black = Color(r: 0, g: 0, b: 0, a: 1)
    static func stops(_ a: Color, _ b: Color) -> [Stop] { [Stop(offset: 0, color: a), Stop(offset: 1, color: b)] }
    static func m(_ a: Float, _ b: Float, _ c: Float, _ d: Float, _ e: Float, _ f: Float) -> Matrix { Matrix(a: a, b: b, c: c, d: d, e: e, f: f) }

    func near(_ px: [UInt8], _ want: [Int], tol: Int = 12, _ message: String = "", file: StaticString = #filePath, line: UInt = #line) {
        let ok = zip(px, want).allSatisfy { abs(Int($0) - $1) <= tol }
        XCTAssertTrue(ok, "\(message) got \(px) want \(want) ±\(tol)", file: file, line: line)
    }

    func testALinearGradientRunsAlongTheMatrixAxisNotItsFirstColumn() throws {
        // Gradient space (0,0)→(1,0) maps to user (10,0)→(10,40): the ramp runs DOWN the rect.
        let paint = PaintData.linear(alpha: 1, matrix: Self.m(0, 40, -40, 0, 10, 0), spread: .pad, stops: Self.stops(Self.red, Self.blue))
        let ctx = try PainterTests.paint(20, 40, [.fillPath(rule: .nonzero, paint: paint, path: PainterTests.rect(0, 0, 20, 40))])
        near(Surface.pixel(ctx, 10, 1), [255, 0, 0, 255], "top is the first stop")
        near(Surface.pixel(ctx, 10, 38), [0, 0, 255, 255], "bottom is the last stop")
        near(Surface.pixel(ctx, 2, 20), Surface.pixel(ctx, 18, 20).map(Int.init), tol: 2, "uniform across the axis")
        near(Surface.pixel(ctx, 10, 20), [128, 0, 128, 255], tol: 20, "the middle is the middle")
    }

    func testARadialGradientHonoursTheFocalPoint() throws {
        // The unit circle at user (20,20) r 20; the focal point at (-0.5, 0) = user (10, 20).
        let paint = PaintData.radial(alpha: 1, matrix: Self.m(20, 0, 0, 20, 20, 20), fx: -0.5, fy: 0, r0: 0, spread: .pad, stops: Self.stops(Self.white, Self.black))
        let ctx = try PainterTests.paint(40, 40, [.fillPath(rule: .nonzero, paint: paint, path: PainterTests.rect(0, 0, 40, 40))])
        near(Surface.pixel(ctx, 10, 20), [255, 255, 255, 255], tol: 24, "white at the focal point")
        let center = Surface.pixel(ctx, 20, 20)
        XCTAssertLessThan(center[0], 200, "the center is not the first color: the highlight moved to the focal point")
        near(Surface.pixel(ctx, 39, 20), [0, 0, 0, 255], tol: 24, "black at the rim")
        XCTAssertGreaterThan(Surface.pixel(ctx, 1, 20)[0], Surface.pixel(ctx, 39, 20)[0], "the rim nearer the focal point is lighter")
    }

    func testARadialStartRadiusLeavesTheInnerDiscTheFirstColor() throws {
        let paint = PaintData.radial(alpha: 1, matrix: Self.m(20, 0, 0, 20, 20, 20), fx: 0, fy: 0, r0: 0.5, spread: .pad, stops: Self.stops(Self.red, Self.blue))
        let ctx = try PainterTests.paint(40, 40, [.fillPath(rule: .nonzero, paint: paint, path: PainterTests.rect(0, 0, 40, 40))])
        near(Surface.pixel(ctx, 20, 20), [255, 0, 0, 255], "the center")
        near(Surface.pixel(ctx, 28, 20), [255, 0, 0, 255], "still inside the start circle (r 10)")
        near(Surface.pixel(ctx, 39, 20), [0, 0, 255, 255], tol: 24, "the rim")
        near(Surface.pixel(ctx, 35, 20), [128, 0, 128, 255], tol: 40, "half way between r0 and 1")
    }

    func testAGradientStrokeColorsTheBandAndNotTheInterior() throws {
        let paint = PaintData.linear(alpha: 1, matrix: Self.m(20, 0, 0, 20, 10, 0), spread: .pad, stops: Self.stops(Self.red, Self.blue))
        let stroke = StrokeData(width: 4, join: .miter, cap: .butt, miterLimit: 10, dashOffset: 0, dash: [])
        let ctx = try PainterTests.paint(40, 40, [.strokePath(stroke: stroke, paint: paint, path: PainterTests.rect(10, 10, 20, 20))])
        XCTAssertEqual(Surface.pixel(ctx, 20, 20), [0, 0, 0, 0], "the interior is untouched")
        near(Surface.pixel(ctx, 10, 20), [255, 0, 0, 255], tol: 24, "the left band is the first stop")
        near(Surface.pixel(ctx, 30, 20), [0, 0, 255, 255], tol: 24, "the right band is the last stop")
        XCTAssertEqual(Surface.pixel(ctx, 5, 20), [0, 0, 0, 0], "outside the band")
    }

    func testAGradientStrokeIsDashed() throws {
        let paint = PaintData.linear(alpha: 1, matrix: Self.m(40, 0, 0, 40, 0, 0), spread: .pad, stops: Self.stops(Self.red, Self.red))
        let stroke = StrokeData(width: 4, join: .miter, cap: .butt, miterLimit: 10, dashOffset: 0, dash: [10, 10])
        let line = PathData(words: [Float(PathVerb.move.rawValue), 0, 10, Float(PathVerb.line.rawValue), 40, 10])
        let ctx = try PainterTests.paint(40, 20, [.strokePath(stroke: stroke, paint: paint, path: line)])
        near(Surface.pixel(ctx, 5, 10), [255, 0, 0, 255], "on a dash")
        XCTAssertEqual(Surface.pixel(ctx, 15, 10), [0, 0, 0, 0], "in a gap")
        near(Surface.pixel(ctx, 25, 10), [255, 0, 0, 255], "on the next dash")
    }

    func testThePaintAlphaAppliesToAGradient() throws {
        let paint = PaintData.linear(alpha: 0.5, matrix: Self.m(20, 0, 0, 20, 0, 0), spread: .pad, stops: Self.stops(Self.red, Self.red))
        let ctx = try PainterTests.paint(20, 20, [.fillPath(rule: .nonzero, paint: paint, path: PainterTests.rect(0, 0, 20, 20))])
        near(Surface.pixel(ctx, 10, 10), [255, 0, 0, 128], tol: 4)
    }

    func testAlphaInTheStopsFades() throws {
        let clear = Color(r: 0, g: 0, b: 1, a: 0)
        let paint = PaintData.linear(alpha: 1, matrix: Self.m(40, 0, 0, 40, 0, 0), spread: .pad, stops: Self.stops(Self.blue, clear))
        let ctx = try PainterTests.paint(40, 10, [.fillPath(rule: .nonzero, paint: paint, path: PainterTests.rect(0, 0, 40, 10))])
        near(Surface.pixel(ctx, 1, 5), [0, 0, 255, 255], tol: 20)
        XCTAssertLessThan(Surface.pixel(ctx, 38, 5)[3], 30)
        near(Surface.pixel(ctx, 20, 5), [0, 0, 255, 128], tol: 24)
    }

    func testPadExtendsTheEndColors() throws {
        let paint = PaintData.linear(alpha: 1, matrix: Self.m(10, 0, 0, 10, 10, 0), spread: .pad, stops: Self.stops(Self.red, Self.blue))
        let ctx = try PainterTests.paint(30, 10, [.fillPath(rule: .nonzero, paint: paint, path: PainterTests.rect(0, 0, 30, 10))])
        near(Surface.pixel(ctx, 2, 5), [255, 0, 0, 255], "before the start")
        near(Surface.pixel(ctx, 28, 5), [0, 0, 255, 255], "after the end")
    }

    func testRepeatAndReflectContinueTheRamp() throws {
        // One period = 10 px starting at x 0; the rect covers three periods.
        let rep = PaintData.linear(alpha: 1, matrix: Self.m(10, 0, 0, 10, 0, 0), spread: .repeat, stops: Self.stops(Self.red, Self.blue))
        let ctx = try PainterTests.paint(30, 10, [.fillPath(rule: .nonzero, paint: rep, path: PainterTests.rect(0, 0, 30, 10))])
        near(Surface.pixel(ctx, 0, 5), [255, 0, 0, 255], tol: 40, "period 0 starts red")
        near(Surface.pixel(ctx, 9, 5), [26, 0, 229, 255], tol: 40, "period 0 ends blue")
        near(Surface.pixel(ctx, 10, 5), [255, 0, 0, 255], tol: 40, "period 1 restarts red (repeat)")
        near(Surface.pixel(ctx, 29, 5), [26, 0, 229, 255], tol: 40, "period 2 ends blue")

        let ref = PaintData.linear(alpha: 1, matrix: Self.m(10, 0, 0, 10, 0, 0), spread: .reflect, stops: Self.stops(Self.red, Self.blue))
        let ctx2 = try PainterTests.paint(30, 10, [.fillPath(rule: .nonzero, paint: ref, path: PainterTests.rect(0, 0, 30, 10))])
        near(Surface.pixel(ctx2, 0, 5), [255, 0, 0, 255], tol: 40, "period 0 starts red")
        near(Surface.pixel(ctx2, 10, 5), [26, 0, 229, 255], tol: 40, "period 1 starts blue (mirrored)")
        near(Surface.pixel(ctx2, 19, 5), [255, 0, 0, 255], tol: 40, "period 1 ends red")
        near(Surface.pixel(ctx2, 20, 5), [255, 0, 0, 255], tol: 40, "period 2 starts red again")
    }

    func testARadialRepeatContinuesPastTheEndCircle() throws {
        let paint = PaintData.radial(alpha: 1, matrix: Self.m(10, 0, 0, 10, 20, 20), fx: 0, fy: 0, r0: 0, spread: .repeat, stops: Self.stops(Self.red, Self.blue))
        let ctx = try PainterTests.paint(40, 40, [.fillPath(rule: .nonzero, paint: paint, path: PainterTests.rect(0, 0, 40, 40))])
        near(Surface.pixel(ctx, 20, 20), [255, 0, 0, 255], tol: 40, "the center")
        near(Surface.pixel(ctx, 29, 20), [26, 0, 229, 255], tol: 40, "just inside the end circle")
        near(Surface.pixel(ctx, 31, 20), [255, 0, 0, 255], tol: 40, "just past it: the ramp restarts")
    }

    func testADegenerateGradientPaintsNothing() throws {
        let paint = PaintData.linear(alpha: 1, matrix: Self.m(0, 0, 0, 0, 5, 5), spread: .pad, stops: Self.stops(Self.red, Self.blue))
        let ctx = try PainterTests.paint(10, 10, [.fillPath(rule: .nonzero, paint: paint, path: PainterTests.rect(0, 0, 10, 10))])
        XCTAssertEqual(Surface.pixel(ctx, 5, 5), [0, 0, 0, 0])
    }

    func testEvenOddLeavesTheHole() throws {
        let outer = PainterTests.rect(0, 0, 30, 30).words, inner = PainterTests.rect(10, 10, 10, 10).words
        let path = PathData(words: outer + inner)
        let ctx = try PainterTests.paint(30, 30, [.fillPath(rule: .evenodd, paint: .solid(alpha: 1, color: Self.red), path: path)])
        XCTAssertEqual(Surface.pixel(ctx, 15, 15), [0, 0, 0, 0])
        XCTAssertEqual(Surface.pixel(ctx, 5, 5), [255, 0, 0, 255])
        let ctx2 = try PainterTests.paint(30, 30, [.fillPath(rule: .nonzero, paint: .solid(alpha: 1, color: Self.red), path: path)])
        XCTAssertEqual(Surface.pixel(ctx2, 15, 15), [255, 0, 0, 255])
    }
}
