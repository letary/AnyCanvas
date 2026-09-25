import { describe, expect, test } from "bun:test"
import { existsSync, readFileSync } from "node:fs"
import { join } from "node:path"
import { Recorder, Gradient } from "anycanvas-recorder"
import { OP, OP_ARGS } from "anycanvas-spec"
import { scenarios } from "./scenarios"
import { goldenDir, recordScenario, type StreamFile } from "./helpers"

// Walk a stream with the spec's operand table; every op must be known and complete.
const walk = (cmd: number[]): number[] => {
  const ops: number[] = []
  let i = 0
  while (i < cmd.length) {
    const op = cmd[i++]
    const layout = OP_ARGS[op]
    if (!layout) throw new Error(`unknown op ${op} at ${i - 1}`)
    const fixed = layout.args.length
    if (i + fixed > cmd.length) throw new Error(`op ${op} truncated`)
    let groups = 0
    if (layout.repeat) groups = cmd[i + fixed - 1] * layout.repeat.length
    i += fixed + groups
    if (i > cmd.length) throw new Error(`op ${op} repeat truncated`)
    ops.push(op)
  }
  return ops
}

describe("recorder", () => {
  test("the committed streams match a fresh recording (run `bun run record` after a scenario change)", () => {
    for (const [name, scenario] of Object.entries(scenarios)) {
      const path = join(goldenDir, "streams", `${name}.json`)
      expect(existsSync(path)).toBe(true)
      const committed = JSON.parse(readFileSync(path, "utf8")) as StreamFile
      expect(recordScenario(new Recorder(), scenario)).toEqual(committed)
    }
  })

  test("every stream is well-formed against the spec's operand table", () => {
    for (const scenario of Object.values(scenarios)) walk(recordScenario(new Recorder(), scenario).cmd)
  })

  test("the all-ops scenario records every opcode", () => {
    const ops = new Set(walk(recordScenario(new Recorder(), scenarios["all-ops"]).cmd))
    const missing = Object.entries(OP).filter(([, id]) => !ops.has(id)).map(([name]) => name)
    expect(missing).toEqual([])
  })

  test("state reads back; refs are interned; a gradient serializes on assignment", () => {
    const c = new Recorder()
    c.fillStyle = "#abc"
    c.fillStyle = "#abc"
    c.font = "12px Inter"
    expect(c.fillStyle).toBe("#abc")
    expect(c.font).toBe("12px Inter")
    expect(c.toJSON().refs).toEqual(["#abc", "12px Inter"])
    const g = c.createLinearGradient(0, 0, 1, 0)
    g.addColorStop(0, "red")
    c.fillStyle = g
    g.addColorStop(1, "blue")   // after the assignment: not in the stream
    const { cmd, refs } = c.toJSON()
    const at = cmd.lastIndexOf(OP.FILL_GRADIENT)
    expect(cmd.slice(at)).toEqual([OP.FILL_GRADIENT, 0, 0, 0, 1, 0, 0, 0, 1, 0, refs.indexOf("red")])
    expect(c.fillStyle).toBeInstanceOf(Gradient)
    expect(() => g.addColorStop(2, "x")).toThrow(RangeError)
    c.setLineDash([1, 2])
    expect(c.getLineDash()).toEqual([1, 2])
    c.reset()
    expect(c.length).toBe(0)
    expect(c.fillStyle as unknown).toEqual("#000000")
  })

  test("drawImage forms", () => {
    const c = new Recorder()
    const img = { surface: 3, width: 10, height: 20 }
    c.drawImage(img, 1, 2)
    c.drawImage(img, 1, 2, 30, 40)
    c.drawImage(img, 0, 0, 5, 5, 1, 2, 30, 40)
    expect(c.toJSON().cmd).toEqual([
      OP.DRAW_IMAGE, 3, 0, 0, 10, 20, 1, 2, 10, 20,
      OP.DRAW_IMAGE, 3, 0, 0, 10, 20, 1, 2, 30, 40,
      OP.DRAW_IMAGE, 3, 0, 0, 5, 5, 1, 2, 30, 40,
    ])
  })
})
