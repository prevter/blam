#include "CSV.hpp"

#include <fmt/format.h>

namespace blam::csv {
    void printHeader(FILE* out) noexcept {
        fmt::print(out, "Language,Files,Total Lines,Code Lines,Comment Lines,Blank Lines\n");
    }

    void printLangStat(
        FILE* out,
        Language language,
        uint64_t fileCount,
        uint64_t totalLines,
        uint64_t codeLines,
        uint64_t commentLines,
        uint64_t blankLines
    ) noexcept {
        fmt::print(out, "{},{},{},{},{},{}\n",
            language,
            fileCount,
            totalLines,
            codeLines,
            commentLines,
            blankLines
        );
    }

    void printFooter(
        FILE* out,
        uint64_t fileCount,
        uint64_t totalLines,
        uint64_t codeLines,
        uint64_t commentLines,
        uint64_t blankLines
    ) noexcept {
        fmt::print(out, "Total,{},{},{},{},{}\n",
            fileCount,
            totalLines,
            codeLines,
            commentLines,
            blankLines
        );
    }
}
