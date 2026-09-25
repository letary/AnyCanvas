// Records every scenario into tests/golden/streams/<name>.json. Run after a scenario or recorder change,
// then `./build.ps1 -Update` (or build.sh --update) for the expected draw lists — and review the diff.
import { writeFileSync } from "node:fs"
import { join } from "node:path"
import { Recorder } from "anycanvas-recorder"
import { scenarios } from "./scenarios"
import { goldenDir, recordScenario } from "./helpers"

for (const [name, scenario] of Object.entries(scenarios)) {
  const text = JSON.stringify(recordScenario(new Recorder(), scenario))
  writeFileSync(join(goldenDir, "streams", `${name}.json`), text + "\n")
  console.log(`wrote tests/golden/streams/${name}.json`)
}
