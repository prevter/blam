#pragma once

#include <atomic>
#include <new>
#include <optional>
#include <type_traits>

#if defined(__x86_64__)
#include <immintrin.h>
#define SPIN_PAUSE() _mm_pause()
#elif defined(__aarch64__)
#define SPIN_PAUSE() asm volatile("yield")
#else
#define SPIN_PAUSE() __asm__ volatile("" ::: "memory")
#endif

namespace wtf {
    template <class T, size_t Cap>
    class ChaseLevQueue {
        static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
        static_assert(Cap > 0, "Capacity must be greater than 0");
        static_assert((Cap & (Cap - 1)) == 0, "Capacity must be a power of 2");

        static constexpr size_t Capacity = Cap;
        static constexpr size_t Mask = Capacity - 1;
        static constexpr size_t Align = std::hardware_destructive_interference_size;

        alignas(Align) std::atomic<int64_t> m_top{0};
        alignas(Align) std::atomic<int64_t> m_bottom{0};
        alignas(Align) T m_buffer[Capacity];

    public:
        [[nodiscard]] bool push(T val) noexcept {
            int64_t b = m_bottom.load(std::memory_order_relaxed);
            int64_t t = m_top.load(std::memory_order_acquire);
            if (b - t >= static_cast<int64_t>(Capacity)) return false;

            m_buffer[b & Mask] = std::move(val);

            std::atomic_thread_fence(std::memory_order_release);
            m_bottom.store(b + 1, std::memory_order_relaxed);
            return true;
        }

        std::optional<T> pop() noexcept {
            int64_t b = m_bottom.load(std::memory_order_relaxed) - 1;
            m_bottom.store(b, std::memory_order_relaxed);

            std::atomic_thread_fence(std::memory_order_seq_cst);
            int64_t t = m_top.load(std::memory_order_relaxed);

            if (t > b) {
                m_bottom.store(b + 1, std::memory_order_relaxed);
                return std::nullopt;
            }

            T val = m_buffer[b & Mask];

            if (t == b) {
                bool won = m_top.compare_exchange_strong(t, t + 1, std::memory_order_seq_cst, std::memory_order_relaxed);
                m_bottom.store(b + 1, std::memory_order_relaxed);
                if (!won) return std::nullopt;
            }

            return val;
        }

        std::optional<T> steal() noexcept {
            int64_t t = m_top.load(std::memory_order_acquire);
            std::atomic_thread_fence(std::memory_order_seq_cst);
            int64_t b = m_bottom.load(std::memory_order_acquire);
            if (t >= b) return std::nullopt;

            T val = m_buffer[t & Mask];

            if (!m_top.compare_exchange_strong(t, t + 1, std::memory_order_seq_cst, std::memory_order_relaxed)) {
                return std::nullopt;
            }

            return val;
        }

        size_t size() const noexcept {
            int64_t b = m_bottom.load(std::memory_order_relaxed);
            int64_t t = m_top.load(std::memory_order_relaxed);
            return b > t ? b - t : 0;
        }
    };
}