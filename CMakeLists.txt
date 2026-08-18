cmake_minimum_required(VERSION 3.21)
project(
    blam
    DESCRIPTION "Bazillion Lines Analyzed in Milliseconds"
    VERSION 2.2.1
    LANGUAGES CXX
    HOMEPAGE_URL "https://github.com/prevter/blam"
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64")

option(BLAM_BUILD_TESTS "Build tests" ON)
option(BLAM_ENABLE_SANITIZERS "Enable sanitizers" OFF)
option(BLAM_ENABLE_COVERAGE "Enable code coverage" OFF)
option(BLAM_ENABLE_LTO "Enable Link Time Optimization" ON)
option(BLAM_USE_AVX2 "Build with AVX2 support" OFF) # falls back to auto-detection if disabled

if (BLAM_ENABLE_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error)
    if(ipo_supported)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
    else()
        message(WARNING "BLAM_ENABLE_LTO ignored: compiler does not support link time optimizations")
    endif()
endif()

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.cpp)
if (WIN32)
    # include manifest to enable long + utf-8 paths on windows
    list(APPEND SOURCES cmake/blam.manifest)
endif()

add_executable(${PROJECT_NAME} ${SOURCES})

add_custom_target(generate_langs DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/languages.hpp)
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/languages.hpp
    COMMAND ${CMAKE_COMMAND} -E env PYTHONPATH=${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_COMMAND} -E
        chdir ${CMAKE_CURRENT_SOURCE_DIR}
        python src/gen_langs.py ${CMAKE_CURRENT_SOURCE_DIR}/languages.json ${CMAKE_CURRENT_BINARY_DIR}/languages.hpp ${CMAKE_CURRENT_BINARY_DIR}/dispatcher.hpp
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/gen_langs.py ${CMAKE_CURRENT_SOURCE_DIR}/languages.json
    COMMENT "Generating languages.hpp from languages.json"
)

add_dependencies(${PROJECT_NAME} generate_langs)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

if (MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4 /permissive- /Zc:__cplusplus)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -pedantic)
    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(${PROJECT_NAME} PRIVATE -Wno-c99-extensions)
    endif()
    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_options(${PROJECT_NAME} PRIVATE -Wno-pedantic -Wno-ignored-attributes)
    endif()
endif()

if (WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE BLAM_PLATFORM_WINDOWS _CRT_SECURE_NO_WARNINGS)
elseif (APPLE)
    target_compile_definitions(${PROJECT_NAME} PRIVATE BLAM_PLATFORM_UNIX BLAM_PLATFORM_MACOS)
elseif (UNIX)
    target_compile_definitions(${PROJECT_NAME} PRIVATE BLAM_PLATFORM_UNIX BLAM_PLATFORM_LINUX)
endif()

target_compile_definitions(${PROJECT_NAME} PRIVATE BLAM_VERSION="${PROJECT_VERSION}")

include(cmake/CPM.cmake)

CPMAddPackage("gh:fmtlib/fmt#12.2.0")
CPMAddPackage("gh:prevter/slic@1.0.1")

target_link_libraries(${PROJECT_NAME} PRIVATE fmt::fmt slic)

if (UNIX AND NOT APPLE)
    target_link_options(${PROJECT_NAME} PRIVATE -Wl,-z,stack-size=8388608)
    add_link_options(-Wl,-z,stack-size=8388608)
endif()

if (BLAM_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if (BLAM_ENABLE_SANITIZERS)
    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(${PROJECT_NAME} PRIVATE -fsanitize=address,undefined)
        target_link_options(${PROJECT_NAME} PRIVATE -fsanitize=address,undefined)
    elseif (MSVC)
        message(WARNING "Sanitizers are not supported on MSVC")
    endif()
endif()

if (BLAM_ENABLE_COVERAGE)
    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(${PROJECT_NAME} PRIVATE --coverage)
        target_link_options(${PROJECT_NAME} PRIVATE --coverage)
    elseif (MSVC)
        message(WARNING "Code coverage is not supported on MSVC")
    endif()
endif()

if (BLAM_USE_AVX2 AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|i.86)$")
    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(${PROJECT_NAME} PRIVATE -mavx2)
    elseif (MSVC)
        target_compile_options(${PROJECT_NAME} PRIVATE /arch:AVX2)
    endif()
elseif (BLAM_USE_AVX2)
    message(WARNING "BLAM_USE_AVX2 ignored: not an x86_64 target")
endif()