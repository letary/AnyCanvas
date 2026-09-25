// The font shorthand. Colors are the header-only parser of anycanvas/css_color.h (css.h wraps it).
#include "css.h"

#include <cctype>
#include <cstdlib>
#include <string>
#include <vector>

namespace anycanvas {

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
