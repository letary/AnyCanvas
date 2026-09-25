// The Swift binding of the core (the C API of core/include/anycanvas/anycanvas.h, module CAnyCanvas).
// The twin of painters/android/.../AnyCanvas.kt. A draw list the core returns is owned by the context
// until the next call, so every method copies it into Swift arrays before returning.
import Foundation
import CAnyCanvas
@_exported import AnyCanvasPainter
@_exported import AnyCanvasSpec

public final class AnyCanvas {
    private var ctx: OpaquePointer?

    public init() { ctx = ac_context_create() }
    deinit { if let c = ctx { ac_context_destroy(c) } }

    /// The spec version the core was built with (must equal `specVersion` of Spec.swift).
    public static var coreSpecVersion: Int32 { ac_spec_version() }

    /// An opcode stream (f32 words + string table) → its draw list, at `scale` device px per unit.
    public func interpret(cmd: [Float], refs: [String], scale: Float = 1) -> DrawList {
        guard let c = ctx else { return DrawList(words: [], strings: []) }
        var list = ac_drawlist()
        cmd.withUnsafeBufferPointer { words in
            withCStrings(refs) { ptrs in
                _ = ac_interpret(c, words.baseAddress, Int32(words.count), ptrs.baseAddress, Int32(ptrs.count), scale, &list)
            }
        }
        return AnyCanvas.copy(list)
    }

    /// Parses SVG markup; nil when nanosvg rejects it or the document has no size.
    public func parseSvg(_ markup: String) -> Svg? {
        var bytes = Array(markup.utf8)
        guard !bytes.isEmpty else { return nil }
        let handle = bytes.withUnsafeMutableBufferPointer { buf -> OpaquePointer? in
            buf.baseAddress!.withMemoryRebound(to: CChar.self, capacity: buf.count) { ac_svg_parse($0, buf.count) }
        }
        guard let h = handle else { return nil }
        return Svg(owner: self, handle: h)
    }

    /// A parsed SVG document.
    public final class Svg {
        private let owner: AnyCanvas
        private var handle: OpaquePointer?
        public let width: Float
        public let height: Float

        fileprivate init(owner: AnyCanvas, handle: OpaquePointer) {
            self.owner = owner
            self.handle = handle
            var w: Float = 0, h: Float = 0
            ac_svg_size(handle, &w, &h)
            width = w
            height = h
        }
        deinit { if let h = handle { ac_svg_free(h) } }

        /// The draw list aspect-fitted into w × h device px (0 × 0: the natural size); `tint` replaces every color.
        public func draw(width w: Float = 0, height h: Float = 0, tint: Color? = nil) -> DrawList {
            guard let handle = handle, let c = owner.ctx else { return DrawList(words: [], strings: []) }
            var list = ac_drawlist()
            if let t = tint {
                let rgba: [Float] = [t.r, t.g, t.b, t.a]
                rgba.withUnsafeBufferPointer { ac_svg_draw(c, handle, w, h, 1, $0.baseAddress, &list) }
            } else {
                ac_svg_draw(c, handle, w, h, 0, nil, &list)
            }
            return AnyCanvas.copy(list)
        }
    }

    /// True if the bytes look like an SVG document.
    public static func looksLikeSvg(_ data: Data) -> Bool {
        data.withUnsafeBytes { raw in ac_looks_like_svg(raw.baseAddress, raw.count) != 0 }
    }

    /// Straight RGBA8 (top row first) → PNG or JPEG bytes; nil on failure.
    public static func encode(rgba: [UInt8], width: Int, height: Int, jpeg: Bool = false, quality: Int = 90) -> Data? {
        guard width > 0, height > 0, rgba.count >= width * height * 4 else { return nil }
        var len = 0
        let out = rgba.withUnsafeBufferPointer { ac_encode($0.baseAddress, Int32(width), Int32(height), jpeg ? 1 : 0, Int32(quality), &len) }
        guard let p = out else { return nil }
        defer { ac_free(p) }
        return Data(bytes: p, count: len)
    }

    /// The deterministic JSON of a draw list (debugging, hashing, the golden tests).
    public static func json(_ list: DrawList) -> String {
        var out = "[]"
        list.words.withUnsafeBufferPointer { words in
            withCStrings(list.strings) { ptrs in
                var l = ac_drawlist(words: words.baseAddress, wordCount: Int32(words.count), strings: ptrs.baseAddress, stringCount: Int32(ptrs.count))
                if let s = ac_drawlist_json(&l) {
                    out = String(cString: s)
                    ac_free(s)
                }
            }
        }
        return out
    }

    /// A CSS font shorthand resolved the way the interpreter resolves it for a text command.
    public static func parseFont(_ css: String) -> FontData {
        var f = ac_font()
        css.withCString { ac_font_parse($0, &f) }
        let familyBytes = f.family
        let family = withUnsafePointer(to: familyBytes) { p in
            p.withMemoryRebound(to: CChar.self, capacity: MemoryLayout.size(ofValue: familyBytes)) { String(cString: $0) }
        }
        return FontData(family: family, size: f.size, weight: f.weight, italic: f.italic != 0)
    }

    private static func copy(_ list: ac_drawlist) -> DrawList {
        let words = list.wordCount > 0 && list.words != nil ? Array(UnsafeBufferPointer(start: list.words, count: Int(list.wordCount))) : []
        var strings: [String] = []
        if list.stringCount > 0, let s = list.strings {
            strings.reserveCapacity(Int(list.stringCount))
            for i in 0..<Int(list.stringCount) { strings.append(s[i].map { String(cString: $0) } ?? "") }
        }
        return DrawList(words: words, strings: strings)
    }
}

/// Runs `body` with C string pointers valid for the call: one packed NUL-separated buffer (the JNI
/// binding's shape), pointers into it.
private func withCStrings<R>(_ strings: [String], _ body: (UnsafeBufferPointer<UnsafePointer<CChar>?>) -> R) -> R {
    var packed: [CChar] = []
    var offsets: [Int] = []
    for s in strings { offsets.append(packed.count); packed.append(contentsOf: s.utf8CString) }
    return packed.withUnsafeBufferPointer { base in
        let ptrs: [UnsafePointer<CChar>?] = offsets.map { off in base.baseAddress.map { $0 + off } }
        return ptrs.withUnsafeBufferPointer { body($0) }
    }
}
