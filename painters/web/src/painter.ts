// The web painter: replays a draw list on anything shaped like a CanvasRenderingContext2D — the
// browser, an OffscreenCanvas, node-canvas / @napi-rs/canvas. A loop with one case per command, no
// state of its own: the list carries absolute transforms and resolved paints.
//
// Platform limits, stated: Canvas2D gradients only PAD (reflect / repeat draw as pad); a gradient
// under a non-similarity transform (skew, non-uniform scale — SVG objectBoundingBox on a non-square
// shape) is drawn by transforming the context, which also distorts a STROKE's pen; letterSpacing
// applies where the context supports it (Chrome 99+, Safari 17+; not node-canvas).
import type { DrawCommand, Paint, Stroke, Font, PathSeg, Matrix6, Color } from "./drawlist"

/** The subset of CanvasRenderingContext2D the painter uses. */
export interface Canvas2DLike {
  save(): void
  restore(): void
  setTransform(a: number, b: number, c: number, d: number, e: number, f: number): void
  transform(a: number, b: number, c: number, d: number, e: number, f: number): void
  beginPath(): void
  moveTo(x: number, y: number): void
  lineTo(x: number, y: number): void
  quadraticCurveTo(cx: number, cy: number, x: number, y: number): void
  bezierCurveTo(c1x: number, c1y: number, c2x: number, c2y: number, x: number, y: number): void
  closePath(): void
  fill(rule?: "nonzero" | "evenodd"): void
  stroke(): void
  clip(rule?: "nonzero" | "evenodd"): void
  fillStyle: any
  strokeStyle: any
  lineWidth: number
  lineJoin: any
  lineCap: any
  miterLimit: number
  setLineDash(segments: number[]): void
  lineDashOffset: number
  globalAlpha: number
  font: string
  textAlign: any
  textBaseline: any
  letterSpacing?: string
  fillText(text: string, x: number, y: number, maxWidth?: number): void
  strokeText(text: string, x: number, y: number, maxWidth?: number): void
  drawImage(image: any, sx: number, sy: number, sw: number, sh: number, dx: number, dy: number, dw: number, dh: number): void
  clearRect(x: number, y: number, w: number, h: number): void
  createLinearGradient(x0: number, y0: number, x1: number, y1: number): any
  createRadialGradient(x0: number, y0: number, r0: number, x1: number, y1: number, r1: number): any
}

export interface PainterHooks {
  /** The drawable for a surface id (a canvas, an ImageBitmap, an Image); null skips the blit. */
  image?(surface: number): any
  /** Map a font family to what this context can render (a registered font's real name, a fallback). */
  fontFamily?(family: string): string
}

const GENERIC_FAMILIES = new Set(["serif", "sans-serif", "monospace", "cursive", "fantasy", "system-ui", "ui-serif", "ui-sans-serif", "ui-monospace", "ui-rounded", "emoji", "math", "fangsong"])

/** A draw-list color as CSS. */
export const cssColor = (c: Color): string => `rgba(${Math.round(c[0] * 255)}, ${Math.round(c[1] * 255)}, ${Math.round(c[2] * 255)}, ${c[3]})`

/** A draw-list font as a CSS font shorthand. */
export const cssFont = (f: Font, family = f.family): string => {
  const fam = GENERIC_FAMILIES.has(family) ? family : `"${family.replace(/"/g, "'")}"`
  return `${f.italic ? "italic " : ""}${f.weight} ${f.size}px ${fam}`
}

const apply = (m: Matrix6, x: number, y: number): [number, number] => [m[0] * x + m[2] * y + m[4], m[1] * x + m[3] * y + m[5]]

// A similarity transform (uniform scale + rotation + translation) keeps circles circles, so the
// gradient can be expressed in user space without transforming the context.
const isSimilarity = (m: Matrix6): boolean => Math.abs(m[0] - m[3]) < 1e-6 && Math.abs(m[1] + m[2]) < 1e-6

const buildPath = (ctx: Canvas2DLike, path: PathSeg[]): void => {
  ctx.beginPath()
  for (const s of path) {
    switch (s[0]) {
      case "M": ctx.moveTo(s[1], s[2]); break
      case "L": ctx.lineTo(s[1], s[2]); break
      case "Q": ctx.quadraticCurveTo(s[1], s[2], s[3], s[4]); break
      case "C": ctx.bezierCurveTo(s[1], s[2], s[3], s[4], s[5], s[6]); break
      case "Z": ctx.closePath(); break
    }
  }
}

const gradientStops = (g: any, paint: Paint & { stops: { offset: number, color: Color }[] }): void => {
  for (const s of paint.stops) g.addColorStop(s.offset, cssColor(s.color))
}

// Sets fillStyle / strokeStyle + globalAlpha, then runs `draw`. A gradient with a general matrix is
// drawn inside a context transform (the path, already built, is not affected — Canvas2D keeps paths
// in device space).
const withPaint = (ctx: Canvas2DLike, paint: Paint, which: "fill" | "stroke", draw: () => void): void => {
  ctx.globalAlpha = paint.alpha
  if (paint.kind === "color") {
    ctx[which === "fill" ? "fillStyle" : "strokeStyle"] = cssColor(paint.color)
    draw()
    return
  }
  const m = paint.matrix
  if (isSimilarity(m)) {
    let g: any
    if (paint.kind === "linear") {
      const [x0, y0] = apply(m, 0, 0), [x1, y1] = apply(m, 1, 0)
      g = ctx.createLinearGradient(x0, y0, x1, y1)
    } else {
      const s = Math.hypot(m[0], m[1])
      const [cx, cy] = apply(m, 0, 0), [fx, fy] = apply(m, paint.fx, paint.fy)
      g = ctx.createRadialGradient(fx, fy, paint.r0 * s, cx, cy, s)
    }
    gradientStops(g, paint)
    ctx[which === "fill" ? "fillStyle" : "strokeStyle"] = g
    draw()
    return
  }
  ctx.save()
  ctx.transform(m[0], m[1], m[2], m[3], m[4], m[5])
  const g = paint.kind === "linear" ? ctx.createLinearGradient(0, 0, 1, 0) : ctx.createRadialGradient(paint.fx, paint.fy, paint.r0, 0, 0, 1)
  gradientStops(g, paint)
  ctx[which === "fill" ? "fillStyle" : "strokeStyle"] = g
  draw()
  ctx.restore()
}

const applyStroke = (ctx: Canvas2DLike, s: Stroke): void => {
  ctx.lineWidth = s.width
  ctx.lineJoin = s.join
  ctx.lineCap = s.cap
  ctx.miterLimit = s.miterLimit
  ctx.setLineDash(s.dash)
  ctx.lineDashOffset = s.dashOffset
}

const applyText = (ctx: Canvas2DLike, c: { font: Font, align: string, baseline: string, letterSpacing: number }, hooks: PainterHooks): void => {
  ctx.font = cssFont(c.font, hooks.fontFamily ? hooks.fontFamily(c.font.family) : c.font.family)
  ctx.textAlign = c.align
  ctx.textBaseline = c.baseline
  if ("letterSpacing" in ctx) ctx.letterSpacing = `${c.letterSpacing}px`
}

/** Replay `commands` on `ctx`. The context should start with the identity transform and an empty
 *  save stack (the painter sets the identity itself); clearing the surface is the caller's decision. */
export const paint = (ctx: Canvas2DLike, commands: DrawCommand[], hooks: PainterHooks = {}): void => {
  ctx.setTransform(1, 0, 0, 1, 0, 0)
  ctx.globalAlpha = 1
  for (const c of commands) {
    switch (c.cmd) {
      case "setTransform": ctx.setTransform(c.matrix[0], c.matrix[1], c.matrix[2], c.matrix[3], c.matrix[4], c.matrix[5]); break
      case "save": ctx.save(); break
      case "restore": ctx.restore(); break
      case "clip": buildPath(ctx, c.path); ctx.clip(c.rule as "nonzero" | "evenodd"); break
      case "fillPath": buildPath(ctx, c.path); withPaint(ctx, c.paint, "fill", () => ctx.fill(c.rule as "nonzero" | "evenodd")); break
      case "strokePath": buildPath(ctx, c.path); applyStroke(ctx, c.stroke); withPaint(ctx, c.paint, "stroke", () => ctx.stroke()); break
      case "fillText": applyText(ctx, c, hooks); withPaint(ctx, c.paint, "fill", () => c.maxWidth > 0 ? ctx.fillText(c.text, c.x, c.y, c.maxWidth) : ctx.fillText(c.text, c.x, c.y)); break
      case "strokeText": applyText(ctx, c, hooks); applyStroke(ctx, c.stroke); withPaint(ctx, c.paint, "stroke", () => c.maxWidth > 0 ? ctx.strokeText(c.text, c.x, c.y, c.maxWidth) : ctx.strokeText(c.text, c.x, c.y)); break
      case "drawImage": {
        const img = hooks.image?.(c.surface)
        if (!img) break
        ctx.globalAlpha = c.alpha
        ctx.drawImage(img, c.src[0], c.src[1], c.src[2], c.src[3], c.dst[0], c.dst[1], c.dst[2], c.dst[3])
        break
      }
      case "clearRect": ctx.clearRect(c.rect[0], c.rect[1], c.rect[2], c.rect[3]); break
      default: { const never: never = c; throw new Error(`unhandled draw command ${(never as DrawCommand).cmd}`) }
    }
  }
}
