#pragma once

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>

namespace blam::test {
    namespace fs = std::filesystem;

    class TempDir {
    public:
        TempDir() {
            auto base = fs::temp_directory_path();
            for (int attempt = 0; attempt < 100; ++attempt) {
                auto candidate = base / ("blam_test_" + std::to_string(
                    std::hash<std::thread::id>{}(std::this_thread::get_id()) ^ (0x9E3779B9u + attempt)
                ));
                std::error_code ec;
                if (fs::create_directory(candidate, ec)) {
                    m_path = candidate;
                    return;
                }
            }
            throw std::runtime_error("TempDir: failed to create a unique temp directory");
        }

        ~TempDir() {
            std::error_code ec;
            fs::remove_all(m_path, ec);
        }

        TempDir(TempDir const&) = delete;
        TempDir& operator=(TempDir const&) = delete;

        [[nodiscard]] std::string path() const { return m_path.string(); }
        [[nodiscard]] size_t size() const { return m_path.native().size(); }

        std::string writeFile(std::string_view relative, std::string_view contents = "") {
            auto full = m_path / relative;
            std::error_code ec;
            fs::create_directories(full.parent_path(), ec);

            std::ofstream f(full, std::ios::binary);
            f << contents;
            f.close();

            return full.string();
        }

        std::string makeDir(std::string_view relative) {
            auto full = m_path / relative;
            std::error_code ec;
            fs::create_directories(full, ec);
            return full.string();
        }

    private:
        fs::path m_path;
    };
}