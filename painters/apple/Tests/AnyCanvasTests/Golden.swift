// The golden corpus (tests/golden) as the Swift tests see it, and the comparison rules the C++ golden
// test and the Android demo use: byte-equal where the goldens were written, numerically equal
// (|a-b| <= 1e-3 + 1e-4·max(|a|,|b|)) elsewhere — Apple's libm moves the last digits of float trig.
//
// The corpus is read from the repo through #filePath: that works for `swift test` on the Mac and for
// the iPhone simulator (which sees the Mac's filesystem). The on-device run is the demo app, which
// bundles the folder.
import Foundation
import XCTest
import AnyCanvas

enum Golden {
    static let repoRoot: URL = {
        var u = URL(fileURLWithPath: #filePath)
        for _ in 0..<5 { u.deleteLastPathComponent() }   // Golden.swift → AnyCanvasTests → Tests → apple → painters → repo
        return u
    }()
    static let dir = repoRoot.appendingPathComponent("tests/golden")
    static let outDir = repoRoot.appendingPathComponent("tests/out/apple")

    static func names() throws -> [String] {
        try FileManager.default.contentsOfDirectory(atPath: dir.appendingPathComponent("expected").path)
            .filter { $0.hasSuffix(".json") }.map { String($0.dropLast(5)) }.sorted()
    }

    static func expectedJson(_ name: String) throws -> String {
        try String(contentsOf: dir.appendingPathComponent("expected/\(name).json"), encoding: .utf8)
    }

    static func expectedList(_ name: String) throws -> DrawList {
        try DrawList(binary: Data(contentsOf: dir.appendingPathComponent("expected/\(name).bin")))
    }

    /// The commands of a golden, from its binary.
    static func commands(_ name: String) throws -> [DrawCommand] { try expectedList(name).commands() }

    struct Stream { let cmd: [Float]; let refs: [String]; let scale: Float }

    static func stream(_ file: String) throws -> Stream {
        let data = try Data(contentsOf: dir.appendingPathComponent("streams/\(file)"))
        let obj = try XCTUnwrap(JSONSerialization.jsonObject(with: data) as? [String: Any])
        let cmd = try XCTUnwrap(obj["cmd"] as? [NSNumber]).map { $0.floatValue }
        let refs = try XCTUnwrap(obj["refs"] as? [String])
        let scale = (obj["scale"] as? NSNumber)?.floatValue ?? 1
        return Stream(cmd: cmd, refs: refs, scale: scale)
    }

    struct SvgCase { let markup: String; let fitW: Float; let fitH: Float; let tint: Color? }

    static func svg(_ file: String) throws -> SvgCase {
        let markup = String(decoding: try Data(contentsOf: dir.appendingPathComponent("svg/\(file)")), as: UTF8.self)
        var fitW: Float = 0, fitH: Float = 0
        var tint: Color? = nil
        let sidecar = dir.appendingPathComponent("svg/\(file).json")
        if let data = try? Data(contentsOf: sidecar), let obj = try? JSONSerialization.jsonObject(with: data) as? [String: Any] {
            if let fit = obj["fit"] as? [NSNumber], fit.count == 2 { fitW = fit[0].floatValue; fitH = fit[1].floatValue }
            if let t = obj["tint"] as? String { tint = cssHex(t) }
        }
        return SvgCase(markup: markup, fitW: fitW, fitH: fitH, tint: tint)
    }

    /// #rrggbb / #rrggbbaa (the sidecar's CSS hex) → a Color.
    static func cssHex(_ s: String) -> Color? {
        var hex = Substring(s)
        if hex.hasPrefix("#") { hex = hex.dropFirst() }
        guard hex.count == 6 || hex.count == 8, let v = UInt32(hex, radix: 16) else { return nil }
        let n = hex.count == 8 ? v : (v << 8) | 0xff
        return Color(r: Float((n >> 24) & 0xff) / 255, g: Float((n >> 16) & 0xff) / 255, b: Float((n >> 8) & 0xff) / 255, a: Float(n & 0xff) / 255)
    }

    // ---- comparison ----------------------------------------------------------------------------

    static func numericallyEqual(_ a: Any, _ b: Any) -> Bool {
        switch (a, b) {
        case let (x as [Any], y as [Any]):
            return x.count == y.count && zip(x, y).allSatisfy { numericallyEqual($0, $1) }
        case let (x as [String: Any], y as [String: Any]):
            return x.count == y.count && x.allSatisfy { k, v in y[k].map { numericallyEqual(v, $0) } ?? false }
        case let (x as NSNumber, y as NSNumber):
            if x === kCFBooleanTrue || x === kCFBooleanFalse || y === kCFBooleanTrue || y === kCFBooleanFalse { return x == y }
            let p = x.doubleValue, q = y.doubleValue
            return abs(p - q) <= 1e-3 + 1e-4 * max(abs(p), abs(q))
        case let (x as String, y as String): return x == y
        case (is NSNull, is NSNull): return true
        default: return false
        }
    }

    /// Byte-equal, or numerically equal as JSON; otherwise a message with the first difference.
    static func compareJson(expected: String, actual: String) -> String? {
        if expected == actual { return nil }
        if let e = try? JSONSerialization.jsonObject(with: Data(expected.utf8)), let a = try? JSONSerialization.jsonObject(with: Data(actual.utf8)), numericallyEqual(e, a) { return nil }
        let ec = Array(expected.utf8), ac = Array(actual.utf8)
        var i = 0
        while i < ec.count && i < ac.count && ec[i] == ac[i] { i += 1 }
        let from = max(0, i - 40)
        let eSnip = String(decoding: ec[from..<min(ec.count, i + 40)], as: UTF8.self)
        let aSnip = String(decoding: ac[from..<min(ac.count, i + 40)], as: UTF8.self)
        return "differs at \(i): expected …\(eSnip)… actual …\(aSnip)…"
    }

    /// The JSON tree (as JSONSerialization builds it) of decoded commands, in the core's own shape,
    /// with every number as a Float — what the reader test compares with the core's JSON dump.
    static func tree(_ commands: [DrawCommand]) throws -> [Any] {
        func f(_ v: Float) -> Any { v == 0 ? 0 : v }
        func nums(_ v: [Float]) -> [Any] { v.map(f) }
        func paint(_ p: PaintData) -> [String: Any] {
            switch p {
            case .solid(let alpha, let c): return ["kind": "color", "alpha": f(alpha), "color": nums(c.values)]
            case .linear(let alpha, let m, let spread, let stops):
                return ["kind": "linear", "alpha": f(alpha), "matrix": nums(m.values), "spread": spreadName(spread), "stops": stops.map { ["offset": f($0.offset), "color": nums($0.color.values)] }]
            case .radial(let alpha, let m, let fx, let fy, let r0, let spread, let stops):
                return ["kind": "radial", "alpha": f(alpha), "matrix": nums(m.values), "fx": f(fx), "fy": f(fy), "r0": f(r0), "spread": spreadName(spread), "stops": stops.map { ["offset": f($0.offset), "color": nums($0.color.values)] }]
            }
        }
        func stroke(_ s: StrokeData) -> [String: Any] {
            ["width": f(s.width), "join": joinName(s.join), "cap": capName(s.cap), "miterLimit": f(s.miterLimit), "dashOffset": f(s.dashOffset), "dash": nums(s.dash)]
        }
        func path(_ p: PathData) throws -> [Any] {
            try p.segments().map { seg -> [Any] in
                switch seg {
                case .move(let x, let y): return ["M", f(x), f(y)]
                case .line(let x, let y): return ["L", f(x), f(y)]
                case .quad(let cx, let cy, let x, let y): return ["Q", f(cx), f(cy), f(x), f(y)]
                case .cubic(let a, let b, let c, let d, let x, let y): return ["C", f(a), f(b), f(c), f(d), f(x), f(y)]
                case .close: return ["Z"]
                }
            }
        }
        func text(_ t: TextData) -> [String: Any] {
            ["text": t.text, "x": f(t.x), "y": f(t.y), "maxWidth": f(t.maxWidth),
             "font": ["family": t.font.family, "size": f(t.font.size), "weight": t.font.weight, "italic": t.font.italic],
             "align": alignName(t.align), "baseline": baselineName(t.baseline), "letterSpacing": f(t.letterSpacing)]
        }
        return try commands.map { c -> [String: Any] in
            switch c {
            case .setTransform(let m): return ["cmd": "setTransform", "matrix": nums(m.values)]
            case .save: return ["cmd": "save"]
            case .restore: return ["cmd": "restore"]
            case .clip(let rule, let p): return ["cmd": "clip", "rule": ruleName(rule), "path": try path(p)]
            case .fillPath(let rule, let pt, let p): return ["cmd": "fillPath", "rule": ruleName(rule), "paint": paint(pt), "path": try path(p)]
            case .strokePath(let s, let pt, let p): return ["cmd": "strokePath", "stroke": stroke(s), "paint": paint(pt), "path": try path(p)]
            case .fillText(let t, let pt): var o = text(t); o["cmd"] = "fillText"; o["paint"] = paint(pt); return o
            case .strokeText(let t, let s, let pt): var o = text(t); o["cmd"] = "strokeText"; o["stroke"] = stroke(s); o["paint"] = paint(pt); return o
            case .drawImage(let surface, let src, let dst, let alpha): return ["cmd": "drawImage", "surface": surface, "src": nums(src.values), "dst": nums(dst.values), "alpha": f(alpha)]
            case .clearRect(let r): return ["cmd": "clearRect", "rect": nums(r.values)]
            }
        }
    }

    /// The core's JSON with every number rounded through Float (its numbers are the shortest decimals
    /// that round-trip a float32; the binary carries the float32 — the two meet at Float).
    static func froundTree(_ v: Any) -> Any {
        switch v {
        case let x as [Any]: return x.map(froundTree)
        case let x as [String: Any]: return x.mapValues(froundTree)
        case let x as NSNumber:
            if x === kCFBooleanTrue || x === kCFBooleanFalse { return x }
            let d = Float(x.doubleValue)
            return d == 0 ? 0 : d
        default: return v
        }
    }

    static func exactlyEqual(_ a: Any, _ b: Any) -> Bool {
        switch (a, b) {
        case let (x as [Any], y as [Any]): return x.count == y.count && zip(x, y).allSatisfy { exactlyEqual($0, $1) }
        case let (x as [String: Any], y as [String: Any]): return x.count == y.count && x.allSatisfy { k, v in y[k].map { exactlyEqual(v, $0) } ?? false }
        case let (x as NSNumber, y as NSNumber): return x.floatValue == y.floatValue && (x === kCFBooleanTrue) == (y === kCFBooleanTrue) && (x === kCFBooleanFalse) == (y === kCFBooleanFalse)
        case let (x as String, y as String): return x == y
        default: return false
        }
    }

    static func spreadName(_ s: Spread) -> String { switch s { case .pad: return "pad"; case .reflect: return "reflect"; case .repeat: return "repeat" } }
    static func joinName(_ j: LineJoin) -> String { switch j { case .miter: return "miter"; case .round: return "round"; case .bevel: return "bevel" } }
    static func capName(_ c: LineCap) -> String { switch c { case .butt: return "butt"; case .round: return "round"; case .square: return "square" } }
    static func ruleName(_ r: FillRule) -> String { r == .evenodd ? "evenodd" : "nonzero" }
    static func alignName(_ a: TextAlign) -> String { switch a { case .left: return "left"; case .center: return "center"; case .right: return "right"; case .start: return "start"; case .end: return "end" } }
    static func baselineName(_ b: TextBaseline) -> String { switch b { case .alphabetic: return "alphabetic"; case .top: return "top"; case .middle: return "middle"; case .bottom: return "bottom"; case .hanging: return "hanging"; case .ideographic: return "ideographic" } }
}
