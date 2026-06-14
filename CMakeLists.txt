cmake_minimum_required(VERSION 3.20)
project(AudioFilterLib
    VERSION 0.1.0
    DESCRIPTION "High-performance C++ audio filter library (IIR & FIR)"
    LANGUAGES CXX
)

# C++ Standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler warnings
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Build options
option(ENABLE_TESTS "Build unit tests" ON)
option(ENABLE_EXAMPLES "Build example programs" ON)
option(ENABLE_SIMD "Enable SIMD optimizations (SSE, AVX)" ON)
option(ENABLE_LTO "Enable Link Time Optimization" ON)
option(BUILD_SHARED_LIBS "Build as shared library" OFF)

# Optimization flags (Release builds)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

if(CMAKE_BUILD_TYPE MATCHES Release)
    if(MSVC)
        add_compile_options(/O2 /Ob2)
        if(ENABLE_SIMD)
            add_compile_options(/arch:AVX2)
        endif()
    else()
        add_compile_options(-O3 -DNDEBUG)
        if(ENABLE_SIMD)
            add_compile_options(-march=native -mtune=native)
        endif()
        # Fast math (be careful: disables strict IEEE 754 semantics)
        add_compile_options(-ffast-math)
    endif()

    if(ENABLE_LTO)
        include(CheckIPOSupported)
        check_ipo_supported(RESULT ipo_supported)
        if(ipo_supported)
            set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
        endif()
    endif()
endif()

# === Dependencies ===
find_package(SndFile REQUIRED)

# Find optional dependencies
if(ENABLE_TESTS)
    find_package(GTest REQUIRED)
endif()

# === Library target ===
add_library(audiofilter
    src/filter_base.cpp
    src/biquad_filter.cpp
    src/iir_designer.cpp
    src/fir_designer.cpp
    src/audio_buffer.cpp
)

target_include_directories(audiofilter
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(audiofilter
    PUBLIC
        SndFile::sndfile
)

# Platform-specific tweaks
if(WIN32)
    target_compile_definitions(audiofilter PRIVATE _USE_MATH_DEFINES)
endif()

# === Testing ===
if(ENABLE_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# === Examples ===
if(ENABLE_EXAMPLES)
    add_subdirectory(examples)
endif()

# === Installation ===
include(GNUInstallDirs)

install(TARGETS audiofilter
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# === Summary ===
message(STATUS "AudioFilterLib Configuration Summary:")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Enable Tests: ${ENABLE_TESTS}")
message(STATUS "  Enable Examples: ${ENABLE_EXAMPLES}")
message(STATUS "  Enable SIMD: ${ENABLE_SIMD}")
message(STATUS "  Enable LTO: ${ENABLE_LTO}")
