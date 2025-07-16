#pragma once
#include <atomic>
#include <vector>
#include <optional>
#include <cstddef>

template<typename T, size_t Capacity>
class NetworkQueue
{
public:
    NetworkQueue() : m_head(0), m_tail(0) {}

    // Producer thread: returns false if full
    bool push(const T& item)
    {
        size_t head = m_head.load(std::memory_order_relaxed);
        size_t next_head = increment(head);
        if (next_head == m_tail.load(std::memory_order_acquire))
        {
            _ASSERT(true && "NetworkQueue is full, cannot push item");
            return false; // queue full
        }
        m_buffer[head] = item;
        m_head.store(next_head, std::memory_order_release);
        return true;
    }

    // Consumer thread: returns std::nullopt if empty
    std::optional<T> pop()
    {
        size_t tail = m_tail.load(std::memory_order_relaxed);
        if (tail == m_head.load(std::memory_order_acquire))
        {
            return std::nullopt; // queue empty
        }
        T item = m_buffer[tail];
        m_tail.store(increment(tail), std::memory_order_release);
        return item;
    }

    bool empty() const
    {
        return m_head.load(std::memory_order_acquire) == m_tail.load(std::memory_order_acquire);
    }

    bool full() const
    {
        return increment(m_head.load(std::memory_order_acquire)) == m_tail.load(std::memory_order_acquire);
    }

private:
    size_t increment(size_t idx) const { return (idx + 1) % Capacity; }

    T m_buffer[Capacity];
    std::atomic<size_t> m_head;
    std::atomic<size_t> m_tail;
};
