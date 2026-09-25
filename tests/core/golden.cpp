// The golden tests: every tests/golden/streams/*.json (an opcode stream) and tests/golden/svg/*.svg
// becomes a draw list whose JSON must equal tests/golden/expected/<name>.json byte for byte. The
// binary twin tests/golden/expected/<name>.bin is what the TS reader test decodes.
//
//   anycanvas-golden <tests/golden>            compare (ctest)
//   anycanvas-golden <tests/golden> --update   rewrite the expected files (review the diff!)
//
// SVG goldens render at the natural size unless a sidecar <name>.svg.json says {"fit": [w, h], "tint": "#..."}.
#include "css.h"
#include "drawlist.h"
#include "interpreter.h"
#include "svg.h"
#include "common/stream.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace anycanvas;

namespace {

bool writeText(const fs::path& p, const std::string& text) {
  std::ofstream out(p, std::ios::binary);
  out.write(text.data(), (std::streamsize)text.size());
  return (bool)out;
}

bool writeBinary(const fs::path& path, const DrawList& list) {
  std::ofstream out(path, std::ios::binary);
  if (!out) return false;
  auto u32 = [&](uint32_t v) { out.write((const char*)&v, 4); };
  u32((uint32_t)list.words.size());
  if (!list.words.empty()) out.write((const char*)list.words.data(), (std::streamsize)(list.words.size() * 4));
  u32((uint32_t)list.strings.size());
  for (const auto& s : list.strings) { u32((uint32_t)s.size()); out.write(s.data(), (std::streamsize)s.size()); }
  return (bool)out;
}

struct Case { std::string name; DrawList list; };

bool buildStream(const fs::path& file, DrawList& list) {
  acstream::Stream st;
  std::string err;
  if (!acstream::load(file.string(), st, &err)) { std::fprintf(stderr, "  %s: %s\n", file.string().c_str(), err.c_str()); return false; }
  std::vector<const char*> refs;
  for (const auto& r : st.refs) refs.push_back(r.c_str());
  interpret(st.cmd.data(), (int32_t)st.cmd.size(), refs.data(), (int32_t)refs.size(), st.scale, list);
  return true;
}

bool buildSvg(const fs::path& file, DrawList& list) {
  std::string text;
  if (!acstream::readFile(file.string(), text)) return false;
  float fitW = 0, fitH = 0;
  bool hasTint = false;
  Color tint;
  const fs::path sidecar = file.string() + ".json";
  if (fs::exists(sidecar)) {
    std::string side;
    acjson::Value v;
    if (acstream::readFile(sidecar.string(), side) && acjson::parse(side, v)) {
      if (const acjson::Value* fit = v.get("fit")) if (fit->isArray() && fit->arr.size() == 2) { fitW = (float)fit->arr[0].num; fitH = (float)fit->arr[1].num; }
      if (const acjson::Value* t = v.get("tint")) if (t->isString() && parseCssColor(t->str.c_str(), tint)) hasTint = true;
    }
  }
  Svg* svg = parseSvg(text.data(), text.size());
  list.clear();
  if (!svg) return true;   // a hostile input: an empty draw list is the expected outcome
  drawSvg(list, *svg, fitW, fitH, hasTint ? &tint : nullptr);
  delete svg;
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) { std::fprintf(stderr, "usage: anycanvas-golden <tests/golden> [--update]\n"); return 2; }
  const fs::path root = argv[1];
  const bool update = argc > 2 && std::string(argv[2]) == "--update";
  const fs::path expectedDir = root / "expected";
  fs::create_directories(expectedDir);

  std::vector<Case> cases;
  std::vector<fs::path> files;
  for (const auto& e : fs::directory_iterator(root / "streams")) if (e.path().extension() == ".json") files.push_back(e.path());
  std::sort(files.begin(), files.end());
  for (const auto& f : files) {
    Case c; c.name = "stream-" + f.stem().string();
    if (!buildStream(f, c.list)) return 1;
    cases.push_back(std::move(c));
  }
  files.clear();
  for (const auto& e : fs::directory_iterator(root / "svg")) if (e.path().extension() == ".svg") files.push_back(e.path());
  std::sort(files.begin(), files.end());
  for (const auto& f : files) {
    Case c; c.name = "svg-" + f.stem().string();
    if (!buildSvg(f, c.list)) return 1;
    cases.push_back(std::move(c));
  }

  int failed = 0;
  for (const auto& c : cases) {
    const std::string json = c.list.toJson();
    const fs::path jsonPath = expectedDir / (c.name + ".json");
    const fs::path binPath = expectedDir / (c.name + ".bin");
    if (update) {
      writeText(jsonPath, json);
      writeBinary(binPath, c.list);
      std::printf("  wrote %s\n", jsonPath.filename().string().c_str());
      continue;
    }
    std::string expected;
    if (!acstream::readFile(jsonPath.string(), expected)) { std::printf("  MISSING %s (run --update)\n", jsonPath.filename().string().c_str()); failed++; continue; }
    if (expected != json) {
      std::printf("  FAIL %s\n", c.name.c_str());
      // Show the first differing line.
      size_t i = 0;
      while (i < expected.size() && i < json.size() && expected[i] == json[i]) i++;
      const size_t ls = expected.rfind('\n', i) == std::string::npos ? 0 : expected.rfind('\n', i) + 1;
      std::printf("    expected: %s\n", expected.substr(ls, expected.find('\n', ls) - ls).c_str());
      const size_t la = json.rfind('\n', i) == std::string::npos ? 0 : json.rfind('\n', i) + 1;
      std::printf("    actual:   %s\n", json.substr(la, json.find('\n', la) - la).c_str());
      failed++;
      continue;
    }
    std::printf("  ok   %s\n", c.name.c_str());
  }
  if (update) { std::printf("updated %zu goldens\n", cases.size()); return 0; }
  std::printf("%zu goldens, %d failed\n", cases.size(), failed);
  return failed ? 1 : 0;
}
