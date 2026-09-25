// Canvas2D path primitives as verbs: arcs, ellipses and rounded rectangles become cubic Béziers here,
// once, so every painter draws the same geometry.
#pragma once

#include "drawlist.h"

namespace anycanvas {

// Canvas2D arc(x, y, r, a0, a1, ccw): a straight line from the current point (hasCur, cx0/cy0) to
// the arc start (or a moveTo without one), then the sweep in ≤ 90° cubic segments.
void appendArc(Path& out, bool hasCur, float cx0, float cy0, float x, float y, float r, float a0, float a1, bool ccw);

// Canvas2D ellipse(x, y, rx, ry, rotation, a0, a1, ccw).
void appendEllipse(Path& out, bool hasCur, float cx0, float cy0, float x, float y, float rx, float ry, float rotation, float a0, float a1, bool ccw);

// Canvas2D arcTo(x1, y1, x2, y2, r) from the current point (x0, y0); without one, a moveTo(x1, y1).
void appendArcTo(Path& out, bool hasCur, float x0, float y0, float x1, float y1, float x2, float y2, float r);

// A closed rectangle subpath; the current point ends at (x, y).
void appendRect(Path& out, float x, float y, float w, float h);

// A closed rounded rectangle (one radius, clamped to half the shorter side).
void appendRoundRect(Path& out, float x, float y, float w, float h, float r);

}  // namespace anycanvas
