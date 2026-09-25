// The draw list: the value types (Matrix, Color, Paint, Stroke, Font, Path), the builder that writes
// the word buffer of spec/draw.h, and the reader that walks it back (the JSON dump, the C++ painters).
#pragma once

#include "spec.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace anycanvas {

// A 2D affine transform, Canvas2D / SVG layout: x' = a*x + c*y + e, y' = b*x + d*y + f.
struct Matrix {
  float a = 1, b = 0, c = 0, d = 1, e = 0, f = 0;

  static Matrix translation(float x, float y) { Matrix m; m.e = x; m.f = y; return m; }
  static Matrix scaling(float sx, float sy) { Matrix m; m.a = sx; m.d = sy; return m; }
  static Matrix rotation(float rad);

  // this ∘ m: apply m first, then this — Canvas2D `transform(m)` is CTM = CTM.mul(m).
  Matrix mul(const Matrix& m) const;
  bool invert(Matrix& out) const;
  void apply(float x, float y, float& ox, float& oy) const { ox = a * x + c * y + e; oy = b * x + d * y + f; }
  bool operator==(const Matrix& o) const { return a == o.a && b == o.b && c == o.c && d == o.d && e == o.e && f == o.f; }
  bool operator!=(const Matrix& o) const { return !(*this == o); }
  bool isIdentity() const { return *this == Matrix(); }
};

struct Color {
  float r = 0, g = 0, b = 0, a = 1;
  static Color rgba8(uint32_t r, uint32_t g, uint32_t b, uint32_t a) { return { r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f }; }
  bool operator==(const Color& o) const { return r == o.r && g == o.g && b == o.b && a == o.a; }
};

struct Stop { float offset = 0; Color color; };

struct PaintData {
  Paint kind = Paint::COLOR;
  float alpha = 1;                 // multiplies the whole paint (globalAlpha, an SVG shape's opacity)
  Color color;                     // COLOR
  Matrix m;                        // LINEAR / RADIAL: gradient space → user space
  float fx = 0, fy = 0, r0 = 0;    // RADIAL: the start circle in gradient space (the end circle is the unit circle)
  Spread spread = Spread::PAD;
  std::vector<Stop> stops;

  static PaintData solid(Color c) { PaintData p; p.color = c; return p; }
  // A linear gradient from (x0,y0) to (x1,y1) in user space (Canvas2D createLinearGradient).
  static PaintData linear(float x0, float y0, float x1, float y1);
  // A two-circle radial gradient (Canvas2D createRadialGradient(x0,y0,r0,x1,y1,r1)).
  static PaintData radial(float x0, float y0, float r0, float x1, float y1, float r1);
  // Painters get clean stops: sorted, clamped to 0..1, at least two. One stop collapses to a COLOR
  // paint, none to transparent black (what Canvas2D draws for an empty gradient).
  void normalize();
};

struct StrokeData {
  float width = 1;
  LineJoin join = LineJoin::MITER;
  LineCap cap = LineCap::BUTT;
  float miterLimit = 10;
  float dashOffset = 0;
  std::vector<float> dash;   // normalized: empty = solid, an odd count is doubled (Canvas2D)
};

struct FontData {
  std::string family = "sans-serif";
  float size = 10;
  int32_t weight = 400;
  bool italic = false;
};

// A path as the draw list carries it: verb words + coordinates (see spec/draw.h "path").
struct Path {
  std::vector<float> words;
  bool hasCurrent = false;
  float curX = 0, curY = 0;      // the current point (the last point added, or the subpath start after close)
  float startX = 0, startY = 0;  // the current subpath's start

  bool empty() const { return words.empty(); }
  void moveTo(float x, float y);
  void lineTo(float x, float y);
  void quadTo(float cx, float cy, float x, float y);
  void cubicTo(float c1x, float c1y, float c2x, float c2y, float x, float y);
  void close();
  // Append another path's verbs; with m, every point goes through m first.
  void append(const Path& other, const Matrix* m);
  Path transformed(const Matrix& m) const { Path p; p.append(*this, &m); return p; }
  void clear() { words.clear(); hasCurrent = false; }
};

// The writer.
class DrawList {
 public:
  std::vector<float> words;
  std::vector<std::string> strings;

  void clear();
  int32_t intern(const std::string& s);

  void setTransform(const Matrix& m);
  void save();
  void restore();
  void clip(FillRule rule, const Path& path);
  void fillPath(FillRule rule, const PaintData& paint, const Path& path);
  void strokePath(const StrokeData& stroke, const PaintData& paint, const Path& path);
  void fillText(const std::string& text, float x, float y, float maxWidth, const FontData& font,
                TextAlign align, TextBaseline baseline, float letterSpacing, const PaintData& paint);
  void strokeText(const std::string& text, float x, float y, float maxWidth, const FontData& font,
                  TextAlign align, TextBaseline baseline, float letterSpacing, const StrokeData& stroke, const PaintData& paint);
  void drawImage(int32_t surface, float sx, float sy, float sw, float sh, float dx, float dy, float dw, float dh, float alpha);
  void clearRect(float x, float y, float w, float h);

  // The deterministic JSON form (tests, debugging, hashing): one command per line.
  std::string toJson() const;

 private:
  std::unordered_map<std::string, int32_t> intern_;
  size_t begin(Draw cmd);
  void end(size_t lenAt);
  void put(float v);
  void putColor(const Color& c);
  void putPaint(const PaintData& p);
  void putStroke(const StrokeData& s);
  void putFont(const FontData& f);
  void putPath(const Path& p);
};

// The reader: walks a word buffer command by command. Painters in C++ use it; the JSON dump uses it.
class DrawReader {
 public:
  DrawReader(const float* words, int32_t count, const char* const* strings, int32_t stringCount)
      : w_(words), n_(count), s_(strings), ns_(stringCount) {}

  // The next command: false at the end (or on a truncated header). `end` is the index after its payload.
  bool next(int32_t& cmd, int32_t& end);
  int32_t pos() const { return pos_; }
  void seek(int32_t p) { pos_ = p; }
  bool ok() const { return ok_; }

  float f();
  int32_t i() { return (int32_t)f(); }
  const char* str();
  Color color();
  PaintData paint();
  StrokeData stroke();
  FontData font();
  Path path();
  Matrix matrix();

 private:
  const float* w_; int32_t n_; const char* const* s_; int32_t ns_;
  int32_t pos_ = 0;
  bool ok_ = true;
};

// Shortest float text that reads back to the same float32 (the JSON dump's number format).
std::string formatFloat(float v);

}  // namespace anycanvas
