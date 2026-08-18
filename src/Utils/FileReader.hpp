#pragma once

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

#ifdef BLAM_PLATFORM_UNIX
#include <fcntl.h>
#include <sys/types.h>

#ifdef BLAM_PLATFORM_MACOS
#include <sys/fcntl.h>
#endif

namespace blam::detail {
    using Handle = int;
    using Offset = int64_t;
    using FileSize = int64_t;

    static constexpr Handle INVALID = -1;
}
#elif defined(BLAM_PLATFORM_WINDOWS)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#if defined(_MSC_VER) && !defined(_SSIZE_T_DEFINED)
#include <basetsd.h>
using ssize_t = SSIZE_T;
#define _SSIZE_T_DEFINED
#endif

namespace blam::detail {
    using Handle = HANDLE;
    using Offset = int64_t;
    using FileSize = int64_t;

    inline static Handle const INVALID = INVALID_HANDLE_VALUE;

    inline LARGE_INTEGER toLargeInt(int64_t v) noexcept {
        LARGE_INTEGER li{};
        li.QuadPart = v;
        return li;
    }
}
#else
#error "Unsupported platform"
#endif

namespace blam::detail {
    Handle open(char const* path) noexcept;
    void close(Handle h) noexcept;
    void prefetch(Handle h, Offset offset, size_t length) noexcept;
    int lastError() noexcept;
}

namespace blam {
    class FileReader {
    public:
        static constexpr size_t PAGE_KILOBYTE = 256;
        static constexpr size_t PAGE_BYTES = PAGE_KILOBYTE * 1024;
        static constexpr size_t ALIGN_BYTES = 4096;

        FileReader() noexcept = default;
        explicit FileReader(char const* path) noexcept { this->open(path); }
        ~FileReader() noexcept { this->close(); }

        FileReader(FileReader const&) = delete;
        FileReader& operator=(FileReader const&) = delete;

        FileReader(FileReader&& o) noexcept;

        [[nodiscard]] bool good() const noexcept { return m_handle != detail::INVALID && m_error == 0; }
        [[nodiscard]] int error() const noexcept { return m_error; }
        [[nodiscard]] bool isSmallFile() const noexcept { return m_isSmall; }
        explicit operator bool() const noexcept { return this->good(); }

        [[nodiscard]] detail::FileSize fileSize() const noexcept { return m_fileSize; }
        [[nodiscard]] size_t totalPages() const noexcept;
        [[nodiscard]] size_t currentPage() const noexcept { return m_pageIndex; }
        [[nodiscard]] bool isLastPage() const noexcept { return m_pageIndex + 1 >= totalPages(); }

        [[nodiscard]] char const* path() const noexcept { return m_path; }
        [[nodiscard]] std::span<uint8_t const> buffer() const noexcept { return { m_buffer, m_bytesInBuffer }; }
        [[nodiscard]] uint8_t const* data() const noexcept { return m_buffer; }
        [[nodiscard]] size_t size() const noexcept { return m_bytesInBuffer; }

        bool advance() noexcept;
        bool seekPage(size_t idx) noexcept;
        bool rewind() noexcept;
        void close() noexcept;
        bool open(char const* path, detail::Handle handle = detail::INVALID) noexcept;

    private:
        bool loadPage(size_t idx) noexcept;
        void prefetchNextPage(size_t idx) const noexcept;

    private:
        char const* m_path = nullptr;
        detail::Handle m_handle = detail::INVALID;
        detail::FileSize m_fileSize = 0;
        size_t m_pageIndex = 0;
        size_t m_bytesInBuffer = 0;
        int m_error = 0;
        bool m_isSmall = false;

        alignas(ALIGN_BYTES) uint8_t m_buffer[PAGE_BYTES];
    };
}
