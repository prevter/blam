#include <doctest/doctest.h>

#include <algorithm>
#include <mutex>
#include <string>
#include <vector>
#include <fmt/format.h>

#include <Utils/DirWalker.hpp>
#include <Utils/FileReader.hpp>
#include "Shared.hpp"

using namespace wtf;
using namespace wtf::test;

struct Recorder {
    std::mutex mutex;
    std::vector<std::pair<std::string, Language>> files;

    static Recorder& get() {
        static Recorder rec;
        return rec;
    }

    void clear() {
        std::lock_guard lock(mutex);
        files.clear();
    }

    [[nodiscard]] bool sawFile(std::string_view suffix) const {
        for (auto const& [path, lang] : files) {
            if (path.size() >= suffix.size() && path.compare(path.size() - suffix.size(), suffix.size(), suffix) == 0) {
                return true;
            }
        }
        return false;
    }
};

void RecordingHandler(char const* path, detail::Handle fd, Language lang, LOCBucket& bucket, std::vector<FileStats>*) {
    (void)bucket;
    detail::close(fd);

    std::lock_guard lock(Recorder::get().mutex);
    Recorder::get().files.emplace_back(path, lang);
}

void walkAndWait(Pool& pool, std::string_view root) {
    pool.retain();
    pool.submitTask(root);
    pool.release();
    pool.waitUntilIdle();
}

TEST_CASE("DirWalker: detect language") {
    Recorder::get().clear();

    TempDir dir;
    dir.writeFile("main.cpp", "int main() {}");
    dir.writeFile("notes.bin", "not source code");

    Pool pool(1, true, false, false, &RecordingHandler);
    walkAndWait(pool, dir.path());

    CHECK(Recorder::get().sawFile("/main.cpp"));
    CHECK_FALSE(Recorder::get().sawFile("/notes.bin"));
}

TEST_CASE("DirWalker: recursion") {
    Recorder::get().clear();

    TempDir dir;
    dir.writeFile("a/b/c/deep.cpp", "// deep");

    Pool pool(1, true, false, false, &RecordingHandler);
    walkAndWait(pool, dir.path());

    CHECK(Recorder::get().sawFile("/a/b/c/deep.cpp"));
}

TEST_CASE("DirWalker: skip hidden") {
    Recorder::get().clear();

    TempDir dir;
    dir.writeFile(".hidden.cpp", "// hidden file");
    dir.writeFile(".hidden_dir/visible.cpp", "// inside a hidden dir");
    dir.writeFile("visible.cpp", "// visible");

    Pool pool(1, true, false, false, &RecordingHandler);
    walkAndWait(pool, dir.path());

    CHECK(Recorder::get().sawFile("/visible.cpp"));
    CHECK_FALSE(Recorder::get().sawFile("/.hidden.cpp"));
    CHECK_FALSE(Recorder::get().sawFile("/.hidden_dir/visible.cpp"));
}

TEST_CASE("DirWalker: include hidden") {
    Recorder::get().clear();

    TempDir dir;
    dir.writeFile(".hidden.cpp", "// hidden file");
    dir.writeFile("visible.cpp", "// visible");

    Pool pool(1, false, false, false, &RecordingHandler);
    walkAndWait(pool, dir.path());

    CHECK(Recorder::get().sawFile("/visible.cpp"));
    CHECK(Recorder::get().sawFile("/.hidden.cpp"));
}

TEST_CASE("DirWalker: respect gitignore") {
    Recorder::get().clear();

    TempDir dir;
    dir.writeFile(".gitignore", "build/\n*.txt\n");
    dir.writeFile("main.cpp", "// kept");
    dir.writeFile("notes.txt", "// ignored by *.txt");
    dir.writeFile("build/output.cpp", "// ignored: whole dir is pruned");

    Pool pool(1, true, true, false, &RecordingHandler);
    walkAndWait(pool, dir.path());

    CHECK(Recorder::get().sawFile("/main.cpp"));
    CHECK_FALSE(Recorder::get().sawFile("/notes.txt"));
    CHECK_FALSE(Recorder::get().sawFile("/build/output.cpp"));
}

TEST_CASE("DirWalker: ignore gitignore") {
    Recorder::get().clear();

    TempDir dir;
    dir.writeFile(".gitignore", "*.txt\n");
    dir.writeFile("notes.txt", "// would be ignored, but useGitignore is off");

    Pool pool(1, true, false, false, &RecordingHandler);
    walkAndWait(pool, dir.path());

    CHECK(Recorder::get().sawFile("/notes.txt"));
}

TEST_CASE("DirWalker: nested gitignore") {
    Recorder::get().clear();

    TempDir dir;
    dir.writeFile(".gitignore", "*.txt\n");
    dir.writeFile("sub/.gitignore", "!keep.txt\n");
    dir.writeFile("sub/scratch.txt", "// ignored by parent's *.txt");
    dir.writeFile("sub/keep.txt", "// re-included by sub's own .gitignore");

    Pool pool(1, true, true, false, &RecordingHandler);
    walkAndWait(pool, dir.path());

    CHECK_FALSE(Recorder::get().sawFile("/sub/scratch.txt"));
    CHECK(Recorder::get().sawFile("/sub/keep.txt"));
}

TEST_CASE("DirWalker: multithreading") {
    Recorder::get().clear();

    TempDir dir;
    for (int i = 0; i < 40; ++i) {
        dir.writeFile("dir" + std::to_string(i % 5) + "/file" + std::to_string(i) + ".cpp", "// file");
    }

    Pool pool(4, true, false, false, &RecordingHandler);
    walkAndWait(pool, dir.path());

    std::lock_guard lock(Recorder::get().mutex);
    CHECK(Recorder::get().files.size() == 40);
}

TEST_CASE("Pool: isDirectory") {
    TempDir dir;
    auto file = dir.writeFile("a_file.txt", "content");

    CHECK(Pool::isDirectory(dir.path().c_str()));
    CHECK_FALSE(Pool::isDirectory(file.c_str()));
    CHECK_FALSE(Pool::isDirectory((dir.path() + "/does_not_exist").c_str()));
}