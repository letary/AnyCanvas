#include "interpreter.h"

#include "css.h"
#include "geometry.h"

#include <cmath>
#include <cstring>
#include <vector>

namespace anycanvas {

namespace {

struct State {
  Matrix ctm;
  PaintData fill = PaintData::solid({ 0, 0, 0, 1 });
  PaintData stroke = PaintData::solid({ 0, 0, 0, 1 });
  StrokeData strokeStyle;
  FontData font;
  TextAlign align = TextAlign::START;
  TextBaseline baseline = TextBaseline::ALPHABETIC;
  float letterSpacing = 0;
  float alpha = 1;
};

class Interpreter {
 public:
  Interpreter(const float* cmd, int32_t len, const char* const* refs, int32_t refsCount, float scale, DrawList& out)
      : w_(cmd), n_(len), refs_(refs), nrefs_(refsCount), out_(out) {
    if (!(scale > 0)) scale = 1;
    st_.ctm = Matrix::scaling(scale, scale);
    st_.font = parseCssFont("10px sans-serif");
  }

  int32_t run() {
    int32_t ops = 0;
    while (pos_ < n_) {
      const int32_t op = (int32_t)w_[pos_];
      const OpLayout* layout = opLayout(op);
      if (!layout) break;
      const int32_t fixed = (int32_t)std::strlen(layout->args);
      if (pos_ + 1 + fixed > n_) break;
      const float* a = w_ + pos_ + 1;
      int32_t groupWords = 0;
      if (layout->repeat[0]) {
        const int32_t count = (int32_t)a[fixed - 1];
        const int32_t per = (int32_t)std::strlen(layout->repeat);
        if (count < 0 || count > (n_ - pos_ - 1 - fixed) / per) break;
        groupWords = count * per;
      }
      pos_ += 1 + fixed + groupWords;
      exec((Op)op, a);
      ops++;
    }
    // Balance the painter's stack: a list never ends inside a save.
    while (painterDepth_ > 0) { out_.restore(); painterDepth_--; painterStack_.pop_back(); }
    return ops;
  }

 private:
  const float* w_; int32_t n_; const char* const* refs_; int32_t nrefs_;
  DrawList& out_;
  int32_t pos_ = 0;

  State st_;
  std::vector<State> stack_;

  // What the painter believes: its transform and its save stack (the draw list is lazy about SET_TRANSFORM).
  Matrix painterCtm_;
  std::vector<Matrix> painterStack_;
  int32_t painterDepth_ = 0;

  // The current path. Points are recorded in user space under `pathCtm_` until the CTM changes
  // mid-path; then the path is kept in DEVICE space (Canvas2D semantics) and mapped back at draw time.
  Path path_;
  Matrix pathCtm_;
  bool pathStarted_ = false;
  bool mixed_ = false;

  const char* ref(float idx) const {
    const int32_t i = (int32_t)idx;
    return (i >= 0 && i < nrefs_ && refs_[i]) ? refs_[i] : "";
  }

  void sync() {
    if (painterCtm_ != st_.ctm) { out_.setTransform(st_.ctm); painterCtm_ = st_.ctm; }
  }

  // ---- paths --------------------------------------------------------------------------------------

  void beginPath() { path_.clear(); pathStarted_ = false; mixed_ = false; }

  // Merge verbs authored in the current user space into the path.
  void addUserPath(const Path& user) {
    if (user.empty()) return;
    if (!pathStarted_) { pathCtm_ = st_.ctm; pathStarted_ = true; }
    if (!mixed_ && st_.ctm != pathCtm_) {
      Path dev = path_.transformed(pathCtm_);
      path_ = dev;
      mixed_ = true;
    }
    path_.append(user, mixed_ ? &st_.ctm : nullptr);
  }

  // The current point in the current user space, for arc / arcTo.
  bool currentPoint(float& x, float& y) {
    if (!path_.hasCurrent) return false;
    if (!mixed_ && st_.ctm == pathCtm_) { x = path_.curX; y = path_.curY; return true; }
    float dx = path_.curX, dy = path_.curY;
    if (!mixed_) pathCtm_.apply(dx, dy, dx, dy);
    Matrix inv;
    if (!st_.ctm.invert(inv)) return false;
    inv.apply(dx, dy, x, y);
    return true;
  }

  // The path as the draw list wants it: user space of the CURRENT transform. False when nothing to draw.
  bool resolvePath(Path& out) {
    if (path_.empty()) return false;
    Matrix inv;
    if (!st_.ctm.invert(inv)) return false;   // a singular transform draws nothing
    if (!mixed_ && st_.ctm == pathCtm_) { out = path_; return true; }
    const Path dev = mixed_ ? path_ : path_.transformed(pathCtm_);
    out = dev.transformed(inv);
    return true;
  }

  PaintData paintOf(const PaintData& p) { PaintData q = p; q.alpha = st_.alpha; return q; }

  void applyGradient(PaintData& target, const float* a, const float* groups, int32_t count) {
    const int32_t kind = (int32_t)a[0];
    PaintData p = kind == (int32_t)Gradient::RADIAL ? PaintData::radial(a[1], a[2], a[5], a[3], a[4], a[6])
                                                    : PaintData::linear(a[1], a[2], a[3], a[4]);
    for (int32_t i = 0; i < count; i++) {
      Stop s;
      s.offset = groups[i * 2];
      if (!parseCssColor(ref(groups[i * 2 + 1]), s.color)) continue;   // an invalid stop color is skipped (Canvas2D throws)
      p.stops.push_back(s);
    }
    p.normalize();
    target = p;
  }

  void exec(Op op, const float* a) {
    switch (op) {
      case Op::SAVE:
        stack_.push_back(st_);
        out_.save(); painterStack_.push_back(painterCtm_); painterDepth_++;
        break;
      case Op::RESTORE:
        if (stack_.empty()) break;
        st_ = stack_.back(); stack_.pop_back();
        out_.restore(); painterCtm_ = painterStack_.back(); painterStack_.pop_back(); painterDepth_--;
        break;
      case Op::TRANSLATE: st_.ctm = st_.ctm.mul(Matrix::translation(a[0], a[1])); break;
      case Op::SCALE: st_.ctm = st_.ctm.mul(Matrix::scaling(a[0], a[1])); break;
      case Op::ROTATE: st_.ctm = st_.ctm.mul(Matrix::rotation(a[0])); break;
      case Op::TRANSFORM: { Matrix m; m.a = a[0]; m.b = a[1]; m.c = a[2]; m.d = a[3]; m.e = a[4]; m.f = a[5]; st_.ctm = st_.ctm.mul(m); break; }
      case Op::SET_TRANSFORM: { Matrix m; m.a = a[0]; m.b = a[1]; m.c = a[2]; m.d = a[3]; m.e = a[4]; m.f = a[5]; st_.ctm = base_.mul(m); break; }
      case Op::RESET_TRANSFORM: st_.ctm = base_; break;

      case Op::GLOBAL_ALPHA: if (a[0] >= 0 && a[0] <= 1) st_.alpha = a[0]; break;
      case Op::FILL_STYLE: { Color c; if (parseCssColor(ref(a[0]), c)) st_.fill = PaintData::solid(c); break; }
      case Op::STROKE_STYLE: { Color c; if (parseCssColor(ref(a[0]), c)) st_.stroke = PaintData::solid(c); break; }
      case Op::FILL_GRADIENT: applyGradient(st_.fill, a, a + 8, (int32_t)a[7]); break;
      case Op::STROKE_GRADIENT: applyGradient(st_.stroke, a, a + 8, (int32_t)a[7]); break;
      case Op::LINE_WIDTH: if (a[0] > 0 && std::isfinite(a[0])) st_.strokeStyle.width = a[0]; break;
      case Op::LINE_JOIN: { const int32_t v = (int32_t)a[0]; if (v >= 0 && v <= 2) st_.strokeStyle.join = (LineJoin)v; break; }
      case Op::LINE_CAP: { const int32_t v = (int32_t)a[0]; if (v >= 0 && v <= 2) st_.strokeStyle.cap = (LineCap)v; break; }
      case Op::MITER_LIMIT: if (a[0] > 0 && std::isfinite(a[0])) st_.strokeStyle.miterLimit = a[0]; break;
      case Op::LINE_DASH: {
        const int32_t n = (int32_t)a[0];
        std::vector<float> dash;
        bool valid = true;
        for (int32_t i = 0; i < n; i++) { if (!(a[1 + i] >= 0) || !std::isfinite(a[1 + i])) { valid = false; break; } dash.push_back(a[1 + i]); }
        if (!valid) break;
        if (dash.size() % 2 == 1) { const size_t m = dash.size(); for (size_t i = 0; i < m; i++) dash.push_back(dash[i]); }
        float sum = 0; for (float d : dash) sum += d;
        if (sum <= 0) dash.clear();   // all-zero = solid
        st_.strokeStyle.dash = dash;
        break;
      }
      case Op::LINE_DASH_OFFSET: if (std::isfinite(a[0])) st_.strokeStyle.dashOffset = a[0]; break;
      case Op::FONT: st_.font = parseCssFont(ref(a[0])); break;
      case Op::TEXT_ALIGN: { const int32_t v = (int32_t)a[0]; if (v >= 0 && v <= 4) st_.align = (TextAlign)v; break; }
      case Op::TEXT_BASELINE: { const int32_t v = (int32_t)a[0]; if (v >= 0 && v <= 5) st_.baseline = (TextBaseline)v; break; }
      case Op::LETTER_SPACING: if (std::isfinite(a[0])) st_.letterSpacing = a[0]; break;

      case Op::PATH_BEGIN: beginPath(); break;
      case Op::PATH_CLOSE: path_.close(); break;
      case Op::MOVE_TO: { Path p; p.moveTo(a[0], a[1]); addUserPath(p); break; }
      case Op::LINE_TO: {
        Path p; float cx, cy;
        if (currentPoint(cx, cy)) { p.hasCurrent = true; p.curX = cx; p.curY = cy; p.lineTo(a[0], a[1]); }
        else p.moveTo(a[0], a[1]);
        addUserPath(p); break;
      }
      case Op::QUADRATIC_TO: {
        Path p; float cx, cy;
        if (currentPoint(cx, cy)) { p.hasCurrent = true; p.curX = cx; p.curY = cy; }
        p.quadTo(a[0], a[1], a[2], a[3]); addUserPath(p); break;
      }
      case Op::BEZIER_TO: {
        Path p; float cx, cy;
        if (currentPoint(cx, cy)) { p.hasCurrent = true; p.curX = cx; p.curY = cy; }
        p.cubicTo(a[0], a[1], a[2], a[3], a[4], a[5]); addUserPath(p); break;
      }
      case Op::ARC: {
        Path p; float cx = 0, cy = 0;
        const bool has = currentPoint(cx, cy);
        appendArc(p, has, cx, cy, a[0], a[1], a[2], a[3], a[4], a[5] != 0);
        addUserPath(p); break;
      }
      case Op::ARC_TO: {
        Path p; float cx = 0, cy = 0;
        const bool has = currentPoint(cx, cy);
        appendArcTo(p, has, cx, cy, a[0], a[1], a[2], a[3], a[4]);
        addUserPath(p); break;
      }
      case Op::ELLIPSE: {
        Path p; float cx = 0, cy = 0;
        const bool has = currentPoint(cx, cy);
        appendEllipse(p, has, cx, cy, a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7] != 0);
        addUserPath(p); break;
      }
      case Op::RECT: { Path p; appendRect(p, a[0], a[1], a[2], a[3]); addUserPath(p); break; }
      case Op::ROUND_RECT: { Path p; appendRoundRect(p, a[0], a[1], a[2], a[3], a[4]); addUserPath(p); break; }

      case Op::FILL: { Path p; if (resolvePath(p)) { sync(); out_.fillPath(ruleOf(a[0]), paintOf(st_.fill), p); } break; }
      case Op::STROKE: { Path p; if (resolvePath(p)) { sync(); out_.strokePath(st_.strokeStyle, paintOf(st_.stroke), p); } break; }
      case Op::CLIP: { Path p; if (resolvePath(p)) { sync(); out_.clip(ruleOf(a[0]), p); } break; }

      case Op::FILL_RECT: { Path p; appendRect(p, a[0], a[1], a[2], a[3]); sync(); out_.fillPath(FillRule::NONZERO, paintOf(st_.fill), p); break; }
      case Op::STROKE_RECT: { Path p; appendRect(p, a[0], a[1], a[2], a[3]); sync(); out_.strokePath(st_.strokeStyle, paintOf(st_.stroke), p); break; }
      case Op::CLEAR_RECT: sync(); out_.clearRect(a[0], a[1], a[2], a[3]); break;
      case Op::FILL_TEXT: {
        const char* t = ref(a[0]);
        if (*t) { sync(); out_.fillText(t, a[1], a[2], a[3] > 0 ? a[3] : 0, st_.font, st_.align, st_.baseline, st_.letterSpacing, paintOf(st_.fill)); }
        break;
      }
      case Op::STROKE_TEXT: {
        const char* t = ref(a[0]);
        if (*t) { sync(); out_.strokeText(t, a[1], a[2], a[3] > 0 ? a[3] : 0, st_.font, st_.align, st_.baseline, st_.letterSpacing, st_.strokeStyle, paintOf(st_.stroke)); }
        break;
      }
      case Op::DRAW_IMAGE:
        if ((int32_t)a[0] > 0 && a[3] > 0 && a[4] > 0) { sync(); out_.drawImage((int32_t)a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7], a[8], st_.alpha); }
        break;
    }
  }

  static FillRule ruleOf(float v) { return (int32_t)v == 1 ? FillRule::EVENODD : FillRule::NONZERO; }

  // SET_TRANSFORM / RESET_TRANSFORM are relative to the device scale, which the app never sees.
  Matrix base_ = Matrix();
 public:
  void setBase(const Matrix& b) { base_ = b; }
};

}  // namespace

int32_t interpret(const float* cmd, int32_t cmdLen, const char* const* refs, int32_t refsCount, float scale, DrawList& out) {
  out.clear();
  if (!cmd || cmdLen <= 0) return 0;
  Interpreter it(cmd, cmdLen, refs, refsCount, scale, out);
  it.setBase(Matrix::scaling(scale > 0 ? scale : 1, scale > 0 ? scale : 1));
  return it.run();
}

}  // namespace anycanvas
