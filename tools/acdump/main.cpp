// acdump — draw lists and PNGs from the command line.
//
//   acdump stream <file.json> [--scale s]                 the draw list (JSON) of an opcode stream file
//   acdump svg <file.svg> [--fit w h] [--tint #rrggbbaa]  the draw list (JSON) of an SVG document
//   acdump png <file.svg> <out.png> [--scale s]           a CPU reference raster of an SVG (nanosvgrast)
//   acdump bin <file.json|file.svg> <out.bin> [...]       the draw list as the binary the painters read
//
// The binary form: u32 wordCount, f32 words[wordCount], u32 stringCount, (u32 byteLen, utf8 bytes)*.
#include "anycanvas/anycanvas.h"

#include "css.h"
#include "drawlist.h"
#include "interpreter.h"
#include "svg.h"
#include "common/stream.h"
#include "raster.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

using namespace anycanvas;

namespace {

int usage() {
  std::fprintf(stderr,
    "usage:\n"
    "  acdump stream <file.json> [--scale s]\n"
    "  acdump svg <file.svg> [--fit w h] [--tint #rrggbbaa]\n"
    "  acdump png <file.svg> <out.png> [--scale s]\n"
    "  acdump bin <file.json|file.svg> <out.bin> [--scale s] [--fit w h] [--tint #rrggbbaa]\n");
  return 2;
}

struct Options {
  float scale = 0;      // 0 = the stream's own
  float fitW = 0, fitH = 0;
  bool hasTint = false;
  Color tint;
};

bool parseOptions(int argc, char** argv, int from, Options& o) {
  for (int i = from; i < argc; i++) {
    const std::string a = argv[i];
    if (a == "--scale" && i + 1 < argc) o.scale = (float)std::atof(argv[++i]);
    else if (a == "--fit" && i + 2 < argc) { o.fitW = (float)std::atof(argv[++i]); o.fitH = (float)std::atof(argv[++i]); }
    else if (a == "--tint" && i + 1 < argc) { if (!parseCssColor(argv[++i], o.tint)) { std::fprintf(stderr, "bad tint\n"); return false; } o.hasTint = true; }
    else { std::fprintf(stderr, "unknown option %s\n", a.c_str()); return false; }
  }
  return true;
}

bool endsWith(const std::string& s, const char* suffix) {
  const size_t n = std::strlen(suffix);
  return s.size() >= n && s.compare(s.size() - n, n, suffix) == 0;
}

// Builds the draw list of a stream file or an SVG file into `list`.
bool build(const std::string& path, const Options& o, DrawList& list) {
  if (endsWith(path, ".svg")) {
    std::string text;
    if (!acstream::readFile(path, text)) { std::fprintf(stderr, "cannot read %s\n", path.c_str()); return false; }
    Svg* svg = parseSvg(text.data(), text.size());
    if (!svg) { std::fprintf(stderr, "not an SVG: %s\n", path.c_str()); return false; }
    list.clear();
    drawSvg(list, *svg, o.fitW, o.fitH, o.hasTint ? &o.tint : nullptr);
    delete svg;
    return true;
  }
  acstream::Stream st;
  std::string err;
  if (!acstream::load(path, st, &err)) { std::fprintf(stderr, "%s: %s\n", path.c_str(), err.c_str()); return false; }
  std::vector<const char*> refs;
  for (const auto& r : st.refs) refs.push_back(r.c_str());
  interpret(st.cmd.data(), (int32_t)st.cmd.size(), refs.data(), (int32_t)refs.size(), o.scale > 0 ? o.scale : st.scale, list);
  return true;
}

bool writeBinary(const std::string& path, const DrawList& list) {
  std::ofstream out(path, std::ios::binary);
  if (!out) return false;
  auto u32 = [&](uint32_t v) { out.write((const char*)&v, 4); };
  u32((uint32_t)list.words.size());
  if (!list.words.empty()) out.write((const char*)list.words.data(), (std::streamsize)(list.words.size() * 4));
  u32((uint32_t)list.strings.size());
  for (const auto& s : list.strings) { u32((uint32_t)s.size()); out.write(s.data(), (std::streamsize)s.size()); }
  return (bool)out;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 3) return usage();
  const std::string mode = argv[1];
  const std::string path = argv[2];

  if (mode == "stream" || mode == "svg") {
    Options o;
    if (!parseOptions(argc, argv, 3, o)) return usage();
    DrawList list;
    if (!build(path, o, list)) return 1;
    std::fputs(list.toJson().c_str(), stdout);
    return 0;
  }
  if (mode == "bin") {
    if (argc < 4) return usage();
    Options o;
    if (!parseOptions(argc, argv, 4, o)) return usage();
    DrawList list;
    if (!build(path, o, list)) return 1;
    if (!writeBinary(argv[3], list)) { std::fprintf(stderr, "cannot write %s\n", argv[3]); return 1; }
    return 0;
  }
  if (mode == "png") {
    if (argc < 4) return usage();
    Options o;
    o.scale = 1;
    if (!parseOptions(argc, argv, 4, o)) return usage();
    std::string text;
    if (!acstream::readFile(path, text)) { std::fprintf(stderr, "cannot read %s\n", path.c_str()); return 1; }
    Svg* svg = parseSvg(text.data(), text.size());
    if (!svg) { std::fprintf(stderr, "not an SVG: %s\n", path.c_str()); return 1; }
    std::vector<uint8_t> rgba;
    int32_t w = 0, h = 0;
    const bool ok = rasterizeSvg(*svg, o.scale > 0 ? o.scale : 1, rgba, w, h);
    delete svg;
    if (!ok) { std::fprintf(stderr, "raster failed\n"); return 1; }
    size_t len = 0;
    uint8_t* png = ac_encode(rgba.data(), w, h, 0, 100, &len);
    if (!png) { std::fprintf(stderr, "encode failed\n"); return 1; }
    std::ofstream out(argv[3], std::ios::binary);
    out.write((const char*)png, (std::streamsize)len);
    ac_free(png);
    std::printf("%s: %dx%d, %zu bytes\n", argv[3], w, h, len);
    return out ? 0 : 1;
  }
  return usage();
}
