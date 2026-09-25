// DRAW_IMAGE (crop in device px, draw in user space, top-down, alpha), and the encode round trip
// through the platform's decoder (the surface host's toFile path).
import CoreGraphics
import ImageIO
import XCTest
import AnyCanvas

final class ImageTests: XCTestCase {
    /// A w×h image from a row-major list of colors.
    static func image(_ w: Int, _ h: Int, _ colors: [Color]) -> CGImage {
        let ctx = Surface.makeContext(width: w, height: h)!
        for (i, c) in colors.enumerated() {
            ctx.setFillColor(Painter.cgColor(c))
            ctx.fill(CGRect(x: i % w, y: i / w, width: 1, height: 1))
        }
        return ctx.makeImage()!
    }
    static let red = Color(r: 1, g: 0, b: 0, a: 1), blue = Color(r: 0, g: 0, b: 1, a: 1)

    struct Hooks: PainterHooks {
        let images: [Int32: CGImage]
        func image(_ surface: Int32) -> CGImage? { images[surface] }
    }

    func testTheImageLandsTopDown() throws {
        let hooks = Hooks(images: [1: Self.image(1, 2, [Self.red, Self.blue])])
        let ctx = try PainterTests.paint(10, 20, [.drawImage(surface: 1, src: Rect4(x: 0, y: 0, w: 1, h: 2), dst: Rect4(x: 0, y: 0, w: 10, h: 20), alpha: 1)], hooks: hooks)
        XCTAssertEqual(Surface.pixel(ctx, 5, 5), [255, 0, 0, 255])
        XCTAssertEqual(Surface.pixel(ctx, 5, 15), [0, 0, 255, 255])
    }

    func testTheSourceRectCropsInDevicePixelsUnderATransform() throws {
        let hooks = Hooks(images: [7: Self.image(2, 1, [Self.red, Self.blue])])
        let ctx = try PainterTests.paint(40, 20, [
            .setTransform(Matrix(a: 2, b: 0, c: 0, d: 2, e: 0, f: 0)),
            .drawImage(surface: 7, src: Rect4(x: 1, y: 0, w: 1, h: 1), dst: Rect4(x: 0, y: 0, w: 10, h: 10), alpha: 1),
        ], hooks: hooks)
        XCTAssertEqual(Surface.pixel(ctx, 5, 5), [0, 0, 255, 255], "only the blue half")
        XCTAssertEqual(Surface.pixel(ctx, 15, 5), [0, 0, 255, 255], "drawn 20 device px wide")
        XCTAssertEqual(Surface.pixel(ctx, 25, 5), [0, 0, 0, 0])
    }

    func testAlphaAndAMissingSurface() throws {
        let hooks = Hooks(images: [1: Self.image(1, 1, [Self.red])])
        let ctx = try PainterTests.paint(10, 10, [
            .drawImage(surface: 1, src: Rect4(x: 0, y: 0, w: 1, h: 1), dst: Rect4(x: 0, y: 0, w: 5, h: 10), alpha: 0.5),
            .drawImage(surface: 2, src: Rect4(x: 0, y: 0, w: 1, h: 1), dst: Rect4(x: 5, y: 0, w: 5, h: 10), alpha: 1),
        ], hooks: hooks)
        let p = Surface.pixel(ctx, 2, 5)
        XCTAssertEqual(p[0], 255); XCTAssertEqual(Int(p[3]), 128, accuracy: 3)
        XCTAssertEqual(Surface.pixel(ctx, 7, 5), [0, 0, 0, 0])
    }

    func testEncodeRoundTripsThroughImageIO() throws {
        let w = 4, h = 3
        var rgba: [UInt8] = []
        for i in 0..<(w * h) { rgba += i % 3 == 0 ? [0, 0, 0, 0] : [UInt8(i * 20), UInt8(255 - i * 20), 77, 255] }
        let png = try XCTUnwrap(AnyCanvas.encode(rgba: rgba, width: w, height: h))
        let src = try XCTUnwrap(CGImageSourceCreateWithData(png as CFData, nil))
        let img = try XCTUnwrap(CGImageSourceCreateImageAtIndex(src, 0, nil))
        XCTAssertEqual(img.width, w); XCTAssertEqual(img.height, h)
        let ctx = try XCTUnwrap(Surface.makeContext(width: w, height: h))
        Painter(hooks: Hooks(images: [1: img])).paint(ctx, [.drawImage(surface: 1, src: Rect4(x: 0, y: 0, w: Float(w), h: Float(h)), dst: Rect4(x: 0, y: 0, w: Float(w), h: Float(h)), alpha: 1)])
        XCTAssertEqual(Surface.straightRGBA(ctx), rgba)
        let jpeg = try XCTUnwrap(AnyCanvas.encode(rgba: rgba, width: w, height: h, jpeg: true, quality: 90))
        let jsrc = try XCTUnwrap(CGImageSourceCreateWithData(jpeg as CFData, nil))
        let jimg = try XCTUnwrap(CGImageSourceCreateImageAtIndex(jsrc, 0, nil))
        XCTAssertEqual(jimg.width, w); XCTAssertEqual(jimg.height, h)
    }

    func testStraightRGBAUnpremultiplies() throws {
        let ctx = try PainterTests.paint(2, 1, [PainterTests.fill(.solid(alpha: 0.5, color: Self.red), 0, 0, 1, 1)])
        let px = Surface.straightRGBA(ctx)
        XCTAssertEqual(px.count, 8)
        XCTAssertEqual(px[0], 255, "straight red, not premultiplied 128"); XCTAssertEqual(Int(px[3]), 128, accuracy: 2)
        XCTAssertEqual(Array(px[4..<8]), [0, 0, 0, 0])
        XCTAssertEqual(Surface.pixel(ctx, 0, 0)[0], 255)
    }
}
