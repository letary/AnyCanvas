// The direct web painter (painters/web/src/replay.ts): an opcode stream on a Canvas2D context with
// no core between them. Held here: every op of the spec is a call of the context with its operands
// in the context's order, a stream that is cut or newer ends the drawing without a throw, and the
// context is given back as it was taken.
import { describe, expect, test } from "bun:test"
import { readFileSync } from "node:fs"
import { join } from "node:path"
import { createCanvas } from "@napi-rs/canvas"
import { OP, OP_ARGS, OP_NAME } from "anycanvas-spec"
import { Recorder } from "anycanvas-recorder"
import { replay, type Canvas2DDirect } from "anycanvas-web"
import { goldenDir, type StreamFile } from "./helpers"

/** A context that writes down what it is told. */
const spy = () => {
  const calls: [string, ...unknown[]][] = []
  const gradient = (name: string, args: unknown[]) => {
    const stops: unknown[] = []
    calls.push([name, ...args, stops])
    return { addColorStop: (offset: number, color: string) => { stops.push([offset, color]) } }
  }
  const ctx = new Proxy({} as Record<string, unknown>, {
    get: (_t, name: string) => {
      if (name === "createLinearGradient" || name === "createRadialGradient") return (...args: unknown[]) => gradient(name, args)
      return (...args: unknown[]) => { calls.push([name, ...args]) }
    },
    set: (_t, name: string, value) => { calls.push([`${name}=`, value]); return true },
    has: () => true,
  }) as unknown as Canvas2DDirect
  return { ctx, calls, names: () => calls.map((c) => c[0]) }
}

const record = (draw: (c: Recorder) => void) => { const c = new Recorder(); draw(c); return c.stream() }

describe("the direct painter", () => {
  test("an op is a call of the context, its operands in the context's order", () => {
    const { ctx, calls } = spy()
    const s = record((c) => {
      c.save().translate(10, 20).rotate(0.5)
      c.fillStyle = "#ff0000"
      c.lineWidth = 3
      c.lineJoin = "round"
      c.setLineDash([4, 2])
      c.font = "bold 16px Inter"
      c.textAlign = "center"
      c.beginPath().arc(5, 6, 7, 0, 1, true).ellipse(1, 2, 3, 4, 0.1, 0, 2, false).closePath().fill("evenodd").stroke()
      c.fillText("hi", 1, 2).fillText("wide", 3, 4, 50)
      c.restore()
    })
    expect(replay(ctx, s.cmd, s.refs)).toBe(18)
    expect(calls.slice(2, -1)).toEqual([
      ["save"], ["translate", 10, 20], ["rotate", 0.5],
      ["fillStyle=", "#ff0000"], ["lineWidth=", 3], ["lineJoin=", "round"], ["setLineDash", [4, 2]],
      ["font=", "bold 16px Inter"], ["textAlign=", "center"],
      ["beginPath"], ["arc", 5, 6, 7, 0, 1, true], ["ellipse", 1, 2, 3, 4, expect.closeTo(0.1, 5), 0, 2, false], ["closePath"],
      ["fill", "evenodd"], ["stroke"],
      ["fillText", "hi", 1, 2], ["fillText", "wide", 3, 4, 50],
      ["restore"],
    ])
  })

  test("a gradient is made with its stops; a radial one in the context's order", () => {
    const { ctx, calls } = spy()
    const s = record((c) => {
      c.fillStyle = c.createLinearGradient(0, 0, 10, 0).addColorStop(0, "#000").addColorStop(1, "clear")
      c.strokeStyle = c.createRadialGradient(1, 2, 3, 4, 5, 6).addColorStop(0.5, "red")
    })
    replay(ctx, s.cmd, s.refs)
    expect(calls.filter((c) => String(c[0]).startsWith("create"))).toEqual([
      ["createLinearGradient", 0, 0, 10, 0, [[0, "#000"], [1, "transparent"]]],
      ["createRadialGradient", 1, 2, 3, 4, 5, 6, [[0.5, "red"]]],
    ])
  })

  test("the stream's transform is over the base: device px per unit", () => {
    const { ctx, calls } = spy()
    const s = record((c) => { c.setTransform(1, 0, 0, 1, 5, 5).resetTransform() })
    replay(ctx, s.cmd, s.refs, { base: [2, 0, 0, 2, 0, 0] })
    expect(calls).toEqual([
      ["save"], ["setTransform", 2, 0, 0, 2, 0, 0],
      ["setTransform", 2, 0, 0, 2, 0, 0], ["transform", 1, 0, 0, 1, 5, 5],
      ["setTransform", 2, 0, 0, 2, 0, 0],
      ["restore"],
    ])
  })

  test("an image is the host's: a surface it does not know is skipped", () => {
    const { ctx, calls } = spy()
    const s = record((c) => { c.drawImage({ surface: 7, width: 8, height: 4 }, 1, 2).drawImage({ surface: 9, width: 8, height: 4 }, 0, 0) })
    replay(ctx, s.cmd, s.refs, { image: (id) => (id === 7 ? "seven" : null) })
    expect(calls.filter((c) => c[0] === "drawImage")).toEqual([["drawImage", "seven", 0, 0, 8, 4, 1, 2, 8, 4]])
  })

  test("the context is given back as it was taken: a save with no restore is restored", () => {
    const { ctx, names } = spy()
    const s = record((c) => { c.save().save().restore().restore().restore() })
    replay(ctx, s.cmd, s.refs)
    // (the third restore has nothing to pop: it is not the context's to take)
    expect(names()).toEqual(["save", "setTransform", "save", "save", "restore", "restore", "restore"])
    const open = spy()
    const t = record((c) => { c.save().save() })
    replay(open.ctx, t.cmd, t.refs)
    expect(open.names()).toEqual(["save", "setTransform", "save", "save", "restore", "restore", "restore"])
  })

  test("a stream that is cut, or newer than the spec, is a shorter drawing", () => {
    const s = record((c) => { c.fillRect(0, 0, 1, 1).fillRect(1, 1, 2, 2) })
    const cut = spy()
    expect(replay(cut.ctx, s.cmd.slice(0, s.cmd.length - 2), s.refs)).toBe(1)
    const newer = spy()
    expect(replay(newer.ctx, Float32Array.from([...s.cmd.slice(0, 5), 999, 1, 2, ...s.cmd.slice(5)]), s.refs)).toBe(1)
    const counted = spy()
    expect(replay(counted.ctx, Float32Array.from([OP.LINE_DASH, 5, 1, 2]), [])).toBe(0)
  })

  test("every op of the spec is drawn", () => {
    const file = JSON.parse(readFileSync(join(goldenDir, "streams", "all-ops.json"), "utf8")) as StreamFile
    const seen = new Set<number>()
    const cmd = Float32Array.from(file.cmd)
    const { ctx } = spy()
    // the stream is walked whole: the count of its ops is the count the painter drew
    let ops = 0
    for (let at = 0; at < cmd.length; ops++) {
      const op = cmd[at]!
      seen.add(op)
      const layout = OP_ARGS[op]!
      at += 1 + layout.args.length + (layout.repeat ? cmd[at + layout.args.length]! * layout.repeat.length : 0)
    }
    expect(replay(ctx, cmd, file.refs, { image: () => "image" })).toBe(ops)
    expect(OP_NAME.filter((_, id) => !seen.has(id))).toEqual([])
  })

  test("it paints on a real canvas", () => {
    const canvas = createCanvas(20, 20)
    const s = record((c) => { c.fillStyle = "#ff0000"; c.fillRect(0, 0, 10, 10) })
    replay(canvas.getContext("2d") as unknown as Canvas2DDirect, s.cmd, s.refs, { base: [2, 0, 0, 2, 0, 0] })
    const px = canvas.getContext("2d").getImageData(0, 0, 20, 20).data
    expect([...px.slice(0, 4)]).toEqual([255, 0, 0, 255])
    expect([...px.slice((19 * 20 + 19) * 4, (19 * 20 + 19) * 4 + 4)]).toEqual([255, 0, 0, 255])
  })
})
