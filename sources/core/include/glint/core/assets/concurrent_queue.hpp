#pragma once

#include <mutex>
#include <optional>
#include <queue>

namespace glint::core {

template<typename T>
class ConcurrentQueue {
  public:
    auto push(T&& value) -> void;
    auto pop() -> std::optional<T>;

  private:
    std::queue<T> m_queue {};
    std::mutex m_mutex {};
};

} // namespace glint::core

// IMPLEMENTATION

namespace glint::core {

template<typename T>
inline auto ConcurrentQueue<T>::push(T&& value) -> void {
    auto lock = std::lock_guard(m_mutex);
    m_queue.push(std::move(value));
}

template<typename T>
inline auto ConcurrentQueue<T>::pop() -> std::optional<T> {
    auto lock = std::lock_guard(m_mutex);
    if (m_queue.empty()) return std::nullopt;

    auto value = m_queue.front();
    m_queue.pop();
    return value;
}

} // namespace glint::core
