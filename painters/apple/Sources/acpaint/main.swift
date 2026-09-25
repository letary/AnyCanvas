// acpaint — a draw list painted by CoreGraphics, from the command line (the Apple twin of acdump's
// `png`, for the owner's eye and the README images). macOS only (an executable target).
//
//   acpaint <expected.bin | expected.json | file.svg> <out.png> [--size w h] [--white] [--fit w h] [--tint #rrggbbaa]
//
// A .bin / .json is a draw list as the golden test writes it; an .svg goes through the core first.
// The canvas is w × h device px (default: the list's extent is unknown, so 640 × 560); --white
// paints on a white card instead of transparency.
import Foundation
import AnyCanvas

func fail(_ message: String) -> Never {
    FileHandle.standardError.write(Data((message + "\n").utf8))
    exit(1)
}

var args = Array(CommandLine.arguments.dropFirst())
guard args.count >= 2 else { fail("usage: acpaint <expected.bin|expected.json|file.svg> <out.png> [--size w h] [--white] [--fit w h] [--tint #rrggbbaa]") }
let input = URL(fileURLWithPath: args.removeFirst())
let output = URL(fileURLWithPath: args.removeFirst())
var width = 640, height = 560, white = false
var fitW: Float = 0, fitH: Float = 0
var tint: Color? = nil
while !args.isEmpty {
    let a = args.removeFirst()
    switch a {
    case "--size": guard args.count >= 2, let w = Int(args.removeFirst()), let h = Int(args.removeFirst()) else { fail("--size w h") }; width = w; height = h
    case "--fit": guard args.count >= 2, let w = Float(args.removeFirst()), let h = Float(args.removeFirst()) else { fail("--fit w h") }; fitW = w; fitH = h
    case "--white": white = true
    case "--tint":
        guard !args.isEmpty else { fail("--tint #rrggbbaa") }
        var hex = Substring(args.removeFirst()); if hex.hasPrefix("#") { hex = hex.dropFirst() }
        guard hex.count == 6 || hex.count == 8, let v = UInt32(hex, radix: 16) else { fail("bad tint") }
        let n = hex.count == 8 ? v : (v << 8) | 0xff
        tint = Color(r: Float((n >> 24) & 0xff) / 255, g: Float((n >> 16) & 0xff) / 255, b: Float((n >> 8) & 0xff) / 255, a: Float(n & 0xff) / 255)
    default: fail("unknown option \(a)")
    }
}

let list: DrawList
switch input.pathExtension {
case "bin": list = try DrawList(binary: Data(contentsOf: input))
case "json":
    // The core's JSON dump has no binary twin here: re-encode is not what this tool does — read the .bin beside it.
    let bin = input.deletingPathExtension().appendingPathExtension("bin")
    guard FileManager.default.fileExists(atPath: bin.path) else { fail("no \(bin.lastPathComponent) beside the JSON (acdump bin writes one)") }
    list = try DrawList(binary: Data(contentsOf: bin))
case "svg":
    let markup = String(decoding: try Data(contentsOf: input), as: UTF8.self)
    guard let svg = AnyCanvas().parseSvg(markup) else { fail("not an SVG: \(input.path)") }
    list = svg.draw(width: fitW, height: fitH, tint: tint)
default: fail("expected .bin, .json or .svg")
}

guard let ctx = Surface.makeContext(width: width, height: height) else { fail("no context") }
if white {
    ctx.setFillColor(Painter.cgColor(Color(r: 1, g: 1, b: 1, a: 1)))
    ctx.fill(CGRect(x: 0, y: 0, width: width, height: height))
}
try Painter().paint(ctx, list)
guard let png = AnyCanvas.encode(rgba: Surface.straightRGBA(ctx), width: width, height: height) else { fail("encode failed") }
try png.write(to: output)
print("\(output.path): \(width)x\(height), \(png.count) bytes")
