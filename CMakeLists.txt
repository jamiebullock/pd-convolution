# pd-convolution -- a partitioned convolution external for Pure Data
#
# SPDX-FileCopyrightText: 2026 Jamie Bullock
# SPDX-License-Identifier: Zlib

cmake_minimum_required(VERSION 3.16)
project(pd-convolution VERSION 1.0.0 LANGUAGES C CXX)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(FetchContent)

set(PD_GIT_TAG "0.55-2" CACHE STRING "Pure Data tag to fetch m_pd.h from")
set(FFTCONVOLVER_GIT_TAG "f2cdeb04c42141d2caec19ca4f137398b2a76b85"
    CACHE STRING "FFTConvolver commit to fetch")
set(GREATEST_GIT_TAG "v1.5.0" CACHE STRING "greatest tag to fetch, for the tests")

set(PD_INCLUDE_DIR "" CACHE PATH "Directory containing m_pd.h")
set(PD_LIBRARY "" CACHE FILEPATH "pd.lib -- required on Windows only")
set(PD_EXECUTABLE "" CACHE FILEPATH "pd, for the smoke test; found if not set")
set(FFTCONVOLVER_DIR "" CACHE PATH "Directory containing FFTConvolver.h")
set(GREATEST_INCLUDE_DIR "" CACHE PATH "Directory containing greatest.h")

option(CONVOLUTION_BUILD_TESTS "Build the core test binary" ON)

# ---------------------------------------------------------------------------
# Pure Data headers
# ---------------------------------------------------------------------------

if(PD_INCLUDE_DIR STREQUAL "")
    message(STATUS "PD_INCLUDE_DIR not set: fetching Pure Data ${PD_GIT_TAG}")
    FetchContent_Declare(puredata
        GIT_REPOSITORY https://github.com/pure-data/pure-data.git
        GIT_TAG        ${PD_GIT_TAG}
        GIT_SHALLOW    TRUE
        SOURCE_SUBDIR  headers-only)
    FetchContent_MakeAvailable(puredata)
    set(PD_INCLUDE_DIR "${puredata_SOURCE_DIR}/src")
endif()

if(NOT EXISTS "${PD_INCLUDE_DIR}/m_pd.h")
    message(FATAL_ERROR "m_pd.h not found in ${PD_INCLUDE_DIR}")
endif()
message(STATUS "Pd headers: ${PD_INCLUDE_DIR}")

# ---------------------------------------------------------------------------
# FFTConvolver
#
# It ships no CMake, so its four translation units are compiled here. Its own
# FFT is used: AudioFFT can be pointed at FFTW3 or Accelerate instead, but
# neither is worth a dependency at the sizes this runs at.
# ---------------------------------------------------------------------------

if(FFTCONVOLVER_DIR STREQUAL "")
    message(STATUS "FFTCONVOLVER_DIR not set: fetching FFTConvolver ${FFTCONVOLVER_GIT_TAG}")
    FetchContent_Declare(fftconvolver
        GIT_REPOSITORY https://github.com/HiFi-LoFi/FFTConvolver.git
        GIT_TAG        ${FFTCONVOLVER_GIT_TAG}
        SOURCE_SUBDIR  headers-only)
    FetchContent_MakeAvailable(fftconvolver)
    set(FFTCONVOLVER_DIR "${fftconvolver_SOURCE_DIR}")
endif()

if(NOT EXISTS "${FFTCONVOLVER_DIR}/FFTConvolver.h")
    message(FATAL_ERROR "FFTConvolver.h not found in ${FFTCONVOLVER_DIR}")
endif()
message(STATUS "FFTConvolver: ${FFTCONVOLVER_DIR}")

# An object library rather than a static one: its objects then land inside
# convolution_core, so the core is one archive that stands on its own, and the
# separation is only so that upstream sources can be compiled without our
# warning settings.
add_library(fftconvolver OBJECT
    "${FFTCONVOLVER_DIR}/FFTConvolver.cpp"
    "${FFTCONVOLVER_DIR}/TwoStageFFTConvolver.cpp"
    "${FFTCONVOLVER_DIR}/AudioFFT.cpp"
    "${FFTCONVOLVER_DIR}/Utilities.cpp")
target_include_directories(fftconvolver SYSTEM PUBLIC "${FFTCONVOLVER_DIR}")
if(NOT MSVC)
    target_compile_options(fftconvolver PRIVATE -w)
endif()

# ---------------------------------------------------------------------------
# The core
# ---------------------------------------------------------------------------

add_library(convolution_core STATIC src/convolution_core.cpp)
target_include_directories(convolution_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(convolution_core PRIVATE fftconvolver)

if(MSVC)
    target_compile_options(convolution_core PRIVATE /W3)
else()
    target_compile_options(convolution_core PRIVATE -Wall -Wextra)
endif()

# ---------------------------------------------------------------------------
# The external
# ---------------------------------------------------------------------------

if(APPLE)
    set(PD_EXTENSION ".pd_darwin")
elseif(WIN32)
    set(PD_EXTENSION ".dll")
else()
    set(PD_EXTENSION ".pd_linux")
endif()

add_library(convolution MODULE
    src/convolution_tilde.c
    src/convolution_setup.c)

target_link_libraries(convolution PRIVATE convolution_core)
target_include_directories(convolution PRIVATE ${PD_INCLUDE_DIR})
target_compile_definitions(convolution PRIVATE
    CONVOLUTION_VERSION="${PROJECT_VERSION}")

set_target_properties(convolution PROPERTIES
    PREFIX ""
    OUTPUT_NAME "convolution"
    SUFFIX "${PD_EXTENSION}"
    LINKER_LANGUAGE CXX)

if(MSVC)
    target_compile_options(convolution PRIVATE /W3)
else()
    target_compile_options(convolution PRIVATE -Wall -Wextra -Wno-cast-function-type)
endif()

if(APPLE)
    target_link_options(convolution PRIVATE
        "LINKER:-undefined" "LINKER:dynamic_lookup")
elseif(WIN32)
    if(PD_LIBRARY STREQUAL "")
        find_library(PD_LIBRARY_FOUND NAMES pd
            HINTS "${PD_INCLUDE_DIR}/../bin" "${PD_INCLUDE_DIR}/../lib"
            NO_DEFAULT_PATH)
        if(PD_LIBRARY_FOUND)
            set(PD_LIBRARY "${PD_LIBRARY_FOUND}")
        else()
            message(FATAL_ERROR
                "On Windows the external must link against Pd's import library. "
                "Set -DPD_LIBRARY=<path to pd.lib>.")
        endif()
    endif()
    message(STATUS "Pd import library: ${PD_LIBRARY}")
    target_link_libraries(convolution PRIVATE "${PD_LIBRARY}")
endif()

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------

if(CONVOLUTION_BUILD_TESTS)
    enable_testing()

    if(GREATEST_INCLUDE_DIR STREQUAL "")
        message(STATUS "GREATEST_INCLUDE_DIR not set: fetching greatest ${GREATEST_GIT_TAG}")
        FetchContent_Declare(greatest
            GIT_REPOSITORY https://github.com/silentbicycle/greatest.git
            GIT_TAG        ${GREATEST_GIT_TAG}
            GIT_SHALLOW    TRUE
            SOURCE_SUBDIR  headers-only)
        FetchContent_MakeAvailable(greatest)
        set(GREATEST_INCLUDE_DIR "${greatest_SOURCE_DIR}")
    endif()

    if(NOT EXISTS "${GREATEST_INCLUDE_DIR}/greatest.h")
        message(FATAL_ERROR "greatest.h not found in ${GREATEST_INCLUDE_DIR}")
    endif()
    message(STATUS "greatest: ${GREATEST_INCLUDE_DIR}")

    add_executable(test_core tests/test_core.c)
    target_link_libraries(test_core PRIVATE convolution_core)
    target_include_directories(test_core SYSTEM PRIVATE ${GREATEST_INCLUDE_DIR})
    set_target_properties(test_core PROPERTIES LINKER_LANGUAGE CXX)
    if(NOT MSVC)
        target_compile_options(test_core PRIVATE -Wall -Wextra)
        target_link_libraries(test_core PRIVATE m)
    endif()

    add_test(NAME core COMMAND test_core)

    find_package(Python3 COMPONENTS Interpreter)
    if(Python3_Interpreter_FOUND)
        if(PD_EXECUTABLE STREQUAL "" AND NOT PD_LIBRARY STREQUAL "")
            get_filename_component(pd_bindir "${PD_LIBRARY}" DIRECTORY)
            find_program(PD_EXECUTABLE_FOUND NAMES pd
                         HINTS "${pd_bindir}" NO_DEFAULT_PATH)
            if(PD_EXECUTABLE_FOUND)
                set(PD_EXECUTABLE "${PD_EXECUTABLE_FOUND}")
            endif()
        endif()
        if(NOT PD_EXECUTABLE STREQUAL "")
            message(STATUS "Pd for the smoke test: ${PD_EXECUTABLE}")
        endif()

        add_test(NAME pd_smoke
                 COMMAND ${Python3_EXECUTABLE}
                         "${CMAKE_CURRENT_SOURCE_DIR}/tests/run_pd_smoke.py"
                         "$<TARGET_FILE_DIR:convolution>"
                         "${PD_EXECUTABLE}")
        set_tests_properties(pd_smoke PROPERTIES SKIP_RETURN_CODE 77)
    endif()
endif()

# ---------------------------------------------------------------------------
# Install
# ---------------------------------------------------------------------------

configure_file(convolution-meta.pd.in
               "${CMAKE_CURRENT_BINARY_DIR}/convolution-meta.pd" @ONLY)

install(TARGETS convolution DESTINATION convolution)
install(DIRECTORY help/ DESTINATION convolution FILES_MATCHING PATTERN "*.pd")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/convolution-meta.pd"
              README.md LICENSE DESTINATION convolution)
