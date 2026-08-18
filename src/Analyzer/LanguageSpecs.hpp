#pragma once

#include <array>
#include <string_view>

namespace blam {
    struct CommentStyle {
        std::string_view start;
        std::string_view end;
        bool isMultiline = false;
    };

    struct StringStyle {
        std::string_view start;
        std::string_view end;
        bool supportsMultiline = false;
    };

    template <class Spec>
    struct LanguageSpec {
        static constexpr std::array<CommentStyle, 0> comments = {};
        static constexpr std::array<StringStyle, 0> strings = {};
        static constexpr std::array<char, 0> significantChars = {};
        static constexpr bool nestedComments = false;
    };
}

// generated language specs
#define BLAM_LANGUAGE_SPECS_HPP
#include <languages.hpp>