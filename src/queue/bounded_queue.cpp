#include "queue/bounded_queue.hpp"  // интерфейс класса ограниченной очереди задач

namespace dispatcher::queue {

// Помещает задачу в очередь (блокируется при заполнении)
void BoundedQueue::push(std::function<void()> task) {
    // Ждем свободный слот (блокируется, если очередь заполнена)
    free_slots_.acquire();

    // Захватываем мьютекс и добавляем задачу
    {
        std::lock_guard<std::mutex> lock(mutex_);
        task_queue_.push(std::move(task));
    }

    // Увеличиваем счетчик занятых слотов (задач) -> будим ждущий try_pop
    busy_slots_.release();
}

// Пытается извлечь задачу из очереди без блокировки
std::optional<std::function<void()>> BoundedQueue::try_pop() {
    // Пытаемся захватить занятый слот (задачу) без блокировки
    if (!busy_slots_.try_acquire()) {
        return std::nullopt;  // возвращаем nullopt, если очередь пуста
    }

    // Захватываем мьютекс и забираем первую добавленную в очередь задачу
    std::lock_guard<std::mutex> lock(mutex_);
    auto task = std::move(task_queue_.front());
    task_queue_.pop();

    // Освобождаем слот -> будим ждущий push
    free_slots_.release();

    return task;
}

}  // namespace dispatcher::queue