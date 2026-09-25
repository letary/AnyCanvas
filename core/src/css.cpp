#include "css.h"

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace anycanvas {

namespace {

struct NamedColor { const char* name; uint32_t rgb; };
// The CSS Color Module 4 named colors (148) + transparent.
const NamedColor kNamed[] = {
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

std::string lowerTrim(const char* s) {
  std::string out;
  const char* end = s + std::strlen(s);
  while (s < end && std::isspace((unsigned char)*s)) s++;
  while (end > s && std::isspace((unsigned char)end[-1])) end--;
  for (; s < end; ++s) out += (char)std::tolower((unsigned char)*s);
  return out;
}

int hexNibble(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return -1;
}

// The arguments of a functional notation: "1, 2, 3" / "1 2 3 / 0.5" → numbers with a % flag each.
struct Num { float v; bool percent; };
bool splitArgs(const std::string& inner, std::vector<Num>& out) {
  size_t i = 0;
  while (i < inner.size()) {
    while (i < inner.size() && (std::isspace((unsigned char)inner[i]) || inner[i] == ',' || inner[i] == '/')) i++;
    if (i >= inner.size()) break;
    const char* start = inner.c_str() + i;
    char* endp = nullptr;
    const float v = std::strtof(start, &endp);
    if (endp == start) return false;
    i = (size_t)(endp - inner.c_str());
    bool pct = false;
    if (i < inner.size() && inner[i] == '%') { pct = true; i++; }
    else if (i < inner.size() && std::isalpha((unsigned char)inner[i])) {   // deg / rad / turn on hue: skip the unit
      std::string unit;
      while (i < inner.size() && std::isalpha((unsigned char)inner[i])) unit += inner[i++];
      if (unit == "rad") out.push_back({ v * 180.0f / 3.14159265f, false });
      else if (unit == "turn") out.push_back({ v * 360.0f, false });
      else if (unit == "grad") out.push_back({ v * 0.9f, false });
      else out.push_back({ v, false });
      continue;
    }
    out.push_back({ v, pct });
  }
  return !out.empty();
}

float clamp01(float v) { return v < 0 ? 0 : v > 1 ? 1 : v; }

float hueToRgb(float p, float q, float t) {
  if (t < 0) t += 1;
  if (t > 1) t -= 1;
  if (t < 1.0f / 6) return p + (q - p) * 6 * t;
  if (t < 0.5f) return q;
  if (t < 2.0f / 3) return p + (q - p) * (2.0f / 3 - t) * 6;
  return p;
}

}  // namespace

bool parseCssColor(const char* in, Color& out) {
  if (!in) return false;
  const std::string s = lowerTrim(in);
  if (s.empty()) return false;

  if (s[0] == '#') {
    const size_t n = s.size() - 1;
    if (n != 3 && n != 4 && n != 6 && n != 8) return false;
    int v[8];
    for (size_t i = 0; i < n; i++) { v[i] = hexNibble(s[i + 1]); if (v[i] < 0) return false; }
    if (n <= 4) {
      out = Color::rgba8((uint32_t)v[0] * 17, (uint32_t)v[1] * 17, (uint32_t)v[2] * 17, n == 4 ? (uint32_t)v[3] * 17 : 255);
    } else {
      out = Color::rgba8((uint32_t)(v[0] * 16 + v[1]), (uint32_t)(v[2] * 16 + v[3]), (uint32_t)(v[4] * 16 + v[5]),
                         n == 8 ? (uint32_t)(v[6] * 16 + v[7]) : 255);
    }
    return true;
  }

  const size_t paren = s.find('(');
  if (paren != std::string::npos && s.back() == ')') {
    const std::string fn = s.substr(0, paren);
    std::vector<Num> args;
    if (!splitArgs(s.substr(paren + 1, s.size() - paren - 2), args) || args.size() < 3 || args.size() > 4) return false;
    float alpha = 1;
    if (args.size() == 4) alpha = clamp01(args[3].percent ? args[3].v / 100.0f : args[3].v);
    if (fn == "rgb" || fn == "rgba") {
      float c[3];
      for (int i = 0; i < 3; i++) c[i] = clamp01(args[i].percent ? args[i].v / 100.0f : args[i].v / 255.0f);
      out = { c[0], c[1], c[2], alpha };
      return true;
    }
    if (fn == "hsl" || fn == "hsla") {
      float h = std::fmod(args[0].v, 360.0f); if (h < 0) h += 360;
      h /= 360.0f;
      const float sat = clamp01(args[1].v / 100.0f), l = clamp01(args[2].v / 100.0f);
      if (sat <= 0) { out = { l, l, l, alpha }; return true; }
      const float q = l < 0.5f ? l * (1 + sat) : l + sat - l * sat;
      const float p = 2 * l - q;
      out = { hueToRgb(p, q, h + 1.0f / 3), hueToRgb(p, q, h), hueToRgb(p, q, h - 1.0f / 3), alpha };
      return true;
    }
    return false;
  }

  if (s == "transparent") { out = { 0, 0, 0, 0 }; return true; }
  for (const auto& nc : kNamed) {
    if (s == nc.name) { out = Color::rgba8((nc.rgb >> 16) & 255, (nc.rgb >> 8) & 255, nc.rgb & 255, 255); return true; }
  }
  return false;
}

FontData parseCssFont(const char* in) {
  FontData f;
  if (!in) return f;
  // Tokenize on spaces, keeping quoted families whole.
  std::vector<std::string> tokens;
  {
    std::string cur;
    char quote = 0;
    for (const char* p = in; *p; ++p) {
      if (quote) { if (*p == quote) quote = 0; else cur += *p; continue; }
      if (*p == '"' || *p == '\'') { quote = *p; continue; }
      if (std::isspace((unsigned char)*p)) { if (!cur.empty()) { tokens.push_back(cur); cur.clear(); } continue; }
      cur += *p;
    }
    if (!cur.empty()) tokens.push_back(cur);
  }
  size_t familyStart = tokens.size();
  for (size_t t = 0; t < tokens.size(); t++) {
    std::string low;
    for (char c : tokens[t]) low += (char)std::tolower((unsigned char)c);
    char* endp = nullptr;
    const float v = std::strtof(low.c_str(), &endp);
    if (endp != low.c_str()) {
      std::string rest(endp);
      const size_t slash = rest.find('/');           // "16px/20px": the line height is ignored
      const std::string unit = slash == std::string::npos ? rest : rest.substr(0, slash);
      const bool integer = low.find('.') == std::string::npos && low.find('e') == std::string::npos;
      if (unit.empty() && slash == std::string::npos && integer && v >= 1 && v <= 1000) { f.weight = (int32_t)v; continue; }
      float px = v;
      if (unit == "pt") px = v * 4.0f / 3.0f;
      else if (unit == "em" || unit == "rem") px = v * 16.0f;
      else if (unit == "%") px = v * 0.16f;
      else if (!unit.empty() && unit != "px") { familyStart = t; break; }   // not a size: the family starts here
      if (px > 0) f.size = px;
      familyStart = t + 1;
      break;
    }
    if (low == "italic" || low == "oblique") f.italic = true;
    else if (low == "bold" || low == "bolder") f.weight = 700;
    else if (low == "lighter") f.weight = 300;
    else if (low == "normal" || low == "small-caps" || low == "ultra-condensed" || low == "condensed" || low == "expanded") { /* ignored */ }
    else { familyStart = t; break; }   // no size before the family: the rest is the family
  }
  std::string family;
  for (size_t t = familyStart; t < tokens.size(); t++) {
    if (!family.empty()) family += ' ';
    family += tokens[t];
  }
  const size_t comma = family.find(',');
  if (comma != std::string::npos) family = family.substr(0, comma);
  while (!family.empty() && std::isspace((unsigned char)family.back())) family.pop_back();
  while (!family.empty() && std::isspace((unsigned char)family.front())) family.erase(family.begin());
  if (!family.empty()) f.family = family;
  return f;
}

}  // namespace anycanvas
