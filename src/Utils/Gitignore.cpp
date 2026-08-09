#include "Gitignore.hpp"

#include <climits>
#include <cstring>

#ifdef WTF_PLATFORM_UNIX
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
#elif defined(WTF_PLATFORM_WINDOWS)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#ifndef PATH_MAX
#define PATH_MAX MAX_PATH
#endif
#else
#error "Unsupported platform"
#endif

namespace wtf {
    bool GitignoreRule::matches(std::string_view relative, bool isDir) const noexcept {
        if (directoryOnly && !isDir) return false;

        if (anchored) {
            return pattern.match(relative);
        }

        size_t lastSlash = relative.rfind('/');
        std::string_view comp = (lastSlash != std::string_view::npos)
            ? relative.substr(lastSlash + 1)
            : relative;

        return pattern.match(comp);
    }

    void GitignoreNode::retain() noexcept {
        refcount.fetch_add(1, std::memory_order_relaxed);
    }

    void GitignoreNode::release() noexcept {
        auto node = this;
        while (node) {
            if (node->refcount.fetch_sub(1, std::memory_order_acq_rel) != 1) {
                return;
            }

            GitignoreNode* p = node->parent;
            delete node;
            node = p;
        }
    }

    bool GitignoreNode::isIgnored(char const* path, uint16_t len, bool isDir) const noexcept {
        static constexpr size_t MaxDepth = 128;
        GitignoreNode const* chain[MaxDepth];
        int depth = 0;

        for (auto n = this; n; n = n->parent) {
            if (depth >= static_cast<int>(MaxDepth)) break;
            chain[depth++] = n;
        }

        bool ignored = false;

        for (int i = depth - 1; i >= 0; --i) {
            auto* n = chain[i];
            if (n->rules.empty() || len <= n->dirLength) continue;

            char const* rel = path + n->dirLength + 1;
            std::string_view relView(rel);

            for (auto const& r : n->rules) {
                if (r.matches(relView, isDir)) {
                    ignored = !r.negated;
                }
            }
        }

        return ignored;
    }

    GitignoreNode* GitignoreNode::makeNode(char const* dirPath, uint16_t len, GitignoreNode* parent) {
        char gi[PATH_MAX];
        if (len + 11u >= sizeof(gi)) {
            if (parent) parent->retain();
            return parent;
        }

        std::memcpy(gi, dirPath, len);
        std::memcpy(gi + len, "/.gitignore", 12);

        FILE* f = ::fopen(gi, "rb");
        if (!f) {
            if (parent) parent->retain();
            return parent;
        }

        auto* node = new GitignoreNode();
        node->parent = parent;
        node->dirLength = len;

        char line[PATH_MAX];
        while (::fgets(line, sizeof(line), f)) {
            int l = ::strlen(line);

            while (l > 0 && (line[l - 1] == '\n' || line[l - 1] == '\r')) {
                line[--l] = '\0';
            }

            if (!l || line[0] == '#') continue;

            // strip trailing whitespace
            while (l > 0) {
                if (line[l - 1] == ' ' || line[l - 1] == '\t') {
                    int backslashes = 0;
                    for (int k = l - 2; k >= 0 && line[k] == '\\'; --k) {
                        backslashes++;
                    }

                    if (backslashes % 2 != 0) {
                        break;
                    }
                    --l;
                } else {
                    break;
                }
            }

            if (!l) continue;

            GitignoreRule rule;
            char const* src = line;

            if (*src == '!') {
                rule.negated = true;
                ++src;
                --l;
            }

            if (!l) continue;

            if (src[l - 1] == '/') {
                rule.directoryOnly = true;
                --l;
            }

            if (!l) continue;

            while (l > 0 && src[l - 1] == ' ' && !(l >= 2 && src[l - 2] == '\\')) {
                --l;
            }

            if (!l) continue;

            if (*src == '/') {
                rule.anchored = true;
                ++src;
                --l;
            }

            if (!l) continue;

            if (!rule.anchored) {
                for (int i = 0; i < l; ++i) {
                    if (src[i] == '/') {
                        rule.anchored = true;
                        break;
                    }
                }
            }

            rule.pattern = glob::Pattern::compile(std::string_view(src, l));
            node->rules.push_back(std::move(rule));
        }

        ::fclose(f);

        if (node->rules.empty()) {
            delete node;
            if (parent) parent->retain();
            return parent;
        }

        return node;
    }
}
