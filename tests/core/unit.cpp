// Unit tests of the core's dangerous invariants: hostile streams never crash, the CSS parsers, the
// geometry, the interpreter's transform / path semantics, the reader round trip, the encoder.
#include "anycanvas/anycanvas.h"

#include "css.h"
#include "drawlist.h"
#include "encode.h"
#include "geometry.h"
#include "interpreter.h"
#include "svg.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using namespace anycanvas;

static int g_failed = 0, g_checks = 0;
#define CHECK(cond) do { g_checks++; if (!(cond)) { g_failed++; std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)
#define CHECK_NEAR(a, b, eps) CHECK(std::fabs((double)(a) - (double)(b)) <= (eps))

static DrawList run(const std::vector<float>& cmd, const std::vector<const char*>& refs = {}, float scale = 1) {
  DrawList l;
  interpret(cmd.data(), (int32_t)cmd.size(), refs.data(), (int32_t)refs.size(), scale, l);
  return l;
}

static int countCmd(const DrawList& l, Draw cmd) {
  std::vector<const char*> ptrs;
  for (const auto& s : l.strings) ptrs.push_back(s.c_str());
  DrawReader r(l.words.data(), (int32_t)l.words.size(), ptrs.data(), (int32_t)ptrs.size());
  int32_t c = 0, end = 0, n = 0;
  while (r.next(c, end)) { if (c == (int32_t)cmd) n++; r.seek(end); }
  return n;
}

static void testColors() {
  Color c;
  CHECK(parseCssColor("#f00", c) && c.r == 1 && c.g == 0 && c.b == 0 && c.a == 1);
  CHECK(parseCssColor("#ff000080", c) && c.r == 1 && std::fabs(c.a - 128 / 255.0f) < 1e-6f);
  CHECK(parseCssColor("rgb(255, 128, 0)", c) && c.r == 1 && std::fabs(c.g - 128 / 255.0f) < 1e-6f && c.b == 0);
  CHECK(parseCssColor("rgba(0 0 255 / 50%)", c) && c.b == 1 && std::fabs(c.a - 0.5f) < 1e-6f);
  CHECK(parseCssColor("hsl(120, 100%, 50%)", c) && c.r == 0 && c.g == 1 && c.b == 0);
  CHECK(parseCssColor("  SteelBlue ", c) && std::fabs(c.r - 0x46 / 255.0f) < 1e-6f);
  CHECK(parseCssColor("transparent", c) && c.a == 0);
  CHECK(!parseCssColor("#12345", c));
  CHECK(!parseCssColor("nonsense", c));
  CHECK(!parseCssColor("", c));
  CHECK(!parseCssColor("rgb(1,2)", c));
  // The wrapper over anycanvas/css_color.h (its own test: css_color.cpp, the bits: tests/golden/colors).
  CHECK(parseCssColor("clear", c) && c.r == 0 && c.g == 0 && c.b == 0 && c.a == 0);
  CHECK(parseCssColor("green", c) && c.r == 0 && std::fabs(c.g - 128 / 255.0f) < 1e-6f && c.a == 1);
  c = Color{ 0.5f, 0.5f, 0.5f, 0.5f };
  CHECK(!parseCssColor("rgb(nan, 0, 0)", c) && c.r == 0.5f && c.a == 0.5f);   // untouched
  CHECK(!parseCssColor(nullptr, c));
}

// The fill colors of an SVG's FILL_PATH commands, in order (straight RGBA, alpha = the color's alpha).
static std::vector<Color> svgFills(const char* markup) {
  std::vector<Color> out;
  Svg* svg = parseSvg(markup, std::strlen(markup));
  if (!svg) return out;
  DrawList l;
  drawSvg(l, *svg, 0, 0, nullptr);
  delete svg;
  std::vector<const char*> ptrs;
  for (const auto& s : l.strings) ptrs.push_back(s.c_str());
  DrawReader r(l.words.data(), (int32_t)l.words.size(), ptrs.data(), (int32_t)ptrs.size());
  int32_t cmd = 0, end = 0;
  while (r.next(cmd, end)) {
    if (cmd == (int32_t)Draw::FILL_PATH) {
      r.f();   // the rule
      const PaintData p = r.paint();
      out.push_back(p.kind == Paint::COLOR ? p.color : p.stops.empty() ? Color{} : p.stops[0].color);
    }
    r.seek(end);
  }
  return out;
}

static bool near(const Color& c, float r, float g, float b, float a) {
  return std::fabs(c.r - r) < 1e-6f && std::fabs(c.g - g) < 1e-6f && std::fabs(c.b - b) < 1e-6f && std::fabs(c.a - a) < 1e-6f;
}

// SVG colors go through the canvas parser (the nanosvg ANYCANVAS patch): CSS names in any case, hsl(),
// the color's own alpha (rgba, #rrggbbaa, transparent) times the *-opacity, and an invalid value
// ignores the declaration (the inherited paint stays) instead of painting gray.
static void testSvgColors() {
  const std::vector<Color> f = svgFills(
    "<svg xmlns='http://www.w3.org/2000/svg' width='10' height='10'>"
    "<defs><linearGradient id='g'><stop offset='0' stop-color='RebeccaPurple'/><stop offset='1'/></linearGradient></defs>"
    "<rect width='1' height='1' fill='transparent'/>"
    "<rect width='1' height='1' fill='rgba(255,0,0,.5)'/>"
    "<rect width='1' height='1' fill='hsl(120,100%,50%)'/>"
    "<rect width='1' height='1' fill='bogus'/>"
    "<g fill='#00f'><rect width='1' height='1' fill='currentColor'/></g>"
    "<rect width='1' height='1' fill='#ff000080' fill-opacity='0.5'/>"
    "<rect width='1' height='1' fill='Red' fill-opacity='0.5'/>"
    "<rect width='1' height='1' fill='url(#g)'/>"
    "</svg>");
  CHECK(f.size() == 8);
  if (f.size() != 8) return;
  CHECK(near(f[0], 0, 0, 0, 0));                        // transparent: alpha 0 (was gray)
  CHECK(near(f[1], 1, 0, 0, 128 / 255.0f));             // rgba(): its alpha
  CHECK(near(f[2], 0, 1, 0, 1));                        // hsl() (was gray)
  CHECK(near(f[3], 0, 0, 0, 1));                        // invalid: the default black stays (was gray)
  CHECK(near(f[4], 0, 0, 1, 1));                        // currentColor (unsupported): the group's fill stays
  CHECK(near(f[5], 1, 0, 0, 64 / 255.0f));              // 0x80 × 0.5, truncated like upstream
  CHECK(near(f[6], 1, 0, 0, 127 / 255.0f));             // opaque × 0.5 = upstream's (unsigned)(0.5 * 255)
  CHECK(near(f[7], 0x66 / 255.0f, 0x33 / 255.0f, 0x99 / 255.0f, 1));   // a stop color by a CSS 4 name
}

static void testFonts() {
  FontData f = parseCssFont("bold 24px Inter");
  CHECK(f.size == 24 && f.weight == 700 && !f.italic && f.family == "Inter");
  f = parseCssFont("italic 500 16px \"Open Sans\", sans-serif");
  CHECK(f.size == 16 && f.weight == 500 && f.italic && f.family == "Open Sans");
  f = parseCssFont("12pt serif");
  CHECK_NEAR(f.size, 16, 1e-4);
  CHECK(f.family == "serif");
  f = parseCssFont("10px sans-serif");
  CHECK(f.size == 10 && f.family == "sans-serif" && f.weight == 400);
  f = parseCssFont("Inter");
  CHECK(f.family == "Inter" && f.size == 10);
  f = parseCssFont("");
  CHECK(f.family == "sans-serif" && f.size == 10);
  f = parseCssFont("normal normal 700 13px/1.5 Roboto Mono, monospace");
  CHECK(f.size == 13 && f.weight == 700 && f.family == "Roboto Mono");
}

static void testMatrix() {
  Matrix t = Matrix::translation(10, 20).mul(Matrix::scaling(2, 3));
  float x, y;
  t.apply(1, 1, x, y);
  CHECK_NEAR(x, 12, 1e-6); CHECK_NEAR(y, 23, 1e-6);
  Matrix inv;
  CHECK(t.invert(inv));
  inv.apply(x, y, x, y);
  CHECK_NEAR(x, 1, 1e-5); CHECK_NEAR(y, 1, 1e-5);
  CHECK(!Matrix::scaling(0, 1).invert(inv));
}

static void testGeometry() {
  Path p;
  appendArc(p, false, 0, 0, 0, 0, 10, 0, 6.2831853f, false);   // a full circle: 4 cubics
  int cubics = 0;
  for (size_t i = 0; i < p.words.size();) {
    const int v = (int)p.words[i++];
    if (v == (int)PathVerb::CUBIC) cubics++;
    i += v == (int)PathVerb::MOVE || v == (int)PathVerb::LINE ? 2 : v == (int)PathVerb::QUAD ? 4 : v == (int)PathVerb::CUBIC ? 6 : 0;
  }
  CHECK(cubics == 4);
  CHECK_NEAR(p.curX, 10, 1e-4); CHECK_NEAR(p.curY, 0, 1e-4);

  // arcTo: from (0,0) via the corner (10,0) to (10,10) with r = 5 → a line to the tangent (5,0) and a quarter arc to (10,5).
  Path a;
  appendArcTo(a, true, 0, 0, 10, 0, 10, 10, 5);
  CHECK(a.words.size() >= 3 && (int)a.words[0] == (int)PathVerb::LINE);
  CHECK_NEAR(a.words[1], 5, 1e-4); CHECK_NEAR(a.words[2], 0, 1e-4);
  CHECK_NEAR(a.curX, 10, 1e-4); CHECK_NEAR(a.curY, 5, 1e-4);

  Path rr;
  appendRoundRect(rr, 0, 0, 100, 50, 1000);   // radius clamps to 25
  CHECK(!rr.empty());
  CHECK(rr.words.back() == (float)PathVerb::CLOSE);
}

static void testInterpreterBasics() {
  // fillRect at scale 2 → SET_TRANSFORM(2,0,0,2,0,0) + a FILL_PATH in user units.
  DrawList l = run({ (float)Op::FILL_STYLE, 0, (float)Op::FILL_RECT, 1, 2, 3, 4 }, { "#f00" }, 2);
  CHECK(countCmd(l, Draw::SET_TRANSFORM) == 1);
  CHECK(countCmd(l, Draw::FILL_PATH) == 1);
  std::string json = l.toJson();
  CHECK(json.find("\"matrix\":[2,0,0,2,0,0]") != std::string::npos);
  CHECK(json.find("\"color\":[1,0,0,1]") != std::string::npos);
  CHECK(json.find("[\"M\",1,2],[\"L\",4,2],[\"L\",4,6],[\"L\",1,6],[\"Z\"]") != std::string::npos);

  // An invalid color keeps the previous style.
  l = run({ (float)Op::FILL_STYLE, 0, (float)Op::FILL_STYLE, 1, (float)Op::FILL_RECT, 0, 0, 1, 1 }, { "#0f0", "bogus" });
  CHECK(l.toJson().find("\"color\":[0,1,0,1]") != std::string::npos);

  // globalAlpha lands on the paint.
  l = run({ (float)Op::GLOBAL_ALPHA, 0.5f, (float)Op::FILL_RECT, 0, 0, 1, 1 });
  CHECK(l.toJson().find("\"alpha\":0.5") != std::string::npos);

  // An empty path draws nothing; an empty text draws nothing.
  l = run({ (float)Op::PATH_BEGIN, (float)Op::FILL, 0, (float)Op::FILL_TEXT, 0, 1, 1, 0 }, { "" });
  CHECK(l.words.empty());
}

static void testInterpreterTransforms() {
  // save / translate / fillRect / restore / fillRect: two transforms, balanced saves.
  DrawList l = run({ (float)Op::SAVE, (float)Op::TRANSLATE, 10, 0, (float)Op::FILL_RECT, 0, 0, 1, 1, (float)Op::RESTORE, (float)Op::FILL_RECT, 0, 0, 1, 1 });
  CHECK(countCmd(l, Draw::SAVE) == 1 && countCmd(l, Draw::RESTORE) == 1);
  CHECK(countCmd(l, Draw::SET_TRANSFORM) == 1);   // identity after restore is what the painter already has
  std::string json = l.toJson();
  CHECK(json.find("\"matrix\":[1,0,0,1,10,0]") != std::string::npos);

  // A path built before a scale and filled after it: the fill carries the new transform and the path is mapped back.
  l = run({ (float)Op::PATH_BEGIN, (float)Op::RECT, 0, 0, 10, 10, (float)Op::SCALE, 2, 2, (float)Op::FILL, 0 });
  json = l.toJson();
  CHECK(json.find("\"matrix\":[2,0,0,2,0,0]") != std::string::npos);
  CHECK(json.find("[\"M\",0,0],[\"L\",5,0],[\"L\",5,5],[\"L\",0,5],[\"Z\"]") != std::string::npos);

  // A singular transform draws nothing (and does not crash).
  l = run({ (float)Op::SCALE, 0, 0, (float)Op::PATH_BEGIN, (float)Op::RECT, 0, 0, 10, 10, (float)Op::FILL, 0 });
  CHECK(countCmd(l, Draw::FILL_PATH) == 0);

  // Unbalanced saves are closed for the painter.
  l = run({ (float)Op::SAVE, (float)Op::SAVE, (float)Op::FILL_RECT, 0, 0, 1, 1 });
  CHECK(countCmd(l, Draw::SAVE) == 2 && countCmd(l, Draw::RESTORE) == 2);
  // Extra restores are ignored.
  l = run({ (float)Op::RESTORE, (float)Op::RESTORE, (float)Op::FILL_RECT, 0, 0, 1, 1 });
  CHECK(countCmd(l, Draw::RESTORE) == 0 && countCmd(l, Draw::FILL_PATH) == 1);
}

static void testInterpreterGradientsAndDash() {
  // A linear gradient with two stops; an odd dash list doubles.
  DrawList l = run({ (float)Op::FILL_GRADIENT, 0, 0, 0, 100, 0, 0, 0, 2, 0, 0, 1, 1,
                     (float)Op::LINE_DASH, 3, 1, 2, 3, (float)Op::FILL_RECT, 0, 0, 1, 1,
                     (float)Op::PATH_BEGIN, (float)Op::MOVE_TO, 0, 0, (float)Op::LINE_TO, 5, 5, (float)Op::STROKE },
                   { "#000", "#fff" });
  std::string json = l.toJson();
  CHECK(json.find("\"kind\":\"linear\"") != std::string::npos);
  CHECK(json.find("\"matrix\":[100,0,0,100,0,0]") != std::string::npos);   // (0,0)→(100,0): a = dx, d = dx
  CHECK(json.find("\"stops\":[{\"offset\":0,\"color\":[0,0,0,1]},{\"offset\":1,\"color\":[1,1,1,1]}]") != std::string::npos);
  CHECK(json.find("\"dash\":[1,2,3,1,2,3]") != std::string::npos);

  // Transparent stops take their neighbour's RGB (premultiplied-equivalent interpolation everywhere);
  // one between two different colors splits into two coincident stops.
  l = run({ (float)Op::FILL_GRADIENT, 0, 0, 0, 1, 0, 0, 0, 2, 0, 0, 1, 1, (float)Op::FILL_RECT, 0, 0, 1, 1 }, { "yellow", "transparent" });
  CHECK(l.toJson().find("\"stops\":[{\"offset\":0,\"color\":[1,1,0,1]},{\"offset\":1,\"color\":[1,1,0,0]}]") != std::string::npos);
  l = run({ (float)Op::FILL_GRADIENT, 0, 0, 0, 1, 0, 0, 0, 3, 0, 0, 0.5f, 1, 1, 2, (float)Op::FILL_RECT, 0, 0, 1, 1 }, { "red", "transparent", "blue" });
  CHECK(l.toJson().find("\"stops\":[{\"offset\":0,\"color\":[1,0,0,1]},{\"offset\":0.5,\"color\":[1,0,0,0]},{\"offset\":0.5,\"color\":[0,0,1,0]},{\"offset\":1,\"color\":[0,0,1,1]}]") != std::string::npos);

  // One stop collapses to a color; unsorted stops sort.
  l = run({ (float)Op::FILL_GRADIENT, 0, 0, 0, 1, 0, 0, 0, 1, 0.5f, 0, (float)Op::FILL_RECT, 0, 0, 1, 1 }, { "#00f" });
  CHECK(l.toJson().find("\"kind\":\"color\",\"alpha\":1,\"color\":[0,0,1,1]") != std::string::npos);
  l = run({ (float)Op::FILL_GRADIENT, 1, 5, 5, 5, 5, 0, 10, 2, 1, 0, 0, 1, (float)Op::FILL_RECT, 0, 0, 1, 1 }, { "#000", "#fff" });
  json = l.toJson();
  CHECK(json.find("\"kind\":\"radial\"") != std::string::npos);
  CHECK(json.find("\"stops\":[{\"offset\":0,\"color\":[1,1,1,1]},{\"offset\":1,\"color\":[0,0,0,1]}]") != std::string::npos);
}

static void testHostileStreams() {
  // Truncated ops, absurd counts, unknown ops, garbage: never a crash, never a read past the end.
  const std::vector<std::vector<float>> bad = {
    { (float)Op::FILL_RECT, 1, 2 },
    { (float)Op::LINE_DASH, 1e9f },
    { (float)Op::LINE_DASH, -5 },
    { (float)Op::FILL_GRADIENT, 0, 0, 0, 1, 1, 0, 0, 1e9f },
    { 999, 1, 2, 3 },
    { (float)Op::FILL_STYLE, 1e9f, (float)Op::FILL_RECT, 0, 0, 1, 1 },
    { (float)Op::FILL_STYLE, -1, (float)Op::FILL_TEXT, 7, 0, 0, 0 },
    { NAN, NAN, NAN },
    { (float)Op::SCALE, INFINITY, INFINITY, (float)Op::FILL_RECT, 0, 0, 1, 1 },
    { (float)Op::DRAW_IMAGE, 1, 0, 0, 0, 0, 0, 0, 0, 0 },
  };
  for (const auto& b : bad) { DrawList l = run(b, { "#fff" }); (void)l; }
  CHECK(true);
  // A stream that stops at an unknown op keeps what came before.
  DrawList l = run({ (float)Op::FILL_RECT, 0, 0, 1, 1, 999, (float)Op::FILL_RECT, 0, 0, 1, 1 });
  CHECK(countCmd(l, Draw::FILL_PATH) == 1);
}

static void testReaderRoundTrip() {
  DrawList l;
  PaintData p = PaintData::radial(1, 2, 0.5f, 3, 4, 10);
  p.stops = { { 0, { 1, 0, 0, 1 } }, { 1, { 0, 0, 1, 0.5f } } };
  p.normalize();
  StrokeData s; s.width = 3; s.dash = { 4, 2 }; s.cap = LineCap::ROUND;
  FontData f; f.family = "Inter"; f.size = 24; f.weight = 700; f.italic = true;
  Path path; path.moveTo(0, 0); path.quadTo(1, 1, 2, 0); path.close();
  l.strokeText("héllo \"q\"", 5, 6, 0, f, TextAlign::CENTER, TextBaseline::MIDDLE, 1.5f, s, p);
  l.drawImage(7, 0, 0, 10, 10, 1, 1, 20, 20, 0.75f);
  std::vector<const char*> ptrs;
  for (const auto& str : l.strings) ptrs.push_back(str.c_str());
  DrawReader r(l.words.data(), (int32_t)l.words.size(), ptrs.data(), (int32_t)ptrs.size());
  int32_t cmd = 0, end = 0;
  CHECK(r.next(cmd, end) && cmd == (int32_t)Draw::STROKE_TEXT);
  CHECK(std::string(r.str()) == "héllo \"q\"");
  CHECK(r.f() == 5 && r.f() == 6 && r.f() == 0);
  FontData f2 = r.font();
  CHECK(f2.family == "Inter" && f2.size == 24 && f2.weight == 700 && f2.italic);
  CHECK(r.i() == (int32_t)TextAlign::CENTER && r.i() == (int32_t)TextBaseline::MIDDLE && r.f() == 1.5f);
  StrokeData s2 = r.stroke();
  CHECK(s2.width == 3 && s2.dash.size() == 2 && s2.cap == LineCap::ROUND);
  PaintData p2 = r.paint();
  CHECK(p2.kind == Paint::RADIAL && p2.stops.size() == 2 && p2.stops[1].color.a == 0.5f);
  CHECK_NEAR(p2.fx, -0.2f, 1e-6); CHECK_NEAR(p2.r0, 0.05f, 1e-6);
  CHECK(r.pos() == end && r.ok());
  r.seek(end);
  CHECK(r.next(cmd, end) && cmd == (int32_t)Draw::DRAW_IMAGE);
  CHECK(r.i() == 7);
  r.seek(end);
  CHECK(!r.next(cmd, end));
  // The JSON of the same list parses as the same values (spot check).
  const std::string json = l.toJson();
  CHECK(json.find("\"text\":\"héllo \\\"q\\\"\"") != std::string::npos);
  CHECK(json.find("\"fx\":-0.2,\"fy\":-0.2,\"r0\":0.05") != std::string::npos);
}

static void testSvg() {
  const char* markup =
    "<svg xmlns='http://www.w3.org/2000/svg' width='100' height='50' viewBox='0 0 100 50'>"
    "<defs><linearGradient id='g' x1='0' y1='0' x2='100' y2='0' gradientUnits='userSpaceOnUse'>"
    "<stop offset='0' stop-color='#f00'/><stop offset='1' stop-color='#00f'/></linearGradient></defs>"
    "<rect x='10' y='10' width='80' height='30' fill='url(#g)' stroke='black' stroke-width='2' stroke-dasharray='4 2' opacity='0.5'/>"
    "<circle cx='50' cy='25' r='5' fill='none'/>"
    "</svg>";
  Svg* svg = parseSvg(markup, std::strlen(markup));
  CHECK(svg != nullptr);
  if (!svg) return;
  float w, h;
  svgSize(*svg, w, h);
  CHECK(w == 100 && h == 50);
  DrawList l;
  drawSvg(l, *svg, 0, 0, nullptr);
  CHECK(countCmd(l, Draw::FILL_PATH) == 1);     // the fill-less circle draws nothing
  CHECK(countCmd(l, Draw::STROKE_PATH) == 1);
  CHECK(countCmd(l, Draw::SET_TRANSFORM) == 1);
  std::string json = l.toJson();
  CHECK(json.find("\"kind\":\"linear\",\"alpha\":0.5") != std::string::npos);
  // The gradient axis: (0,0)→(100,0) in user space, i.e. matrix a = 100, b = 0 (nanosvg stores the
  // inverse; the perpendicular is canonicalized to a similarity).
  CHECK(json.find("\"matrix\":[100,0,0,100,0,0]") != std::string::npos);
  CHECK(json.find("\"dash\":[4,2]") != std::string::npos);
  // Fit into 200×200: scale 2, centered vertically (y offset 50).
  DrawList fit;
  drawSvg(fit, *svg, 200, 200, nullptr);
  CHECK(fit.toJson().find("\"matrix\":[2,0,0,2,0,50]") != std::string::npos);
  // Tint replaces every paint, opacity stays.
  Color tint{ 0, 1, 0, 1 };
  DrawList tinted;
  drawSvg(tinted, *svg, 0, 0, &tint);
  json = tinted.toJson();
  CHECK(json.find("\"kind\":\"color\",\"alpha\":0.5,\"color\":[0,1,0,1]") != std::string::npos);
  CHECK(json.find("\"kind\":\"linear\"") == std::string::npos);
  delete svg;

  // Hostile inputs.
  CHECK(parseSvg("", 0) == nullptr);
  CHECK(parseSvg("<svg", 4) == nullptr || true);
  Svg* bad = parseSvg("<svg xmlns='http://www.w3.org/2000/svg'><rect width='1", 55);
  delete bad;
  Svg* sizeless = parseSvg("<svg xmlns='http://www.w3.org/2000/svg'></svg>", 46);
  CHECK(sizeless == nullptr);
  delete sizeless;
  CHECK(looksLikeSvg("  <?xml version='1.0'?><svg>", 28));
  CHECK(!looksLikeSvg("\x89PNG\r\n", 6));
}

static void testSvgText() {
  const char* markup =
    "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 100 100' width='200' height='200'>"
    "<text x='10' y='20' font-family='\"Open Sans\", serif' font-size='12' font-weight='bold' text-anchor='middle' fill='#00f' letter-spacing='1'>"
    "  Hi &amp; &#x41;<tspan> there</tspan></text>"
    "<text x='5' y='50' font-style='italic' dominant-baseline='hanging' fill='none' stroke='red' stroke-width='2'>a<tspan x='5' y='70'>b</tspan></text>"
    "<g transform='rotate(90)'><text font-size='8' fill='#000'>r</text></g>"
    "<text x='1' y='1' fill='none'>none</text>"
    "</svg>";
  Svg* svg = parseSvg(markup, std::strlen(markup));
  CHECK(svg != nullptr);
  if (!svg) return;
  DrawList l;
  drawSvg(l, *svg, 0, 0, nullptr);
  CHECK(countCmd(l, Draw::FILL_TEXT) == 2);     // "Hi & A there" + "r"; the none-filled text draws nothing
  CHECK(countCmd(l, Draw::STROKE_TEXT) == 2);   // "a" and its positioned tspan "b"
  const std::string json = l.toJson();
  // Entities decoded, whitespace collapsed, the unpositioned tspan merged; the viewBox scale (2) sits in
  // the transform, so the font size and the origin stay in the run's units.
  CHECK(json.find("\"text\":\"Hi & A there\",\"x\":10,\"y\":20") != std::string::npos);
  CHECK(json.find("\"font\":{\"family\":\"Open Sans\",\"size\":12,\"weight\":700,\"italic\":false},\"align\":\"center\",\"baseline\":\"alphabetic\",\"letterSpacing\":1") != std::string::npos);
  CHECK(json.find("\"matrix\":[2,0,0,2,0,0]") != std::string::npos);
  CHECK(json.find("\"text\":\"b\",\"x\":5,\"y\":70") != std::string::npos);
  CHECK(json.find("\"italic\":true},\"align\":\"start\",\"baseline\":\"hanging\"") != std::string::npos);
  CHECK(json.find("\"stroke\":{\"width\":2,") != std::string::npos);
  CHECK(json.find(",2,-2,") != std::string::npos && json.find("\"text\":\"r\"") != std::string::npos);   // rotate(90) under the viewBox scale (cos 90° is float noise)
  CHECK(json.find("\"text\":\"none\"") == std::string::npos);
  delete svg;
  // A truncated <text> and stray </tspan> never crash.
  const char* truncated = "<svg xmlns='http://www.w3.org/2000/svg' width='10' height='10'><text x='1'>abc<tspan x='2'>de";
  Svg* bad = parseSvg(truncated, std::strlen(truncated));
  delete bad;
  const char* strayMarkup = "<svg xmlns='http://www.w3.org/2000/svg' width='10' height='10'></tspan><tspan>x</tspan></svg>";
  Svg* stray = parseSvg(strayMarkup, std::strlen(strayMarkup));
  delete stray;
  CHECK(true);
}

static void testEncodeAndApi() {
  const uint8_t px[16] = { 255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 255, 128 };
  size_t len = 0;
  uint8_t* png = encodeImage(px, 2, 2, ImageFormat::PNG, 100, &len);
  CHECK(png && len > 8 && png[1] == 'P' && png[2] == 'N' && png[3] == 'G');
  std::free(png);
  uint8_t* jpg = encodeImage(px, 2, 2, ImageFormat::JPEG, 80, &len);
  CHECK(jpg && len > 2 && jpg[0] == 0xFF && jpg[1] == 0xD8);
  std::free(jpg);
  CHECK(encodeImage(nullptr, 2, 2, ImageFormat::PNG, 100, &len) == nullptr && len == 0);

  ac_context* ctx = ac_context_create();
  int32_t pw = 0, ph = 0; int resized = 0;
  const int32_t id = ac_surface_prepare(ctx, 0, 100, 50, 2, &pw, &ph, &resized);
  CHECK(id == 1 && pw == 200 && ph == 100 && resized == 1);
  CHECK(ac_surface_prepare(ctx, id, 100, 50, 2, &pw, &ph, &resized) == id && resized == 0);
  CHECK(ac_surface_prepare(ctx, id, 10, 50, 2, &pw, &ph, &resized) == id && resized == 1 && pw == 20);
  CHECK(ac_surface_prepare(ctx, 99, 1, 1, 1, &pw, &ph, &resized) == 2);   // an unknown id creates
  float scale = 0;
  CHECK(ac_surface_info(ctx, id, &pw, &ph, &scale) == 1 && scale == 2);
  ac_surface_remove(ctx, id);
  CHECK(ac_surface_info(ctx, id, &pw, &ph, &scale) == 0);
  const float cmd[] = { (float)Op::FILL_RECT, 0, 0, 1, 1 };
  ac_drawlist list;
  CHECK(ac_interpret(ctx, cmd, 5, nullptr, 0, 1, &list) == 1 && list.wordCount > 0);
  char* json = ac_drawlist_json(&list);
  CHECK(json && std::strstr(json, "fillPath"));
  ac_free(json);
  ac_font font;
  ac_font_parse("italic 700 18px \"Roboto Mono\", monospace", &font);
  CHECK(std::string(font.family) == "Roboto Mono" && font.size == 18 && font.weight == 700 && font.italic == 1);
  ac_context_destroy(ctx);
}

int main() {
  testColors();
  testFonts();
  testMatrix();
  testGeometry();
  testInterpreterBasics();
  testInterpreterTransforms();
  testInterpreterGradientsAndDash();
  testHostileStreams();
  testReaderRoundTrip();
  testSvg();
  testSvgColors();
  testSvgText();
  testEncodeAndApi();
  std::printf("%d checks, %d failed\n", g_checks, g_failed);
  return g_failed ? 1 : 0;
}
