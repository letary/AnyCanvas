// The TS reader against the core: for every golden, the binary the core wrote decodes to the very
// commands its JSON dump describes. This is what keeps the TS painter honest about the wire format.
import { describe, expect, test } from "bun:test"
import { readdirSync, readFileSync } from "node:fs"
import { join } from "node:path"
import { readDrawList, parseDrawListJson, readDrawListBinary, type DrawCommand } from "anycanvas-web"
import { DRAW } from "anycanvas-spec"
import { goldenDir } from "./helpers"

const expectedDir = join(goldenDir, "expected")
const names = readdirSync(expectedDir).filter(f => f.endsWith(".json")).map(f => f.slice(0, -5))

// JSON carries the shortest decimal that round-trips the float32; the binary carries the float32.
// Comparing through Math.fround makes the two identical.
const froundDeep = (v: unknown): unknown => {
  if (typeof v === "number") return v === 0 ? 0 : Math.fround(v)
  if (Array.isArray(v)) return v.map(froundDeep)
  if (v && typeof v === "object") return Object.fromEntries(Object.entries(v).map(([k, x]) => [k, froundDeep(x)]))
  return v
}

describe("draw list", () => {
  test("there are goldens to read (build the core and run the golden test first)", () => {
    expect(names.length).toBeGreaterThan(0)
  })

  for (const name of names) {
    test(`binary == json: ${name}`, () => {
      const json = parseDrawListJson(readFileSync(join(expectedDir, `${name}.json`), "utf8"))
      const { words, strings } = readDrawListBinary(new Uint8Array(readFileSync(join(expectedDir, `${name}.bin`))))
      const bin = readDrawList(words, strings)
      expect(froundDeep(bin)).toEqual(froundDeep(json))
    })
  }

  test("an unknown command is skipped by its length; a truncated one throws", () => {
    const words = new Float32Array([99, 3, 1, 2, 3, DRAW.SAVE, 0])
    expect(readDrawList(words, [])).toEqual([{ cmd: "save" }] as DrawCommand[])
    expect(() => readDrawList(new Float32Array([DRAW.SET_TRANSFORM, 6, 1, 0]), [])).toThrow()
    expect(() => readDrawList(new Float32Array([DRAW.FILL_TEXT, 1, 5]), [])).toThrow()
  })
})
