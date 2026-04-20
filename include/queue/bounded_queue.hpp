#pragma once
#include "queue/queue.hpp"

#include <format>
#include <functional>
#include <mutex>
#include <queue>
#include <semaphore>

namespace dispatcher::queue {

// Класс ограниченной очереди задач (семафоры + мьютекс)
class BoundedQueue : public IQueue {
public:
    // Явный параметрический конструктор, принимающий max размер очереди
    explicit BoundedQueue(size_t capacity)
        : free_slots_(capacity),  // исходно число свободных слотов = max размеру очереди
          busy_slots_(0)          // исходно занятых слотов нет
    {
        // Проверям, что задана ненулевая емкость очереди
        if (capacity == 0) {
            throw std::invalid_argument(
                std::format("Failed to create BoundedQueue: capacity must be positive, got {}", capacity));
        }
    }

    // Виртуальный деструктор по умолчанию
    ~BoundedQueue() override = default;

    // === Методы, реализующие интерфейс базового абстрактного класса ===

    // Помещает задачу в очередь (блокируется при заполнении)
    void push(std::function<void()> task) override;

    // Пытается извлечь задачу из очереди без блокировки
    std::optional<std::function<void()>> try_pop() override;

private:
    std::counting_semaphore<> free_slots_;          // семафор свободных слотов очереди
    std::counting_semaphore<> busy_slots_;          // семафор занятых слотов очереди
    std::queue<std::function<void()>> task_queue_;  // FIFO-хранилище задач
    std::mutex mutex_;                              // мьютекс для защиты FIFO-хранилища
};

}  // namespace dispatcher::queue