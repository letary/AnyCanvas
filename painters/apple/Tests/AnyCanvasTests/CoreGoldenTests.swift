// The core on Apple: every stream and SVG golden through the C API, its JSON against the expected file
// — byte-equal, or numerically equal (Apple's libm moves the last digits of float trig). The twin of
// the Android demo's check and of tests/core/golden.cpp.
import XCTest
import AnyCanvas

final class CoreGoldenTests: XCTestCase {
    func testStreams() throws {
        let core = AnyCanvas()
        let files = try FileManager.default.contentsOfDirectory(atPath: Golden.dir.appendingPathComponent("streams").path).filter { $0.hasSuffix(".json") }.sorted()
        XCTAssertGreaterThan(files.count, 0)
        for file in files {
            let name = "stream-" + String(file.dropLast(5))
            let s = try Golden.stream(file)
            let list = core.interpret(cmd: s.cmd, refs: s.refs, scale: s.scale)
            if let diff = Golden.compareJson(expected: try Golden.expectedJson(name), actual: AnyCanvas.json(list)) { XCTFail("\(name): \(diff)") }
        }
    }

    func testSvgs() throws {
        let core = AnyCanvas()
        let files = try FileManager.default.contentsOfDirectory(atPath: Golden.dir.appendingPathComponent("svg").path).filter { $0.hasSuffix(".svg") }.sorted()
        XCTAssertGreaterThan(files.count, 0)
        for file in files {
            let name = "svg-" + String(file.dropLast(4))
            let c = try Golden.svg(file)
            let list = core.parseSvg(c.markup)?.draw(width: c.fitW, height: c.fitH, tint: c.tint) ?? DrawList(words: [], strings: [])
            if let diff = Golden.compareJson(expected: try Golden.expectedJson(name), actual: AnyCanvas.json(list)) { XCTFail("\(name): \(diff)") }
        }
    }

    func testSvgSizeAndSniff() throws {
        let core = AnyCanvas()
        let svg = try XCTUnwrap(core.parseSvg(#"<svg xmlns="http://www.w3.org/2000/svg" width="30" height="20"><rect width="30" height="20" fill="red"/></svg>"#))
        XCTAssertEqual(svg.width, 30)
        XCTAssertEqual(svg.height, 20)
        XCTAssertTrue(AnyCanvas.looksLikeSvg(Data("  <svg xmlns='x'></svg>".utf8)))
        XCTAssertFalse(AnyCanvas.looksLikeSvg(Data([0x89, 0x50, 0x4e, 0x47])))
        XCTAssertNil(core.parseSvg("not svg at all"))
        XCTAssertNil(core.parseSvg(""))
    }

    func testEncodeProducesPngAndJpegHeaders() {
        let rgba = [UInt8](repeating: 255, count: 4 * 4 * 4)
        let png = AnyCanvas.encode(rgba: rgba, width: 4, height: 4)
        XCTAssertEqual(png?.prefix(4), Data([0x89, 0x50, 0x4e, 0x47]))
        let jpeg = AnyCanvas.encode(rgba: rgba, width: 4, height: 4, jpeg: true, quality: 80)
        XCTAssertEqual(jpeg?.prefix(2), Data([0xff, 0xd8]))
        XCTAssertNil(AnyCanvas.encode(rgba: rgba, width: 8, height: 8))
    }
}
