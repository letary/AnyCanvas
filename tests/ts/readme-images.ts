// Renders the README images from the goldens with the web painter (Skia via @napi-rs/canvas) and
// crops the phone screenshot. Run from the repo root after the goldens exist:
//   bun tests/ts/readme-images.ts [tests/out/phone-final.png]
import { readFileSync, writeFileSync, existsSync } from "node:fs"
import { join } from "node:path"
import { createCanvas, loadImage } from "@napi-rs/canvas"
import { paint, parseDrawListJson } from "anycanvas-web"

const root = join(import.meta.dir, "..", "..")
const expected = (name: string) => parseDrawListJson(readFileSync(join(root, "tests", "golden", "expected", `${name}.json`), "utf8"))
const out = (name: string) => join(root, "docs", "images", name)

// A draw list painted into a w × h (device px) canvas on a white card.
const render = (commands: ReturnType<typeof parseDrawListJson>, w: number, h: number, file: string) => {
  const canvas = createCanvas(w, h)
  const ctx = canvas.getContext("2d")
  ctx.fillStyle = "#fff"
  ctx.fillRect(0, 0, w, h)
  paint(ctx as any, commands, {})
  writeFileSync(out(file), canvas.toBuffer("image/png"))
  console.log(`wrote docs/images/${file}`)
}

render(expected("stream-shapes"), 700, 560, "shapes-skia.png")      // recorded at 2x: 350 × 280 logical
render(expected("svg-23"), 494, 800, "tiger-skia.png")

// The text SVG at 2x: the core fits it into 600 × 400 (a painter starts from the identity transform,
// so scaling lives in the draw list — acdump's --fit).
const acdump = ["build/acdump.exe", "build/acdump", "build-posix/acdump"].map(p => join(root, p)).find(existsSync)
if (acdump) {
  const { spawnSync } = await import("node:child_process")
  const res = spawnSync(acdump, ["svg", join(root, "tests", "golden", "svg", "text.svg"), "--fit", "600", "400"], { encoding: "utf8" })
  render(parseDrawListJson(res.stdout), 600, 400, "text-skia.png")
} else {
  console.log("no acdump build; text-skia.png kept as is")
}

// The phone: crop the shapes card out of a screenshot of the demo app. The card is 1080 px wide for
// 720 golden units (1.5 px per unit); the golden is 700 × 560 units → 1050 × 840 px.
const shot = process.argv[2] ?? join(root, "tests", "out", "phone-final.png")
if (existsSync(shot)) {
  const img = await loadImage(readFileSync(shot))
  const x = 24, y = 652, cw = 1050, ch = 840   // the card's top-left in that screenshot
  const canvas = createCanvas(700, 560)
  canvas.getContext("2d").drawImage(img, x, y, cw, ch, 0, 0, 700, 560)
  writeFileSync(out("shapes-android.png"), canvas.toBuffer("image/png"))
  console.log("wrote docs/images/shapes-android.png")
} else {
  console.log(`no phone screenshot at ${shot}; shapes-android.png kept as is`)
}
