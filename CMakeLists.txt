cmake_minimum_required(VERSION 3.16)
project(GraphExperiments LANGUAGES CXX)

# Toolchain-wide settings
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Default to Release if not specified
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# ---- IPO/LTO (enable if toolchain supports it) ------------------------------
include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error)
if(ipo_supported)
  # Enable IPO only for Release by default
  set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
else()
  message(STATUS "IPO/LTO not enabled: ${ipo_error}")
endif()

# ---- Feature flags: BMI/BMI2 for GCC/Clang; /arch:AVX2 for MSVC ------------
include(CheckCXXCompilerFlag)

set(COMPILER_SUPPORTS_BMI FALSE)
set(COMPILER_SUPPORTS_BMI2 FALSE)
set(MSVC_SUPPORTS_AVX2 FALSE)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  check_cxx_compiler_flag("-mbmi"  COMPILER_SUPPORTS_BMI)
  check_cxx_compiler_flag("-mbmi2" COMPILER_SUPPORTS_BMI2)
elseif(MSVC)
  # Note: MSVC has no /arch:BMI2; /arch:AVX2 is the usual switch that also defines __AVX2__.
  check_cxx_compiler_flag("/arch:AVX2" MSVC_SUPPORTS_AVX2)
endif()

# ---- Interface library for headers under include/ ---------------------------
add_library(core INTERFACE)
target_include_directories(core INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(core INTERFACE cxx_std_20)

# ---- Global compile options (per-compiler, per-config) ----------------------
# GCC/Clang
add_compile_options(
  $<$<AND:$<CXX_COMPILER_ID:GNU,Clang>,$<CONFIG:Release>>:-O3 -DNDEBUG -fno-omit-frame-pointer>
  $<$<AND:$<CXX_COMPILER_ID:GNU,Clang>,$<CONFIG:RelWithDebInfo>>:-O3 -g -fno-omit-frame-pointer>
  $<$<AND:$<CXX_COMPILER_ID:GNU,Clang>,$<CONFIG:Debug>>:-Og -g3 -fno-omit-frame-pointer>
  $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
  # Optional: native tuning (comment out if you need portable binaries)
  $<$<AND:$<CXX_COMPILER_ID:GNU,Clang>,$<CONFIG:Release>>:-march=native>
  # Enable BMI/BMI2 only if supported
  $<$<AND:$<CXX_COMPILER_ID:GNU,Clang>,$<BOOL:${COMPILER_SUPPORTS_BMI}>>:-mbmi>
  $<$<AND:$<CXX_COMPILER_ID:GNU,Clang>,$<BOOL:${COMPILER_SUPPORTS_BMI2}>>:-mbmi2>
)

# MSVC
add_compile_options(
  $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive- /EHsc>
  $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Release>>:/O2 /DNDEBUG /Gy /GF /Zc:inline /Oi /Oy->   # /Oy- keeps frame pointer
  $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:RelWithDebInfo>>:/O2 /Zi /Gy /Zc:inline /Oi /Oy->
  $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Debug>>:/Od /Zi /Oy->
  # Enable AVX2 if the compiler accepts it (commonly defines __AVX2__)
  $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<BOOL:${MSVC_SUPPORTS_AVX2}>>:/arch:AVX2>
)

# ---- Compile-time selection parameters --------------------------------------
set(GRAPH "PATH" CACHE STRING "Graph type (PATH, CLIQUE)")
set(K 2 CACHE STRING "Number of colors (K>=1)")
set(N 64 CACHE STRING "Number of vertices for the selected graph")

# ---- Example executable (optional) ------------------------------------------
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/run_experiment.cpp")
  add_executable(run_experiment src/run_experiment.cpp)
  target_link_libraries(run_experiment PRIVATE core)
  target_compile_definitions(run_experiment PRIVATE GRAPH_${GRAPH} K=${K} N=${N})
  if(ipo_supported)
    set_property(TARGET run_experiment PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
  end_if()
endif()
