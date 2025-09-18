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

# (tests removed)

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

# ---- Third-party: plf_bitsets (header-only) ---------------------------------
# Make the plf::bitset (used by bitset benchmarks in the parent workspace) available
# to projects in this repo via the core interface include path.
include(FetchContent)
FetchContent_Declare(plf_bitsets
  GIT_REPOSITORY https://github.com/mattreecebentley/plf_bitsets.git
  GIT_TAG main
)
FetchContent_MakeAvailable(plf_bitsets)
target_include_directories(core INTERFACE ${plf_bitsets_SOURCE_DIR})

# ---- Global compile options (per-compiler, per-config) ----------------------
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
  if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_options(-O3 -DNDEBUG -fno-omit-frame-pointer -march=native)
  elseif(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    add_compile_options(-O3 -g -fno-omit-frame-pointer)
  elseif(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-Og -g3 -fno-omit-frame-pointer)
  endif()
  if(COMPILER_SUPPORTS_BMI)
    add_compile_options(-mbmi)
  endif()
  if(COMPILER_SUPPORTS_BMI2)
    add_compile_options(-mbmi2)
  endif()
elseif(MSVC)
  add_compile_options(/W4 /permissive- /EHsc)
  if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_options(/O2 /DNDEBUG /Gy /GF /Zc:inline /Oi /Oy-)
  elseif(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    add_compile_options(/O2 /Zi /Gy /Zc:inline /Oi /Oy-)
  elseif(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(/Od /Zi /Oy-)
  endif()
  if(MSVC_SUPPORTS_AVX2)
    add_compile_options(/arch:AVX2)
  endif()
endif()

# ---- Compile-time selection parameters --------------------------------------
set(GRAPH "PATH" CACHE STRING "Graph type (PATH, CLIQUE)")
set(K 2 CACHE STRING "Number of colors (K>=1)")
set(N 64 CACHE STRING "Number of vertices for the selected graph")
## (test parameters removed)

# ---- Executables -------------------------------------------------------------
# Build main app if present
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")
  add_executable(schelling src/main.cpp)
  target_link_libraries(schelling PRIVATE core)
  if(ipo_supported)
    set_property(TARGET schelling PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
  endif()
endif()

# Optional experiment entry (legacy name)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/run_experiment.cpp")
  add_executable(run_experiment src/run_experiment.cpp)
  target_link_libraries(run_experiment PRIVATE core)
  target_compile_definitions(run_experiment PRIVATE GRAPH_${GRAPH} K=${K} N=${N})
  if(ipo_supported)
    set_property(TARGET run_experiment PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
  endif()
endif()

# No built-in tests by default.
