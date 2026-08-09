#pragma once

#include "../Analyzer/Languages.hpp"

namespace wtf::csv {
    void printHeader(FILE* out) noexcept;
    void printLangStat(
        FILE* out,
        Language language,
        uint64_t fileCount, uint64_t totalLines,
        uint64_t codeLines, uint64_t commentLines, uint64_t blankLines
    ) noexcept;
    void printFooter(
        FILE* out,
        uint64_t fileCount, uint64_t totalLines,
        uint64_t codeLines, uint64_t commentLines, uint64_t blankLines
    ) noexcept;
}