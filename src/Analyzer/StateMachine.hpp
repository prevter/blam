#pragma once

#include "LOCStats.hpp"
#include "../SIMD/Search.hpp"
#include "../Utils/FileReader.hpp"

#if FMT_GCC_VERSION || FMT_CLANG_VERSION
#define ALWAYS_INLINE __attribute__((always_inline))
#else
#define ALWAYS_INLINE
#endif

namespace blam {
    template <class Language>
    struct Parser {
        using Spec = LanguageSpec<Language>;

        static void parse(FileReader& reader, LOCStats& stats, std::vector<FileStats>* perFileStats, blam::Language lang) noexcept {
            enum BaseState : uint8_t {
                None,
                InLineComment,
                InBlockComment,
                InString,
            };

            BaseState state = None;
            uint8_t activeComment = 0;
            uint8_t activeString = 0;
            uint32_t commentDepth = 0;
            bool escaped = false;

            bool hasCode = false;
            bool lineNotBlank = false;

            uint64_t localCode = 0;
            uint64_t localComments = 0;
            uint64_t localBlanks = 0;

            auto const finishLine = [&] ALWAYS_INLINE {
                localBlanks += static_cast<uint64_t>(!lineNotBlank);
                localCode += static_cast<uint64_t>(lineNotBlank & hasCode);
                localComments += static_cast<uint64_t>(lineNotBlank & !hasCode);
                hasCode = false;
                lineNotBlank = false;
            };

            do {
                auto ptr = reinterpret_cast<char const*>(reader.data());
                auto end = ptr + reader.size();

                while (ptr < end) {
                    switch (state) {
                        case InLineComment: ptr = simd::skipToNewline(ptr, end);
                            if (ptr < end) {
                                finishLine();
                                state = None;
                                ++ptr;
                            }
                            continue;

                        case InBlockComment: {
                            auto const& style = Spec::comments[activeComment];

                            auto next = simd::skipNonSignificant<Spec::significantChars>(ptr, end);
                            if (next > ptr) {
                                lineNotBlank = true;
                                ptr = next;
                                if (ptr >= end) continue;
                            }

                            if (*ptr == '\n') {
                                finishLine();
                                ++ptr;
                                continue;
                            }

                            lineNotBlank = true;

                            if constexpr (Spec::nestedComments) {
                                size_t nestedStartLen = matchAnyNestedBlockStart(ptr, end);
                                if (nestedStartLen != 0) {
                                    ++commentDepth;
                                    ptr += nestedStartLen;
                                    continue;
                                }
                            }

                            if (startsWith(ptr, end, style.end)) {
                                if (--commentDepth == 0)
                                    state = None;
                                ptr += style.end.size();
                                continue;
                            }

                            ++ptr;
                            continue;
                        }

                        case InString: {
                            auto const& style = Spec::strings[activeString];

                            if (!escaped) {
                                char const endFirst = style.end.empty() ? '\0' : style.end[0];
                                char const* next = simd::findAnyOfThree(ptr, end, '\n', '\\', endFirst);
                                if (next > ptr) {
                                    hasCode = lineNotBlank = true;
                                    ptr = next;
                                    if (ptr >= end) continue;
                                }
                            }

                            if (*ptr == '\n') {
                                if (!style.supportsMultiline) {
                                    state = None;
                                    escaped = false;
                                }
                                finishLine();
                                ++ptr;
                                continue;
                            }

                            hasCode = lineNotBlank = true;

                            if (escaped) {
                                escaped = false;
                                ++ptr;
                                continue;
                            }

                            if (*ptr == '\\') {
                                escaped = true;
                                ++ptr;
                                continue;
                            }

                            if (startsWith(ptr, end, style.end)) {
                                ptr += style.end.size();
                                state = None;
                                escaped = false;
                                continue;
                            }

                            ++ptr;
                            continue;
                        }

                        case None: {
                            while (ptr < end) {
                                {
                                    auto [next, sawCode] = simd::skipNonSignificantClassified<Spec::significantChars>(ptr, end);
                                    if (next > ptr) {
                                        if (sawCode) lineNotBlank = hasCode = true;
                                        ptr = next;
                                        if (ptr >= end) break;
                                    }
                                }

                                char ch = *ptr;

                                if (ch == '\n') {
                                    finishLine();
                                    ++ptr;
                                    continue;
                                }

                                auto [lineCommentIdx, lineCommentLen] = matchCommentStart<false>(ptr, end);
                                if (lineCommentIdx >= 0) {
                                    lineNotBlank = true;
                                    state = InLineComment;
                                    ptr += lineCommentLen;
                                    break;
                                }

                                auto [blockCommentIdx, blockCommentLen] = matchCommentStart<true>(ptr, end);
                                if (blockCommentIdx >= 0) {
                                    lineNotBlank = true;
                                    state = InBlockComment;
                                    activeComment = static_cast<uint8_t>(blockCommentIdx);
                                    commentDepth = 1;
                                    ptr += blockCommentLen;
                                    break;
                                }

                                auto [stringIdx, stringLen] = matchStringStart(ptr, end);
                                if (stringIdx >= 0) {
                                    hasCode = lineNotBlank = true;
                                    state = InString;
                                    activeString = static_cast<uint8_t>(stringIdx);
                                    escaped = false;
                                    ptr += stringLen;
                                    break;
                                }

                                if (ch != ' ' && ch != '\t' && ch != '\r') {
                                    lineNotBlank = hasCode = true;
                                }

                                ++ptr;
                            }
                            break;
                        }
                    }
                }
            } while (reader.advance());

            if (lineNotBlank) finishLine();

            stats.totalLines += localCode + localComments + localBlanks;
            stats.fileSize += reader.fileSize();
            stats.codeLines += localCode;
            stats.commentLines += localComments;
            stats.blankLines += localBlanks;

            if (perFileStats) {
                perFileStats->push_back({
                    .path = reader.path(),
                    .totalLines = localCode + localComments + localBlanks,
                    .codeLines = localCode,
                    .commentLines = localComments,
                    .blankLines = localBlanks,
                    .fileSize = static_cast<uint64_t>(reader.fileSize()),
                    .language = lang,
                });
            }
        }

    private:
        struct MatchResult {
            int index;
            size_t len;
        };

        static bool startsWith(char const* ptr, char const* end, std::string_view token) noexcept {
            return std::string_view(ptr, static_cast<size_t>(end - ptr)).starts_with(token);
        }

        template <bool Multiline>
        static MatchResult matchCommentStart(char const* ptr, char const* end) noexcept {
            MatchResult result{-1, 0};

            auto check = [&]<size_t... Is>(std::index_sequence<Is...>) ALWAYS_INLINE {
                (([&]() {
                    auto const& style = Spec::comments[Is];
                    if constexpr (style.isMultiline == Multiline && !style.start.empty()) {
                        if (style.start[0] == *ptr && startsWith(ptr, end, style.start)) {
                            if (style.start.size() > result.len) {
                                result = {Is, style.start.size()};
                            }
                        }
                    }
                }()), ...);
            };

            check(std::make_index_sequence<Spec::comments.size()>{});
            return result;
        }

        static MatchResult matchStringStart(char const* ptr, char const* end) noexcept {
            MatchResult result{-1, 0};

            auto check = [&]<size_t... Is>(std::index_sequence<Is...>) ALWAYS_INLINE {
                (([&]() {
                    auto const& style = Spec::strings[Is];
                    if (!style.start.empty()) {
                        if (style.start[0] == *ptr && startsWith(ptr, end, style.start)) {
                            if (style.start.size() > result.len) {
                                result = {Is, style.start.size()};
                            }
                        }
                    }
                }()), ...);
            };

            check(std::make_index_sequence<Spec::strings.size()>{});
            return result;
        }

        static size_t matchAnyNestedBlockStart(char const* ptr, char const* end) noexcept {
            for (size_t i = 0; i < Spec::comments.size(); ++i) {
                auto const& style = Spec::comments[i];
                if (!style.isMultiline || style.start.empty()) continue;
                if (style.start[0] != *ptr) continue;
                if (startsWith(ptr, end, style.start))
                    return style.start.size();
            }
            return 0;
        }
    };
}

// register all language parsers
#define BLAM_STATE_MACHINE_HPP
#include <dispatcher.hpp>