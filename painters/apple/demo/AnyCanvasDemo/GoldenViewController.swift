import AnyCanvas
import UIKit
import os

private let log = Logger(subsystem: "io.letary.anycanvas.demo", category: "golden")

/// One golden: its name, the draw list the core produced on this device, and whether it matched the expected JSON.
private struct Golden {
    let name: String
    let list: DrawList
    let ok: Bool
    let detail: String
}

final class GoldenViewController: UIViewController {
    override func viewDidLoad() {
        super.viewDidLoad()
        view.backgroundColor = .white
        let goldens = runGoldens()
        let failed = goldens.filter { !$0.ok }
        let summary = "AnyCanvas on-device golden check: \(goldens.count - failed.count) ok, \(failed.count) failed"
            + (failed.isEmpty ? "" : " [" + failed.map(\.name).joined(separator: ", ") + "]")
        log.info("\(summary, privacy: .public)")
        print(summary)
        for g in failed { log.warning("\(g.name, privacy: .public): \(g.detail, privacy: .public)"); print("\(g.name): \(g.detail)") }

        let scroll = UIScrollView()
        scroll.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(scroll)
        let column = UIStackView()
        column.axis = .vertical
        column.spacing = 4
        column.translatesAutoresizingMaskIntoConstraints = false
        scroll.addSubview(column)
        NSLayoutConstraint.activate([
            scroll.topAnchor.constraint(equalTo: view.safeAreaLayoutGuide.topAnchor),
            scroll.bottomAnchor.constraint(equalTo: view.bottomAnchor),
            scroll.leadingAnchor.constraint(equalTo: view.leadingAnchor),
            scroll.trailingAnchor.constraint(equalTo: view.trailingAnchor),
            column.topAnchor.constraint(equalTo: scroll.contentLayoutGuide.topAnchor, constant: 12),
            column.bottomAnchor.constraint(equalTo: scroll.contentLayoutGuide.bottomAnchor, constant: -24),
            column.leadingAnchor.constraint(equalTo: scroll.contentLayoutGuide.leadingAnchor, constant: 12),
            column.trailingAnchor.constraint(equalTo: scroll.contentLayoutGuide.trailingAnchor, constant: -12),
            column.widthAnchor.constraint(equalTo: scroll.frameLayoutGuide.widthAnchor, constant: -24),
        ])

        let title = UILabel()
        title.text = summary
        title.numberOfLines = 0
        title.font = .systemFont(ofSize: 16, weight: .semibold)
        title.textColor = failed.isEmpty ? UIColor(red: 0, green: 0.47, blue: 0, alpha: 1) : .red
        title.accessibilityIdentifier = "summary"
        column.addArrangedSubview(title)

        let painter = Painter()
        for g in goldens {
            let label = UILabel()
            label.text = (g.ok ? "✓ " : "✗ ") + g.name
            label.font = .systemFont(ofSize: 14)
            label.textColor = g.ok ? .darkGray : .red
            column.addArrangedSubview(label)
            let gv = GoldenView(painter: painter, list: g.list)
            gv.heightAnchor.constraint(equalToConstant: 300).isActive = true
            column.addArrangedSubview(gv)
        }
    }

    private func runGoldens() -> [Golden] {
        var out: [Golden] = []
        guard let root = Bundle.main.url(forResource: "golden", withExtension: nil) else {
            return [Golden(name: "bundle", list: DrawList(words: [], strings: []), ok: false, detail: "no golden folder in the bundle")]
        }
        let fm = FileManager.default
        let core = AnyCanvas()
        for file in ((try? fm.contentsOfDirectory(atPath: root.appendingPathComponent("streams").path)) ?? []).filter({ $0.hasSuffix(".json") }).sorted() {
            let name = "stream-" + String(file.dropLast(5))
            guard let data = try? Data(contentsOf: root.appendingPathComponent("streams/\(file)")),
                  let obj = try? JSONSerialization.jsonObject(with: data) as? [String: Any],
                  let cmd = (obj["cmd"] as? [NSNumber])?.map({ $0.floatValue }), let refs = obj["refs"] as? [String] else {
                out.append(Golden(name: name, list: DrawList(words: [], strings: []), ok: false, detail: "unreadable stream")); continue
            }
            let scale = (obj["scale"] as? NSNumber)?.floatValue ?? 1
            out.append(check(root, name, core.interpret(cmd: cmd, refs: refs, scale: scale)))
        }
        for file in ((try? fm.contentsOfDirectory(atPath: root.appendingPathComponent("svg").path)) ?? []).filter({ $0.hasSuffix(".svg") }).sorted() {
            let name = "svg-" + String(file.dropLast(4))
            guard let data = try? Data(contentsOf: root.appendingPathComponent("svg/\(file)")) else { continue }
            var fitW: Float = 0, fitH: Float = 0
            var tint: Color? = nil
            if let side = try? Data(contentsOf: root.appendingPathComponent("svg/\(file).json")), let obj = try? JSONSerialization.jsonObject(with: side) as? [String: Any] {
                if let fit = obj["fit"] as? [NSNumber], fit.count == 2 { fitW = fit[0].floatValue; fitH = fit[1].floatValue }
                if let t = obj["tint"] as? String { tint = cssHex(t) }
            }
            let list = core.parseSvg(String(decoding: data, as: UTF8.self))?.draw(width: fitW, height: fitH, tint: tint) ?? DrawList(words: [], strings: [])
            out.append(check(root, name, list))
        }
        return out
    }

    private func check(_ root: URL, _ name: String, _ list: DrawList) -> Golden {
        let actual = AnyCanvas.json(list)
        guard let expected = try? String(contentsOf: root.appendingPathComponent("expected/\(name).json"), encoding: .utf8) else {
            return Golden(name: name, list: list, ok: false, detail: "no expected file")
        }
        if expected == actual { return Golden(name: name, list: list, ok: true, detail: "") }
        // The goldens were written on the desktop: another libm moves the last digits of float trig,
        // so the device check is structural with a numeric tolerance.
        if let e = try? JSONSerialization.jsonObject(with: Data(expected.utf8)), let a = try? JSONSerialization.jsonObject(with: Data(actual.utf8)), numericallyEqual(e, a) {
            return Golden(name: name, list: list, ok: true, detail: "numerically equal")
        }
        let ec = Array(expected.utf8), ac = Array(actual.utf8)
        var i = 0
        while i < ec.count && i < ac.count && ec[i] == ac[i] { i += 1 }
        let from = max(0, i - 40)
        return Golden(name: name, list: list, ok: false, detail: "differs at \(i): expected …\(String(decoding: ec[from..<min(ec.count, i + 40)], as: UTF8.self))… actual …\(String(decoding: ac[from..<min(ac.count, i + 40)], as: UTF8.self))…")
    }

    private func numericallyEqual(_ a: Any, _ b: Any) -> Bool {
        switch (a, b) {
        case let (x as [Any], y as [Any]): return x.count == y.count && zip(x, y).allSatisfy { numericallyEqual($0, $1) }
        case let (x as [String: Any], y as [String: Any]): return x.count == y.count && x.allSatisfy { k, v in y[k].map { numericallyEqual(v, $0) } ?? false }
        case let (x as NSNumber, y as NSNumber):
            if x === kCFBooleanTrue || x === kCFBooleanFalse || y === kCFBooleanTrue || y === kCFBooleanFalse { return x == y }
            let p = x.doubleValue, q = y.doubleValue
            return abs(p - q) <= 1e-3 + 1e-4 * max(abs(p), abs(q))
        case let (x as String, y as String): return x == y
        default: return false
        }
    }

    /// #rrggbb / #rrggbbaa (CSS order) → a Color.
    private func cssHex(_ s: String) -> Color? {
        var hex = Substring(s)
        if hex.hasPrefix("#") { hex = hex.dropFirst() }
        guard hex.count == 6 || hex.count == 8, let v = UInt32(hex, radix: 16) else { return nil }
        let n = hex.count == 8 ? v : (v << 8) | 0xff
        return Color(r: Float((n >> 24) & 0xff) / 255, g: Float((n >> 16) & 0xff) / 255, b: Float((n >> 8) & 0xff) / 255, a: Float(n & 0xff) / 255)
    }
}

/// Paints one draw list, scaled so 720 device-px units of the golden fill the view width.
private final class GoldenView: UIView {
    private let painter: Painter
    private let commands: [DrawCommand]

    init(painter: Painter, list: DrawList) {
        self.painter = painter
        self.commands = (try? list.commands()) ?? []
        super.init(frame: .zero)
        backgroundColor = .white
        contentMode = .redraw
    }
    required init?(coder: NSCoder) { fatalError() }

    override func draw(_ rect: CGRect) {
        guard let ctx = UIGraphicsGetCurrentContext() else { return }
        // A UIKit drawing context is already y-down: it is the painter's base.
        let s = bounds.width / 720
        ctx.saveGState()
        ctx.scaleBy(x: s, y: s)
        painter.paint(ctx, commands)
        ctx.restoreGState()
    }
}
