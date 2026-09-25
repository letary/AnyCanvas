// The cross-painter reference (the plan's "painter smoke"): the shapes-only SVG goldens painted by
// CoreGraphics against `acdump png` (nanosvgrast, the CPU eye reference) at the same size. A loose
// metric — the mean channel error away from edges — sanity, not conformance: anti-aliasing, gradient
// interpolation and stroke geometry legitimately differ between rasterizers. Needs the CMake build
// (./build.sh → build-posix/acdump); skipped without it, and on iOS (no Process).
import CoreGraphics
import ImageIO
import XCTest
import AnyCanvas

final class CrossPainterTests: XCTestCase {
    #if os(macOS)
    static let acdump: URL? = ["build-posix/acdump", "build/acdump"].map { Golden.repoRoot.appendingPathComponent($0) }.first { FileManager.default.isExecutableFile(atPath: $0.path) }

    /// Straight RGBA8 of a PNG file plus its size.
    static func decode(_ url: URL) throws -> ([UInt8], Int, Int) {
        let src = try XCTUnwrap(CGImageSourceCreateWithURL(url as CFURL, nil))
        let img = try XCTUnwrap(CGImageSourceCreateImageAtIndex(src, 0, nil))
        let ctx = try XCTUnwrap(Surface.makeContext(width: img.width, height: img.height))
        struct Hooks: PainterHooks { let img: CGImage; func image(_ surface: Int32) -> CGImage? { img } }
        let r = Rect4(x: 0, y: 0, w: Float(img.width), h: Float(img.height))
        Painter(hooks: Hooks(img: img)).paint(ctx, [.drawImage(surface: 1, src: r, dst: r, alpha: 1)])
        return (Surface.straightRGBA(ctx), img.width, img.height)
    }

    /// Mean absolute channel error (0..255) over the pixels that are not on an edge of the reference
    /// (an edge: the alpha or luminance of a 3×3 neighbourhood varies by more than 48), and the share
    /// of those pixels that differ by more than 64 in some channel.
    static func compare(_ a: [UInt8], _ b: [UInt8], _ w: Int, _ h: Int) -> (mean: Double, gross: Double, counted: Int) {
        func lum(_ p: [UInt8], _ i: Int) -> Int { (Int(p[i]) * 3 + Int(p[i + 1]) * 6 + Int(p[i + 2])) / 10 }
        var sum = 0, n = 0, gross = 0
        for y in 1..<(h - 1) { for x in 1..<(w - 1) {
            let i = (y * w + x) * 4
            var edge = false
            for dy in -1...1 where !edge { for dx in -1...1 {
                let j = ((y + dy) * w + (x + dx)) * 4
                if abs(Int(a[i + 3]) - Int(a[j + 3])) > 48 || abs(lum(a, i) - lum(a, j)) > 48 { edge = true; break }
            } }
            if edge { continue }
            var worst = 0
            for c in 0..<4 { let d = abs(Int(a[i + c]) - Int(b[i + c])); sum += d; worst = max(worst, d) }
            n += 1
            if worst > 64 { gross += 1 }
        } }
        return (n == 0 ? 0 : Double(sum) / Double(n * 4), n == 0 ? 0 : Double(gross) / Double(n), n)
    }

    func testShapesOnlySvgsMatchTheCpuReference() throws {
        guard let acdump = Self.acdump else { throw XCTSkip("no acdump build (./build.sh)") }
        let core = AnyCanvas()
        try FileManager.default.createDirectory(at: Golden.outDir, withIntermediateDirectories: true)
        // Shapes, strokes, dashes, transforms, linear gradients — what nanosvgrast draws the same way.
        // Not radial-focal (nanosvgrast's radial is concentric) and not text (nanosvgrast has none).
        for name in ["nano", "evenodd-transform", "gradient-rect", "ios-shapes", "drawing"] {
            let ref = Golden.outDir.appendingPathComponent("ref-\(name).png")
            let p = Process()
            p.executableURL = acdump
            p.arguments = ["png", Golden.dir.appendingPathComponent("svg/\(name).svg").path, ref.path]
            p.standardOutput = FileHandle.nullDevice
            try p.run(); p.waitUntilExit()
            XCTAssertEqual(p.terminationStatus, 0, "acdump png \(name)")
            let (expected, w, h) = try Self.decode(ref)
            let svg = try XCTUnwrap(core.parseSvg(try Golden.svg("\(name).svg").markup))
            let ctx = try XCTUnwrap(Surface.makeContext(width: w, height: h))
            try Painter().paint(ctx, svg.draw())
            let actual = Surface.straightRGBA(ctx)
            if let png = AnyCanvas.encode(rgba: actual, width: w, height: h) { try png.write(to: Golden.outDir.appendingPathComponent("cg-\(name).png")) }
            let r = Self.compare(expected, actual, w, h)
            print("cross-painter \(name): \(w)x\(h) mean \(String(format: "%.2f", r.mean)) gross \(String(format: "%.3f", r.gross)) over \(r.counted) px")
            XCTAssertGreaterThan(r.counted, w * h / 4, "\(name): enough flat pixels to judge")
            XCTAssertLessThan(r.mean, 4, "\(name): mean channel error")
            XCTAssertLessThan(r.gross, 0.01, "\(name): pixels off by more than 64")
        }
    }
    #endif
}
