cmake_minimum_required(VERSION 3.21)
project(wtf_tests)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

CPMAddPackage("gh:doctest/doctest@2.5.3")

add_executable(${PROJECT_NAME}
    TestMain.cpp
    GitignoreTests.cpp
    DirWalkerTests.cpp
    LanguageDetectTests.cpp

    ${CMAKE_SOURCE_DIR}/src/Utils/DirWalker.cpp
    ${CMAKE_SOURCE_DIR}/src/Utils/FileReader.cpp
    ${CMAKE_SOURCE_DIR}/src/Utils/Gitignore.cpp
    ${CMAKE_SOURCE_DIR}/src/SIMD/Common.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_BINARY_DIR})
add_dependencies(${PROJECT_NAME} generate_langs)

target_compile_definitions(${PROJECT_NAME} PRIVATE WTF_VERSION="test")
if (WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE WTF_PLATFORM_WINDOWS _CRT_SECURE_NO_WARNINGS)
elseif (APPLE)
    target_compile_definitions(${PROJECT_NAME} PRIVATE WTF_PLATFORM_UNIX WTF_PLATFORM_MACOS)
elseif (UNIX)
    target_compile_definitions(${PROJECT_NAME} PRIVATE WTF_PLATFORM_UNIX WTF_PLATFORM_LINUX)
endif()

if (MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4 /permissive- /Zc:__cplusplus)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra)
endif()

if (WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE shlwapi)
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE fmt::fmt doctest::doctest)

include(${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake)
doctest_discover_tests(${PROJECT_NAME})