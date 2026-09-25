// The TypeScript recorder: a Canvas2D-shaped API whose calls WRITE the opcode stream of spec/ops.h.
// Nothing is drawn here — the stream goes to the core interpreter (native, or wasm on the web), which
// turns it into a draw list a painter replays. An app-facing Canvas class wraps a Recorder and adds
// what needs a painter (measureText, surfaces, textures).
//
// Differences from the browser, on purpose:
//   - colors and fonts are CSS strings resolved by the core (an invalid one keeps the previous style)
//   - a Gradient is serialized when it is ASSIGNED to fillStyle / strokeStyle; stops added later
//     do not affect an assignment already made
//   - drawImage takes a surface id (an ImageRef with its device size for the short forms)
//   - every drawing call returns `this`, so drawing chains
import { OP, LineJoin, LineCap, TextAlign, TextBaseline, FillRule, Gradient as GradientKind } from "anycanvas-spec"

export type FillRuleName = "nonzero" | "evenodd"
export type LineJoinName = "miter" | "round" | "bevel"
export type LineCapName = "butt" | "round" | "square"
export type TextAlignName = "left" | "center" | "right" | "start" | "end"
export type TextBaselineName = "alphabetic" | "top" | "middle" | "bottom" | "hanging" | "ideographic"

const JOIN: Record<LineJoinName, number> = { miter: LineJoin.MITER, round: LineJoin.ROUND, bevel: LineJoin.BEVEL }
const CAP: Record<LineCapName, number> = { butt: LineCap.BUTT, round: LineCap.ROUND, square: LineCap.SQUARE }
const ALIGN: Record<TextAlignName, number> = { left: TextAlign.LEFT, center: TextAlign.CENTER, right: TextAlign.RIGHT, start: TextAlign.START, end: TextAlign.END }
const BASELINE: Record<TextBaselineName, number> = {
  alphabetic: TextBaseline.ALPHABETIC, top: TextBaseline.TOP, middle: TextBaseline.MIDDLE,
  bottom: TextBaseline.BOTTOM, hanging: TextBaseline.HANGING, ideographic: TextBaseline.IDEOGRAPHIC,
}
const RULE: Record<FillRuleName, number> = { nonzero: FillRule.NONZERO, evenodd: FillRule.EVENODD }
// The shadowed style fields save() / restore() snapshot, as the core's state stack does.
const SHADOWED = ["_globalAlpha", "_fillStyle", "_strokeStyle", "_lineWidth", "_lineJoin", "_lineCap", "_miterLimit",
  "_lineDash", "_lineDashOffset", "_font", "_textAlign", "_textBaseline", "_letterSpacing"] as const

/** A gradient built by createLinearGradient / createRadialGradient; assign it to fillStyle / strokeStyle. */
export class Gradient {
  readonly stops: { offset: number, color: string }[] = []
  readonly kind: number
  readonly coords: [number, number, number, number, number, number]
  /** @internal */
  constructor(kind: number, coords: [number, number, number, number, number, number]) { this.kind = kind; this.coords = coords }
  addColorStop(offset: number, color: string): this {
    if (!(offset >= 0 && offset <= 1)) throw new RangeError(`gradient stop offset ${offset} is outside 0..1`)
    this.stops.push({ offset, color })
    return this
  }
}

/** The recorded stream: the f32 words and the string table the core reads. */
export interface Stream { cmd: Float32Array, refs: string[] }

/** A surface with its device size, for the short drawImage forms. */
export interface ImageRef { surface: number, width: number, height: number }

export class Recorder {
  private cmd: number[] = []
  private refs: string[] = []
  private refIndex = new Map<string, number>()

  // Shadowed state so the getters read back what was set (the stream itself is write-only).
  private _globalAlpha = 1
  private _fillStyle: string | Gradient = "#000000"
  private _strokeStyle: string | Gradient = "#000000"
  private _lineWidth = 1
  private _lineJoin: LineJoinName = "miter"
  private _lineCap: LineCapName = "butt"
  private _miterLimit = 10
  private _lineDash: number[] = []
  private _lineDashOffset = 0
  private _font = "10px sans-serif"
  private _textAlign: TextAlignName = "start"
  private _textBaseline: TextBaselineName = "alphabetic"
  private _letterSpacing = 0
  // One snapshot of the shadowed fields per open save(), so the getters read back what restore()
  // brings back (by reference: setLineDash stores a fresh array, a Gradient is the assigned object).
  private _saved: Record<string, unknown>[] = []

  // ---- the stream -----------------------------------------------------------------------------

  /** The words recorded so far (a copy) + the string table. */
  stream(): Stream { return { cmd: new Float32Array(this.cmd), refs: this.refs.slice() } }
  /** Plain arrays, for a JSON file (the golden tests' input). */
  toJSON(): { cmd: number[], refs: string[] } { return { cmd: this.cmd.slice(), refs: this.refs.slice() } }
  /** The number of words recorded. */
  get length(): number { return this.cmd.length }
  /** Discard the recording; the shadowed state is reset too (a fresh replay starts from defaults). */
  reset(): this {
    this.cmd.length = 0
    this.refs.length = 0
    this.refIndex.clear()
    this._globalAlpha = 1; this._fillStyle = "#000000"; this._strokeStyle = "#000000"
    this._lineWidth = 1; this._lineJoin = "miter"; this._lineCap = "butt"; this._miterLimit = 10
    this._lineDash = []; this._lineDashOffset = 0
    this._font = "10px sans-serif"; this._textAlign = "start"; this._textBaseline = "alphabetic"; this._letterSpacing = 0
    this._saved.length = 0
    return this
  }

  private ref(s: string): number {
    let i = this.refIndex.get(s)
    if (i === undefined) { i = this.refs.length; this.refs.push(s); this.refIndex.set(s, i) }
    return i
  }
  private push(...words: number[]): this { for (const w of words) this.cmd.push(w); return this }

  // ---- state stack + transforms ---------------------------------------------------------------

  save(): this {
    const s: Record<string, unknown> = {}
    for (const k of SHADOWED) s[k] = this[k]
    this._saved.push(s)
    return this.push(OP.SAVE)
  }
  restore(): this {
    const s = this._saved.pop()   // an unbalanced restore() keeps the state, like the core
    if (s) Object.assign(this, s)
    return this.push(OP.RESTORE)
  }
  translate(x: number, y: number): this { return this.push(OP.TRANSLATE, x, y) }
  scale(sx: number, sy: number): this { return this.push(OP.SCALE, sx, sy) }
  rotate(rad: number): this { return this.push(OP.ROTATE, rad) }
  transform(a: number, b: number, c: number, d: number, e: number, f: number): this { return this.push(OP.TRANSFORM, a, b, c, d, e, f) }
  setTransform(a: number, b: number, c: number, d: number, e: number, f: number): this { return this.push(OP.SET_TRANSFORM, a, b, c, d, e, f) }
  resetTransform(): this { return this.push(OP.RESET_TRANSFORM) }

  // ---- styles ---------------------------------------------------------------------------------

  get globalAlpha(): number { return this._globalAlpha }
  set globalAlpha(a: number) { this._globalAlpha = a; this.push(OP.GLOBAL_ALPHA, a) }

  get fillStyle(): string | Gradient { return this._fillStyle }
  set fillStyle(v: string | Gradient) { this._fillStyle = v; this.style(OP.FILL_STYLE, OP.FILL_GRADIENT, v) }
  get strokeStyle(): string | Gradient { return this._strokeStyle }
  set strokeStyle(v: string | Gradient) { this._strokeStyle = v; this.style(OP.STROKE_STYLE, OP.STROKE_GRADIENT, v) }
  private style(colorOp: number, gradientOp: number, v: string | Gradient): void {
    if (typeof v === "string") { this.push(colorOp, this.ref(v)); return }
    this.push(gradientOp, v.kind, ...v.coords, v.stops.length)
    for (const s of v.stops) this.push(s.offset, this.ref(s.color))
  }

  createLinearGradient(x0: number, y0: number, x1: number, y1: number): Gradient {
    return new Gradient(GradientKind.LINEAR, [x0, y0, x1, y1, 0, 0])
  }
  createRadialGradient(x0: number, y0: number, r0: number, x1: number, y1: number, r1: number): Gradient {
    return new Gradient(GradientKind.RADIAL, [x0, y0, x1, y1, r0, r1])
  }

  get lineWidth(): number { return this._lineWidth }
  set lineWidth(w: number) { this._lineWidth = w; this.push(OP.LINE_WIDTH, w) }
  get lineJoin(): LineJoinName { return this._lineJoin }
  set lineJoin(j: LineJoinName) { this._lineJoin = j; this.push(OP.LINE_JOIN, JOIN[j]) }
  get lineCap(): LineCapName { return this._lineCap }
  set lineCap(c: LineCapName) { this._lineCap = c; this.push(OP.LINE_CAP, CAP[c]) }
  get miterLimit(): number { return this._miterLimit }
  set miterLimit(m: number) { this._miterLimit = m; this.push(OP.MITER_LIMIT, m) }
  setLineDash(segments: number[]): this {
    this._lineDash = segments.slice()
    return this.push(OP.LINE_DASH, segments.length, ...segments)
  }
  getLineDash(): number[] { return this._lineDash.slice() }
  get lineDashOffset(): number { return this._lineDashOffset }
  set lineDashOffset(o: number) { this._lineDashOffset = o; this.push(OP.LINE_DASH_OFFSET, o) }

  get font(): string { return this._font }
  set font(f: string) { this._font = f; this.push(OP.FONT, this.ref(f)) }
  get textAlign(): TextAlignName { return this._textAlign }
  set textAlign(a: TextAlignName) { this._textAlign = a; this.push(OP.TEXT_ALIGN, ALIGN[a]) }
  get textBaseline(): TextBaselineName { return this._textBaseline }
  set textBaseline(b: TextBaselineName) { this._textBaseline = b; this.push(OP.TEXT_BASELINE, BASELINE[b]) }
  get letterSpacing(): number { return this._letterSpacing }
  set letterSpacing(px: number) { this._letterSpacing = px; this.push(OP.LETTER_SPACING, px) }

  // ---- paths ----------------------------------------------------------------------------------

  beginPath(): this { return this.push(OP.PATH_BEGIN) }
  closePath(): this { return this.push(OP.PATH_CLOSE) }
  moveTo(x: number, y: number): this { return this.push(OP.MOVE_TO, x, y) }
  lineTo(x: number, y: number): this { return this.push(OP.LINE_TO, x, y) }
  quadraticCurveTo(cx: number, cy: number, x: number, y: number): this { return this.push(OP.QUADRATIC_TO, cx, cy, x, y) }
  bezierCurveTo(c1x: number, c1y: number, c2x: number, c2y: number, x: number, y: number): this {
    return this.push(OP.BEZIER_TO, c1x, c1y, c2x, c2y, x, y)
  }
  arc(x: number, y: number, r: number, a0: number, a1: number, ccw = false): this { return this.push(OP.ARC, x, y, r, a0, a1, ccw ? 1 : 0) }
  arcTo(x1: number, y1: number, x2: number, y2: number, r: number): this { return this.push(OP.ARC_TO, x1, y1, x2, y2, r) }
  ellipse(x: number, y: number, rx: number, ry: number, rotation: number, a0: number, a1: number, ccw = false): this {
    return this.push(OP.ELLIPSE, x, y, rx, ry, rotation, a0, a1, ccw ? 1 : 0)
  }
  rect(x: number, y: number, w: number, h: number): this { return this.push(OP.RECT, x, y, w, h) }
  roundRect(x: number, y: number, w: number, h: number, r: number): this { return this.push(OP.ROUND_RECT, x, y, w, h, r) }

  fill(rule: FillRuleName = "nonzero"): this { return this.push(OP.FILL, RULE[rule]) }
  stroke(): this { return this.push(OP.STROKE) }
  clip(rule: FillRuleName = "nonzero"): this { return this.push(OP.CLIP, RULE[rule]) }

  // ---- rectangles, text, images ---------------------------------------------------------------

  fillRect(x: number, y: number, w: number, h: number): this { return this.push(OP.FILL_RECT, x, y, w, h) }
  strokeRect(x: number, y: number, w: number, h: number): this { return this.push(OP.STROKE_RECT, x, y, w, h) }
  clearRect(x: number, y: number, w: number, h: number): this { return this.push(OP.CLEAR_RECT, x, y, w, h) }
  fillText(text: string, x: number, y: number, maxWidth = 0): this { return this.push(OP.FILL_TEXT, this.ref(text), x, y, maxWidth) }
  strokeText(text: string, x: number, y: number, maxWidth = 0): this { return this.push(OP.STROKE_TEXT, this.ref(text), x, y, maxWidth) }

  /** Blit a surface. Three browser-style forms; the source rect is in the surface's device px. */
  drawImage(img: ImageRef, dx: number, dy: number): this
  drawImage(img: ImageRef, dx: number, dy: number, dw: number, dh: number): this
  drawImage(img: ImageRef, sx: number, sy: number, sw: number, sh: number, dx: number, dy: number, dw: number, dh: number): this
  drawImage(img: ImageRef, a: number, b: number, c?: number, d?: number, e?: number, f?: number, g?: number, h?: number): this {
    if (e === undefined) {
      return this.push(OP.DRAW_IMAGE, img.surface, 0, 0, img.width, img.height, a, b, c ?? img.width, d ?? img.height)
    }
    return this.push(OP.DRAW_IMAGE, img.surface, a, b, c!, d!, e, f!, g!, h!)
  }
}
