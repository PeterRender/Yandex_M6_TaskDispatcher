#include "queue/unbounded_queue.hpp"  // интерфейс класса неограниченной очереди задач

namespace dispatcher::queue {

// Помещает задачу в очередь без блокировки
void UnboundedQueue::push(std::function<void()> task) {
    // Захватываем мьютекс и добавляем задачу
    std::lock_guard<std::mutex> lock(mutex_);
    task_queue_.push(std::move(task));
}

// Пытается извлечь задачу из очереди без блокировки
std::optional<std::function<void()>> UnboundedQueue::try_pop() {
    // Захватываем мьютекс
    std::lock_guard<std::mutex> lock(mutex_);

    // Проверяем, есть ли в очереди задачи
    if (task_queue_.empty()) {
        return std::nullopt;  // возвращаем nullopt, если очередь пуста
    }

    // Очередь не пуста -> забираем первую добавленную в нее задачу
    auto task = std::move(task_queue_.front());
    task_queue_.pop();
    return task;
}

}  // namespace dispatcher::queue