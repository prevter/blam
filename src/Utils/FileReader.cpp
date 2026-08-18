#include "FileReader.hpp"

#ifdef BLAM_PLATFORM_UNIX
#include <cerrno>
#include <unistd.h>
#include <sys/stat.h>

namespace blam::detail {
    Handle open(char const* path) noexcept {
    #if defined(BLAM_PLATFORM_LINUX) && defined(O_NOATIME)
        int fd = ::open(path, O_RDONLY | O_CLOEXEC | O_NOATIME);
        if (fd < 0 && errno == EPERM) {
            fd = ::open(path, O_RDONLY | O_CLOEXEC);
        }
        return fd;
    #else
        return ::open(path, O_RDONLY | O_CLOEXEC);
    #endif
    }

    void close(Handle h) noexcept {
        if (h != INVALID) ::close(h);
    }

    static ssize_t read(Handle h, void* buffer, size_t bytes) noexcept {
        return ::read(h, buffer, bytes);
    }

    static ssize_t pread(Handle h, void* buffer, size_t bytes, Offset offset) noexcept {
        size_t done = 0;
        auto* dst = static_cast<uint8_t*>(buffer);
        while (done < bytes) {
            ssize_t n = ::pread(h, dst + done, bytes - done, offset + done);
            if (n == 0) break;
            if (n == -1) {
                if (errno == EINTR) continue;
                return -1;
            }
            done += n;
        }
        return done;
    }

    static FileSize fileSize(Handle h) noexcept {
        struct stat st{};
        if (::fstat(h, &st) == -1) return -1;
        return st.st_size;
    }

    void prefetch(Handle h, Offset offset, size_t length) noexcept {
    #ifdef BLAM_PLATFORM_LINUX
        ::readahead(h, offset, length);
    #else
        #ifdef BLAM_PLATFORM_MACOS
        radvisory adv{ .ra_offset = offset, .ra_count = static_cast<int>(length) };
        ::fcntl(h, F_RDADVISE, &adv);
        #else
        (void)h; (void)offset; (void)length;
        #endif
    #endif
    }

    int lastError() noexcept { return errno; }
}
#elif defined(BLAM_PLATFORM_WINDOWS)
#include <algorithm>
#include <limits>

namespace blam::detail {
    Handle open(char const* path) noexcept {
        return ::CreateFileA(
            path,
            GENERIC_READ,
            FILE_SHARE_READ,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
            nullptr
        );
    }

    void close(Handle h) noexcept {
        if (h != INVALID) ::CloseHandle(h);
    }

    static ssize_t read(Handle h, void* buffer, size_t bytes) noexcept {
        DWORD got = 0;
        if (!::ReadFile(h, buffer, static_cast<DWORD>(bytes), &got, nullptr)) {
            return -1;
        }
        return static_cast<ssize_t>(got);
    }

    static ssize_t pread(Handle h, void* buffer, size_t bytes, Offset offset) noexcept {
        size_t done = 0;
        auto* dst = static_cast<uint8_t*>(buffer);

        while (done < bytes) {
            OVERLAPPED ov{};
            LARGE_INTEGER pos = toLargeInt(offset + static_cast<int64_t>(done));
            ov.Offset = pos.LowPart;
            ov.OffsetHigh = static_cast<DWORD>(pos.HighPart);

            DWORD toRead = static_cast<DWORD>(std::min<size_t>(bytes - done, (std::numeric_limits<DWORD>::max)()));
            DWORD got = 0;
            BOOL ok = ::ReadFile(h, dst + done, toRead, &got, &ov);
            if (!ok) {
                DWORD err = ::GetLastError();
                if (err == ERROR_HANDLE_EOF) break;
                return -1;
            }
            if (got == 0) break;
            done += got;
        }
        return static_cast<ssize_t>(done);
    }

    static FileSize fileSize(Handle h) noexcept {
        LARGE_INTEGER size{};
        if (!::GetFileSizeEx(h, &size)) {
            return -1;
        }
        return size.QuadPart;
    }

    void prefetch(Handle, Offset, size_t) noexcept {}

    int lastError() noexcept {
        return static_cast<int>(::GetLastError());
    }
}
#else
#error "Unsupported platform"
#endif

namespace blam {
    FileReader::FileReader(FileReader&& o) noexcept
        : m_handle(o.m_handle), m_fileSize(o.m_fileSize),
          m_pageIndex(o.m_pageIndex), m_bytesInBuffer(o.m_bytesInBuffer),
          m_error(o.m_error), m_isSmall(o.m_isSmall)
    {
        std::memcpy(m_buffer, o.m_buffer, o.m_bytesInBuffer);
        o.m_handle = detail::INVALID;
        o.m_error = 0;
    }

    size_t FileReader::totalPages() const noexcept {
        return m_fileSize <= 0 ? 0 : static_cast<size_t>((m_fileSize + PAGE_BYTES - 1) / PAGE_BYTES);
    }

    bool FileReader::advance() noexcept {
        if (!good() || isLastPage()) return false;
        return this->loadPage(m_pageIndex + 1);
    }

    bool FileReader::seekPage(size_t idx) noexcept {
        if (!good()) return false;
        if (idx >= totalPages()) {
            m_error = EINVAL;
            return false;
        }
        if (idx == 0 && m_isSmall) return true;
        return this->loadPage(idx);
    }

    bool FileReader::rewind() noexcept {
        return seekPage(0);
    }

    void FileReader::close() noexcept {
        detail::close(m_handle);
        m_handle = detail::INVALID;
    }

    bool FileReader::open(char const* path, detail::Handle handle) noexcept {
        this->close();
        m_path = path;
        m_error = 0;
        m_isSmall = false;
        m_fileSize = 0;
        m_pageIndex = 0;
        m_bytesInBuffer = 0;

        if (handle != detail::INVALID) {
            m_handle = handle;
        } else {
            m_handle = detail::open(path);
            if (m_handle == detail::INVALID) {
                m_error = detail::lastError();
                return false;
            }
        }

        ssize_t n = detail::read(m_handle, m_buffer, PAGE_BYTES);
        if (n < 0) {
            m_error = detail::lastError();
            this->close();
            return false;
        }

        if (n == 0) {
            m_fileSize = 0;
            m_isSmall = true;
            return true;
        }

        m_bytesInBuffer = static_cast<size_t>(n);

        if (static_cast<size_t>(n) < PAGE_BYTES) {
            m_fileSize = static_cast<detail::FileSize>(n);
            m_isSmall = true;
            return true;
        }

        m_fileSize = detail::fileSize(m_handle);
        if (m_fileSize < 0) {
            m_error = detail::lastError();
            this->close();
            return false;
        }

        m_isSmall = false;

        this->prefetchNextPage(0);
        return true;
    }

    bool FileReader::loadPage(size_t idx) noexcept {
        detail::Offset offset = static_cast<detail::Offset>(idx) * PAGE_BYTES;
        size_t wanted = (m_fileSize - offset >= static_cast<detail::Offset>(PAGE_BYTES)) ? PAGE_BYTES : static_cast<size_t>(m_fileSize - offset);

        ssize_t n = detail::pread(m_handle, m_buffer, wanted, offset);
        if (n < 0) {
            m_error = detail::lastError();
            return false;
        }

        m_bytesInBuffer = static_cast<size_t>(n);
        m_pageIndex = idx;
        this->prefetchNextPage(idx);
        return true;
    }

    void FileReader::prefetchNextPage(size_t idx) const noexcept {
        size_t next = idx + 1;
        if (next < totalPages()) {
            detail::Offset offset = static_cast<detail::Offset>(next) * PAGE_BYTES;
            size_t len = (m_fileSize - offset >= static_cast<detail::Offset>(PAGE_BYTES)) ? PAGE_BYTES : static_cast<size_t>(m_fileSize - offset);
            detail::prefetch(m_handle, offset, len);
        }
    }
}
