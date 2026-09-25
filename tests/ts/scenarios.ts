// The golden scenarios: each records ONE drawing with the TS recorder. `bun tests/ts/record.ts`
// writes them to tests/golden/streams/<name>.json (committed), the C++ golden test turns those into
// tests/golden/expected/stream-<name>.json, and the TS tests hold the committed streams against a
// fresh recording (recorder drift) and the expected lists against the TS reader.
import type { Recorder } from "anycanvas-recorder"

export interface Scenario { scale?: number, record(c: Recorder): void }

export const scenarios: Record<string, Scenario> = {
  // Everyday drawing: rectangles, a pie, a rounded card with a gradient, dashes, a clip, text; at 2x.
  shapes: {
    scale: 2,
    record(c) {
      c.fillStyle = "#1e90ff"
      c.fillRect(10, 10, 100, 60)
      c.strokeStyle = "rgba(0, 0, 0, 0.5)"
      c.lineWidth = 3
      c.setLineDash([6, 3])
      c.strokeRect(10, 10, 100, 60)
      c.setLineDash([])

      c.beginPath()
      c.moveTo(200, 60)
      c.arc(200, 60, 40, 0, Math.PI * 1.5)
      c.closePath()
      c.fillStyle = "orange"
      c.fill()

      const g = c.createLinearGradient(0, 120, 0, 200)
      g.addColorStop(0, "#fff").addColorStop(1, "#888")
      c.fillStyle = g
      c.beginPath()
      c.roundRect(10, 120, 200, 80, 16)
      c.fill()
      c.strokeStyle = "#333"
      c.lineWidth = 1
      c.stroke()

      const r = c.createRadialGradient(300, 160, 0, 300, 160, 40)
      r.addColorStop(0, "yellow").addColorStop(1, "transparent")
      c.fillStyle = r
      c.beginPath()
      c.ellipse(300, 160, 40, 40, 0, 0, Math.PI * 2)
      c.fill()

      c.save()
      c.beginPath()
      c.rect(240, 10, 80, 80)
      c.clip()
      c.fillStyle = "hsl(120, 60%, 40%)"
      c.beginPath()
      c.arc(280, 50, 60, 0, Math.PI * 2)
      c.fill()
      c.restore()

      c.font = "bold 24px Inter"
      c.textAlign = "center"
      c.textBaseline = "middle"
      c.fillStyle = "#000"
      c.fillText("AnyCanvas", 160, 230)
      c.font = "italic 12px serif"
      c.textAlign = "left"
      c.textBaseline = "alphabetic"
      c.letterSpacing = 1
      c.strokeStyle = "#a00"
      c.lineWidth = 0.5
      c.strokeText("outlined", 20, 260, 60)
    },
  },

  // Every opcode at least once, in spec order — the interpreter's coverage scenario.
  "all-ops": {
    record(c) {
      c.save(); c.restore()
      c.translate(5, 5); c.scale(1.5, 1.5); c.rotate(0.1)
      c.transform(1, 0, 0.2, 1, 0, 0); c.setTransform(2, 0, 0, 2, 3, 4); c.resetTransform()
      c.globalAlpha = 0.8
      c.fillStyle = "#123456"; c.strokeStyle = "rgb(10, 20, 30)"
      const lg = c.createLinearGradient(0, 0, 50, 50); lg.addColorStop(0, "#f00").addColorStop(0.5, "#0f0").addColorStop(1, "#00f")
      c.fillStyle = lg
      const rg = c.createRadialGradient(20, 20, 5, 25, 25, 30); rg.addColorStop(0, "white").addColorStop(1, "black")
      c.strokeStyle = rg
      c.lineWidth = 2.5; c.lineJoin = "round"; c.lineCap = "square"; c.miterLimit = 4
      c.setLineDash([4, 2, 1]); c.lineDashOffset = 1.5
      c.font = "600 14px 'Roboto Mono', monospace"; c.textAlign = "right"; c.textBaseline = "top"; c.letterSpacing = 0.5
      c.beginPath()
      c.moveTo(0, 0); c.lineTo(10, 0); c.quadraticCurveTo(15, 5, 10, 10); c.bezierCurveTo(8, 12, 4, 12, 0, 10); c.closePath()
      c.arc(30, 30, 5, 0, 1, true); c.arcTo(40, 30, 40, 40, 4); c.ellipse(60, 30, 8, 4, 0.3, 0, 2, false)
      c.rect(70, 0, 10, 10); c.roundRect(85, 0, 20, 10, 3)
      c.fill("evenodd"); c.stroke(); c.clip("nonzero")
      c.fillRect(1, 2, 3, 4); c.strokeRect(1, 2, 3, 4); c.clearRect(1, 2, 3, 4)
      c.fillText("fill", 10, 20); c.strokeText("stroke", 10, 40, 30)
      c.drawImage({ surface: 7, width: 64, height: 32 }, 5, 5)
      c.drawImage({ surface: 7, width: 64, height: 32 }, 5, 5, 20, 10)
      c.drawImage({ surface: 7, width: 64, height: 32 }, 0, 0, 32, 32, 50, 50, 16, 16)
    },
  },

  // Transform semantics: nested saves, a path built across a transform change, a singular scale.
  transforms: {
    record(c) {
      c.save()
      c.translate(50, 50)
      c.rotate(Math.PI / 4)
      c.fillStyle = "red"
      c.fillRect(-10, -10, 20, 20)
      c.save()
      c.scale(2, 0.5)
      c.fillStyle = "blue"
      c.fillRect(0, 0, 10, 10)
      c.restore()
      c.fillRect(20, 20, 5, 5)
      c.restore()
      c.fillRect(0, 0, 5, 5)

      // A rect recorded at identity, then scaled before the stroke: the stroke is 2x wide, the geometry not.
      c.beginPath()
      c.rect(100, 100, 40, 20)
      c.scale(2, 2)
      c.strokeStyle = "green"
      c.lineWidth = 1
      c.stroke()
      c.resetTransform()

      // Points added under two different transforms end up in one path (device-space semantics).
      c.beginPath()
      c.moveTo(0, 0)
      c.translate(100, 0)
      c.lineTo(0, 50)
      c.resetTransform()
      c.lineTo(0, 50)
      c.closePath()
      c.fillStyle = "#888"
      c.fill()

      // Nothing is drawn under a singular transform.
      c.save()
      c.scale(0, 1)
      c.fillRect(0, 0, 100, 100)
      c.restore()
    },
  },

  // What must NOT break: invalid colors, empty text, extra restores, alpha out of range, an empty path.
  edge: {
    record(c) {
      c.fillStyle = "#0f0"
      c.fillStyle = "not-a-color"
      c.fillRect(0, 0, 10, 10)
      c.fillText("", 5, 5)
      c.restore()
      c.restore()
      c.globalAlpha = 2
      c.globalAlpha = -1
      c.fillRect(20, 0, 10, 10)
      c.beginPath()
      c.fill()
      c.stroke()
      c.lineWidth = 0
      c.lineWidth = -3
      c.strokeRect(40, 0, 10, 10)
      c.setLineDash([0, 0])
      c.strokeRect(60, 0, 10, 10)
      const g = c.createLinearGradient(0, 0, 10, 0)
      c.fillStyle = g
      c.fillRect(80, 0, 10, 10)
      g.addColorStop(1, "black")
      c.fillStyle = g
      c.fillRect(100, 0, 10, 10)
      c.save()
      c.save()
      c.fillRect(120, 0, 10, 10)
    },
  },
}
