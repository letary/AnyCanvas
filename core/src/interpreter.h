// The opcode interpreter: an opcode stream (spec/ops.h) → a draw list (spec/draw.h).
#pragma once

#include "drawlist.h"

namespace anycanvas {

// Replays `cmd` (cmdLen words) with the string table `refs` into `out` (cleared first). `scale` is the
// device-pixel multiplier: the base transform, so the draw list is in device px and a painter of a
// w*scale × h*scale surface replays it as is. Stops at the first unknown or truncated op (the drawing
// so far stays) and always leaves the painter's save stack balanced. Returns the number of ops consumed.
int32_t interpret(const float* cmd, int32_t cmdLen, const char* const* refs, int32_t refsCount, float scale, DrawList& out);

}  // namespace anycanvas
