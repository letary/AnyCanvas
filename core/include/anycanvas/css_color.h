// The CSS color parser of AnyCanvas — one header, C++17, the standard library only: no macros, no
// <cctype> (ASCII whitespace and case only, locale-free), no heap. The canvas core resolves
// fillStyle / strokeStyle / gradient stops and SVG's color attributes through it; other engines
// include it BY PATH (a sibling engine's public include dir) and never link the library. Its
// TypeScript twin is recorders/ts/src/cssColor.ts; tests/golden/colors holds the two bit-equal.
//
// Grammar — case-insensitive, surrounding ASCII whitespace ignored:
//   #rgb #rgba #rrggbb #rrggbbaa
//   rgb() / rgba()   three or four arguments separated by commas, spaces or "/" (free-form): numbers
//                    0..255 or percentages; the fourth is the alpha, 0..1 or a percentage
//   hsl() / hsla()   the hue in degrees (a bare number or deg / rad / grad / turn), saturation and
//                    lightness read as percentages (with or without the "%"), the same alpha
//   the 148 CSS Color 4 names · transparent · clear (the ONE non-CSS alias, = transparent)
// Numbers are CSS number tokens: [+-] digits [. digits] [e [+-] digits] (nan, inf, 0x.. reject), a
// unit other than deg / rad / grad / turn rejects, a non-finite value rejects. Out-of-range values
// clamp to 0..1 (an alpha above 1 is 1, never read as 0..255). Anything else is not a color:
// parseColor returns false and leaves `out` untouched (a canvas keeps its previous style then, like
// the browser). Not accepted: rgba(#hex, a), currentcolor, hwb() / lab() / color().
//
// Floats: straight (non-premultiplied) float32 0..1, computed in a FIXED operation order — the canvas
// core's goldens and the TS twin depend on it bit for bit. Every function is `static inline`: each
// consumer keeps its own copy under its own fp flags (a plain inline would leave one arbitrary copy
// of whichever TU the linker picks). Every multiply-add is split into separate statements, because
// clang contracts a*b+c into an FMA within one expression.
//
// Bytes: toByte = clamp, then round half up in float32 (0.5 -> 128, k/255 -> k). toRGBA8 packs
// 0xRRGGBBAA — the one wire layout of the project.
#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace anycanvas {
namespace css {

// A color: straight float32 RGBA, 0..1.
struct Rgba { float r = 0, g = 0, b = 0, a = 1; };

// A named color: 0xRRGGBB, opaque.
struct NamedColor { const char* name; uint32_t rgb; };

// The CSS Color Module 4 named colors (148), sorted by name (strcmp order). `transparent` and the alias
// `clear` are keywords of parseColor, not entries: they carry an alpha.
inline constexpr NamedColor kNamedColors[] = {
  { "aliceblue", 0xf0f8ff }, { "antiquewhite", 0xfaebd7 }, { "aqua", 0x00ffff }, { "aquamarine", 0x7fffd4 },
  { "azure", 0xf0ffff }, { "beige", 0xf5f5dc }, { "bisque", 0xffe4c4 }, { "black", 0x000000 },
  { "blanchedalmond", 0xffebcd }, { "blue", 0x0000ff }, { "blueviolet", 0x8a2be2 }, { "brown", 0xa52a2a },
  { "burlywood", 0xdeb887 }, { "cadetblue", 0x5f9ea0 }, { "chartreuse", 0x7fff00 }, { "chocolate", 0xd2691e },
  { "coral", 0xff7f50 }, { "cornflowerblue", 0x6495ed }, { "cornsilk", 0xfff8dc }, { "crimson", 0xdc143c },
  { "cyan", 0x00ffff }, { "darkblue", 0x00008b }, { "darkcyan", 0x008b8b }, { "darkgoldenrod", 0xb8860b },
  { "darkgray", 0xa9a9a9 }, { "darkgreen", 0x006400 }, { "darkgrey", 0xa9a9a9 }, { "darkkhaki", 0xbdb76b },
  { "darkmagenta", 0x8b008b }, { "darkolivegreen", 0x556b2f }, { "darkorange", 0xff8c00 }, { "darkorchid", 0x9932cc },
  { "darkred", 0x8b0000 }, { "darksalmon", 0xe9967a }, { "darkseagreen", 0x8fbc8f }, { "darkslateblue", 0x483d8b },
  { "darkslategray", 0x2f4f4f }, { "darkslategrey", 0x2f4f4f }, { "darkturquoise", 0x00ced1 }, { "darkviolet", 0x9400d3 },
  { "deeppink", 0xff1493 }, { "deepskyblue", 0x00bfff }, { "dimgray", 0x696969 }, { "dimgrey", 0x696969 },
  { "dodgerblue", 0x1e90ff }, { "firebrick", 0xb22222 }, { "floralwhite", 0xfffaf0 }, { "forestgreen", 0x228b22 },
  { "fuchsia", 0xff00ff }, { "gainsboro", 0xdcdcdc }, { "ghostwhite", 0xf8f8ff }, { "gold", 0xffd700 },
  { "goldenrod", 0xdaa520 }, { "gray", 0x808080 }, { "green", 0x008000 }, { "greenyellow", 0xadff2f },
  { "grey", 0x808080 }, { "honeydew", 0xf0fff0 }, { "hotpink", 0xff69b4 }, { "indianred", 0xcd5c5c },
  { "indigo", 0x4b0082 }, { "ivory", 0xfffff0 }, { "khaki", 0xf0e68c }, { "lavender", 0xe6e6fa },
  { "lavenderblush", 0xfff0f5 }, { "lawngreen", 0x7cfc00 }, { "lemonchiffon", 0xfffacd }, { "lightblue", 0xadd8e6 },
  { "lightcoral", 0xf08080 }, { "lightcyan", 0xe0ffff }, { "lightgoldenrodyellow", 0xfafad2 }, { "lightgray", 0xd3d3d3 },
  { "lightgreen", 0x90ee90 }, { "lightgrey", 0xd3d3d3 }, { "lightpink", 0xffb6c1 }, { "lightsalmon", 0xffa07a },
  { "lightseagreen", 0x20b2aa }, { "lightskyblue", 0x87cefa }, { "lightslategray", 0x778899 }, { "lightslategrey", 0x778899 },
  { "lightsteelblue", 0xb0c4de }, { "lightyellow", 0xffffe0 }, { "lime", 0x00ff00 }, { "limegreen", 0x32cd32 },
  { "linen", 0xfaf0e6 }, { "magenta", 0xff00ff }, { "maroon", 0x800000 }, { "mediumaquamarine", 0x66cdaa },
  { "mediumblue", 0x0000cd }, { "mediumorchid", 0xba55d3 }, { "mediumpurple", 0x9370db }, { "mediumseagreen", 0x3cb371 },
  { "mediumslateblue", 0x7b68ee }, { "mediumspringgreen", 0x00fa9a }, { "mediumturquoise", 0x48d1cc }, { "mediumvioletred", 0xc71585 },
  { "midnightblue", 0x191970 }, { "mintcream", 0xf5fffa }, { "mistyrose", 0xffe4e1 }, { "moccasin", 0xffe4b5 },
  { "navajowhite", 0xffdead }, { "navy", 0x000080 }, { "oldlace", 0xfdf5e6 }, { "olive", 0x808000 },
  { "olivedrab", 0x6b8e23 }, { "orange", 0xffa500 }, { "orangered", 0xff4500 }, { "orchid", 0xda70d6 },
  { "palegoldenrod", 0xeee8aa }, { "palegreen", 0x98fb98 }, { "paleturquoise", 0xafeeee }, { "palevioletred", 0xdb7093 },
  { "papayawhip", 0xffefd5 }, { "peachpuff", 0xffdab9 }, { "peru", 0xcd853f }, { "pink", 0xffc0cb },
  { "plum", 0xdda0dd }, { "powderblue", 0xb0e0e6 }, { "purple", 0x800080 }, { "rebeccapurple", 0x663399 },
  { "red", 0xff0000 }, { "rosybrown", 0xbc8f8f }, { "royalblue", 0x4169e1 }, { "saddlebrown", 0x8b4513 },
  { "salmon", 0xfa8072 }, { "sandybrown", 0xf4a460 }, { "seagreen", 0x2e8b57 }, { "seashell", 0xfff5ee },
  { "sienna", 0xa0522d }, { "silver", 0xc0c0c0 }, { "skyblue", 0x87ceeb }, { "slateblue", 0x6a5acd },
  { "slategray", 0x708090 }, { "slategrey", 0x708090 }, { "snow", 0xfffafa }, { "springgreen", 0x00ff7f },
  { "steelblue", 0x4682b4 }, { "tan", 0xd2b48c }, { "teal", 0x008080 }, { "thistle", 0xd8bfd8 },
  { "tomato", 0xff6347 }, { "turquoise", 0x40e0d0 }, { "violet", 0xee82ee }, { "wheat", 0xf5deb3 },
  { "white", 0xffffff }, { "whitesmoke", 0xf5f5f5 }, { "yellow", 0xffff00 }, { "yellowgreen", 0x9acd32 },
};

// The named-color table (without the transparent / clear keywords); `n` = its length.
static inline const NamedColor* namedColors(size_t& n) {
  n = sizeof(kNamedColors) / sizeof(kNamedColors[0]);
  return kNamedColors;
}

namespace detail {

static inline bool isSpace(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r'; }
static inline bool isDigit(char c) { return c >= '0' && c <= '9'; }
static inline char lower(char c) { return c >= 'A' && c <= 'Z' ? (char)(c - 'A' + 'a') : c; }
static inline bool isLetter(char c) { const char l = lower(c); return l >= 'a' && l <= 'z'; }

static inline int hexNibble(char c) {
  const char l = lower(c);
  if (l >= '0' && l <= '9') return l - '0';
  if (l >= 'a' && l <= 'f') return l - 'a' + 10;
  return -1;
}

// s[0..n) against a lower-case ASCII word, ignoring the case of s: <0, 0, >0 like strcmp.
static inline int compareLower(const char* s, size_t n, const char* word) {
  for (size_t i = 0; i < n; i++) {
    const unsigned char a = (unsigned char)lower(s[i]), w = (unsigned char)word[i];
    if (w == 0) return 1;
    if (a != w) return a < w ? -1 : 1;
  }
  return word[n] == 0 ? 0 : -1;
}
static inline bool equalsLower(const char* s, size_t n, const char* word) { return compareLower(s, n, word) == 0; }

// The length of the CSS number token at the start of s[0..n): [+-]? (digits (. digits*)? | . digits)
// ([eE] [+-]? digits)?; 0 when there is none. An "e" without digits after it is not part of the token.
static inline size_t numberLength(const char* s, size_t n) {
  size_t i = 0;
  if (i < n && (s[i] == '+' || s[i] == '-')) i++;
  size_t intDigits = 0, fracDigits = 0;
  while (i < n && isDigit(s[i])) { i++; intDigits++; }
  if (i < n && s[i] == '.') {
    size_t j = i + 1;
    while (j < n && isDigit(s[j])) { j++; fracDigits++; }
    if (intDigits > 0 || fracDigits > 0) i = j;
  }
  if (intDigits == 0 && fracDigits == 0) return 0;
  if (i < n && (s[i] == 'e' || s[i] == 'E')) {
    size_t j = i + 1;
    if (j < n && (s[j] == '+' || s[j] == '-')) j++;
    if (j < n && isDigit(s[j])) {
      while (j < n && isDigit(s[j])) j++;
      i = j;
    }
  }
  return i;
}

struct Arg { float v; bool percent; };

// The arguments of a functional notation, s[0..n) between the parentheses: "1, 2, 3" / "1 2 3 / 0.5"
// → 3 or 4 numbers with a % flag each. Separators (spaces, commas, slashes) are free-form. A number is
// converted by strtof on the validated token (from a bounded stack buffer: a longer token rejects).
static inline bool parseArgs(const char* s, size_t n, Arg (&out)[4], size_t& count) {
  count = 0;
  size_t i = 0;
  while (i < n) {
    while (i < n && (isSpace(s[i]) || s[i] == ',' || s[i] == '/')) i++;
    if (i >= n) break;
    const size_t len = numberLength(s + i, n - i);
    char buf[64];
    if (len == 0 || len >= sizeof(buf)) return false;
    std::memcpy(buf, s + i, len);
    buf[len] = '\0';
    float v = std::strtof(buf, nullptr);
    i += len;
    bool percent = false;
    if (i < n && s[i] == '%') {
      percent = true;
      i++;
    } else if (i < n && isLetter(s[i])) {   // an angle unit (on any argument, as it always was)
      const size_t u = i;
      while (i < n && isLetter(s[i])) i++;
      const size_t ul = i - u;
      if (equalsLower(s + u, ul, "deg")) {
      } else if (equalsLower(s + u, ul, "rad")) {
        v = v * 180.0f;
        v = v / 3.14159265f;
      } else if (equalsLower(s + u, ul, "turn")) {
        v = v * 360.0f;
      } else if (equalsLower(s + u, ul, "grad")) {
        v = v * 0.9f;
      } else {
        return false;
      }
    }
    if (!std::isfinite(v)) return false;
    if (count == 4) return false;
    out[count].v = v;
    out[count].percent = percent;
    count++;
  }
  return count >= 3;
}

static inline float clamp01(float v) { return v < 0 ? 0 : v > 1 ? 1 : v; }

static inline float hueToRgb(float p, float q, float t) {
  if (t < 0) t += 1;
  if (t > 1) t -= 1;
  const float d = q - p;
  if (t < 1.0f / 6) {
    float m = d * 6;
    m = m * t;
    return p + m;
  }
  if (t < 0.5f) return q;
  if (t < 2.0f / 3) {
    const float u = 2.0f / 3 - t;
    float m = d * u;
    m = m * 6;
    return p + m;
  }
  return p;
}

static inline Rgba fromBytes(uint32_t r, uint32_t g, uint32_t b, uint32_t a) {
  Rgba c;
  c.r = (float)r / 255.0f;
  c.g = (float)g / 255.0f;
  c.b = (float)b / 255.0f;
  c.a = (float)a / 255.0f;
  return c;
}

}  // namespace detail

// A CSS color → straight float32 RGBA. False (and `out` untouched) when `s` is not a color or null.
static inline bool parseColor(const char* s, Rgba& out) {
  using namespace detail;
  if (!s) return false;
  const char* b = s;
  const char* e = s + std::strlen(s);
  while (b < e && isSpace(*b)) b++;
  while (e > b && isSpace(e[-1])) e--;
  const size_t n = (size_t)(e - b);
  if (n == 0) return false;

  if (b[0] == '#') {
    const size_t digits = n - 1;
    if (digits != 3 && digits != 4 && digits != 6 && digits != 8) return false;
    uint32_t v[8] = {};
    for (size_t i = 0; i < digits; i++) {
      const int h = hexNibble(b[i + 1]);
      if (h < 0) return false;
      v[i] = (uint32_t)h;
    }
    if (digits <= 4) out = fromBytes(v[0] * 17, v[1] * 17, v[2] * 17, digits == 4 ? v[3] * 17 : 255);
    else out = fromBytes(v[0] * 16 + v[1], v[2] * 16 + v[3], v[4] * 16 + v[5], digits == 8 ? v[6] * 16 + v[7] : 255);
    return true;
  }

  const char* paren = (const char*)std::memchr(b, '(', n);
  if (paren && e[-1] == ')') {
    const size_t fnLen = (size_t)(paren - b);
    Arg args[4] = {};
    size_t count = 0;
    if (!parseArgs(paren + 1, (size_t)(e - 1 - (paren + 1)), args, count)) return false;
    float alpha = 1;
    if (count == 4) alpha = clamp01(args[3].percent ? args[3].v / 100.0f : args[3].v);
    if (equalsLower(b, fnLen, "rgb") || equalsLower(b, fnLen, "rgba")) {
      float c[3];
      for (int i = 0; i < 3; i++) c[i] = clamp01(args[i].percent ? args[i].v / 100.0f : args[i].v / 255.0f);
      Rgba o;
      o.r = c[0];
      o.g = c[1];
      o.b = c[2];
      o.a = alpha;
      out = o;
      return true;
    }
    if (equalsLower(b, fnLen, "hsl") || equalsLower(b, fnLen, "hsla")) {
      float h = std::fmod(args[0].v, 360.0f);
      if (h < 0) h += 360;
      h /= 360.0f;
      const float sat = clamp01(args[1].v / 100.0f);
      const float l = clamp01(args[2].v / 100.0f);
      Rgba o;
      o.a = alpha;
      if (sat <= 0) {
        o.r = l;
        o.g = l;
        o.b = l;
        out = o;
        return true;
      }
      float q;
      if (l < 0.5f) {
        const float k = 1 + sat;
        q = l * k;
      } else {
        const float sum = l + sat;
        const float prod = l * sat;
        q = sum - prod;
      }
      const float l2 = 2 * l;
      const float p = l2 - q;
      o.r = clamp01(hueToRgb(p, q, h + 1.0f / 3));
      o.g = clamp01(hueToRgb(p, q, h));
      o.b = clamp01(hueToRgb(p, q, h - 1.0f / 3));
      out = o;
      return true;
    }
    return false;
  }

  // A name: looked up only when it can be one ('lightgoldenrodyellow' is the longest, 20).
  if (n > 20) return false;
  if (equalsLower(b, n, "transparent") || equalsLower(b, n, "clear")) {
    out = fromBytes(0, 0, 0, 0);
    return true;
  }
  size_t lo = 0, hi = sizeof(kNamedColors) / sizeof(kNamedColors[0]);
  while (lo < hi) {
    const size_t mid = lo + (hi - lo) / 2;
    const int cmp = compareLower(b, n, kNamedColors[mid].name);
    if (cmp == 0) {
      const uint32_t rgb = kNamedColors[mid].rgb;
      out = fromBytes((rgb >> 16) & 255, (rgb >> 8) & 255, rgb & 255, 255);
      return true;
    }
    if (cmp < 0) hi = mid;
    else lo = mid + 1;
  }
  return false;
}

// A float channel → a byte: clamp (NaN → 0), then round half up in float32 (k/255 → k, 0.5 → 128).
static inline uint32_t toByte(float v) {
  if (!(v > 0)) return 0;
  if (v >= 1) return 255;
  const float s = v * 255.0f;
  return (uint32_t)(s + 0.5f);
}

// 0xRRGGBBAA.
static inline uint32_t toRGBA8(const Rgba& c) {
  return (toByte(c.r) << 24) | (toByte(c.g) << 16) | (toByte(c.b) << 8) | toByte(c.a);
}

// A CSS color → 0xRRGGBBAA. False (and `out` untouched) when `s` is not a color or null.
static inline bool parseColorRGBA8(const char* s, uint32_t& out) {
  Rgba c;
  if (!parseColor(s, c)) return false;
  out = toRGBA8(c);
  return true;
}

}  // namespace css
}  // namespace anycanvas
