// anycanvas/css_color.h ALONE: built with only core/include on the include path (the header needs
// nothing from core/src — other engines include it by path) and with warnings as errors (consumers
// build it under -Wall -Werror). The float bits of every golden input are pinned by
// tests/golden/colors; this pins the rules around them: the byte rounding, the rejections, the
// out-untouched contract, the table's order.
#include "anycanvas/css_color.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>

using namespace anycanvas;

static int g_failed = 0, g_checks = 0;

static void check(bool ok, int line, const char* what) {
  g_checks++;
  if (!ok) {
    g_failed++;
    std::printf("  FAIL %s:%d: %s\n", __FILE__, line, what);
  }
}

static bool rgba8(const char* s, uint32_t expected) {
  uint32_t v = 0;
  return css::parseColorRGBA8(s, v) && v == expected;
}

static bool rejects(const char* s) {
  css::Rgba c;
  c.r = 0.25f;
  c.g = 0.5f;
  c.b = 0.75f;
  c.a = 0.125f;
  uint32_t v = 0x12345678u;
  // `out` untouched on failure, in both entry points.
  return !css::parseColor(s, c) && c.r == 0.25f && c.g == 0.5f && c.b == 0.75f && c.a == 0.125f &&
         !css::parseColorRGBA8(s, v) && v == 0x12345678u;
}

static void testTable() {
  size_t n = 0;
  const css::NamedColor* t = css::namedColors(n);
  check(n == 148, __LINE__, "148 CSS Color 4 names");
  size_t longest = 0;
  bool sorted = true, lower = true, opaque = true;
  for (size_t i = 0; i < n; i++) {
    if (i > 0 && std::strcmp(t[i - 1].name, t[i].name) >= 0) sorted = false;
    for (const char* p = t[i].name; *p; ++p) if (*p < 'a' || *p > 'z') lower = false;
    if (t[i].rgb > 0xffffffu) opaque = false;
    if (std::strlen(t[i].name) > longest) longest = std::strlen(t[i].name);
    // Every entry parses to itself, in any case.
    std::string upper = t[i].name;
    for (char& ch : upper) ch = (char)(ch - 'a' + 'A');
    if (!rgba8(t[i].name, (t[i].rgb << 8) | 0xffu) || !rgba8(upper.c_str(), (t[i].rgb << 8) | 0xffu))
      check(false, __LINE__, t[i].name);
  }
  check(sorted, __LINE__, "the table is sorted (binary search)");
  check(lower, __LINE__, "the names are lower-case ASCII");
  check(opaque, __LINE__, "the values are 0xRRGGBB");
  check(longest == 20, __LINE__, "lightgoldenrodyellow is the longest name (the lookup bound)");
}

static void testBytes() {
  bool roundTrip = true;
  for (uint32_t k = 0; k < 256; k++) {
    if (css::toByte((float)k / 255.0f) != k) roundTrip = false;
    char hex[16];
    std::snprintf(hex, sizeof hex, "#%02x%02x%02x%02x", k, k, k, k);
    if (!rgba8(hex, k * 0x01010101u)) roundTrip = false;
  }
  check(roundTrip, __LINE__, "k/255 -> k, #kkkkkkkk -> 0xkkkkkkkk for every byte");
  check(css::toByte(0.5f) == 128, __LINE__, "0.5 rounds half up to 128");
  check(css::toByte(0.8f) == 204 && css::toByte(0.3f) == 77, __LINE__, "0.8 -> 204, 0.3 -> 77 (like the browser)");
  check(css::toByte(-1) == 0 && css::toByte(2) == 255 && css::toByte(1) == 255 && css::toByte(0) == 0, __LINE__, "clamped");
  check(css::toByte(std::numeric_limits<float>::quiet_NaN()) == 0, __LINE__, "NaN -> 0");
  check(css::toByte(std::numeric_limits<float>::infinity()) == 255, __LINE__, "inf -> 255");
  check(rgba8("#ff000080", 0xff000080u), __LINE__, "#ff000080");
  check(rgba8("rgb(127.5, 0, 0)", 0x800000ffu), __LINE__, "127.5 -> 128");
  check(rgba8("rgba(0, 0, 0, .5)", 0x00000080u), __LINE__, "alpha .5 -> 128");
  check(rgba8("rgb(255 0 0 / 50%)", 0xff000080u), __LINE__, "alpha 50% -> 128");
  check(rgba8("#f008", 0xff000088u), __LINE__, "#rgba");
}

static void testGrammar() {
  check(rgba8("green", 0x008000ffu) && rgba8("lime", 0x00ff00ffu), __LINE__, "CSS green, lime");
  check(rgba8("RED", 0xff0000ffu) && rgba8("Red", 0xff0000ffu) && rgba8(" red\t", 0xff0000ffu), __LINE__, "case, spaces");
  check(rgba8("rebeccapurple", 0x663399ffu), __LINE__, "rebeccapurple");
  check(rgba8("transparent", 0) && rgba8("TRANSPARENT", 0), __LINE__, "transparent");
  check(rgba8("clear", 0) && rgba8("Clear", 0), __LINE__, "clear = transparent (the one alias)");
  check(rgba8("hsl(120, 100%, 25%)", 0x008000ffu), __LINE__, "hsl");
  check(rgba8("hsl(0.5turn, 100%, 50%)", 0x00ffffffu), __LINE__, "turn");
  css::Rgba c;
  check(css::parseColor("hsl(0.5turn, 100%, 50%)", c) && c.g == 1, __LINE__, "hsl outputs clamp to 1 (g was 1.00000012)");
  check(rgba8("rgba(255, 0, 0, 128)", 0xff0000ffu), __LINE__, "an alpha above 1 clamps (never 0..255)");
  check(rgba8("rgb(1e38, 0, 0)", 0xff0000ffu), __LINE__, "a huge finite value clamps");
  check(rgba8("rgb(1.0000000000000000000000000000000000000000000000000000000000000, 0, 0)", 0x010000ffu), __LINE__, "a 63-char number");
  check(rejects("rgb(1.00000000000000000000000000000000000000000000000000000000000000, 0, 0)"), __LINE__, "a 64-char number rejects");

  check(rejects(nullptr), __LINE__, "nullptr");
  check(rejects("") && rejects("  ") && rejects("nonsense"), __LINE__, "empty, junk");
  check(rejects("#12345") && rejects("#ggg") && rejects("#12zz56") && rejects("#ff00zz"), __LINE__, "bad hex");
  check(rejects("rgb(nan, 0, 0)") && rejects("rgb(inf, 0, 0)") && rejects("rgb(0x10, 0, 0)"), __LINE__, "nan / inf / hex numbers");
  check(rejects("hsl(1e999, 50%, 50%)") && rejects("rgb(1e39, 0, 0)") && rejects("hsl(1e37turn, 50%, 50%)"), __LINE__, "non-finite");
  check(rejects("rgb(10px, 0, 0)") && rejects("rgb(1e, 0, 0)") && rejects("hsl(120px, 100%, 50%)"), __LINE__, "unknown units");
  check(rejects("rgba(#ff0000, .5)") && rejects("currentcolor") && rejects("rgb (1,2,3)"), __LINE__, "not CSS here");
  check(rejects("rgb(1,2)") && rejects("rgb(1,2,3,4,5)") && rejects("rgb(1,2,3"), __LINE__, "argument count, parens");
  check(rejects("\xc2\xa0red") && rejects("r\xd0\xb5" "d") && rejects("\xe2\x84\xaahaki"), __LINE__, "ASCII only");
  check(rejects("lightgoldenrodyellowx") && rejects("constructor"), __LINE__, "not a name");
}

int main() {
  testTable();
  testBytes();
  testGrammar();
  std::printf("%d checks, %d failed\n", g_checks, g_failed);
  return g_failed ? 1 : 0;
}
