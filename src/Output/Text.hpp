#pragma once

#include <utility>
#include <fmt/color.h>
#include <fmt/format.h>

#include "../Analyzer/Languages.hpp"

namespace wtf::text {
    constexpr auto title = fmt::fg(fmt::color::cyan) | fmt::emphasis::bold;
    constexpr auto accent = fmt::fg(fmt::color::cornflower_blue) | fmt::emphasis::bold;
    constexpr auto good = fmt::fg(fmt::color::light_green) | fmt::emphasis::bold;
    constexpr auto dim = fmt::fg(fmt::color::gray);
    constexpr auto langStyle = fmt::fg(fmt::color::yellow_green) | fmt::emphasis::bold;
    constexpr auto statStyle = fmt::fg(fmt::color::white);

    bool supportsColorOutput(FILE* out) noexcept;

    template <typename... Args>
    void printStyled(
        FILE* out,
        fmt::text_style style,
        fmt::format_string<Args...> format,
        Args&&... args
    ) {
        if (supportsColorOutput(out)) {
            fmt::print(out, style, format, std::forward<Args>(args)...);
        } else {
            fmt::print(out, format, std::forward<Args>(args)...);
        }
    }

    int getTotalWidth(size_t languageWidth) noexcept;
    fmt::color rainbowColor(size_t i) noexcept;

    void printHeader(
        FILE* out,
        size_t languageWidth,
        double analysisTimeMs,
        double filesPerSec,
        double linesPerSec,
        double bytesPerSec
    ) noexcept;

    void printLangStat(
        FILE* out,
        size_t languageWidth,
        Language language, uint64_t fileCount, uint64_t totalLines,
        uint64_t codeLines, uint64_t commentLines, uint64_t blankLines
    ) noexcept;

    void printFooter(
        FILE* out,
        size_t languageWidth, uint64_t fileCount, uint64_t totalLines,
        uint64_t codeLines, uint64_t commentLines, uint64_t blankLines,
        uint64_t totalBytes
    ) noexcept;
}
