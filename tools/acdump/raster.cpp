#define NANOSVG_CPLUSPLUS
#include "nanosvg/nanosvg.h"          // declarations only: the implementation lives in core (svg.cpp)
#define NANOSVGRAST_IMPLEMENTATION
#include "nanosvg/nanosvgrast.h"

#include "raster.h"
#include "svg.h"

#include <cmath>

bool rasterizeSvg(const anycanvas::Svg& svg, float scale, std::vector<uint8_t>& rgba, int32_t& w, int32_t& h) {
  if (!svg.image || !(scale > 0)) return false;
  w = (int32_t)std::lround(svg.image->width * scale);
  h = (int32_t)std::lround(svg.image->height * scale);
  if (w < 1) w = 1;
  if (h < 1) h = 1;
  NSVGrasterizer* rast = nsvgCreateRasterizer();
  if (!rast) return false;
  rgba.assign((size_t)w * h * 4, 0);
  nsvgRasterize(rast, svg.image, 0, 0, scale, rgba.data(), w, h, w * 4);
  nsvgDeleteRasterizer(rast);
  return true;
}
