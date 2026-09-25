#include "geometry.h"

#include <cmath>

namespace anycanvas {

namespace {

const float PI = 3.14159265358979f;
const float TAU = 6.28318530717959f;
const float KAPPA = 0.5522847498f;   // the cubic control-arm length of a quarter circle

// The Canvas2D sweep: the signed angle actually travelled from a0 towards a1.
float sweepOf(float a0, float a1, bool ccw) {
  float delta = a1 - a0;
  if (!ccw) {
    if (delta >= TAU) return TAU;
    delta = std::fmod(delta, TAU);
    if (delta < 0) delta += TAU;
  } else {
    if (-delta >= TAU) return -TAU;
    delta = std::fmod(delta, TAU);
    if (delta > 0) delta -= TAU;
  }
  return delta;
}

// The unit-circle sweep from a0 by `delta`, as cubics, each point mapped through m (center + radii + rotation).
void unitSweep(Path& out, const Matrix& m, float a0, float delta) {
  if (std::fabs(delta) < 1e-7f) return;
  const int segs = (int)std::ceil(std::fabs(delta) / (PI / 2) - 1e-6f);
  const float seg = delta / (float)(segs < 1 ? 1 : segs);
  const float k = (4.0f / 3.0f) * std::tan(seg / 4.0f);
  float a = a0;
  for (int s = 0; s < (segs < 1 ? 1 : segs); s++) {
    const float a2 = a + seg;
    const float x1 = std::cos(a), y1 = std::sin(a);
    const float x2 = std::cos(a2), y2 = std::sin(a2);
    float c1x = x1 - k * y1, c1y = y1 + k * x1;
    float c2x = x2 + k * y2, c2y = y2 - k * x2;
    float ex = x2, ey = y2;
    m.apply(c1x, c1y, c1x, c1y);
    m.apply(c2x, c2y, c2x, c2y);
    m.apply(ex, ey, ex, ey);
    out.cubicTo(c1x, c1y, c2x, c2y, ex, ey);
    a = a2;
  }
}

}  // namespace

void appendEllipse(Path& out, bool hasCur, float cx0, float cy0, float x, float y, float rx, float ry, float rotation, float a0, float a1, bool ccw) {
  if (!(rx >= 0) || !(ry >= 0)) return;   // Canvas2D throws; we draw nothing
  const float cr = std::cos(rotation), sr = std::sin(rotation);
  Matrix m;
  m.a = rx * cr; m.b = rx * sr; m.c = -ry * sr; m.d = ry * cr; m.e = x; m.f = y;
  float sx = std::cos(a0), sy = std::sin(a0);
  m.apply(sx, sy, sx, sy);
  if (hasCur) {
    // A fresh path has no current point: seed it from the caller's, so the connecting line is one verb
    // (and none at all when the arc starts where the path is).
    if (out.empty()) { out.hasCurrent = true; out.curX = cx0; out.curY = cy0; }
    if (std::fabs(out.curX - sx) > 1e-6f || std::fabs(out.curY - sy) > 1e-6f) out.lineTo(sx, sy);
  } else {
    out.moveTo(sx, sy);
  }
  unitSweep(out, m, a0, sweepOf(a0, a1, ccw));
}

void appendArc(Path& out, bool hasCur, float cx0, float cy0, float x, float y, float r, float a0, float a1, bool ccw) {
  appendEllipse(out, hasCur, cx0, cy0, x, y, r, r, 0, a0, a1, ccw);
}

void appendArcTo(Path& out, bool hasCur, float x0, float y0, float x1, float y1, float x2, float y2, float r) {
  if (!hasCur) { out.moveTo(x1, y1); return; }
  if (out.empty()) { out.hasCurrent = true; out.curX = x0; out.curY = y0; }
  if (!(r >= 0)) return;
  const float d0x = x0 - x1, d0y = y0 - y1, d2x = x2 - x1, d2y = y2 - y1;
  const float l0 = std::sqrt(d0x * d0x + d0y * d0y), l2 = std::sqrt(d2x * d2x + d2y * d2y);
  const float cross = d0x * d2y - d0y * d2x;
  if (l0 < 1e-9f || l2 < 1e-9f || r == 0 || std::fabs(cross) < 1e-9f) { out.lineTo(x1, y1); return; }
  const float u0x = d0x / l0, u0y = d0y / l0, u2x = d2x / l2, u2y = d2y / l2;
  float cosTheta = u0x * u2x + u0y * u2y;
  if (cosTheta > 1) cosTheta = 1; else if (cosTheta < -1) cosTheta = -1;
  const float theta = std::acos(cosTheta);            // the angle between the two rays
  const float t = r / std::tan(theta / 2);             // distance from (x1,y1) to each tangent point
  const float t1x = x1 + u0x * t, t1y = y1 + u0y * t;
  const float t2x = x1 + u2x * t, t2y = y1 + u2y * t;
  float bx = u0x + u2x, by = u0y + u2y;
  const float bl = std::sqrt(bx * bx + by * by);
  bx /= bl; by /= bl;
  const float dc = r / std::sin(theta / 2);
  const float cx = x1 + bx * dc, cy = y1 + by * dc;
  const float a0 = std::atan2(t1y - cy, t1x - cx), a1 = std::atan2(t2y - cy, t2x - cx);
  float delta = a1 - a0;
  while (delta > PI) delta -= TAU;
  while (delta < -PI) delta += TAU;
  out.lineTo(t1x, t1y);
  Matrix m;
  m.a = r; m.d = r; m.e = cx; m.f = cy;
  unitSweep(out, m, a0, delta);
}

void appendRect(Path& out, float x, float y, float w, float h) {
  out.moveTo(x, y);
  out.lineTo(x + w, y);
  out.lineTo(x + w, y + h);
  out.lineTo(x, y + h);
  out.close();
}

void appendRoundRect(Path& out, float x, float y, float w, float h, float r) {
  if (w < 0) { x += w; w = -w; }
  if (h < 0) { y += h; h = -h; }
  if (!(r > 0)) { appendRect(out, x, y, w, h); return; }
  const float half = std::fmin(w, h) / 2;
  if (r > half) r = half;
  const float k = KAPPA * r;
  out.moveTo(x + r, y);
  if (w > 2 * r) out.lineTo(x + w - r, y);
  out.cubicTo(x + w - r + k, y, x + w, y + r - k, x + w, y + r);
  if (h > 2 * r) out.lineTo(x + w, y + h - r);
  out.cubicTo(x + w, y + h - r + k, x + w - r + k, y + h, x + w - r, y + h);
  if (w > 2 * r) out.lineTo(x + r, y + h);
  out.cubicTo(x + r - k, y + h, x, y + h - r + k, x, y + h - r);
  if (h > 2 * r) out.lineTo(x, y + r);
  out.cubicTo(x, y + r - k, x + r - k, y, x + r, y);
  out.close();
}

}  // namespace anycanvas
