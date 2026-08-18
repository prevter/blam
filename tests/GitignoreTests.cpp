#include <doctest/doctest.h>

#include <fmt/format.h>
#include <Utils/Gitignore.hpp>
#include "Shared.hpp"

using namespace blam;
using namespace blam::test;

static std::string child(std::string_view dir, std::string_view name) {
    return fmt::format("{}/{}", dir, name);
}

TEST_CASE("Gitignore: missing file") {
    TempDir dir;

    auto* node = GitignoreNode::makeNode(dir.path().c_str(), dir.size(), nullptr);

    CHECK(node == nullptr);
}

TEST_CASE("Gitignore: extension pattern") {
    TempDir dir;
    dir.writeFile(".gitignore", "*.log\n");

    auto* node = GitignoreNode::makeNode(dir.path().c_str(), dir.size(), nullptr);
    REQUIRE(node != nullptr);

    auto logPath = child(dir.path(), "debug.log");
    auto txtPath = child(dir.path(), "readme.txt");

    CHECK(node->isIgnored(logPath.c_str(), logPath.size(), false));
    CHECK_FALSE(node->isIgnored(txtPath.c_str(), txtPath.size(), false));

    node->release();
}

TEST_CASE("Gitignore: directory pattern") {
    TempDir dir;
    dir.writeFile(".gitignore", "build/\n");

    auto* node = GitignoreNode::makeNode(dir.path().c_str(), dir.size(), nullptr);
    REQUIRE(node != nullptr);

    auto buildDir = child(dir.path(), "build");
    auto buildFile = child(dir.path(), "build");

    CHECK(node->isIgnored(buildDir.c_str(), buildDir.size(), true));
    CHECK_FALSE(node->isIgnored(buildFile.c_str(), buildFile.size(), false));

    node->release();
}

TEST_CASE("Gitignore: anchored pattern") {
    TempDir dir;
    dir.writeFile(".gitignore", "/root_only.txt\n");

    auto* node = GitignoreNode::makeNode(dir.path().c_str(), dir.size(), nullptr);
    REQUIRE(node != nullptr);

    auto atRoot = child(dir.path(), "root_only.txt");
    auto nested = child(dir.path(), "sub/root_only.txt");

    CHECK(node->isIgnored(atRoot.c_str(), atRoot.size(), false));
    CHECK_FALSE(node->isIgnored(nested.c_str(), nested.size(), false));

    node->release();
}

TEST_CASE("Gitignore: unanchored pattern") {
    TempDir dir;
    dir.writeFile(".gitignore", "*.o\n");

    auto* node = GitignoreNode::makeNode(dir.path().c_str(), dir.size(), nullptr);
    REQUIRE(node != nullptr);

    auto shallow = child(dir.path(), "foo.o");
    auto deep = child(dir.path(), "a/b/c/foo.o");

    CHECK(node->isIgnored(shallow.c_str(), shallow.size(), false));
    CHECK(node->isIgnored(deep.c_str(), deep.size(), false));

    node->release();
}

TEST_CASE("Gitignore: negation") {
    TempDir dir;
    dir.writeFile(".gitignore", "*.log\n!important.log\n");

    auto* node = GitignoreNode::makeNode(dir.path().c_str(), dir.size(), nullptr);
    REQUIRE(node != nullptr);

    auto normalLog = child(dir.path(), "debug.log");
    auto importantLog = child(dir.path(), "important.log");

    CHECK(node->isIgnored(normalLog.c_str(), normalLog.size(), false));
    CHECK_FALSE(node->isIgnored(importantLog.c_str(), importantLog.size(), false));

    node->release();
}

TEST_CASE("Gitignore: rule precedence") {
    TempDir dir;
    dir.writeFile(".gitignore", "*.log\n!important.log\nimportant.log\n");

    auto* node = GitignoreNode::makeNode(dir.path().c_str(), dir.size(), nullptr);
    REQUIRE(node != nullptr);

    auto importantLog = child(dir.path(), "important.log");
    CHECK(node->isIgnored(importantLog.c_str(), importantLog.size(), false));

    node->release();
}

TEST_CASE("Gitignore: comments and whitespace") {
    TempDir dir;
    dir.writeFile(".gitignore", "# this is a comment\n\n*.log\n\n  \n");

    auto* node = GitignoreNode::makeNode(dir.path().c_str(), dir.size(), nullptr);
    REQUIRE(node != nullptr);

    CHECK(node->rules.size() == 1);

    node->release();
}

TEST_CASE("Gitignore: double asterisk pattern") {
    TempDir dir;
    dir.writeFile(".gitignore", "vendor/**\n");

    auto* node = GitignoreNode::makeNode(dir.path().c_str(), dir.size(), nullptr);
    REQUIRE(node != nullptr);

    auto direct = child(dir.path(), "vendor/lib.a");
    auto nested = child(dir.path(), "vendor/pkg/sub/file.c");
    auto sibling = child(dir.path(), "vendored_thing.txt");

    CHECK(node->isIgnored(direct.c_str(), direct.size(), false));
    CHECK(node->isIgnored(nested.c_str(), nested.size(), false));
    CHECK_FALSE(node->isIgnored(sibling.c_str(), sibling.size(), false));

    node->release();
}

TEST_CASE("Gitignore: parent directory inheritance") {
    TempDir dir;
    dir.writeFile(".gitignore", "*.tmp\n");
    dir.makeDir("subdir");

    auto* root = GitignoreNode::makeNode(dir.path().c_str(), dir.size(), nullptr);
    REQUIRE(root != nullptr);

    auto subPath = dir.path() + "/subdir";
    auto* subNode = GitignoreNode::makeNode(subPath.c_str(), subPath.size(), root);
    REQUIRE(subNode == root);

    auto deepTmp = child(subPath, "cache.tmp");
    CHECK(subNode->isIgnored(deepTmp.c_str(), deepTmp.size(), false));

    subNode->release();
}

TEST_CASE("Gitignore: child directory override") {
    TempDir dir;
    dir.writeFile(".gitignore", "*.tmp\n");
    dir.writeFile("subdir/.gitignore", "!keep.tmp\n");

    auto* root = GitignoreNode::makeNode(dir.path().c_str(), dir.size(), nullptr);
    REQUIRE(root != nullptr);

    auto subPath = dir.path() + "/subdir";
    auto* subNode = GitignoreNode::makeNode(subPath.c_str(), subPath.size(), root);
    REQUIRE(subNode != nullptr);
    REQUIRE(subNode != root);

    auto discard = child(subPath, "scratch.tmp");
    auto keep = child(subPath, "keep.tmp");

    CHECK(subNode->isIgnored(discard.c_str(), discard.size(), false));
    CHECK_FALSE(subNode->isIgnored(keep.c_str(), keep.size(), false));

    subNode->release();
}

TEST_CASE("Gitignore: character classes") {
    TempDir dir;
    dir.writeFile(".gitignore", "file[0-9].txt\n");

    auto* node = GitignoreNode::makeNode(dir.path().c_str(), dir.size(), nullptr);
    REQUIRE(node != nullptr);

    auto match = child(dir.path(), "file3.txt");
    auto noMatch = child(dir.path(), "fileA.txt");

    CHECK(node->isIgnored(match.c_str(), match.size(), false));
    CHECK_FALSE(node->isIgnored(noMatch.c_str(), noMatch.size(), false));

    node->release();
}

TEST_CASE("Gitignore: negated character class") {
    TempDir dir;
    dir.writeFile(".gitignore", "file[!0-9].txt\n");

    auto* node = GitignoreNode::makeNode(dir.path().c_str(), dir.size(), nullptr);
    REQUIRE(node != nullptr);

    auto match = child(dir.path(), "fileA.txt");
    auto noMatch = child(dir.path(), "file3.txt");

    CHECK(node->isIgnored(match.c_str(), match.size(), false));
    CHECK_FALSE(node->isIgnored(noMatch.c_str(), noMatch.size(), false));

    node->release();
}

TEST_CASE("Gitignore: '?' matches one char") {
    TempDir dir;
    dir.writeFile(".gitignore", "file?.txt\n");

    auto* node = GitignoreNode::makeNode(dir.path().c_str(), dir.size(), nullptr);
    REQUIRE(node != nullptr);

    auto match = child(dir.path(), "fileA.txt");
    auto tooShort = child(dir.path(), "file.txt");
    auto tooLong = child(dir.path(), "fileAB.txt");

    CHECK(node->isIgnored(match.c_str(), match.size(), false));
    CHECK_FALSE(node->isIgnored(tooShort.c_str(), tooShort.size(), false));
    CHECK_FALSE(node->isIgnored(tooLong.c_str(), tooLong.size(), false));

    node->release();
}

TEST_CASE("Gitignore: anchored glob with '*'") {
    TempDir dir;
    dir.writeFile(".gitignore", "/src/*.cpp\n");

    auto* node = GitignoreNode::makeNode(dir.path().c_str(), dir.size(), nullptr);
    REQUIRE(node != nullptr);

    auto direct = child(dir.path(), "src/main.cpp");
    auto nested = child(dir.path(), "src/nested/main.cpp");

    CHECK(node->isIgnored(direct.c_str(), direct.size(), false));
    CHECK_FALSE(node->isIgnored(nested.c_str(), nested.size(), false));

    node->release();
}

TEST_CASE("Gitignore: multiple stars") {
    TempDir dir;
    dir.writeFile(".gitignore", "*.min.*.js\n");

    auto* node = GitignoreNode::makeNode(dir.path().c_str(), dir.size(), nullptr);
    REQUIRE(node != nullptr);

    auto match = child(dir.path(), "app.min.abc123.js");
    auto noMatch = child(dir.path(), "app.js");

    CHECK(node->isIgnored(match.c_str(), match.size(), false));
    CHECK_FALSE(node->isIgnored(noMatch.c_str(), noMatch.size(), false));

    node->release();
}

TEST_CASE("Gitignore: '*' matches dotfiles") {
    TempDir dir;
    dir.writeFile(".gitignore", "*.log\n");

    auto* node = GitignoreNode::makeNode(dir.path().c_str(), dir.size(), nullptr);
    REQUIRE(node != nullptr);

    auto dotfile = child(dir.path(), ".debug.log");
    CHECK(node->isIgnored(dotfile.c_str(), dotfile.size(), false));

    node->release();
}

TEST_CASE("Gitignore: escape sequences") {
    TempDir dir;
    dir.writeFile(".gitignore", "literal\\*star.txt\n");

    auto* node = GitignoreNode::makeNode(dir.path().c_str(), dir.size(), nullptr);
    REQUIRE(node != nullptr);

    auto literalMatch = child(dir.path(), "literal*star.txt");
    auto wouldMatchIfUnescaped = child(dir.path(), "literalXstar.txt");

    CHECK(node->isIgnored(literalMatch.c_str(), literalMatch.size(), false));
    CHECK_FALSE(node->isIgnored(wouldMatchIfUnescaped.c_str(), wouldMatchIfUnescaped.size(), false));

    node->release();
}

TEST_CASE("Gitignore: escaped whitespace") {
    TempDir dir;
    dir.writeFile(".gitignore", "keep\\ me\\ \nunescaped  \n");

    auto* node = GitignoreNode::makeNode(dir.path().c_str(), dir.size(), nullptr);
    REQUIRE(node != nullptr);

    auto withSpace = child(dir.path(), "keep me ");
    auto withoutSpace = child(dir.path(), "keep me");
    auto trimmed = child(dir.path(), "unescaped");

    CHECK(node->isIgnored(withSpace.c_str(), withSpace.size(), false));
    CHECK_FALSE(node->isIgnored(withoutSpace.c_str(), withoutSpace.size(), false));
    CHECK(node->isIgnored(trimmed.c_str(), trimmed.size(), false));

    node->release();
}

TEST_CASE("Gitignore: wildcard slash rejection") {
    TempDir dir;
    dir.writeFile(".gitignore", "a?c\n");

    auto* node = GitignoreNode::makeNode(dir.path().c_str(), dir.size(), nullptr);
    REQUIRE(node != nullptr);

    auto path = child(dir.path(), "a/c");
    CHECK_FALSE(node->isIgnored(path.c_str(), path.size(), false));

    node->release();
}

TEST_CASE("Gitignore: parent chain cleanup") {
    TempDir dir;
    dir.writeFile(".gitignore", "*.tmp\n");
    dir.writeFile("subdir/.gitignore", "*.bak\n");

    auto* root = GitignoreNode::makeNode(dir.path().c_str(), dir.size(), nullptr);
    REQUIRE(root != nullptr);

    auto subPath = dir.path() + "/subdir";
    auto* subNode = GitignoreNode::makeNode(subPath.c_str(), subPath.size(), root);
    REQUIRE(subNode != nullptr);
    REQUIRE(subNode != root);

    root->retain();

    subNode->release();
    root->release();
}
