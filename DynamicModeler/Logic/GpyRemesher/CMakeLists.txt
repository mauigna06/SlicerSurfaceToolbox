cmake_minimum_required(VERSION 3.16)

project(GpyRemesher LANGUAGES CXX)

# Configure libigl as a header-only dependency.
set(LIBIGL_USE_STATIC_LIBRARY OFF CACHE BOOL "" FORCE)
set(LIBIGL_WITH_OPENGL OFF CACHE BOOL "" FORCE)
set(LIBIGL_WITH_OPENGL_GLFW OFF CACHE BOOL "" FORCE)
set(LIBIGL_WITH_OPENGL_GLFW_IMGUI OFF CACHE BOOL "" FORCE)
set(LIBIGL_WITH_CGAL OFF CACHE BOOL "" FORCE)
set(LIBIGL_WITH_COMISO OFF CACHE BOOL "" FORCE)
set(LIBIGL_WITH_TETGEN OFF CACHE BOOL "" FORCE)
set(LIBIGL_WITH_EMBREE OFF CACHE BOOL "" FORCE)
set(LIBIGL_WITH_PNG OFF CACHE BOOL "" FORCE)
set(LIBIGL_WITH_XML OFF CACHE BOOL "" FORCE)
set(LIBIGL_WITH_PYTHON OFF CACHE BOOL "" FORCE)
set(LIBIGL_WITH_TUTORIAL OFF CACHE BOOL "" FORCE)

include(FetchContent)
FetchContent_Declare(
  libigl
  GIT_REPOSITORY https://github.com/libigl/libigl.git
  GIT_TAG 08be0704c26359bcdbe17449054e6c56b9b7538c
)
FetchContent_MakeAvailable(libigl)

set(GPY_REMESHER_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../gpytoolbox/src/cpp/remesher)

set(GPY_REMESHER_SOURCES
  ${GPY_REMESHER_SOURCE_DIR}/collapse_edges.cpp
  ${GPY_REMESHER_SOURCE_DIR}/equalize_valences.cpp
  ${GPY_REMESHER_SOURCE_DIR}/remesh_botsch.cpp
  ${GPY_REMESHER_SOURCE_DIR}/split_edges.cpp
  ${GPY_REMESHER_SOURCE_DIR}/split_edges_until_bound.cpp
  ${GPY_REMESHER_SOURCE_DIR}/tangential_relaxation.cpp
)

add_library(GpyRemesher STATIC ${GPY_REMESHER_SOURCES})

set_target_properties(GpyRemesher PROPERTIES
  POSITION_INDEPENDENT_CODE ON
)

target_include_directories(GpyRemesher
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../gpytoolbox/src/cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../gpytoolbox/src/cpp/remesher
)

target_link_libraries(GpyRemesher
  PUBLIC
    igl::core
)

target_compile_features(GpyRemesher PUBLIC cxx_std_17)

set_property(GLOBAL APPEND PROPERTY Slicer_TARGETS GpyRemesher)
