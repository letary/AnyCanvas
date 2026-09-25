// The draw list in TypeScript: the typed commands, the reader of the word buffer (spec/draw.h) and
// the reader of the core's JSON dump. Both produce the same DrawCommand objects — the tests hold the
// two readers against each other on every golden.
import {
  DRAW, Paint as PaintKind, PathVerb, LineJoinName, LineCapName, TextAlignName, TextBaselineName, FillRuleName, SpreadName,
} from "anycanvas-spec"

export type Color = [number, number, number, number]
export type Matrix6 = [number, number, number, number, number, number]
export interface Stop { offset: number, color: Color }
export interface ColorPaint { kind: "color", alpha: number, color: Color }
export interface LinearPaint { kind: "linear", alpha: number, matrix: Matrix6, spread: string, stops: Stop[] }
export interface RadialPaint { kind: "radial", alpha: number, matrix: Matrix6, fx: number, fy: number, r0: number, spread: string, stops: Stop[] }
export type Paint = ColorPaint | LinearPaint | RadialPaint
export interface Stroke { width: number, join: string, cap: string, miterLimit: number, dashOffset: number, dash: number[] }
export interface Font { family: string, size: number, weight: number, italic: boolean }
export type PathSeg = ["M", number, number] | ["L", number, number] | ["Q", number, number, number, number] | ["C", number, number, number, number, number, number] | ["Z"]

export type DrawCommand =
  | { cmd: "setTransform", matrix: Matrix6 }
  | { cmd: "save" }
  | { cmd: "restore" }
  | { cmd: "clip", rule: string, path: PathSeg[] }
  | { cmd: "fillPath", rule: string, paint: Paint, path: PathSeg[] }
  | { cmd: "strokePath", stroke: Stroke, paint: Paint, path: PathSeg[] }
  | { cmd: "fillText", text: string, x: number, y: number, maxWidth: number, font: Font, align: string, baseline: string, letterSpacing: number, paint: Paint }
  | { cmd: "strokeText", text: string, x: number, y: number, maxWidth: number, font: Font, align: string, baseline: string, letterSpacing: number, stroke: Stroke, paint: Paint }
  | { cmd: "drawImage", surface: number, src: [number, number, number, number], dst: [number, number, number, number], alpha: number }
  | { cmd: "clearRect", rect: [number, number, number, number] }

class Reader {
  pos = 0
  constructor(readonly w: Float32Array, readonly strings: readonly string[]) {}
  f(): number { if (this.pos >= this.w.length) throw new Error("draw list: truncated"); return this.w[this.pos++] }
  i(): number { return this.f() | 0 }
  str(): string { const i = this.i(); if (i < 0 || i >= this.strings.length) throw new Error(`draw list: bad string index ${i}`); return this.strings[i] }
  color(): Color { return [this.f(), this.f(), this.f(), this.f()] }
  matrix(): Matrix6 { return [this.f(), this.f(), this.f(), this.f(), this.f(), this.f()] }
  paint(): Paint {
    const kind = this.i(), alpha = this.f()
    if (kind === PaintKind.COLOR) return { kind: "color", alpha, color: this.color() }
    const matrix = this.matrix()
    let fx = 0, fy = 0, r0 = 0
    if (kind === PaintKind.RADIAL) { fx = this.f(); fy = this.f(); r0 = this.f() }
    const spread = SpreadName[this.i()] ?? "pad"
    const n = this.i()
    const stops: Stop[] = []
    for (let k = 0; k < n; k++) stops.push({ offset: this.f(), color: this.color() })
    return kind === PaintKind.RADIAL ? { kind: "radial", alpha, matrix, fx, fy, r0, spread, stops } : { kind: "linear", alpha, matrix, spread, stops }
  }
  stroke(): Stroke {
    const width = this.f(), join = LineJoinName[this.i()] ?? "miter", cap = LineCapName[this.i()] ?? "butt"
    const miterLimit = this.f(), dashOffset = this.f(), n = this.i()
    const dash: number[] = []
    for (let k = 0; k < n; k++) dash.push(this.f())
    return { width, join, cap, miterLimit, dashOffset, dash }
  }
  font(): Font { return { family: this.str(), size: this.f(), weight: this.i(), italic: this.i() !== 0 } }
  path(): PathSeg[] {
    const n = this.i()
    const end = this.pos + n
    if (end > this.w.length) throw new Error("draw list: truncated path")
    const segs: PathSeg[] = []
    while (this.pos < end) {
      switch (this.i()) {
        case PathVerb.MOVE: segs.push(["M", this.f(), this.f()]); break
        case PathVerb.LINE: segs.push(["L", this.f(), this.f()]); break
        case PathVerb.QUAD: segs.push(["Q", this.f(), this.f(), this.f(), this.f()]); break
        case PathVerb.CUBIC: segs.push(["C", this.f(), this.f(), this.f(), this.f(), this.f(), this.f()]); break
        case PathVerb.CLOSE: segs.push(["Z"]); break
        default: throw new Error("draw list: bad path verb")
      }
    }
    return segs
  }
  rect4(): [number, number, number, number] { return [this.f(), this.f(), this.f(), this.f()] }
}

/** Decode a word buffer + string table into commands. A command this build does not know is skipped. */
export const readDrawList = (words: Float32Array, strings: readonly string[]): DrawCommand[] => {
  const r = new Reader(words, strings)
  const out: DrawCommand[] = []
  while (r.pos + 2 <= words.length) {
    const cmd = r.i(), len = r.i()
    const end = r.pos + len
    if (len < 0 || end > words.length) throw new Error("draw list: truncated command")
    switch (cmd) {
      case DRAW.SET_TRANSFORM: out.push({ cmd: "setTransform", matrix: r.matrix() }); break
      case DRAW.SAVE: out.push({ cmd: "save" }); break
      case DRAW.RESTORE: out.push({ cmd: "restore" }); break
      case DRAW.CLIP: { const rule = FillRuleName[r.i()] ?? "nonzero"; out.push({ cmd: "clip", rule, path: r.path() }); break }
      case DRAW.FILL_PATH: { const rule = FillRuleName[r.i()] ?? "nonzero"; const paint = r.paint(); out.push({ cmd: "fillPath", rule, paint, path: r.path() }); break }
      case DRAW.STROKE_PATH: { const stroke = r.stroke(); const paint = r.paint(); out.push({ cmd: "strokePath", stroke, paint, path: r.path() }); break }
      case DRAW.FILL_TEXT: case DRAW.STROKE_TEXT: {
        const text = r.str(), x = r.f(), y = r.f(), maxWidth = r.f(), font = r.font()
        const align = TextAlignName[r.i()] ?? "start", baseline = TextBaselineName[r.i()] ?? "alphabetic", letterSpacing = r.f()
        if (cmd === DRAW.STROKE_TEXT) { const stroke = r.stroke(); out.push({ cmd: "strokeText", text, x, y, maxWidth, font, align, baseline, letterSpacing, stroke, paint: r.paint() }) }
        else out.push({ cmd: "fillText", text, x, y, maxWidth, font, align, baseline, letterSpacing, paint: r.paint() })
        break
      }
      case DRAW.DRAW_IMAGE: { const surface = r.i(); const src = r.rect4(); const dst = r.rect4(); out.push({ cmd: "drawImage", surface, src, dst, alpha: r.f() }); break }
      case DRAW.CLEAR_RECT: out.push({ cmd: "clearRect", rect: r.rect4() }); break
      default: break   // unknown: skipped by len
    }
    if (r.pos !== end) {
      if (r.pos > end) throw new Error(`draw list: command ${cmd} overran its length`)
      r.pos = end
    }
  }
  return out
}

/** The core's JSON dump (ac_drawlist_json / acdump) → commands. */
export const parseDrawListJson = (text: string): DrawCommand[] => {
  const v = JSON.parse(text)
  if (!Array.isArray(v)) throw new Error("draw list JSON: expected an array")
  for (const c of v) if (typeof c !== "object" || typeof c.cmd !== "string") throw new Error("draw list JSON: bad command")
  return v as DrawCommand[]
}

/** The binary file acdump / the golden test write: u32 wordCount, f32 words, u32 stringCount, (u32 len, utf8)*. */
export const readDrawListBinary = (bytes: Uint8Array): { words: Float32Array, strings: string[] } => {
  const dv = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength)
  let pos = 0
  const wordCount = dv.getUint32(pos, true); pos += 4
  const words = new Float32Array(wordCount)
  for (let i = 0; i < wordCount; i++) { words[i] = dv.getFloat32(pos, true); pos += 4 }
  const stringCount = dv.getUint32(pos, true); pos += 4
  const strings: string[] = []
  const dec = new TextDecoder()
  for (let i = 0; i < stringCount; i++) {
    const len = dv.getUint32(pos, true); pos += 4
    strings.push(dec.decode(bytes.subarray(pos, pos + len))); pos += len
  }
  return { words, strings }
}
