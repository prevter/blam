from __future__ import annotations

from dataclasses import dataclass, field
from datetime import datetime
from pathlib import Path
import json
import sys


@dataclass(eq=True)
class CommentStyle:
    start: str
    end: str
    is_multiline: bool


@dataclass(eq=True)
class StringStyle:
    start: str
    end: str
    multiline: bool


@dataclass
class LanguageSpec:
    name: str
    comment_styles: list[CommentStyle]
    string_styles: list[StringStyle]
    nested_comments: bool = False

    significant_chars: list[str] = field(default_factory=list)
    alt_names: list[str] = field(default_factory=list)

    def __post_init__(self):
        def first_literal_char(token: str) -> str:
            if not token:
                return ""

            if token[0] != "\\" or len(token) == 1:
                return token[0]

            return token[1]

        chars = set()

        for c in self.comment_styles:
            chars.add(first_literal_char(c.start))
            if c.end:
                chars.add(first_literal_char(c.end))

        for s in self.string_styles:
            chars.add(first_literal_char(s.start))
            if s.end:
                chars.add(first_literal_char(s.end))

        self.significant_chars = sorted(chars)

    def is_equivalent(self, other: LanguageSpec) -> bool:
        return (
                self.nested_comments == other.nested_comments
                and self.comment_styles == other.comment_styles
                and self.string_styles == other.string_styles
        )


def collect_pairs(items: list[str], lang: str) -> list[tuple[str, str]]:
    return [(item, lang) for item in items]


def escape_char(c: str) -> str:
    if c == "'":
        return "\\'"
    if c == "\\":
        return "\\\\"
    return c


def format_cpp_array(items: list[str]) -> str:
    return ", ".join(f"'{escape_char(c)}'" for c in items)


def main(input_file, output_file, dispatch_file):
    print(f"Generating languages from {input_file} to {output_file}")

    data = json.loads(Path(input_file).read_text())["languages"]

    extensions: list[tuple[str, str]] = []
    filenames: list[tuple[str, str]] = []
    names: list[str] = []
    unique_specs: list[LanguageSpec] = []

    for lang, info in data.items():
        extensions += collect_pairs(info.get("extensions", []), lang)
        filenames += collect_pairs(info.get("filenames", []), lang)

        names.append(info.get("name", lang))

        comment_styles = [
            *(CommentStyle(c, "", False) for c in info.get("line_comment", [])),
            *(CommentStyle(c[0], c[1], True) for c in info.get("multi_line_comments", [])),
        ]

        string_styles = [
            *(StringStyle(s[0], s[1], False) for s in info.get("quotes", [])),
            *(StringStyle(s[0], s[1], True) for s in info.get("verbatim_quotes", [])),
        ]

        spec = LanguageSpec(
            name=lang,
            comment_styles=comment_styles,
            string_styles=string_styles,
            nested_comments=info.get("nested", False),
        )

        for existing in unique_specs:
            if spec.is_equivalent(existing):
                existing.alt_names.append(lang)
                break
        else:
            unique_specs.append(spec)

    print(f"Found {len(unique_specs)} unique language specifications")

    extensions.sort()
    filenames.sort()

    Path(output_file).write_text(generate_languages_hpp(data, extensions, filenames, names, unique_specs))
    Path(dispatch_file).write_text(generate_dispatcher_hpp(data))


def generate_languages_hpp(data, extensions, filenames, names, unique_specs) -> str:
    now = datetime.now().strftime("%Y-%m-%d %H:%M:%S")

    lines: list[str] = []

    def w(line=""):
        lines.append(line)

    w(f"// This file was automatically generated at {now}")
    w("#pragma once\n")
    w('#ifndef WTF_LANGUAGE_SPECS_HPP')
    w('#error "LanguageSpecs.hpp should be included instead of languages.hpp"')
    w("#endif\n")

    w("#include <array>")
    w("#include <cstddef>")
    w("#include <cstdint>")
    w("#include <string_view>")
    w("#include <utility>\n")

    w("namespace wtf {")
    w("    enum class Language : uint16_t {")
    w("        Unknown,")

    for lang in data:
        w(f"        {lang},")

    w("    };\n")

    w(f"    constexpr size_t LanguageCount = {len(data)};\n")

    # Extension map
    w("    constexpr auto ExtensionMap = std::to_array<std::pair<std::string_view, Language>>({")
    for ext, lang in extensions:
        w(f'        {{"{ext}", Language::{lang}}},')
    w("    });\n")

    # Filename map
    w("    constexpr auto FilenameMap = std::to_array<std::pair<std::string_view, Language>>({")
    for f, lang in filenames:
        w(f'        {{"{f}", Language::{lang}}},')
    w("    });\n")

    # Name map
    w("    constexpr auto NameMap = std::to_array<std::string_view>({")
    w('        "Unknown",')
    for name in names:
        w(f'        "{name}",')
    w("    });\n")

    w("    // Language specializations")

    for spec in unique_specs:
        w(f"\n    struct Language{spec.name} {{}};")

        for alt in spec.alt_names:
            w(f"    using Language{alt} = Language{spec.name};")

        # Comments
        w(f"""\n    template <>
    struct LanguageSpec<Language{spec.name}> {{""")

        if spec.comment_styles:
            w(f"        static constexpr std::array<CommentStyle, {len(spec.comment_styles)}> comments = {{""")

            for c in spec.comment_styles:
                w(f'            CommentStyle{{"{c.start}", "{c.end}", {str(c.is_multiline).lower()}}},')

            w("        };")
        else:
            w("        static constexpr std::array<CommentStyle, 0> comments = {};")

        # Strings
        if spec.string_styles:
            w(f"        static constexpr std::array<StringStyle, {len(spec.string_styles)}> strings = {{")
            for s in spec.string_styles:
                w(f'            StringStyle{{"{s.start}", "{s.end}", {str(s.multiline).lower()}}},')
            w("        };")
        else:
            w("        static constexpr std::array<StringStyle, 0> strings = {};")

        # Significant chars
        w(f"        static constexpr std::array<char, {len(spec.significant_chars)}> significantChars = "
          f"{{{format_cpp_array(spec.significant_chars)}}};")

        w(f"        static constexpr bool nestedComments = {str(spec.nested_comments).lower()};")
        w("    };")

    w("}")

    return "\n".join(lines)


def generate_dispatcher_hpp(data) -> str:
    now = datetime.now().strftime("%Y-%m-%d %H:%M:%S")

    lines: list[str] = []

    def w(line=""):
        lines.append(line)

    w(f"// This file was automatically generated at {now}")
    w("#pragma once\n")
    w('#ifndef WTF_STATE_MACHINE_HPP')
    w('#error "StateMachine.hpp should be included instead of dispatcher.hpp"')
    w("#endif\n")

    w("namespace wtf {")
    w("    inline void dispatchParser(FileReader& reader, LOCStats& stats, std::vector<wtf::FileStats>* perFileStats, Language lang) noexcept {")
    w("        switch (lang) {")
    w("            default: case Language::Unknown: std::unreachable(); break;")

    for lang in data:
        w(f"            case Language::{lang}: Parser<Language{lang}>::parse(reader, stats, perFileStats, lang); break;")

    w("        }")
    w("    }")
    w("}")

    return "\n".join(lines)


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2], sys.argv[3])
