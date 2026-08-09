#include <doctest/doctest.h>

#include <Analyzer/Languages.hpp>

using namespace wtf;

TEST_CASE("detectLanguage: match by extension") {
    CHECK(detectLanguage("main.cpp") == Language::Cpp);
    CHECK(detectLanguage("main.py") == Language::Python);
    CHECK(detectLanguage("src/lib.rs") == Language::Rust);
}

TEST_CASE("detectLanguage: case-insensitive match") {
    CHECK(detectLanguage("Main.CPP") == Language::Cpp);
    CHECK(detectLanguage("SCRIPT.PY") == Language::Python);
}

TEST_CASE("detectLanguage: exact filename match") {
    CHECK(detectLanguage("Makefile") == Language::Makefile);
    CHECK(detectLanguage("makefile") == Language::Makefile);
    CHECK(detectLanguage("path/to/Makefile") == Language::Makefile);
}

TEST_CASE("detectLanguage: filename takes priority") {
    CHECK(detectLanguage("CMakeLists.txt") == Language::CMake);
}

TEST_CASE("detectLanguage: unknown/unrecognized files") {
    CHECK(detectLanguage("data.bin") == Language::Unknown);
    CHECK(detectLanguage("no_extension_at_all") == Language::Unknown);
    CHECK(detectLanguage("") == Language::Unknown);
}