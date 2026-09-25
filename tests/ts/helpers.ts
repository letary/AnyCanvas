import { dirname, join } from "node:path"
import { fileURLToPath } from "node:url"
import type { Recorder } from "anycanvas-recorder"
import type { Scenario } from "./scenarios"

export const repoRoot = join(dirname(fileURLToPath(import.meta.url)), "..", "..")
export const goldenDir = join(repoRoot, "tests", "golden")

export interface StreamFile { cmd: number[], refs: string[], scale?: number }

export const recordScenario = (c: Recorder, s: Scenario): StreamFile => {
  s.record(c)
  const json = c.toJSON()
  return s.scale !== undefined ? { ...json, scale: s.scale } : json
}
