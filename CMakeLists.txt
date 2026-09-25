cmake_minimum_required(VERSION 3.22)
project(anycanvas LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ANYCANVAS_BUILD_TOOLS "Build acdump (draw lists / PNGs from the command line)" ON)
option(ANYCANVAS_BUILD_TESTS "Build the unit + golden tests (ctest)" ON)

if(MSVC)
  # The conformant preprocessor: the spec's X-macros paste enum names (Name##Name).
  add_compile_options(/Zc:preprocessor /utf-8)
  add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif()

# ---- core: the platform-free library --------------------------------------------------------------
add_library(anycanvas STATIC
  core/src/spec.cpp
  core/src/drawlist.cpp
  core/src/css.cpp
  core/src/geometry.cpp
  core/src/interpreter.cpp
  core/src/svg.cpp
  core/src/encode.cpp
  core/src/api.cpp
)
target_include_directories(anycanvas
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/core/include
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/core/src ${CMAKE_CURRENT_SOURCE_DIR}/core/vendor
)
if(MSVC)
  target_compile_options(anycanvas PRIVATE /W3)
else()
  # No FMA contraction: keeps the draw lists closer across compilers (libm still differs; the
  # golden test compares numerically across platforms, byte-exact on the platform that wrote them).
  target_compile_options(anycanvas PRIVATE -Wall -Wextra -Wno-unused-parameter -ffp-contract=off)
endif()

# A consumer that wants the C++ internals (a C++ painter, the tools) links `anycanvas-internal`.
add_library(anycanvas-internal INTERFACE)
target_include_directories(anycanvas-internal INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/core/src ${CMAKE_CURRENT_SOURCE_DIR}/core/vendor)
target_link_libraries(anycanvas-internal INTERFACE anycanvas)
if(MSVC)
  # The internals' X-macro enum names need the conformant preprocessor in every consumer too.
  target_compile_options(anycanvas-internal INTERFACE /Zc:preprocessor)
endif()

# ---- tools ----------------------------------------------------------------------------------------
if(ANYCANVAS_BUILD_TOOLS)
  add_executable(acdump tools/acdump/main.cpp tools/acdump/raster.cpp)
  target_link_libraries(acdump PRIVATE anycanvas-internal)
  target_include_directories(acdump PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tools)
endif()

# ---- tests ----------------------------------------------------------------------------------------
if(ANYCANVAS_BUILD_TESTS)
  enable_testing()
  add_executable(anycanvas-unit tests/core/unit.cpp)
  target_link_libraries(anycanvas-unit PRIVATE anycanvas-internal)
  add_test(NAME unit COMMAND anycanvas-unit)

  add_executable(anycanvas-golden tests/core/golden.cpp)
  target_link_libraries(anycanvas-golden PRIVATE anycanvas-internal)
  target_include_directories(anycanvas-golden PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tools)
  add_test(NAME golden COMMAND anycanvas-golden ${CMAKE_CURRENT_SOURCE_DIR}/tests/golden)
endif()
