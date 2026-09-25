#include "drawlist.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace anycanvas {

// ---- Matrix ---------------------------------------------------------------------------------------

Matrix Matrix::rotation(float rad) {
  Matrix m;
  const float c = std::cos(rad), s = std::sin(rad);
  m.a = c; m.b = s; m.c = -s; m.d = c;
  return m;
}

Matrix Matrix::mul(const Matrix& m) const {
  Matrix r;
  r.a = a * m.a + c * m.b;
  r.b = b * m.a + d * m.b;
  r.c = a * m.c + c * m.d;
  r.d = b * m.c + d * m.d;
  r.e = a * m.e + c * m.f + e;
  r.f = b * m.e + d * m.f + f;
  return r;
}

bool Matrix::invert(Matrix& out) const {
  const double det = (double)a * d - (double)b * c;
  if (!(std::fabs(det) > 1e-12)) return false;
  const double id = 1.0 / det;
  out.a = (float)(d * id);
  out.b = (float)(-b * id);
  out.c = (float)(-c * id);
  out.d = (float)(a * id);
  out.e = (float)((c * f - d * e) * id);
  out.f = (float)((b * e - a * f) * id);
  return true;
}

// ---- Paint ----------------------------------------------------------------------------------------

PaintData PaintData::linear(float x0, float y0, float x1, float y1) {
  PaintData p;
  p.kind = Paint::LINEAR;
  const float dx = x1 - x0, dy = y1 - y0;
  p.m.a = dx; p.m.b = dy; p.m.c = -dy; p.m.d = dx; p.m.e = x0; p.m.f = y0;
  return p;
}

PaintData PaintData::radial(float x0, float y0, float r0, float x1, float y1, float r1) {
  PaintData p;
  p.kind = Paint::RADIAL;
  if (!(r1 > 0)) {
    // A zero end circle: Canvas2D paints the inverted cone; we keep the shape (the unit circle at the
    // START circle) by swapping the roles, which is what every painter can draw.
    if (r0 > 0) {
      p.m.a = r0; p.m.d = r0; p.m.e = x0; p.m.f = y0;
      p.fx = (x1 - x0) / r0; p.fy = (y1 - y0) / r0; p.r0 = 0;
      return p;
    }
    r1 = 1e-6f;
  }
  p.m.a = r1; p.m.d = r1; p.m.e = x1; p.m.f = y1;
  p.fx = (x0 - x1) / r1; p.fy = (y0 - y1) / r1; p.r0 = r0 / r1;
  return p;
}

void PaintData::normalize() {
  if (kind == Paint::COLOR) { stops.clear(); return; }
  // A linear gradient only cares about its axis: canonicalize the perpendicular so the matrix is a
  // similarity (painters then express it as a plain two-point gradient without a context transform).
  if (kind == Paint::LINEAR) { m.c = -m.b; m.d = m.a; }
  for (auto& s : stops) {
    if (!(s.offset >= 0)) s.offset = 0;   // also catches NaN
    if (s.offset > 1) s.offset = 1;
  }
  std::stable_sort(stops.begin(), stops.end(), [](const Stop& a, const Stop& b) { return a.offset < b.offset; });
  if (stops.empty()) { kind = Paint::COLOR; color = { 0, 0, 0, 0 }; return; }
  if (stops.size() == 1) { kind = Paint::COLOR; color = stops[0].color; stops.clear(); return; }
}

// ---- Path -----------------------------------------------------------------------------------------

void Path::moveTo(float x, float y) {
  words.push_back((float)PathVerb::MOVE); words.push_back(x); words.push_back(y);
  hasCurrent = true; curX = startX = x; curY = startY = y;
}
void Path::lineTo(float x, float y) {
  if (!hasCurrent) { moveTo(x, y); return; }
  words.push_back((float)PathVerb::LINE); words.push_back(x); words.push_back(y);
  curX = x; curY = y;
}
void Path::quadTo(float cx, float cy, float x, float y) {
  if (!hasCurrent) moveTo(cx, cy);
  words.push_back((float)PathVerb::QUAD); words.push_back(cx); words.push_back(cy); words.push_back(x); words.push_back(y);
  curX = x; curY = y;
}
void Path::cubicTo(float c1x, float c1y, float c2x, float c2y, float x, float y) {
  if (!hasCurrent) moveTo(c1x, c1y);
  words.push_back((float)PathVerb::CUBIC);
  words.push_back(c1x); words.push_back(c1y); words.push_back(c2x); words.push_back(c2y); words.push_back(x); words.push_back(y);
  curX = x; curY = y;
}
void Path::close() {
  if (!hasCurrent) return;
  if (!words.empty() && words.back() == (float)PathVerb::CLOSE) return;   // idempotent
  words.push_back((float)PathVerb::CLOSE);
  curX = startX; curY = startY;
}

void Path::append(const Path& o, const Matrix* m) {
  size_t i = 0;
  auto pt = [&](float& x, float& y) { if (m) { float ox, oy; m->apply(x, y, ox, oy); x = ox; y = oy; } };
  while (i < o.words.size()) {
    const int verb = (int)o.words[i++];
    switch ((PathVerb)verb) {
      case PathVerb::MOVE: { float x = o.words[i], y = o.words[i + 1]; i += 2; pt(x, y); moveTo(x, y); break; }
      case PathVerb::LINE: { float x = o.words[i], y = o.words[i + 1]; i += 2; pt(x, y); lineTo(x, y); break; }
      case PathVerb::QUAD: {
        float cx = o.words[i], cy = o.words[i + 1], x = o.words[i + 2], y = o.words[i + 3]; i += 4;
        pt(cx, cy); pt(x, y); quadTo(cx, cy, x, y); break;
      }
      case PathVerb::CUBIC: {
        float c1x = o.words[i], c1y = o.words[i + 1], c2x = o.words[i + 2], c2y = o.words[i + 3], x = o.words[i + 4], y = o.words[i + 5]; i += 6;
        pt(c1x, c1y); pt(c2x, c2y); pt(x, y); cubicTo(c1x, c1y, c2x, c2y, x, y); break;
      }
      case PathVerb::CLOSE: close(); break;
      default: return;
    }
  }
}

// ---- DrawList (writer) ----------------------------------------------------------------------------

void DrawList::clear() { words.clear(); strings.clear(); intern_.clear(); }

int32_t DrawList::intern(const std::string& s) {
  auto it = intern_.find(s);
  if (it != intern_.end()) return it->second;
  const int32_t id = (int32_t)strings.size();
  strings.push_back(s);
  intern_.emplace(s, id);
  return id;
}

size_t DrawList::begin(Draw cmd) {
  words.push_back((float)cmd);
  words.push_back(0);   // len, patched by end()
  return words.size() - 1;
}
void DrawList::end(size_t lenAt) { words[lenAt] = (float)(words.size() - lenAt - 1); }

void DrawList::put(float v) { words.push_back(v == 0 ? 0.0f : v); }   // folds -0: the binary equals the JSON

void DrawList::putColor(const Color& c) { put(c.r); put(c.g); put(c.b); put(c.a); }

void DrawList::putPaint(const PaintData& p) {
  put((float)p.kind);
  put(p.alpha);
  if (p.kind == Paint::COLOR) { putColor(p.color); return; }
  put(p.m.a); put(p.m.b); put(p.m.c); put(p.m.d); put(p.m.e); put(p.m.f);
  if (p.kind == Paint::RADIAL) { put(p.fx); put(p.fy); put(p.r0); }
  put((float)p.spread);
  put((float)p.stops.size());
  for (const auto& s : p.stops) { put(s.offset); putColor(s.color); }
}

void DrawList::putStroke(const StrokeData& s) {
  put(s.width); put((float)s.join); put((float)s.cap); put(s.miterLimit); put(s.dashOffset);
  put((float)s.dash.size());
  for (float d : s.dash) put(d);
}

void DrawList::putFont(const FontData& f) {
  put((float)intern(f.family)); put(f.size); put((float)f.weight); put(f.italic ? 1.0f : 0.0f);
}

void DrawList::putPath(const Path& p) {
  put((float)p.words.size());
  words.insert(words.end(), p.words.begin(), p.words.end());
}

void DrawList::setTransform(const Matrix& m) {
  const size_t at = begin(Draw::SET_TRANSFORM);
  put(m.a); put(m.b); put(m.c); put(m.d); put(m.e); put(m.f);
  end(at);
}
void DrawList::save() { end(begin(Draw::SAVE)); }
void DrawList::restore() { end(begin(Draw::RESTORE)); }
void DrawList::clip(FillRule rule, const Path& path) {
  const size_t at = begin(Draw::CLIP);
  put((float)rule); putPath(path);
  end(at);
}
void DrawList::fillPath(FillRule rule, const PaintData& paint, const Path& path) {
  const size_t at = begin(Draw::FILL_PATH);
  put((float)rule); putPaint(paint); putPath(path);
  end(at);
}
void DrawList::strokePath(const StrokeData& stroke, const PaintData& paint, const Path& path) {
  const size_t at = begin(Draw::STROKE_PATH);
  putStroke(stroke); putPaint(paint); putPath(path);
  end(at);
}
void DrawList::fillText(const std::string& text, float x, float y, float maxWidth, const FontData& font,
                        TextAlign align, TextBaseline baseline, float letterSpacing, const PaintData& paint) {
  const size_t at = begin(Draw::FILL_TEXT);
  put((float)intern(text)); put(x); put(y); put(maxWidth); putFont(font);
  put((float)align); put((float)baseline); put(letterSpacing); putPaint(paint);
  end(at);
}
void DrawList::strokeText(const std::string& text, float x, float y, float maxWidth, const FontData& font,
                          TextAlign align, TextBaseline baseline, float letterSpacing, const StrokeData& stroke, const PaintData& paint) {
  const size_t at = begin(Draw::STROKE_TEXT);
  put((float)intern(text)); put(x); put(y); put(maxWidth); putFont(font);
  put((float)align); put((float)baseline); put(letterSpacing); putStroke(stroke); putPaint(paint);
  end(at);
}
void DrawList::drawImage(int32_t surface, float sx, float sy, float sw, float sh, float dx, float dy, float dw, float dh, float alpha) {
  const size_t at = begin(Draw::DRAW_IMAGE);
  put((float)surface); put(sx); put(sy); put(sw); put(sh); put(dx); put(dy); put(dw); put(dh); put(alpha);
  end(at);
}
void DrawList::clearRect(float x, float y, float w, float h) {
  const size_t at = begin(Draw::CLEAR_RECT);
  put(x); put(y); put(w); put(h);
  end(at);
}

// ---- DrawReader -----------------------------------------------------------------------------------

bool DrawReader::next(int32_t& cmd, int32_t& end) {
  if (!ok_ || pos_ + 2 > n_) return false;
  cmd = (int32_t)w_[pos_];
  const int32_t len = (int32_t)w_[pos_ + 1];
  if (len < 0 || pos_ + 2 + len > n_) { ok_ = false; return false; }
  pos_ += 2;
  end = pos_ + len;
  return true;
}

float DrawReader::f() {
  if (pos_ >= n_) { ok_ = false; return 0; }
  return w_[pos_++];
}

const char* DrawReader::str() {
  const int32_t id = i();
  if (id < 0 || id >= ns_ || !s_[id]) { ok_ = false; return ""; }
  return s_[id];
}

Color DrawReader::color() { Color c; c.r = f(); c.g = f(); c.b = f(); c.a = f(); return c; }

Matrix DrawReader::matrix() { Matrix m; m.a = f(); m.b = f(); m.c = f(); m.d = f(); m.e = f(); m.f = f(); return m; }

PaintData DrawReader::paint() {
  PaintData p;
  p.kind = (Paint)i();
  p.alpha = f();
  if (p.kind == Paint::COLOR) { p.color = color(); return p; }
  p.m = matrix();
  if (p.kind == Paint::RADIAL) { p.fx = f(); p.fy = f(); p.r0 = f(); }
  p.spread = (Spread)i();
  const int32_t n = i();
  if (n < 0 || pos_ + n * 5 > n_) { ok_ = false; return p; }
  p.stops.resize((size_t)n);
  for (auto& s : p.stops) { s.offset = f(); s.color = color(); }
  return p;
}

StrokeData DrawReader::stroke() {
  StrokeData s;
  s.width = f(); s.join = (LineJoin)i(); s.cap = (LineCap)i(); s.miterLimit = f(); s.dashOffset = f();
  const int32_t n = i();
  if (n < 0 || pos_ + n > n_) { ok_ = false; return s; }
  s.dash.resize((size_t)n);
  for (auto& d : s.dash) d = f();
  return s;
}

FontData DrawReader::font() {
  FontData fd;
  fd.family = str(); fd.size = f(); fd.weight = i(); fd.italic = i() != 0;
  return fd;
}

Path DrawReader::path() {
  Path p;
  const int32_t n = i();
  if (n < 0 || pos_ + n > n_) { ok_ = false; return p; }
  p.words.assign(w_ + pos_, w_ + pos_ + n);
  pos_ += n;
  return p;
}

// ---- JSON ------------------------------------------------------------------------------------------

std::string formatFloat(float v) {
  if (std::isnan(v)) return "null";
  if (std::isinf(v)) return v > 0 ? "1e38" : "-1e38";
  if (v == 0) return "0";   // folds -0
  char buf[32];
  for (int prec = 6; prec <= 9; prec++) {
    std::snprintf(buf, sizeof buf, "%.*g", prec, (double)v);
    if (std::strtof(buf, nullptr) == v) break;
  }
  return buf;
}

namespace {

std::string jsonString(const char* s) {
  std::string out = "\"";
  for (const unsigned char* p = (const unsigned char*)s; *p; ++p) {
    switch (*p) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if (*p < 0x20) { char b[8]; std::snprintf(b, sizeof b, "\\u%04x", *p); out += b; }
        else out += (char)*p;
    }
  }
  return out + "\"";
}

std::string jsonColor(const Color& c) {
  return "[" + formatFloat(c.r) + "," + formatFloat(c.g) + "," + formatFloat(c.b) + "," + formatFloat(c.a) + "]";
}
std::string jsonMatrix(const Matrix& m) {
  return "[" + formatFloat(m.a) + "," + formatFloat(m.b) + "," + formatFloat(m.c) + "," + formatFloat(m.d) + "," + formatFloat(m.e) + "," + formatFloat(m.f) + "]";
}
std::string jsonPaint(const PaintData& p) {
  std::string s = "{\"kind\":\"" + std::string(PaintName((int32_t)p.kind)) + "\",\"alpha\":" + formatFloat(p.alpha);
  if (p.kind == Paint::COLOR) return s + ",\"color\":" + jsonColor(p.color) + "}";
  s += ",\"matrix\":" + jsonMatrix(p.m);
  if (p.kind == Paint::RADIAL) s += ",\"fx\":" + formatFloat(p.fx) + ",\"fy\":" + formatFloat(p.fy) + ",\"r0\":" + formatFloat(p.r0);
  s += ",\"spread\":\"" + std::string(SpreadName((int32_t)p.spread)) + "\",\"stops\":[";
  for (size_t i = 0; i < p.stops.size(); i++) {
    if (i) s += ",";
    s += "{\"offset\":" + formatFloat(p.stops[i].offset) + ",\"color\":" + jsonColor(p.stops[i].color) + "}";
  }
  return s + "]}";
}
std::string jsonStroke(const StrokeData& st) {
  std::string s = "{\"width\":" + formatFloat(st.width) + ",\"join\":\"" + LineJoinName((int32_t)st.join) + "\",\"cap\":\"" + LineCapName((int32_t)st.cap) +
                  "\",\"miterLimit\":" + formatFloat(st.miterLimit) + ",\"dashOffset\":" + formatFloat(st.dashOffset) + ",\"dash\":[";
  for (size_t i = 0; i < st.dash.size(); i++) { if (i) s += ","; s += formatFloat(st.dash[i]); }
  return s + "]}";
}
std::string jsonFont(const FontData& f) {
  return "{\"family\":" + jsonString(f.family.c_str()) + ",\"size\":" + formatFloat(f.size) + ",\"weight\":" + std::to_string(f.weight) +
         ",\"italic\":" + (f.italic ? "true" : "false") + "}";
}
std::string jsonPath(const Path& p) {
  std::string s = "[";
  size_t i = 0;
  bool first = true;
  auto coords = [&](int n) { for (int k = 0; k < n; k++) { s += ","; s += formatFloat(i < p.words.size() ? p.words[i] : 0); i++; } };
  while (i < p.words.size()) {
    if (!first) s += ",";
    first = false;
    const int verb = (int)p.words[i++];
    switch ((PathVerb)verb) {
      case PathVerb::MOVE: s += "[\"M\""; coords(2); break;
      case PathVerb::LINE: s += "[\"L\""; coords(2); break;
      case PathVerb::QUAD: s += "[\"Q\""; coords(4); break;
      case PathVerb::CUBIC: s += "[\"C\""; coords(6); break;
      case PathVerb::CLOSE: s += "[\"Z\""; break;
      default: s += "[\"?\""; i = p.words.size(); break;
    }
    s += "]";
  }
  return s + "]";
}

}  // namespace

std::string DrawList::toJson() const {
  std::vector<const char*> ptrs;
  ptrs.reserve(strings.size());
  for (const auto& s : strings) ptrs.push_back(s.c_str());
  DrawReader r(words.data(), (int32_t)words.size(), ptrs.data(), (int32_t)ptrs.size());
  std::string out = "[";
  int32_t cmd = 0, end = 0;
  bool first = true;
  while (r.next(cmd, end)) {
    out += first ? "\n" : ",\n";
    first = false;
    const char* name = drawName(cmd);
    std::string line = "{\"cmd\":\"" + std::string(name ? name : "?") + "\"";
    switch ((Draw)cmd) {
      case Draw::SET_TRANSFORM: line += ",\"matrix\":" + jsonMatrix(r.matrix()); break;
      case Draw::SAVE: case Draw::RESTORE: break;
      case Draw::CLIP: {
        const int32_t rule = r.i();
        line += ",\"rule\":\"" + std::string(FillRuleName(rule)) + "\",\"path\":" + jsonPath(r.path());
        break;
      }
      case Draw::FILL_PATH: {
        const int32_t rule = r.i();
        const PaintData paint = r.paint();
        line += ",\"rule\":\"" + std::string(FillRuleName(rule)) + "\",\"paint\":" + jsonPaint(paint) + ",\"path\":" + jsonPath(r.path());
        break;
      }
      case Draw::STROKE_PATH: {
        const StrokeData stroke = r.stroke();
        const PaintData paint = r.paint();
        line += ",\"stroke\":" + jsonStroke(stroke) + ",\"paint\":" + jsonPaint(paint) + ",\"path\":" + jsonPath(r.path());
        break;
      }
      case Draw::FILL_TEXT: case Draw::STROKE_TEXT: {
        const std::string text = r.str();
        const float x = r.f(), y = r.f(), maxWidth = r.f();
        const FontData font = r.font();
        const int32_t align = r.i(), baseline = r.i();
        const float ls = r.f();
        line += ",\"text\":" + jsonString(text.c_str()) + ",\"x\":" + formatFloat(x) + ",\"y\":" + formatFloat(y) + ",\"maxWidth\":" + formatFloat(maxWidth) +
                ",\"font\":" + jsonFont(font) + ",\"align\":\"" + TextAlignName(align) + "\",\"baseline\":\"" + TextBaselineName(baseline) +
                "\",\"letterSpacing\":" + formatFloat(ls);
        if ((Draw)cmd == Draw::STROKE_TEXT) line += ",\"stroke\":" + jsonStroke(r.stroke());
        line += ",\"paint\":" + jsonPaint(r.paint());
        break;
      }
      case Draw::DRAW_IMAGE: {
        const int32_t surface = r.i();
        const float sx = r.f(), sy = r.f(), sw = r.f(), sh = r.f(), dx = r.f(), dy = r.f(), dw = r.f(), dh = r.f(), alpha = r.f();
        line += ",\"surface\":" + std::to_string(surface) + ",\"src\":[" + formatFloat(sx) + "," + formatFloat(sy) + "," + formatFloat(sw) + "," + formatFloat(sh) +
                "],\"dst\":[" + formatFloat(dx) + "," + formatFloat(dy) + "," + formatFloat(dw) + "," + formatFloat(dh) + "],\"alpha\":" + formatFloat(alpha);
        break;
      }
      case Draw::CLEAR_RECT: {
        const float x = r.f(), y = r.f(), w = r.f(), h = r.f();
        line += ",\"rect\":[" + formatFloat(x) + "," + formatFloat(y) + "," + formatFloat(w) + "," + formatFloat(h) + "]";
        break;
      }
      default: line += ",\"unknown\":true"; break;
    }
    out += line + "}";
    r.seek(end);
  }
  return out + "\n]\n";
}

}  // namespace anycanvas
