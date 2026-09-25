cmake_minimum_required(VERSION 3.22.1)
project(anycanvas-android LANGUAGES C CXX)

# The core from the repo root, without its tools and tests.
set(ANYCANVAS_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(ANYCANVAS_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../../../../.. ${CMAKE_CURRENT_BINARY_DIR}/anycanvas-core)

# The JNI library: `libanycanvas.so` (the core target is also called `anycanvas`, hence the target name).
add_library(anycanvas-jni SHARED anycanvas_jni.cpp)
set_target_properties(anycanvas-jni PROPERTIES OUTPUT_NAME anycanvas)
target_link_libraries(anycanvas-jni PRIVATE anycanvas-internal log)
