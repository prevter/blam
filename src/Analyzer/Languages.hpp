#pragma once

#include <algorithm>
#include <cctype>
#include "LanguageSpecs.hpp"

namespace wtf {
    struct CaseInsensitiveCompare {
        using is_transparent = void;

        static constexpr uint8_t toLower(uint8_t c) noexcept {
            return c >= 'A' && c <= 'Z' ? c | 0x20 : c;
        }

        constexpr bool operator()(std::string_view a, std::string_view b) const noexcept {
            return std::ranges::lexicographical_compare(a, b,
                [](uint8_t x, uint8_t y) {
                    return toLower(x) < toLower(y);
                }
            );
        }
    };

    inline Language detectLanguage(std::string_view filename) noexcept {
        auto lastSlash = filename.find_last_of('/');
        if (lastSlash != std::string_view::npos) {
            filename = filename.substr(lastSlash + 1);
        }

        auto filenameIt = std::lower_bound(
            FilenameMap.begin(),
            FilenameMap.end(),
            filename, [](auto const& pair, std::string_view val) {
                return CaseInsensitiveCompare{}(pair.first, val);
            }
        );

        if (
            filenameIt != FilenameMap.end()
            && !CaseInsensitiveCompare{}(filename, filenameIt->first)
        ) {
            return filenameIt->second;
        }

        auto extPos = filename.find_last_of('.');
        if (extPos == std::string_view::npos || extPos == filename.length() - 1) {
            return Language::Unknown;
        }

        auto ext = filename.substr(extPos + 1);
        auto extIt = std::lower_bound(
            ExtensionMap.begin(),
            ExtensionMap.end(),
            ext, [](auto const& pair, std::string_view val) {
                return CaseInsensitiveCompare{}(pair.first, val);
            }
        );

        if (
            extIt != ExtensionMap.end()
            && !CaseInsensitiveCompare{}(ext, extIt->first)
        ) {
            return extIt->second;
        }

        return Language::Unknown;
    }

    inline std::string_view format_as(Language lang) noexcept {
        return NameMap[static_cast<size_t>(lang)];
    }
}