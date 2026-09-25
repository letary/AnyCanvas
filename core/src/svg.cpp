#define NANOSVG_CPLUSPLUS
#define NANOSVG_IMPLEMENTATION
#define NANOSVG_ALL_COLOR_KEYWORDS
#include "nanosvg/nanosvg.h"

#include "svg.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace anycanvas {

Svg::~Svg() { if (image) nsvgDelete(image); }

Svg* parseSvg(const char* data, size_t len) {
  if (!data || len == 0) return nullptr;
  std::vector<char> buf(len + 1);
  std::memcpy(buf.data(), data, len);
  buf[len] = '\0';
  NSVGimage* img = nsvgParse(buf.data(), "px", 96.0f);
  if (!img) return nullptr;
  if (!(img->width > 0) || !(img->height > 0)) { nsvgDelete(img); return nullptr; }
  Svg* svg = new Svg();
  svg->image = img;
  return svg;
}

void svgSize(const Svg& svg, float& w, float& h) {
  w = svg.image ? svg.image->width : 0;
  h = svg.image ? svg.image->height : 0;
}

bool looksLikeSvg(const void* data, size_t len) {
  if (!data || len < 4) return false;
  const char* p = (const char*)data;
  const size_t n = std::min<size_t>(len, 512);
  for (size_t i = 0; i + 4 <= n; i++) {
    if (p[i] == '<' && (p[i + 1] == 's' || p[i + 1] == 'S') && (p[i + 2] == 'v' || p[i + 2] == 'V') && (p[i + 3] == 'g' || p[i + 3] == 'G'))
      return true;
  }
  return false;
}

namespace {

// nanosvg packs colors as 0xAABBGGRR.
Color nsvgColor(unsigned int c) {
  return Color::rgba8(c & 0xff, (c >> 8) & 0xff, (c >> 16) & 0xff, (c >> 24) & 0xff);
}

Spread spreadOf(char s) {
  switch (s) { case NSVG_SPREAD_REFLECT: return Spread::REFLECT; case NSVG_SPREAD_REPEAT: return Spread::REPEAT; default: return Spread::PAD; }
}

// The gradient xform nanosvg hands out is the INVERSE: image space → gradient space (its rasterizer
// samples with it; nsvg__scaleToViewbox inverts it at the end of the parse). In gradient space the
// linear axis runs along Y ((0,0) = start, (0,1) = end) and the radial is the unit circle. Our
// gradient space runs the linear axis along X, so the linear matrix also swaps the columns.
// (Both earlier ports, Kotlin and Swift, read this field as gradient → user; the goldens and the
// nanosvgrast PNG of acdump are the reference now.)
bool paintOf(const NSVGpaint& np, float opacity, const Color* tint, PaintData& out) {
  if (np.type == NSVG_PAINT_NONE || np.type == NSVG_PAINT_UNDEF) return false;
  if (tint) { out = PaintData::solid(*tint); out.alpha = opacity; return true; }
  if (np.type == NSVG_PAINT_COLOR) { out = PaintData::solid(nsvgColor(np.color)); out.alpha = opacity; return true; }
  const NSVGgradient* g = np.gradient;
  if (!g || g->nstops <= 0) return false;
  PaintData p;
  p.alpha = opacity;
  p.spread = spreadOf(g->spread);
  for (int i = 0; i < g->nstops; i++) p.stops.push_back({ g->stops[i].offset, nsvgColor(g->stops[i].color) });
  Matrix toGradient;
  toGradient.a = g->xform[0]; toGradient.b = g->xform[1]; toGradient.c = g->xform[2]; toGradient.d = g->xform[3]; toGradient.e = g->xform[4]; toGradient.f = g->xform[5];
  Matrix fwd;
  if (!toGradient.invert(fwd)) {
    // A degenerate gradient (r = 0, a zero-length axis): the SVG spec paints the last stop.
    out = PaintData::solid(p.stops.back().color);
    out.alpha = opacity;
    return true;
  }
  if (np.type == NSVG_PAINT_LINEAR_GRADIENT) {
    p.kind = Paint::LINEAR;
    p.m.a = fwd.c; p.m.b = fwd.d; p.m.c = fwd.a; p.m.d = fwd.b; p.m.e = fwd.e; p.m.f = fwd.f;
  } else {
    p.kind = Paint::RADIAL;
    p.m = fwd;
    p.fx = g->fx; p.fy = g->fy; p.r0 = 0;
  }
  p.normalize();
  out = p;
  return true;
}

}  // namespace

void drawSvg(DrawList& out, const Svg& svg, float w, float h, const Color* tint) {
  const NSVGimage* img = svg.image;
  if (!img) return;
  const float W = img->width, H = img->height;
  Matrix fit;
  if (w > 0 && h > 0) {
    const float s = std::fmin(w / W, h / H);
    fit = Matrix::translation((w - W * s) / 2, (h - H * s) / 2).mul(Matrix::scaling(s, s));
  }
  Matrix current;
  bool transformSet = false;
  auto use = [&](const Matrix& m) {
    if (!transformSet || current != m) { out.setTransform(m); current = m; transformSet = true; }
  };
  for (const NSVGshape* shape = img->shapes; shape; shape = shape->next) {
    if (!(shape->flags & NSVG_FLAGS_VISIBLE)) continue;
    if (shape->text) {
      // A text run: drawn under fit ∘ textXform in its own space (the font size scales with the
      // transform, a rotated <text> rotates), as the same fillText / strokeText a canvas produces.
      Matrix X;
      X.a = shape->textXform[0]; X.b = shape->textXform[1]; X.c = shape->textXform[2];
      X.d = shape->textXform[3]; X.e = shape->textXform[4]; X.f = shape->textXform[5];
      PaintData fill, stroke;
      const bool hasFill = paintOf(shape->fill, shape->opacity, tint, fill);
      const bool hasStroke = paintOf(shape->stroke, shape->opacity, tint, stroke) && shape->strokeWidth > 0;
      if (!hasFill && !hasStroke) continue;
      Matrix invX;
      if (X.invert(invX)) {
        // Gradient matrices are in image space; the command's user space is the run's: rebase them.
        if (fill.kind != Paint::COLOR) fill.m = invX.mul(fill.m);
        if (stroke.kind != Paint::COLOR) stroke.m = invX.mul(stroke.m);
      }
      use(fit.mul(X));
      FontData font;
      font.family = shape->fontFamily[0] ? shape->fontFamily : "sans-serif";
      font.size = shape->fontSize;
      font.weight = shape->fontWeight;
      font.italic = shape->fontItalic != 0;
      const TextAlign align = shape->textAnchor == 1 ? TextAlign::CENTER : shape->textAnchor == 2 ? TextAlign::END : TextAlign::START;
      const TextBaseline baseline = shape->textBaseline >= 0 && shape->textBaseline <= 5 ? (TextBaseline)shape->textBaseline : TextBaseline::ALPHABETIC;
      if (hasFill) out.fillText(shape->text, shape->textX, shape->textY, 0, font, align, baseline, shape->letterSpacing, fill);
      if (hasStroke) {
        StrokeData st;
        st.width = shape->strokeWidth;
        st.join = shape->strokeLineJoin == NSVG_JOIN_ROUND ? LineJoin::ROUND : shape->strokeLineJoin == NSVG_JOIN_BEVEL ? LineJoin::BEVEL : LineJoin::MITER;
        st.cap = shape->strokeLineCap == NSVG_CAP_ROUND ? LineCap::ROUND : shape->strokeLineCap == NSVG_CAP_SQUARE ? LineCap::SQUARE : LineCap::BUTT;
        st.miterLimit = shape->miterLimit > 0 ? shape->miterLimit : 4;
        out.strokeText(shape->text, shape->textX, shape->textY, 0, font, align, baseline, shape->letterSpacing, st, stroke);
      }
      continue;
    }
    Path path;
    for (const NSVGpath* np = shape->paths; np; np = np->next) {
      if (np->npts < 1) continue;
      path.moveTo(np->pts[0], np->pts[1]);
      for (int i = 1; i + 2 < np->npts; i += 3) {
        const float* p = &np->pts[i * 2];
        path.cubicTo(p[0], p[1], p[2], p[3], p[4], p[5]);
      }
      if (np->closed) path.close();
    }
    if (path.empty()) continue;
    PaintData fill, stroke;
    const bool hasFill = paintOf(shape->fill, shape->opacity, tint, fill);
    const bool hasStroke = paintOf(shape->stroke, shape->opacity, tint, stroke) && shape->strokeWidth > 0;
    if (!hasFill && !hasStroke) continue;
    use(fit);
    if (hasFill) out.fillPath(shape->fillRule == NSVG_FILLRULE_EVENODD ? FillRule::EVENODD : FillRule::NONZERO, fill, path);
    if (hasStroke) {
      StrokeData st;
      st.width = shape->strokeWidth;
      st.join = shape->strokeLineJoin == NSVG_JOIN_ROUND ? LineJoin::ROUND : shape->strokeLineJoin == NSVG_JOIN_BEVEL ? LineJoin::BEVEL : LineJoin::MITER;
      st.cap = shape->strokeLineCap == NSVG_CAP_ROUND ? LineCap::ROUND : shape->strokeLineCap == NSVG_CAP_SQUARE ? LineCap::SQUARE : LineCap::BUTT;
      st.miterLimit = shape->miterLimit > 0 ? shape->miterLimit : 4;
      st.dashOffset = shape->strokeDashOffset;
      float sum = 0;
      for (int i = 0; i < shape->strokeDashCount; i++) { st.dash.push_back(shape->strokeDashArray[i]); sum += shape->strokeDashArray[i]; }
      if (st.dash.size() % 2 == 1) { const size_t m = st.dash.size(); for (size_t i = 0; i < m; i++) st.dash.push_back(st.dash[i]); }
      if (sum <= 0) st.dash.clear();
      out.strokePath(st, stroke, path);
    }
  }
}

}  // namespace anycanvas
