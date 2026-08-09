#include "Text.hpp"

#include <array>

#ifdef WTF_PLATFORM_WINDOWS
#include <io.h>
#else
#include <unistd.h>
#endif

struct Time { double ms; };
struct Bytes { double count; };
struct Count { double value; };

template <>
struct fmt::formatter<Time> {
    constexpr auto parse(format_parse_context& ctx) noexcept { return ctx.begin(); }

    template <typename FormatContext>
    auto format(Time t, FormatContext& ctx) const noexcept {
        double v = t.ms;
        std::string_view unit = "ms";

        if (v < 0.001) {
            v *= 1'000'000;
            unit = "ns";
        } else if (v < 1.0) {
            v *= 1'000;
            unit = "µs";
        } else if (v >= 1000.0) {
            v /= 1000;
            unit = "s";
        }

        return fmt::format_to(ctx.out(), "{:.2f}{}", v, unit);
    }
};

template <>
struct fmt::formatter<Bytes> {
    constexpr auto parse(format_parse_context& ctx) noexcept { return ctx.begin(); }

    template <typename FormatContext>
    auto format(Bytes b, FormatContext& ctx) const noexcept {
        double v = b.count;
        int i = 0;

        static constexpr std::array units = std::to_array<std::string_view>({
            "B", "KiB", "MiB", "GiB", "TiB"
        });

        while (v >= 1024 && i < static_cast<int>(units.size()) - 1) {
            v /= 1024;
            ++i;
        }

        return fmt::format_to(ctx.out(), "{:.2f} {}", v, units[i]);
    }
};

template <>
struct fmt::formatter<Count> {
    constexpr auto parse(format_parse_context& ctx) noexcept { return ctx.begin(); }

    template <typename FormatContext>
    auto format(Count c, FormatContext& ctx) const noexcept {
        double v = c.value;
        std::string_view suffix;

        if (v >= 1'000'000'000) {
            v /= 1'000'000'000;
            suffix = "B";
        } else if (v >= 1'000'000) {
            v /= 1'000'000;
            suffix = "M";
        } else if (v >= 1'000) {
            v /= 1'000;
            suffix = "K";
        }

        return fmt::format_to(ctx.out(), "{:.2f}{}", v, suffix);
    }
};

namespace wtf::text {
    bool supportsColorOutput(FILE* out) noexcept {
        #ifdef _WIN32
        static bool res = _isatty(_fileno(out)) != 0;
        #else
        static bool res = isatty(fileno(out)) != 0;
        #endif
        return res;
    }

    int getTotalWidth(size_t languageWidth) noexcept {
        return 1 + languageWidth +
               1 + 10 + // Files
               1 + 12 + // Lines
               1 + 12 + // Code
               1 + 12 + // Comments
               1 + 12 + // Blanks
               1;
    }

    void printHeader(
        FILE* out, size_t languageWidth, double analysisTimeMs, double filesPerSec, double linesPerSec,
        double bytesPerSec
    ) noexcept {
        printStyled(out, title, "wtf v{}", WTF_VERSION);
        printStyled(out, dim, " | ");
        printStyled(out, good, "analysis: {}", Time{analysisTimeMs});
        printStyled(out, dim, " | ");
        printStyled(out, good, "{} files/s", Count{filesPerSec});
        printStyled(out, dim, " | ");
        printStyled(out, good, "{} lines/s", Count{linesPerSec});
        printStyled(out, dim, " | ");
        printStyled(out, good, "{}/s\n", Bytes{bytesPerSec});

        auto totalWidth = getTotalWidth(languageWidth);

        printStyled(out,dim, "{:─<{}}\n", "", totalWidth);
        printStyled(
            out, accent, " {:<{}} {:>10} {:>12} {:>12} {:>12} {:>12}\n",
            "Language", languageWidth,
            "Files", "Lines", "Code", "Comments", "Blanks"
        );
        printStyled(out, dim, "{:─<{}}\n", "", totalWidth);
    }

    void printLangStat(
        FILE* out,
        size_t languageWidth,
        Language language, uint64_t fileCount, uint64_t totalLines,
        uint64_t codeLines, uint64_t commentLines, uint64_t blankLines
    ) noexcept {
        printStyled(
            out,
            langStyle,
            " {:<{}}", language, languageWidth
        );

        printStyled(
            out, statStyle,
            " {:>10} {:>12} {:>12} {:>12} {:>12}\n",
            fileCount, totalLines, codeLines, commentLines, blankLines
        );
    }

    void printFooter(
        FILE* out, size_t languageWidth, uint64_t fileCount, uint64_t totalLines, uint64_t codeLines,
        uint64_t commentLines, uint64_t blankLines, uint64_t totalBytes
    ) noexcept {
        auto totalWidth = getTotalWidth(languageWidth);

        printStyled(out, dim, "{:─<{}}\n", "", totalWidth);
        printStyled(
            out, accent, " {:<{}} {:>10} {:>12} {:>12} {:>12} {:>12}\n",
            "Total", languageWidth,
            fileCount, totalLines, codeLines, commentLines, blankLines
        );
        printStyled(out, dim, "{:─<{}}\n", "", totalWidth);

        printStyled(out, langStyle, "Total size: ");
        printStyled(out, good, "{} bytes ({})\n", fmt::group_digits(totalBytes), Bytes{static_cast<double>(totalBytes)});
    }
}
