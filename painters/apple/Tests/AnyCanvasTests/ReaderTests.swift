// The Swift reader against the core: for every golden, the binary the core wrote decodes to the very
// commands its JSON dump describes (the twin of tests/ts/drawlist.test.ts). This is what keeps the
// Apple painter honest about the wire format.
import XCTest
import AnyCanvas

final class ReaderTests: XCTestCase {
    func testThereAreGoldens() throws {
        XCTAssertGreaterThan(try Golden.names().count, 0, "build the core and run the golden test first")
    }

    func testBinaryEqualsJsonOnEveryGolden() throws {
        for name in try Golden.names() {
            let bin = try Golden.tree(try Golden.commands(name))
            let json = Golden.froundTree(try JSONSerialization.jsonObject(with: Data(try Golden.expectedJson(name).utf8)))
            XCTAssertTrue(Golden.exactlyEqual(bin, json), "\(name): the binary decodes differently from the JSON")
        }
    }

    func testSpecVersionsAgree() {
        XCTAssertEqual(AnyCanvas.coreSpecVersion, specVersion)
    }

    func testUnknownCommandIsSkippedAndTruncationThrows() throws {
        let words: [Float] = [99, 3, 1, 2, 3, Float(DrawCmd.save.rawValue), 0]
        XCTAssertEqual(try DrawList(words: words, strings: []).commands(), [.save])
        XCTAssertThrowsError(try DrawList(words: [Float(DrawCmd.setTransform.rawValue), 6, 1, 0], strings: []).commands())
        XCTAssertThrowsError(try DrawList(words: [Float(DrawCmd.fillText.rawValue), 1, 5], strings: []).commands())
        // A command that claims fewer words than its payload overruns its length.
        XCTAssertThrowsError(try DrawList(words: [Float(DrawCmd.setTransform.rawValue), 2, 1, 0, 0, 1, 0, 0], strings: []).commands())
    }

    func testBadPathVerbThrowsAtDecode() throws {
        XCTAssertThrowsError(try PathData(words: [7, 1, 2]).segments())
        XCTAssertEqual(try PathData(words: [0, 1, 2, 4]).segments(), [.move(1, 2), .close])
    }

    func testFontParseMatchesTheInterpreter() {
        let f = AnyCanvas.parseFont("italic bold 24px Inter, sans-serif")
        XCTAssertEqual(f, FontData(family: "Inter", size: 24, weight: 700, italic: true))
    }
}
