// The web painter on a real 2D context (@napi-rs/canvas, Skia): every golden replays without
// throwing, and the "shapes" golden puts known colors where the drawing says.
import { describe, expect, test } from "bun:test"
import { readdirSync, readFileSync, mkdirSync, writeFileSync } from "node:fs"
import { join } from "node:path"
import { createCanvas } from "@napi-rs/canvas"
import { paint, parseDrawListJson } from "anycanvas-web"
import { goldenDir } from "./helpers"

const expectedDir = join(goldenDir, "expected")
const outDir = join(goldenDir, "..", "out")
const names = readdirSync(expectedDir).filter(f => f.endsWith(".json")).map(f => f.slice(0, -5))

const render = (name: string, w: number, h: number) => {
  const canvas = createCanvas(w, h)
  const ctx = canvas.getContext("2d")
  const commands = parseDrawListJson(readFileSync(join(expectedDir, `${name}.json`), "utf8"))
  const img = createCanvas(64, 32)
  img.getContext("2d").fillStyle = "#f0f"
  img.getContext("2d").fillRect(0, 0, 64, 32)
  paint(ctx as any, commands, { image: () => img })
  mkdirSync(outDir, { recursive: true })
  writeFileSync(join(outDir, `${name}.png`), canvas.toBuffer("image/png"))
  return ctx
}

const pixel = (ctx: any, x: number, y: number): number[] => Array.from(ctx.getImageData(x, y, 1, 1).data)

describe("web painter", () => {
  for (const name of names) {
    test(`replays ${name}`, () => { render(name, 640, 560) })
  }

  test("shapes: the colors land where the drawing says (2x)", () => {
    const ctx = render("stream-shapes", 700, 600)
    // The blue rect at (10,10)-(110,70) logical → (20,20)-(220,140) device.
    expect(pixel(ctx, 100, 60)).toEqual([30, 144, 255, 255])
    // Outside everything: transparent.
    expect(pixel(ctx, 690, 590)).toEqual([0, 0, 0, 0])
    // The orange pie: its center area is filled (the 270° sweep covers the upper-left quadrant).
    expect(pixel(ctx, 380, 100)).toEqual([255, 165, 0, 255])
    // The clipped green circle: inside the clip rect it is painted, just outside it is not.
    expect(pixel(ctx, 560, 100)[1]).toBeGreaterThan(100)
    expect(pixel(ctx, 470, 100)).toEqual([0, 0, 0, 0])
    // The gradient card: white-ish at the top, grey at the bottom.
    const top = pixel(ctx, 200, 250), bottom = pixel(ctx, 200, 390)
    expect(top[0]).toBeGreaterThan(bottom[0] + 60)
  })
})
