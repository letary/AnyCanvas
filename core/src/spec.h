// The spec tables as C++ enums and lookup tables (from spec/*.h — the single source).
#pragma once

#include "../../spec/enums.h"
#include "../../spec/ops.h"
#include "../../spec/draw.h"

#include <cstdint>

namespace anycanvas {

// ---- enums ----------------------------------------------------------------------------------------
#define AC_ENUM_MEMBER(NAME, VALUE) NAME = VALUE,
#define AC_DEFINE_ENUM(Name, MACRO) enum class Name : int32_t { MACRO(AC_ENUM_MEMBER) };
ANYCANVAS_ENUMS(AC_DEFINE_ENUM)
#undef AC_DEFINE_ENUM
#undef AC_ENUM_MEMBER

// The lowercase member name of a wire value (the JSON dump); "?" when out of range.
#define AC_ENUM_NAME_CASE(NAME, VALUE) case VALUE: return ac_lower(#NAME);
#define AC_DEFINE_ENUM_NAME(E, MACRO) \
  inline const char* E##Name(int32_t v) { switch (v) { MACRO(AC_ENUM_NAME_CASE) default: return "?"; } }

// A tiny static-string lowercaser (names are ASCII identifiers; the table is built once per name).
const char* ac_lower(const char* upper);

ANYCANVAS_ENUMS(AC_DEFINE_ENUM_NAME)
#undef AC_DEFINE_ENUM_NAME
#undef AC_ENUM_NAME_CASE

// ---- ops ------------------------------------------------------------------------------------------
enum class Op : int32_t {
#define AC_OP(NAME, ID, ARGS, REPEAT, DOC) NAME = ID,
  ANYCANVAS_OPS(AC_OP)
#undef AC_OP
};

struct OpLayout { const char* name; const char* args; const char* repeat; };
// Indexed by op id; a null name = an unknown id.
const OpLayout* opLayout(int32_t id);
int32_t opCount();

// ---- draw commands --------------------------------------------------------------------------------
enum class Draw : int32_t {
#define AC_DRAW(NAME, ID, DOC) NAME = ID,
  ANYCANVAS_DRAW(AC_DRAW)
#undef AC_DRAW
};
// The camelCase name of a draw command (the JSON dump); null for an unknown id.
const char* drawName(int32_t id);

}  // namespace anycanvas
