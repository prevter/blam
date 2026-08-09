#pragma once

#include <algorithm>
#include <bitset>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace wtf::glob {
    struct CharacterClass {
        std::bitset<256> bits{};
    };

    struct Atom {
        enum class Kind : uint8_t { Literal, AnyChar, Class, Star, DoubleStar };

        Kind kind;

        union {
            char literal = 0;
            uint8_t classIndex;
        };

        static Atom makeLiteral(char c) noexcept {
            return {.kind = Kind::Literal, .literal = c};
        }

        static Atom makeAny() noexcept {
            return {.kind = Kind::AnyChar, .classIndex = 0};
        }

        static Atom makeStar() noexcept {
            return {.kind = Kind::Star, .classIndex = 0};
        }

        static Atom makeDoubleStar() noexcept {
            return {.kind = Kind::DoubleStar, .classIndex = 0};
        }

        static Atom makeClass(uint8_t index) noexcept {
            return {.kind = Kind::Class, .classIndex = index};
        }

        [[nodiscard]] bool matches(char c, std::span<CharacterClass const> classes) const noexcept {
            switch (kind) {
                case Kind::Literal: return c == literal;
                case Kind::AnyChar: return c != '/';
                case Kind::Class: return c != '/' && classes[classIndex].bits.test(static_cast<uint8_t>(c));
                case Kind::Star: return false;
                case Kind::DoubleStar: return false;
            }
            std::unreachable();
        }
    };

    class Pattern {
    public:
        static Pattern compile(std::string_view pattern) {
            Pattern result;
            result.m_atoms.reserve(pattern.size());

            size_t i = 0;

            while (i < pattern.size()) {
                char c = pattern[i];

                if (c == '*') {
                    result.m_hasStar = true;
                    if (i + 1 < pattern.size() && pattern[i + 1] == '*') {
                        result.m_atoms.push_back(Atom::makeDoubleStar());
                        i += 2;
                        while (i < pattern.size() && pattern[i] == '*') ++i;
                    } else {
                        result.m_atoms.push_back(Atom::makeStar());
                        ++i;
                    }
                    continue;
                }

                if (c == '\\' && i + 1 < pattern.size()) {
                    result.m_atoms.push_back(Atom::makeLiteral(pattern[i + 1]));
                    i += 2;
                    continue;
                }

                if (c == '?') {
                    result.m_atoms.push_back(Atom::makeAny());
                    ++i;
                    continue;
                }

                if (c == '[') {
                    size_t consumed = 0;
                    if (auto cls = parseBracket(pattern.substr(i), consumed)) {
                        result.m_classes.push_back(*cls);
                        result.m_atoms.push_back(Atom::makeClass(static_cast<uint8_t>(result.m_classes.size() - 1)));
                        i += consumed;
                        continue;
                    }
                }

                result.m_atoms.push_back(Atom::makeLiteral(c));
                ++i;
            }

            return result;
        }

        [[nodiscard]] bool match(std::string_view text) const noexcept {
            if (!m_hasStar && m_classes.empty()) {
                if (m_atoms.size() != text.size()) return false;
                for (size_t i = 0; i < m_atoms.size(); ++i) {
                    if (m_atoms[i].kind == Atom::Kind::AnyChar) {
                        if (text[i] == '/') return false;
                    } else if (m_atoms[i].literal != text[i]) {
                        return false;
                    }
                }
                return true;
            }

            size_t pos = 0;
            size_t end = text.size();
            size_t atomIdx = 0;

            while (
                atomIdx < m_atoms.size()
                && m_atoms[atomIdx].kind != Atom::Kind::Star
                && m_atoms[atomIdx].kind != Atom::Kind::DoubleStar
            ) {
                if (pos >= end || !m_atoms[atomIdx].matches(text[pos], m_classes)) return false;
                ++pos;
                ++atomIdx;
            }

            if (atomIdx == m_atoms.size()) return pos == end;

            size_t tailAtomIdx = m_atoms.size();
            while (
                tailAtomIdx > atomIdx
                && m_atoms[tailAtomIdx - 1].kind != Atom::Kind::Star
                && m_atoms[tailAtomIdx - 1].kind != Atom::Kind::DoubleStar
            ) {
                --tailAtomIdx;
                if (end <= pos || !m_atoms[tailAtomIdx].matches(text[end - 1], m_classes)) return false;
                --end;
            }

            while (atomIdx < tailAtomIdx) {
                auto kind = m_atoms[atomIdx].kind;
                if (kind == Atom::Kind::Star || kind == Atom::Kind::DoubleStar) {
                    ++atomIdx;
                    continue;
                }

                size_t compEnd = atomIdx;
                while (
                    compEnd < tailAtomIdx
                    && m_atoms[compEnd].kind != Atom::Kind::Star
                    && m_atoms[compEnd].kind != Atom::Kind::DoubleStar
                ) {
                    ++compEnd;
                }
                size_t compLen = compEnd - atomIdx;

                bool isDoubleStar = m_atoms[atomIdx - 1].kind == Atom::Kind::DoubleStar;
                size_t start = pos;
                bool found = false;

                while (start + compLen <= end) {
                    bool ok = true;
                    for (size_t k = 0; k < compLen; ++k) {
                        if (!m_atoms[atomIdx + k].matches(text[start + k], m_classes)) {
                            ok = false;
                            break;
                        }
                    }

                    if (ok) {
                        pos = start + compLen;
                        found = true;
                        break;
                    }

                    if (!isDoubleStar && text[start] == '/') break;
                    ++start;
                }

                if (!found) return false;
                atomIdx = compEnd;
            }

            if (m_atoms[tailAtomIdx - 1].kind != Atom::Kind::DoubleStar) {
                for (size_t i = pos; i < end; ++i) {
                    if (text[i] == '/') return false;
                }
            }

            return true;
        }

    private:
        static std::optional<CharacterClass> parseBracket(std::string_view s, size_t& consumed) {
            size_t q = 1;
            bool negate = false;
            if (q < s.size() && (s[q] == '!' || s[q] == '^')) {
                negate = true;
                ++q;
            }

            CharacterClass cls;
            bool first = true;

            while (q < s.size() && (s[q] != ']' || first)) {
                char lo = s[q];

                if (!first && s[q] == '-' && q + 1 < s.size() && s[q + 1] != ']') {
                    cls.bits.set('-');
                    ++q;
                } else if (q + 2 < s.size() && s[q + 1] == '-' && s[q + 2] != ']') {
                    auto loCode = static_cast<uint8_t>(lo);
                    auto hiCode = static_cast<uint8_t>(s[q + 2]);
                    if (loCode > hiCode) std::swap(loCode, hiCode);

                    for (uint32_t c = loCode; c <= hiCode; ++c) {
                        cls.bits.set(c);
                    }

                    q += 3;
                } else {
                    cls.bits.set(lo);
                    ++q;
                }
                first = false;
            }

            if (q >= s.size() || s[q] != ']') return std::nullopt;

            if (negate) cls.bits.flip();
            consumed = q + 1;
            return cls;
        }

    private:
        std::vector<Atom> m_atoms;
        std::vector<CharacterClass> m_classes;
        bool m_hasStar = false;
    };
}
