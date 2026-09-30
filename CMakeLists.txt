cmake_minimum_required(VERSION 3.20)
project(gl_hwbp_hook LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_library(hwbp_hook STATIC src/hwbp_hook.cpp src/hwbp_hook.h)
target_include_directories(hwbp_hook PUBLIC src)

add_library(example_hook SHARED src/example_hook.cpp)
target_link_libraries(example_hook PRIVATE hwbp_hook user32 gdi32)

add_executable(reader_example src/reader_example.cpp)
target_include_directories(reader_example PRIVATE src)
