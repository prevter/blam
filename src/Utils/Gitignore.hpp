#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

#include "Glob.hpp"

namespace wtf {
    struct GitignoreRule {
        glob::Pattern pattern;
        bool negated = false;
        bool directoryOnly = false;
        bool anchored = false;

        bool matches(std::string_view relative, bool isDir) const noexcept;
    };

    struct GitignoreNode {
        std::vector<GitignoreRule> rules;
        GitignoreNode* parent = nullptr;
        std::atomic<int> refcount{1};
        uint16_t dirLength = 0;

        void retain() noexcept;
        void release() noexcept;
        bool isIgnored(char const* path, uint16_t len, bool isDir) const noexcept;

        static GitignoreNode* makeNode(char const* dirPath, uint16_t len, GitignoreNode* parent);
    };
}
