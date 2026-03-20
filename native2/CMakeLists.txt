cmake_minimum_required(VERSION 3.10)

project(render)

set(CMAKE_CXX_STANDARD 20)

add_definitions(-DUNICODE -D_UNICODE)
add_compile_options(/utf-8)

set(CMAKE_WIN32_EXECUTABLE TRUE)

add_executable(render main.cc)

target_link_libraries(
  render
  PRIVATE 
  d3dcompiler.lib
  d3d11.lib
  dxgi.lib)