#pragma once

#include <bitset>
#include <cstddef>
#include <cstdint>
#include <new>
#include <vector>

#include "LanguageSpecs.hpp"

namespace blam {
    struct LOCStats {
        uint64_t fileCount = 0;
        uint64_t fileSize = 0;
        uint64_t totalLines = 0;
        uint64_t codeLines = 0;
        uint64_t commentLines = 0;
        uint64_t blankLines = 0;

        LOCStats& operator+=(LOCStats const& other) noexcept {
            fileCount += other.fileCount;
            fileSize += other.fileSize;
            totalLines += other.totalLines;
            codeLines += other.codeLines;
            commentLines += other.commentLines;
            blankLines += other.blankLines;
            return *this;
        }
    };

    struct alignas(std::hardware_destructive_interference_size) LOCBucket {
        std::bitset<LanguageCount> hasData{};
        std::array<LOCStats, LanguageCount> stats{};

        void markHasData(Language lang) noexcept {
            if (!hasData.test(static_cast<size_t>(lang))) {
                hasData.set(static_cast<size_t>(lang));
            }
        }

        LOCStats& operator[](Language lang) noexcept {
            return stats[static_cast<size_t>(lang)];
        }

        [[nodiscard]] LOCStats const& operator[](Language lang) const noexcept {
            return stats[static_cast<size_t>(lang)];
        }
    };

    struct FileStats {
        char const* path;
        uint64_t totalLines = 0;
        uint64_t codeLines = 0;
        uint64_t commentLines = 0;
        uint64_t blankLines = 0;
        uint64_t fileSize = 0;
        Language language;
    };

    using ShardedLOCStats = std::vector<LOCBucket>;
}