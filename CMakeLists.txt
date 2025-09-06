cmake_minimum_required(VERSION 3.16)
project(GraphExperiments LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Prefer LTO in Release for better codegen when supported
include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error)
if(ipo_supported)
  set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
endif()

# Compile-time selection and params
set(GRAPH "PATH" CACHE STRING "Graph type (PATH, CLIQUE)")
set(K 2 CACHE STRING "Number of colors (K>=1)")
set(N 64 CACHE STRING "Number of vertices for the selected graph")

add_library(core INTERFACE)
target_include_directories(core INTERFACE include)

# Global compile options leaning performance-first
add_compile_options(
  $<$<CONFIG:Release>:-O3 -DNDEBUG -march=native -fno-omit-frame-pointer>
  $<$<CONFIG:RelWithDebInfo>:-O3 -g -march=native -fno-omit-frame-pointer>
  $<$<CONFIG:Debug>:-O1 -g3 -fno-omit-frame-pointer>
  -Wall -Wextra -Wpedantic
)

# Keep existing executable setup if present in your tree
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/run_experiment.cpp")
  add_executable(run_experiment src/run_experiment.cpp)
  target_link_libraries(run_experiment PRIVATE core)
  target_compile_definitions(run_experiment PRIVATE GRAPH_${GRAPH} K=${K} N=${N})
endif()
