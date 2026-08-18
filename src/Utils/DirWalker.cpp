#include "DirWalker.hpp"

#include <climits>
#include <cstdlib>
#include <cstring>
#include <fmt/format.h>

#include "FileReader.hpp"
#include "../Analyzer/Languages.hpp"

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#if defined(BLAM_PLATFORM_LINUX)
    #include <dirent.h>
    #include <fcntl.h>
    #include <unistd.h>
    #include <sys/stat.h>
    #include <sys/syscall.h>

    struct Dirent64 {
        ino_t d_ino;
        off_t d_off;
        unsigned short d_reclen;
        unsigned char d_type;
        char d_name[];
    };
#elif defined(BLAM_PLATFORM_MACOS)
    #include <dirent.h>
    #include <fcntl.h>
    #include <unistd.h>
    #include <sys/stat.h>
    #include <sys/attr.h>
    #include <sys/vnode.h>

    struct BulkAttrHeader {
        uint32_t length;
        attribute_set_t returned;
        attrreference_t nameRef;
        fsobj_type_t objType;
    };
#elif defined(BLAM_PLATFORM_WINDOWS)
    #define WIN32_LEAN_AND_MEAN
    #define NOMINMAX
    #include <windows.h>
#else
    #error "Unsupported platform"
#endif

static char* dupPath(char const* s) noexcept {
#ifdef BLAM_PLATFORM_WINDOWS
    return ::_strdup(s);
#else
    return ::strdup(s);
#endif
}

namespace blam {
    Pool::Pool(int numThreads, bool skipHidden, bool useGitignore, bool collectPerFile, FileHandler handler)
        : m_fileHandler(handler), m_numThreads(numThreads),
          m_skipHidden(skipHidden), m_useGitignore(useGitignore),
          m_collectPerFile(collectPerFile)
    {
        m_queues.reserve(numThreads);
        m_threads.reserve(numThreads);
        m_stats.resize(numThreads);

        if (collectPerFile) {
            m_perFileStats.resize(numThreads);
        }

        for (int i = 0; i < numThreads; ++i) {
            m_queues.emplace_back(std::make_unique<Queue>());
        }

        for (int i = 0; i < numThreads; ++i) {
            m_threads.emplace_back(&Pool::workerThread, this, i);
        }
    }

    Pool::~Pool() {
        m_shutdown.store(true, std::memory_order_release);
        m_cv.notify_all();
        for (auto& thread : m_threads) {
            if (thread.joinable()) thread.join();
        }
    }

    void Pool::waitUntilIdle() {
        std::unique_lock lock(m_idleCvMutex);
        m_idleCv.wait(lock, [&] {
            return m_pending.load(std::memory_order_acquire) == 0;
        });
    }

    bool Pool::isDirectory(char const* path) {
#ifdef BLAM_PLATFORM_UNIX
        struct stat st;
        if (::stat(path, &st) != 0) {
            return false;
        }
        return S_ISDIR(st.st_mode);
#elif defined(BLAM_PLATFORM_WINDOWS)
        DWORD attrs = ::GetFileAttributesA(path);
        return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY);
#endif
    }

    void Pool::submitTask(std::string_view path) {
        Task task;

        if (isDirectory(path.data())) {
            task = {
                .path = dupPath(path.data()),
                .gitignore = nullptr,
                .pathLength = static_cast<uint16_t>(path.length()),
                .kind = Task::Kind::ScanDir
            };
        } else {
            auto lang = detectLanguage(path);
            if (lang == Language::Unknown) {
                return;
            }

            task = {
                .path = dupPath(path.data()),
                .gitignore = nullptr,
                .pathLength = static_cast<uint16_t>(path.length()),
                .language = lang,
                .kind = Task::Kind::ParseFile
            };
        }

        m_pending.fetch_add(1, std::memory_order_relaxed);
        {
            std::lock_guard lock(m_pendingTasksMutex);
            m_pendingTasks.push_back(task);
        }
        m_cv.notify_one();
    }

    void Pool::submitTaskLocal(Task const& task, int threadId) {
        m_pending.fetch_add(1, std::memory_order_relaxed);

        if (!m_queues[threadId]->push(task)) [[unlikely]] {
            {
                std::lock_guard lock(m_pendingTasksMutex);
                m_pendingTasks.push_back(task);
            }
            m_cv.notify_one();
            return;
        }

        if (m_sleeping.load(std::memory_order_relaxed) > 0) {
            m_cv.notify_one();
        }
    }

    void Pool::retain() {
        m_pending.fetch_add(1, std::memory_order_relaxed);
    }

    void Pool::release() {
        this->taskDone();
    }

    std::vector<FileStats> Pool::perFileStats() const {
        std::vector<FileStats> result;
        if (!m_collectPerFile) return result;

        size_t totalFiles = 0;
        for (auto const& bucket : m_perFileStats) {
            totalFiles += bucket.size();
        }
        result.reserve(totalFiles);

        for (auto const& bucket : m_perFileStats) {
            result.insert(result.end(), bucket.begin(), bucket.end());
        }

        return result;
    }

    std::vector<std::pair<Language, LOCStats>> Pool::aggregateStats() const {
        std::bitset<LanguageCount> combinedHasData{};

        for (auto const& bucket : m_stats) {
            combinedHasData |= bucket.hasData;
        }

        std::vector<std::pair<Language, LOCStats>> result;
        result.reserve(combinedHasData.count());

        for (size_t i = 0; i < LanguageCount; ++i) {
            if (!combinedHasData.test(i)) continue;

            auto lang = static_cast<Language>(i);
            LOCStats aggregated{};

            for (auto const& bucket : m_stats) {
                aggregated += bucket[lang];
            }

            result.emplace_back(lang, aggregated);
        }

        return result;
    }

    void Pool::taskDone() {
        if (m_pending.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            m_idleCv.notify_all();
        }
    }

    void Pool::processTask(Task const& task, int threadId) {
        if (task.kind == Task::Kind::ParseFile) {
            m_fileHandler(task.path, task.fileDescriptor, task.language, m_stats[threadId], m_collectPerFile ? &m_perFileStats[threadId] : nullptr);
        } else {
            this->scanDir(task.path, task.pathLength, task.gitignore, threadId);
        }

        if (task.kind == Task::Kind::ScanDir || !m_collectPerFile) {
            std::free(const_cast<char*>(task.path));
        }

        this->taskDone();
    }

    std::optional<Task> Pool::popPending() {
        std::lock_guard lock(m_pendingTasksMutex);
        if (m_pendingTasks.empty()) return std::nullopt;
        Task task = m_pendingTasks.back();
        m_pendingTasks.pop_back();
        return task;
    }

    void Pool::workerThread(int threadId) {
        while (true) {
            if (auto t = m_queues[threadId]->pop()) {
                processTask(*t, threadId);
                continue;
            }

            if (auto t = popPending()) {
                processTask(*t, threadId);
                continue;
            }

            // try to steal the task
            for (int i = 1; i < m_numThreads; ++i) {
                if (auto t = m_queues[(threadId + i) % m_numThreads]->steal()) {
                    processTask(*t, threadId);
                    goto next;
                }
            }

            if (m_shutdown.load(std::memory_order_acquire)) {
                break;
            }

            for (int s = 0; s < 32; ++s) {
                SPIN_PAUSE();
                if (auto t = m_queues[threadId]->pop()) {
                    processTask(*t, threadId);
                    goto next;
                }
            }

            for (int i = 1; i < m_numThreads; ++i) {
                if (auto t = m_queues[(threadId + i) % m_numThreads]->steal()) {
                    processTask(*t, threadId);
                    goto next;
                }
            }

            m_sleeping.fetch_add(1, std::memory_order_relaxed);
            {
                std::unique_lock lock(m_cvMutex);
                m_cv.wait_for(lock, std::chrono::microseconds(500), [&] {
                    return m_shutdown.load(std::memory_order_relaxed) || m_pending.load(std::memory_order_relaxed) > 0;
                });
            }
            m_sleeping.fetch_sub(1, std::memory_order_relaxed);

        next:;
        }
    }

#if defined(BLAM_PLATFORM_LINUX)
    void Pool::scanDir(char const* path, uint16_t len, GitignoreNode* gitignore, int threadId) {
        int fd = ::open(path, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
        if (fd < 0) {
            if (gitignore) gitignore->release();
            return;
        }

        auto* node = m_useGitignore ? GitignoreNode::makeNode(path, len, gitignore) : nullptr;

        char child[PATH_MAX];
        std::memcpy(child, path, len);
        child[len] = '/';

        alignas(alignof(Dirent64)) char buffer[256 * 1024]; // 256 KB buffer for readdir

        while (true) {
            long n = ::syscall(SYS_getdents64, fd, buffer, sizeof(buffer));
            if (n <= 0) break;

            for (long pos = 0; pos < n;) {
                auto* d = reinterpret_cast<Dirent64*>(buffer + pos);
                pos += d->d_reclen;

                char const* name = d->d_name;
                if (name[0] == '.' && (!name[1] || (name[1] == '.' && !name[2]))) continue; // skip . and ..
                if (m_skipHidden && name[0] == '.') continue;                               // skip hidden files

                int nlen = std::strlen(name);
                int flen = len + 1 + nlen;
                if (flen >= PATH_MAX) continue;

                std::memcpy(child + len + 1, name, nlen + 1);
                uint16_t flen16 = flen;

                uint8_t type = d->d_type;
                if (type == DT_UNKNOWN) {
                    struct stat st{};
                    if (::fstatat(fd, name, &st, AT_SYMLINK_NOFOLLOW) == 0) {
                        type = S_ISDIR(st.st_mode) ? DT_DIR : DT_REG;
                    }
                }

                if (type == DT_DIR) {
                    if (node) {
                        if (node->isIgnored(child, flen16, true)) continue;
                        node->retain();
                    }

                    this->submitTaskLocal(Task {
                        .path = dupPath(child),
                        .gitignore = node,
                        .pathLength = flen16,
                        .kind = Task::Kind::ScanDir
                    }, threadId);
                } else if (type == DT_REG) {
                    // perform language check
                    auto lang = detectLanguage(std::string_view(name, nlen));
                    if (lang == Language::Unknown) continue;
                    if (node && node->isIgnored(child, flen16, false)) continue;

                    // preload the file to improve cache locality
                    int fileFd = ::openat(fd, name, O_RDONLY | O_CLOEXEC | O_NOATIME);
                    if (fileFd >= 0) {
                        ::readahead(fileFd, 0, FileReader::PAGE_BYTES);
                    } else {
                        // skip if failed to open
                        continue;
                    }

                    this->submitTaskLocal(Task {
                        .path = dupPath(child),
                        .gitignore = nullptr,
                        .fileDescriptor = fileFd,
                        .pathLength = flen16,
                        .language = lang,
                        .kind = Task::Kind::ParseFile,
                    }, threadId);
                }
            }
        }

        ::close(fd);
        if (node) node->release();
    }
#elif defined(BLAM_PLATFORM_MACOS)
    void Pool::scanDir(char const* path, uint16_t len, GitignoreNode* gitignore, int threadId) {
        int fd = ::open(path, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
        if (fd < 0) {
            if (gitignore) gitignore->release();
            return;
        }

        auto* node = m_useGitignore ? GitignoreNode::makeNode(path, len, gitignore) : nullptr;

        char child[PATH_MAX];
        std::memcpy(child, path, len);
        child[len] = '/';

        attrlist alist{};
        alist.bitmapcount = ATTR_BIT_MAP_COUNT;
        alist.commonattr = ATTR_CMN_RETURNED_ATTRS | ATTR_CMN_NAME | ATTR_CMN_OBJTYPE;

        alignas(4) char buffer[64 * 1024];

        while (true) {
            int count = ::getattrlistbulk(fd, &alist, buffer, sizeof(buffer), 0);
            if (count <= 0) break; // 0 = no more entries, -1 = error

            char* cursor = buffer;
            for (int i = 0; i < count; ++i) {
                auto* hdr = reinterpret_cast<BulkAttrHeader*>(cursor);
                char* next = cursor + hdr->length;

                char const* name = reinterpret_cast<char const*>(&hdr->nameRef) + hdr->nameRef.attr_dataoffset;
                size_t maxLen = static_cast<size_t>(next - name);
                size_t nlen = ::strnlen(name, maxLen);

                cursor = next;

                if (name[0] == '.' && (nlen == 1 || (nlen == 2 && name[1] == '.'))) continue; // skip . and ..
                if (m_skipHidden && name[0] == '.') continue;                                 // skip hidden files

                int flen = len + 1 + static_cast<int>(nlen);
                if (flen >= PATH_MAX) continue;

                std::memcpy(child + len + 1, name, nlen + 1);
                uint16_t flen16 = flen;

                fsobj_type_t type = hdr->objType;

                if (type == VDIR) {
                    if (node) {
                        if (node->isIgnored(child, flen16, true)) continue;
                        node->retain();
                    }

                    this->submitTaskLocal(Task {
                        .path = dupPath(child),
                        .gitignore = node,
                        .pathLength = flen16,
                        .kind = Task::Kind::ScanDir
                    }, threadId);
                } else if (type == VREG) {
                    auto lang = detectLanguage(std::string_view(name, nlen));
                    if (lang == Language::Unknown) continue;
                    if (node && node->isIgnored(child, flen16, false)) continue;

                    // preload the file to improve cache locality
                    int fileFd = ::openat(fd, name, O_RDONLY | O_CLOEXEC);
                    if (fileFd >= 0) {
                        detail::prefetch(fileFd, 0, FileReader::PAGE_BYTES);
                    } else {
                        continue; // skip if failed to open
                    }

                    this->submitTaskLocal(Task {
                        .path = dupPath(child),
                        .gitignore = nullptr,
                        .fileDescriptor = fileFd,
                        .pathLength = flen16,
                        .language = lang,
                        .kind = Task::Kind::ParseFile,
                    }, threadId);
                }
            }
        }

        ::close(fd);
        if (node) node->release();
    }
#elif defined(BLAM_PLATFORM_WINDOWS)
    void Pool::scanDir(char const* path, uint16_t len, GitignoreNode* gitignore, int threadId) {
        auto* node = m_useGitignore ? GitignoreNode::makeNode(path, len, gitignore) : nullptr;

        char child[PATH_MAX];
        std::memcpy(child, path, len);
        child[len] = '/';

        char pattern[PATH_MAX];
        std::memcpy(pattern, path, len);
        pattern[len] = '/';
        pattern[len + 1] = '*';
        pattern[len + 2] = '\0';

        WIN32_FIND_DATAA findData;
        HANDLE h = ::FindFirstFileExA(
            pattern, FindExInfoBasic, &findData,
            FindExSearchNameMatch, nullptr, FIND_FIRST_EX_LARGE_FETCH
        );
        if (h == INVALID_HANDLE_VALUE) {
            if (node) node->release();
            return;
        }

        do {
            char const* name = findData.cFileName;
            if (name[0] == '.' && (!name[1] || (name[1] == '.' && !name[2]))) continue; // skip . and ..
            if (m_skipHidden && (name[0] == '.' || (findData.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN))) continue;

            int nlen = static_cast<int>(std::strlen(name));
            int flen = len + 1 + nlen;
            if (flen >= PATH_MAX) continue;

            std::memcpy(child + len + 1, name, nlen + 1);
            uint16_t flen16 = flen;

            // check if directory
            if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
                if (node) {
                    if (node->isIgnored(child, flen16, true)) continue;
                    node->retain();
                }

                this->submitTaskLocal(Task {
                    .path = dupPath(child),
                    .gitignore = node,
                    .pathLength = flen16,
                    .kind = Task::Kind::ScanDir
                }, threadId);
            } else {
                auto lang = detectLanguage(std::string_view(name, nlen));
                if (lang == Language::Unknown) continue;
                if (node && node->isIgnored(child, flen16, false)) continue;

                detail::Handle fileHandle = detail::open(child);
                if (fileHandle == detail::INVALID) continue;

                this->submitTaskLocal(Task {
                    .path = dupPath(child),
                    .gitignore = nullptr,
                    .fileDescriptor = fileHandle,
                    .pathLength = flen16,
                    .language = lang,
                    .kind = Task::Kind::ParseFile,
                }, threadId);
            }
        } while (::FindNextFileA(h, &findData));

        ::FindClose(h);
        if (node) node->release();
    }
#else
    #error "Unsupported platform"
#endif
}
