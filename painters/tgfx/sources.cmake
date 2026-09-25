# The tgfx painter is compiled by the INCLUDING build, against the tgfx that build provides (LeCodes
# vendors tgfx with patches; a second copy would be a bug). Usage from that build's CMakeLists:
#
#   include(${ANYCANVAS_DIR}/painters/tgfx/sources.cmake)
#   target_sources(my-renderer PRIVATE ${ANYCANVAS_TGFX_SOURCES})
#   target_include_directories(my-renderer PRIVATE ${ANYCANVAS_TGFX_INCLUDE_DIRS})
#   target_link_libraries(my-renderer PRIVATE anycanvas-internal tgfx)
#
set(ANYCANVAS_TGFX_DIR ${CMAKE_CURRENT_LIST_DIR})
set(ANYCANVAS_TGFX_SOURCES ${ANYCANVAS_TGFX_DIR}/tgfx_painter.cpp)
set(ANYCANVAS_TGFX_INCLUDE_DIRS ${ANYCANVAS_TGFX_DIR})
