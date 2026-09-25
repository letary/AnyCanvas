// The tgfx painter. Ported from the LeCodes desktop interpreter (renderers/tgfx/src/canvas.cpp) with
// the interpretation removed: what arrives is already resolved. One case per draw command; the
// switch is exhaustive on purpose (no default), so a new command in spec/draw.h fails to compile
// here until it is handled.
#include "tgfx_painter.h"

#include "tgfx/core/Canvas.h"
#include "tgfx/core/Color.h"
#include "tgfx/core/Font.h"
#include "tgfx/core/Image.h"
#include "tgfx/core/Matrix.h"
#include "tgfx/core/Paint.h"
#include "tgfx/core/Path.h"
#include "tgfx/core/PathEffect.h"
#include "tgfx/core/Point.h"
#include "tgfx/core/Rect.h"
#include "tgfx/core/Shader.h"

#include <cmath>
#include <vector>

namespace anycanvas {

namespace {

tgfx::Matrix toTgfx(const Matrix& m) {
  // tgfx::Matrix::MakeAll(scaleX, skewX, transX, skewY, scaleY, transY)
  return tgfx::Matrix::MakeAll(m.a, m.c, m.e, m.b, m.d, m.f);
}

tgfx::Color toTgfx(const Color& c) { return tgfx::Color{ c.r, c.g, c.b, c.a }; }

tgfx::Path toTgfx(const Path& p) {
  tgfx::Path path;
  size_t i = 0;
  const auto& w = p.words;
  while (i < w.size()) {
    switch ((PathVerb)(int)w[i++]) {
      case PathVerb::MOVE: path.moveTo(w[i], w[i + 1]); i += 2; break;
      case PathVerb::LINE: path.lineTo(w[i], w[i + 1]); i += 2; break;
      case PathVerb::QUAD: path.quadTo(w[i], w[i + 1], w[i + 2], w[i + 3]); i += 4; break;
      case PathVerb::CUBIC: path.cubicTo(w[i], w[i + 1], w[i + 2], w[i + 3], w[i + 4], w[i + 5]); i += 6; break;
      case PathVerb::CLOSE: path.close(); break;
      default: return path;
    }
  }
  return path;
}

// The paint's color or shader, times its alpha. setColor replaces the alpha too, so the paint alpha
// goes INTO every color (tgfx::Color is unpremultiplied).
void applyPaint(tgfx::Paint& paint, const PaintData& p) {
  auto withAlpha = [&](const Color& c) { tgfx::Color t = toTgfx(c); t.alpha *= p.alpha; return t; };
  if (p.kind == Paint::COLOR) {
    paint.setColor(withAlpha(p.color));
    return;
  }
  // The unit end circle at the origin; tgfx's radial gradient is concentric, so the focal point is
  // ignored here like on Android (documented). The start radius r0 is honoured by moving the stops
  // onto [r0, 1] (tgfx pads [0, r0) with the first color) — exact for concentric circles.
  const float r0 = (p.kind == Paint::RADIAL && p.r0 > 0 && p.r0 < 1) ? p.r0 : 0.0f;
  std::vector<tgfx::Color> colors;
  std::vector<float> positions;
  for (const auto& s : p.stops) { colors.push_back(toTgfx(s.color)); positions.push_back(r0 + s.offset * (1 - r0)); }
  std::shared_ptr<tgfx::Shader> shader;
  if (p.kind == Paint::LINEAR) {
    shader = tgfx::Shader::MakeLinearGradient(tgfx::Point::Make(0, 0), tgfx::Point::Make(1, 0), colors, positions);
  } else {
    shader = tgfx::Shader::MakeRadialGradient(tgfx::Point::Make(0, 0), 1.0f, colors, positions);
  }
  if (shader) shader = shader->makeWithMatrix(toTgfx(p.m));
  if (!shader) { paint.setColor(withAlpha(p.stops.empty() ? Color{ 0, 0, 0, 0 } : p.stops.back().color)); return; }
  paint.setColor(tgfx::Color{ 1, 1, 1, p.alpha });   // the gradient multiplies its output by this alpha
  paint.setShader(shader);                            // after setColor: a shader that reduces to a color folds this alpha in
}

void applyStroke(tgfx::Paint& paint, const StrokeData& s) {
  paint.setStyle(tgfx::PaintStyle::Stroke);
  paint.setStrokeWidth(s.width);
  paint.setLineJoin(s.join == LineJoin::ROUND ? tgfx::LineJoin::Round : s.join == LineJoin::BEVEL ? tgfx::LineJoin::Bevel : tgfx::LineJoin::Miter);
  paint.setLineCap(s.cap == LineCap::ROUND ? tgfx::LineCap::Round : s.cap == LineCap::SQUARE ? tgfx::LineCap::Square : tgfx::LineCap::Butt);
  paint.setMiterLimit(s.miterLimit);
}

// Dashes: tgfx dashes a path through a PathEffect; the stroked geometry is then the dashed path.
tgfx::Path dashed(const tgfx::Path& path, const StrokeData& s) {
  if (s.dash.empty()) return path;
  auto effect = tgfx::PathEffect::MakeDash(s.dash.data(), (int)s.dash.size(), s.dashOffset);
  if (!effect) return path;
  tgfx::Path out = path;
  if (!effect->filterPath(&out)) return path;
  return out;
}

float alignedX(float x, float w, TextAlign align) {
  if (align == TextAlign::CENTER) return x - w / 2.0f;
  if (align == TextAlign::RIGHT || align == TextAlign::END) return x - w;
  return x;
}

float baselineY(float y, float asc, float desc, TextBaseline baseline) {
  switch (baseline) {
    case TextBaseline::TOP: return y + asc;
    case TextBaseline::MIDDLE: return y + (asc - desc) / 2.0f;
    case TextBaseline::BOTTOM: return y - desc;
    case TextBaseline::HANGING: return y + asc * 0.8f;
    case TextBaseline::IDEOGRAPHIC: return y - desc;
    default: return y;
  }
}

void drawText(tgfx::Canvas* canvas, const TgfxHooks& hooks, const std::string& text, float x, float y, float maxWidth,
              const FontData& fd, TextAlign align, TextBaseline baseline, float letterSpacing, const tgfx::Paint& paint) {
  if (!hooks.font || !hooks.measure || !hooks.drawLine || text.empty()) return;
  tgfx::Font font = hooks.font(fd);
  float asc = 0, desc = 0;
  if (hooks.metrics) hooks.metrics(font, asc, desc);
  else { auto m = font.getMetrics(); asc = -m.ascent; desc = m.descent; }
  const float w = hooks.measure(text, font, letterSpacing);   // with the gap after every glyph, as drawn
  const float ax = alignedX(x, w, align);
  const float by = baselineY(y, asc, desc, baseline);
  const bool condense = maxWidth > 0 && w > maxWidth;
  if (condense) {
    canvas->save();
    canvas->translate(ax, 0);
    canvas->scale(maxWidth / w, 1.0f);
    canvas->translate(-ax, 0);
  }
  hooks.drawLine(canvas, text, ax, by, font, paint, letterSpacing);
  if (condense) canvas->restore();
}

}  // namespace

void paintTgfx(tgfx::Canvas* canvas, const float* words, int32_t wordCount, const char* const* strings, int32_t stringCount, const TgfxHooks& hooks) {
  if (!canvas) return;
  canvas->restoreToCount(0);   // tgfx's base save count is 0 (not Skia's 1)
  canvas->resetMatrix();
  // A list-level CLIP (an app clip() without save()) lands inside this save, and the final
  // restoreToCount(0) drops it: tgfx keeps a base-level clip on the canvas, and the next paint's
  // clear() would fill only that clip. RESTORE is guarded by `saves`, so the list never pops it.
  canvas->save();
  DrawReader r(words, wordCount, strings, stringCount);
  int32_t cmd = 0, end = 0;
  int saves = 0;
  while (r.next(cmd, end)) {
    switch ((Draw)cmd) {
      case Draw::SET_TRANSFORM: canvas->setMatrix(toTgfx(r.matrix())); break;
      case Draw::SAVE: canvas->save(); saves++; break;
      case Draw::RESTORE: if (saves > 0) { canvas->restore(); saves--; } break;
      case Draw::CLIP: {
        const FillRule rule = (FillRule)r.i();
        tgfx::Path path = toTgfx(r.path());
        path.setFillType(rule == FillRule::EVENODD ? tgfx::PathFillType::EvenOdd : tgfx::PathFillType::Winding);
        canvas->clipPath(path);
        break;
      }
      case Draw::FILL_PATH: {
        const FillRule rule = (FillRule)r.i();
        const PaintData pd = r.paint();
        tgfx::Path path = toTgfx(r.path());
        path.setFillType(rule == FillRule::EVENODD ? tgfx::PathFillType::EvenOdd : tgfx::PathFillType::Winding);
        tgfx::Paint paint;
        paint.setStyle(tgfx::PaintStyle::Fill);
        applyPaint(paint, pd);
        canvas->drawPath(path, paint);
        break;
      }
      case Draw::STROKE_PATH: {
        const StrokeData sd = r.stroke();
        const PaintData pd = r.paint();
        tgfx::Path path = dashed(toTgfx(r.path()), sd);
        tgfx::Paint paint;
        applyStroke(paint, sd);
        applyPaint(paint, pd);
        canvas->drawPath(path, paint);
        break;
      }
      case Draw::FILL_TEXT: case Draw::STROKE_TEXT: {
        const std::string text = r.str();
        const float x = r.f(), y = r.f(), maxWidth = r.f();
        const FontData fd = r.font();
        const TextAlign align = (TextAlign)r.i();
        const TextBaseline baseline = (TextBaseline)r.i();
        const float ls = r.f();
        tgfx::Paint paint;
        if ((Draw)cmd == Draw::STROKE_TEXT) applyStroke(paint, r.stroke());
        else paint.setStyle(tgfx::PaintStyle::Fill);
        applyPaint(paint, r.paint());
        drawText(canvas, hooks, text, x, y, maxWidth, fd, align, baseline, ls, paint);
        break;
      }
      case Draw::DRAW_IMAGE: {
        const int32_t surface = r.i();
        const float sx = r.f(), sy = r.f(), sw = r.f(), sh = r.f(), dx = r.f(), dy = r.f(), dw = r.f(), dh = r.f(), alpha = r.f();
        std::shared_ptr<tgfx::Image> img = hooks.image ? hooks.image(surface) : nullptr;
        if (img) {
          tgfx::Paint paint;
          paint.setAlpha(alpha);
          canvas->drawImageRect(img, tgfx::Rect::MakeXYWH(sx, sy, sw, sh), tgfx::Rect::MakeXYWH(dx, dy, dw, dh), {}, alpha < 1.0f ? &paint : nullptr);
        }
        break;
      }
      case Draw::CLEAR_RECT: {
        const float x = r.f(), y = r.f(), w = r.f(), h = r.f();
        tgfx::Paint paint;
        paint.setColor(tgfx::Color::Transparent());
        paint.setBlendMode(tgfx::BlendMode::Clear);
        canvas->drawRect(tgfx::Rect::MakeXYWH(x, y, w, h), paint);
        break;
      }
    }
    if (!r.ok()) break;
    r.seek(end);
  }
  canvas->restoreToCount(0);   // pops the wrapper save with every clip the list set
}

}  // namespace anycanvas
