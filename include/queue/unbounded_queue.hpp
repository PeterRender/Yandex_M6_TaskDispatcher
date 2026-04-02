#pragma once
#include "queue/queue.hpp"

#include <functional>
#include <mutex>
#include <queue>
#include <semaphore>

namespace dispatcher::queue {

// Класс неограниченной очереди задач (простой мьютекс)
class UnboundedQueue : public IQueue {
public:
    // Конструктор по умолчанию
    UnboundedQueue() : busy_slots_(0) {}  // исходно занятых слотов нет

    // Виртуальный деструктор по умолчанию
    ~UnboundedQueue() override = default;

    // === Методы, реализующие интерфейс базового абстрактного класса ===

    // Помещает задачу в очередь без блокировки
    void push(std::function<void()> task) override;

    // Пытается извлечь задачу из очереди без блокировки
    std::optional<std::function<void()>> try_pop() override;

private:
    std::counting_semaphore<> busy_slots_;          // семафор занятых слотов очереди
    std::queue<std::function<void()>> task_queue_;  // FIFO-хранилище задач
    std::mutex mutex_;                              // мьютекс для защиты FIFO-хранилища
};

}  // namespace dispatcher::queue