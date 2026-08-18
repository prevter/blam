#pragma once
#include "FileReader.hpp"
#include "Gitignore.hpp"
#include "LockFreeQueue.hpp"
#include "../Analyzer/LOCStats.hpp"

#include <condition_variable>
#include <thread>

namespace blam {
    constexpr size_t QueueCap = 1024;

    struct Task {
        enum class Kind : uint8_t { ScanDir, ParseFile };

        char const* path = nullptr;
        GitignoreNode* gitignore = nullptr;
        detail::Handle fileDescriptor = detail::INVALID;
        uint16_t pathLength = 0;
        Language language = Language::Unknown;
        Kind kind = Kind::ScanDir;
    };

    using FileHandler = void(*)(char const* path, detail::Handle fd, Language lang, LOCBucket& bucket, std::vector<FileStats>* perFileStats);

    class Pool {
        using Queue = ChaseLevQueue<Task, QueueCap>;

        std::vector<std::unique_ptr<Queue>> m_queues;
        std::vector<std::thread> m_threads;
        ShardedLOCStats m_stats;
        std::vector<std::vector<FileStats>> m_perFileStats;

        std::vector<Task> m_pendingTasks;
        std::mutex m_pendingTasksMutex;

        std::condition_variable m_cv, m_idleCv;
        std::mutex m_cvMutex, m_idleCvMutex;

        FileHandler m_fileHandler;

        int m_numThreads;

        std::atomic<int> m_pending{0};
        std::atomic<int> m_sleeping{0};
        std::atomic<bool> m_shutdown{false};

        bool m_skipHidden;
        bool m_useGitignore;
        bool m_collectPerFile;

    public:
        Pool(int numThreads, bool skipHidden, bool useGitignore, bool collectPerFile, FileHandler handler);
        ~Pool();

        static bool isDirectory(char const* path);

        void waitUntilIdle();
        void submitTask(std::string_view path);
        void submitTaskLocal(Task const& task, int threadId);

        void retain();
        void release();

        std::vector<FileStats> perFileStats() const;
        std::vector<std::pair<Language, LOCStats>> aggregateStats() const;

    private:
        void taskDone();
        void processTask(Task const& task, int threadId);
        std::optional<Task> popPending();
        void workerThread(int threadId);
        void scanDir(char const* path, uint16_t len, GitignoreNode* gitignore, int threadId);
    };
}
