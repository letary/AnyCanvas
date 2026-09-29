// The DIRECT web painter: replays an OPCODE STREAM (spec/ops.h) on a Canvas2D context, with no
// core between them. The stream's semantics are Canvas2D's, so an op is a call of the context:
// the browser keeps the state stack, builds the paths, resolves the colors and the fonts, shapes
// the text. For a page that paints canvases and carries no core (no wasm) — the draw-list painter
// (./painter.ts) is the one for a host that has the core.
//
// What it does not give: the draw list (nothing is resolved into one — `debugDrawList` has
// nothing to say), and the core's own reading of a color or a font where a browser reads it
// otherwise. SVG is not its matter: a browser draws an SVG as an image.
//
// Like the core's interpreter it stops at the first unknown or truncated op: a newer stream is a
// shorter drawing, never a throw.
import { OP, OP_ARGS, LineJoinName, LineCapName, TextAlignName, TextBaselineName, FillRuleName, Gradient } from "anycanvas-spec"
import type { Canvas2DLike } from "./painter"
import type { Matrix6 } from "./drawlist"

/** What the direct painter calls of a CanvasRenderingContext2D, beside the draw-list painter's part. */
export interface Canvas2DDirect extends Canvas2DLike {
  translate(x: number, y: number): void
  scale(x: number, y: number): void
  rotate(radians: number): void
  arc(x: number, y: number, r: number, a0: number, a1: number, ccw?: boolean): void
  arcTo(x1: number, y1: number, x2: number, y2: number, r: number): void
  ellipse(x: number, y: number, rx: number, ry: number, rotation: number, a0: number, a1: number, ccw?: boolean): void
  rect(x: number, y: number, w: number, h: number): void
  roundRect?(x: number, y: number, w: number, h: number, r: number): void
  fillRect(x: number, y: number, w: number, h: number): void
  strokeRect(x: number, y: number, w: number, h: number): void
}

export interface ReplayHooks {
  /** The drawable for a surface id (a canvas, an ImageBitmap, an Image); null skips the blit. */
  image?(surface: number): any
  /** A font shorthand as this context can render it (a registered font's real name). */
  font?(shorthand: string): string
  /** The transform under the stream's own: device px per unit (`setTransform` of the stream is
   *  over it, `resetTransform` comes back to it). Identity when absent. */
  base?: Matrix6
}

/** The words an op takes after its id, or -1 when the stream ends inside it. */
const lengthOf = (cmd: ArrayLike<number>, at: number, args: string, repeat: string): number => {
  let n = args.length
  if (at + n > cmd.length) return -1
  if (repeat !== "") {
    // the count is the last fixed word
    const count = cmd[at + n - 1]! | 0
    if (count < 0) return -1
    n += count * repeat.length
  }
  return at + n > cmd.length ? -1 : n
}

// `clear` is the core's name of transparent black; the rest of a color is CSS.
const color = (text: string | undefined): string => (text === undefined ? "" : text.trim() === "clear" ? "transparent" : text)

/** Replay `cmd` + `refs` on `ctx`. Returns the number of ops drawn. The context is left as it
 *  was found: what the stream saved and did not restore is restored. */
export const replay = (ctx: Canvas2DDirect, cmd: ArrayLike<number>, refs: readonly string[], hooks: ReplayHooks = {}): number => {
  const base: Matrix6 = hooks.base ?? [1, 0, 0, 1, 0, 0]
  const home = () => ctx.setTransform(base[0], base[1], base[2], base[3], base[4], base[5])
  let depth = 0
  let ops = 0
  ctx.save()
  home()

  const gradient = (at: number): any => {
    const kind = cmd[at]! | 0
    const x0 = cmd[at + 1]!, y0 = cmd[at + 2]!, x1 = cmd[at + 3]!, y1 = cmd[at + 4]!, r0 = cmd[at + 5]!, r1 = cmd[at + 6]!
    const g = kind === Gradient.RADIAL ? ctx.createRadialGradient(x0, y0, r0, x1, y1, r1) : ctx.createLinearGradient(x0, y0, x1, y1)
    const stops = cmd[at + 7]! | 0
    for (let s = 0; s < stops; s++) {
      const offset = cmd[at + 8 + s * 2]!
      // (a stop a browser refuses — an offset off 0..1, a color that is none — is left out)
      try { g.addColorStop(offset, color(refs[cmd[at + 9 + s * 2]! | 0])) } catch { /* left out */ }
    }
    return g
  }

  const roundRect = (x: number, y: number, w: number, h: number, radius: number) => {
    const r = Math.max(0, Math.min(radius, Math.abs(w) / 2, Math.abs(h) / 2))
    if (ctx.roundRect) { ctx.roundRect(x, y, w, h, r); return }
    ctx.moveTo(x + r, y)
    ctx.arcTo(x + w, y, x + w, y + h, r)
    ctx.arcTo(x + w, y + h, x, y + h, r)
    ctx.arcTo(x, y + h, x, y, r)
    ctx.arcTo(x, y, x + w, y, r)
    ctx.closePath()
  }

  let at = 0
  while (at < cmd.length) {
    const op = cmd[at]! | 0
    const layout = OP_ARGS[op]
    if (!layout) break
    const a = at + 1
    const n = lengthOf(cmd, a, layout.args, layout.repeat)
    if (n < 0) break
    const f = (i: number): number => cmd[a + i]!
    const str = (i: number): string | undefined => refs[cmd[a + i]! | 0]

    switch (op) {
      case OP.SAVE: ctx.save(); depth++; break
      case OP.RESTORE: if (depth > 0) { ctx.restore(); depth-- } break
      case OP.TRANSLATE: ctx.translate(f(0), f(1)); break
      case OP.SCALE: ctx.scale(f(0), f(1)); break
      case OP.ROTATE: ctx.rotate(f(0)); break
      case OP.TRANSFORM: ctx.transform(f(0), f(1), f(2), f(3), f(4), f(5)); break
      case OP.SET_TRANSFORM: home(); ctx.transform(f(0), f(1), f(2), f(3), f(4), f(5)); break
      case OP.RESET_TRANSFORM: home(); break

      case OP.GLOBAL_ALPHA: ctx.globalAlpha = f(0); break
      case OP.FILL_STYLE: ctx.fillStyle = color(str(0)); break
      case OP.STROKE_STYLE: ctx.strokeStyle = color(str(0)); break
      case OP.FILL_GRADIENT: ctx.fillStyle = gradient(a); break
      case OP.STROKE_GRADIENT: ctx.strokeStyle = gradient(a); break
      case OP.LINE_WIDTH: ctx.lineWidth = f(0); break
      case OP.LINE_JOIN: ctx.lineJoin = LineJoinName[f(0) | 0] ?? "miter"; break
      case OP.LINE_CAP: ctx.lineCap = LineCapName[f(0) | 0] ?? "butt"; break
      case OP.MITER_LIMIT: ctx.miterLimit = f(0); break
      case OP.LINE_DASH: {
        const dash: number[] = []
        for (let i = 0; i < (f(0) | 0); i++) dash.push(f(1 + i))
        ctx.setLineDash(dash)
        break
      }
      case OP.LINE_DASH_OFFSET: ctx.lineDashOffset = f(0); break
      case OP.FONT: {
        const font = str(0)
        if (font !== undefined) ctx.font = hooks.font ? hooks.font(font) : font
        break
      }
      case OP.TEXT_ALIGN: ctx.textAlign = TextAlignName[f(0) | 0] ?? "start"; break
      case OP.TEXT_BASELINE: ctx.textBaseline = TextBaselineName[f(0) | 0] ?? "alphabetic"; break
      case OP.LETTER_SPACING: if ("letterSpacing" in ctx) ctx.letterSpacing = `${f(0)}px`; break

      case OP.PATH_BEGIN: ctx.beginPath(); break
      case OP.PATH_CLOSE: ctx.closePath(); break
      case OP.MOVE_TO: ctx.moveTo(f(0), f(1)); break
      case OP.LINE_TO: ctx.lineTo(f(0), f(1)); break
      case OP.QUADRATIC_TO: ctx.quadraticCurveTo(f(0), f(1), f(2), f(3)); break
      case OP.BEZIER_TO: ctx.bezierCurveTo(f(0), f(1), f(2), f(3), f(4), f(5)); break
      case OP.ARC: if (f(2) >= 0) ctx.arc(f(0), f(1), f(2), f(3), f(4), f(5) !== 0); break
      case OP.ARC_TO: if (f(4) >= 0) ctx.arcTo(f(0), f(1), f(2), f(3), f(4)); break
      case OP.ELLIPSE: if (f(2) >= 0 && f(3) >= 0) ctx.ellipse(f(0), f(1), f(2), f(3), f(4), f(5), f(6), f(7) !== 0); break
      case OP.RECT: ctx.rect(f(0), f(1), f(2), f(3)); break
      case OP.ROUND_RECT: roundRect(f(0), f(1), f(2), f(3), f(4)); break

      case OP.FILL: ctx.fill((FillRuleName[f(0) | 0] ?? "nonzero") as "nonzero" | "evenodd"); break
      case OP.STROKE: ctx.stroke(); break
      case OP.CLIP: ctx.clip((FillRuleName[f(0) | 0] ?? "nonzero") as "nonzero" | "evenodd"); break

      case OP.FILL_RECT: ctx.fillRect(f(0), f(1), f(2), f(3)); break
      case OP.STROKE_RECT: ctx.strokeRect(f(0), f(1), f(2), f(3)); break
      case OP.CLEAR_RECT: ctx.clearRect(f(0), f(1), f(2), f(3)); break
      case OP.FILL_TEXT: {
        const text = str(0)
        if (text !== undefined) { if (f(3) > 0) ctx.fillText(text, f(1), f(2), f(3)); else ctx.fillText(text, f(1), f(2)) }
        break
      }
      case OP.STROKE_TEXT: {
        const text = str(0)
        if (text !== undefined) { if (f(3) > 0) ctx.strokeText(text, f(1), f(2), f(3)); else ctx.strokeText(text, f(1), f(2)) }
        break
      }
      case OP.DRAW_IMAGE: {
        const image = hooks.image?.(f(0) | 0)
        if (image && f(3) > 0 && f(4) > 0) ctx.drawImage(image, f(1), f(2), f(3), f(4), f(5), f(6), f(7), f(8))
        break
      }
    }
    ops++
    at = a + n
  }

  for (; depth > 0; depth--) ctx.restore()
  ctx.restore()
  return ops
}
