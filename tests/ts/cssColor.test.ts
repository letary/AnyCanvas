// The TS color twin against the C++ header: tests/golden/colors/expected.json is what
// anycanvas/css_color.h makes of the corpus and of every name (written by `anycanvas-golden --update`);
// recorders/ts/src/cssColor.ts must produce the same bytes and the same float32 bits, and know the
// same names.
import { describe, expect, test } from "bun:test"
import { readFileSync } from "node:fs"
import { join } from "node:path"
import { parseCssColor, cssHex8, cssRgba8, cssToByte, CSS_COLOR_NAMES } from "anycanvas-recorder"
import { goldenDir } from "./helpers"

interface Case { in: string, rgba8: string | null, rgba: number[] | null }
const golden = JSON.parse(readFileSync(join(goldenDir, "colors", "expected.json"), "utf8")) as { names: string[], cases: Case[] }

// JSON carries the shortest decimal that reads back to the float32; -0 folds to 0 (the dump folds it).
const bits = (v: number[]): number[] => v.map(x => x === 0 ? 0 : Math.fround(x))

describe("css color: TS == C++", () => {
  test("the golden covers the corpus and every name in both cases", () => {
    expect(golden.cases.length).toBeGreaterThan(golden.names.length * 2)
  })

  test("every case: the same bytes and the same float32 bits", () => {
    const mismatches: string[] = []
    for (const e of golden.cases) {
      const c = parseCssColor(e.in)
      const hex = c ? cssHex8(c) : null
      const floats = c ? bits([c.r, c.g, c.b, c.a]) : null
      if (hex !== e.rgba8 || JSON.stringify(floats) !== JSON.stringify(e.rgba ? bits(e.rgba) : null))
        mismatches.push(`${JSON.stringify(e.in)}: TS ${hex} ${JSON.stringify(floats)}, C++ ${e.rgba8} ${JSON.stringify(e.rgba)}`)
    }
    expect(mismatches).toEqual([])
  })

  test("the same name table", () => {
    expect(Object.keys(CSS_COLOR_NAMES).sort()).toEqual([...golden.names].sort())
    for (const name of golden.names) {
      const c = parseCssColor(name)
      expect(c && cssRgba8(c)).toBe(CSS_COLOR_NAMES[name])
    }
  })
})

describe("css color: TS only", () => {
  test("a string stops at an embedded NUL, like the C string", () => {
    expect(parseCssColor("red\0junk")).toEqual(parseCssColor("red"))
    expect(parseCssColor("\0red")).toBeNull()
  })

  test("names are own keys only; a non-string is not a color", () => {
    for (const s of ["constructor", "__proto__", "toString", "hasOwnProperty"]) expect(parseCssColor(s)).toBeNull()
    expect(parseCssColor(0xff0000 as unknown as string)).toBeNull()
    expect(parseCssColor(undefined as unknown as string)).toBeNull()
  })

  test("bytes: float32, clamp, round half up", () => {
    expect(cssToByte(0.5)).toBe(128)
    expect(cssToByte(0.8)).toBe(204)
    expect(cssToByte(0.3)).toBe(77)
    expect(cssToByte(NaN)).toBe(0)
    expect(cssToByte(-1)).toBe(0)
    expect(cssToByte(2)).toBe(255)
    for (let k = 0; k < 256; k++) expect(cssToByte(Math.fround(k / 255))).toBe(k)
    expect(cssRgba8({ r: 1, g: 0, b: 0, a: 1 })).toBe(0xff0000ff)
    expect(cssHex8({ r: 0, g: 0, b: 0, a: 0 })).toBe("#00000000")
    expect(cssHex8(parseCssColor("rgba(0, 0, 255, .5)")!)).toBe("#0000ff80")
  })
})
