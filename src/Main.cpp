#include <chrono>
#include <ranges>
#include <thread>
#include <vector>

#include <slic.hpp>
#include <fmt/format.h>

#ifdef BLAM_PLATFORM_UNIX
#include <sys/resource.h>
#endif

#include "Utils/DirWalker.hpp"
#include "Utils/FileReader.hpp"

#include "Analyzer/Languages.hpp"
#include "Analyzer/StateMachine.hpp"
#include "Output/CSV.hpp"
#include "Output/Text.hpp"

enum class OutputType { Text, JSON, CSV };
enum class SortingMode { Files, TotalLines, CodeLines, CommentLines, BlankLines };

template <>
struct slic::ValueParser<OutputType> {
    static std::optional<OutputType> parse(std::string_view str) noexcept {
        if (str == "text") return OutputType::Text;
        // if (str == "json") return OutputType::JSON; // TODO
        if (str == "csv") return OutputType::CSV;
        return std::nullopt;
    }
};

template <>
struct slic::ValueParser<SortingMode> {
    static std::optional<SortingMode> parse(std::string_view str) noexcept {
        if (str == "files") return SortingMode::Files;
        if (str == "lines") return SortingMode::TotalLines;
        if (str == "code") return SortingMode::CodeLines;
        if (str == "comments") return SortingMode::CommentLines;
        if (str == "blanks") return SortingMode::BlankLines;

        // add extra aliases for convenience
        if (str == "file") return SortingMode::Files;
        if (str == "total") return SortingMode::TotalLines;
        if (str == "comment") return SortingMode::CommentLines;
        if (str == "blank") return SortingMode::BlankLines;

        return std::nullopt;
    }
};

struct Args {
    slic::ArgSpan paths;
    std::string_view outFile;
    OutputType outputType = OutputType::Text;
    SortingMode sortingMode = SortingMode::TotalLines;
    size_t threads = 0;
    size_t topN = 0;
    bool collectPerFile = false;
    bool showHidden = false;
    bool noGitignore = false;
    bool help = false;
    bool version = false;

    static constexpr std::string_view Description = "blam - Bazillion Lines Analyzed in Milliseconds";
    static constexpr std::tuple Options = {
        slic::Option{"--threads", "-t", &Args::threads, "Number of threads to use (default: number of CPU cores)"},
        slic::Option{"--hidden", "-i", &Args::showHidden, "Include hidden files and directories"},
        slic::Option{"--no-gitignore", "-g", &Args::noGitignore, "Disable .gitignore support"},
        slic::Option{"--sort", "-s", &Args::sortingMode, "Sorting mode for output (files, lines, code, comments, blanks)"},
        slic::Option{"--top", "-n", &Args::topN, "Show only the top N languages (default: show all)"},
        // slic::Option{"--per-file", "-p", &Args::collectPerFile, "Collect stats for each file (increases memory usage)"}, // TODO: output
        slic::Option{"--output", "-o", &Args::outFile, "Output file (default: stdout)"},
        slic::Option{"--format", "-f", &Args::outputType, "Output format (text, csv)"},
        slic::Option{"--help", "-h", &Args::help, "Show this help message"},
        slic::Option{"--version", "-v", &Args::version, "Show version information"},
        slic::VarArgs{"Files/directories to scan", &Args::paths}
    };
};

static void HandleFile(
    char const* file,
    blam::detail::Handle fd,
    blam::Language language,
    blam::LOCBucket& bucket,
    std::vector<blam::FileStats>* perFileStats
) {
    auto& stat = bucket[language];
    bucket.markHasData(language);

    ++stat.fileCount;

    blam::FileReader reader;
    if (!reader.open(file, fd)) {
        return;
    }

    blam::dispatchParser(reader, stat, perFileStats, language);
}

int main(int argc, char** argv) {
    auto start = std::chrono::high_resolution_clock::now();

    slic::ArgParser<Args> parser(argc, argv);
    auto result = parser.parse();
    if (!result) {
        result.print();
        fmt::println("Use --help to see usage information.");
        return 1;
    }

    auto const& args = parser.result();
    if (args.version) {
        fmt::println("blam version v" BLAM_VERSION);
        return 0;
    }

    if (args.help) {
        parser.printHelp();
        return 0;
    }

#ifdef BLAM_PLATFORM_UNIX
    // increase the open-file-descriptor limit
    rlimit rl{};
    if (::getrlimit(RLIMIT_NOFILE, &rl) == 0) {
        rl.rlim_cur = rl.rlim_max == RLIM_INFINITY ? 65535 : std::min<rlim_t>(65535, rl.rlim_max);
        ::setrlimit(RLIMIT_NOFILE, &rl);
    }
#endif

    {
        blam::Pool pool(
            args.threads ? args.threads : std::thread::hardware_concurrency(),
            !args.showHidden,
            !args.noGitignore,
            args.collectPerFile,
            &HandleFile
        );

        {
            pool.retain();

            if (args.paths.empty()) {
                pool.submitTask(".");
            } else for (auto path : args.paths) {
                pool.submitTask(path);
            }

            pool.release();
            pool.waitUntilIdle();
        }

        auto stats = pool.aggregateStats();
        auto perFileStats = pool.perFileStats();

        switch (args.sortingMode) {
            case SortingMode::Files:
                std::ranges::sort(stats, [](auto const& a, auto const& b) {
                    return a.second.fileCount > b.second.fileCount;
                });
                break;
            case SortingMode::TotalLines:
                std::ranges::sort(stats, [](auto const& a, auto const& b) {
                    return a.second.totalLines > b.second.totalLines;
                });
                break;
            case SortingMode::CodeLines:
                std::ranges::sort(stats, [](auto const& a, auto const& b) {
                    return a.second.codeLines > b.second.codeLines;
                });
                break;
            case SortingMode::CommentLines:
                std::ranges::sort(stats, [](auto const& a, auto const& b) {
                    return a.second.commentLines > b.second.commentLines;
                });
                break;
            case SortingMode::BlankLines:
                std::ranges::sort(stats, [](auto const& a, auto const& b) {
                    return a.second.blankLines > b.second.blankLines;
                });
                break;
        }

        if (args.topN > 0 && stats.size() > args.topN) {
            stats.resize(args.topN);
        }

        auto end = std::chrono::high_resolution_clock::now();

        // calculate overall metrics
        double totalTime = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count() / 1'000'000.0;
        double filesPerSec = 0;
        double linesPerSec = 0;
        double bytesPerSec = 0;

        uint64_t totalFiles = 0;
        uint64_t totalLines = 0;
        uint64_t totalBytes = 0;
        uint64_t totalCodeLines = 0;
        uint64_t totalCommentLines = 0;
        uint64_t totalBlankLines = 0;
        size_t longestLang = sizeof("Language") - 1;

        for (auto const& [lang, stat] : stats) {
            if (lang == blam::Language::Unknown) continue;
            longestLang = std::max(longestLang, blam::format_as(lang).size());
            totalFiles += stat.fileCount;
            totalLines += stat.totalLines;
            totalBytes += stat.fileSize;
            totalCodeLines += stat.codeLines;
            totalCommentLines += stat.commentLines;
            totalBlankLines += stat.blankLines;
        }

        if (totalTime > 0) {
            filesPerSec = totalFiles / (totalTime / 1000.0);
            linesPerSec = totalLines / (totalTime / 1000.0);
            bytesPerSec = totalBytes / (totalTime / 1000.0);
        }

        FILE* out = stdout;
        if (!args.outFile.empty()) {
            out = std::fopen(args.outFile.data(), "w");
            if (!out) {
                fmt::println(stderr, "Failed to open output file '{}'", args.outFile);
                return 1;
            }
        }

        switch (args.outputType) {
            case OutputType::Text: {
                blam::text::printHeader(out, longestLang, totalTime, filesPerSec, linesPerSec, bytesPerSec);

                for (auto const& [lang, stat] : stats) {
                    if (lang == blam::Language::Unknown) continue;
                    blam::text::printLangStat(
                        out,
                        longestLang,
                        lang,
                        stat.fileCount,
                        stat.totalLines,
                        stat.codeLines,
                        stat.commentLines,
                        stat.blankLines
                    );
                }

                blam::text::printFooter(
                    out,
                    longestLang,
                    totalFiles,
                    totalLines,
                    totalCodeLines,
                    totalCommentLines,
                    totalBlankLines,
                    totalBytes
                );
                break;
            }
            case OutputType::CSV: {
                blam::csv::printHeader(out);

                for (auto const& [lang, stat] : stats) {
                    if (lang == blam::Language::Unknown) continue;
                    blam::csv::printLangStat(
                        out,
                        lang,
                        stat.fileCount,
                        stat.totalLines,
                        stat.codeLines,
                        stat.commentLines,
                        stat.blankLines
                    );
                }

                blam::csv::printFooter(
                    out,
                    totalFiles,
                    totalLines,
                    totalCodeLines,
                    totalCommentLines,
                    totalBlankLines
                );
                break;
            }
            case OutputType::JSON: {
                break;
            }
        }

        if (out != stdout) {
            std::fclose(out);
        }
    }

    return 0;
}