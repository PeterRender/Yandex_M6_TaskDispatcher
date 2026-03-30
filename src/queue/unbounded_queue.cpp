#include "queue/unbounded_queue.hpp"  // интерфейс класса неограниченной очереди задач

namespace dispatcher::queue {

// Помещает задачу в очередь без блокировки
void UnboundedQueue::push(std::function<void()> task) {
    {
        // Захватываем мьютекс и добавляем задачу
        std::lock_guard<std::mutex> lock(mutex_);
        task_queue_.push(std::move(task));
    }

    // Увеличиваем счетчик занятых слотов (задач) -> будим ждущий try_pop
    busy_slots_.release();
}

// Пытается извлечь задачу из очереди без блокировки
std::optional<std::function<void()>> UnboundedQueue::try_pop() {
    // Пытаемся захватить занятый слот (задачу) без блокировки
    // (быстрая проверка наличия задач без захвата мьютекса)
    if (!busy_slots_.try_acquire()) {
        return std::nullopt;  // возвращаем nullopt, если очередь пуста
    }

    // Захватываем мьютекс и забираем первую добавленную в очередь задачу
    std::lock_guard<std::mutex> lock(mutex_);
    auto task = std::move(task_queue_.front());
    task_queue_.pop();

    // Перемещаем std::function в результат (копирование может быть затратным или невозможным)
    return std::move(task);
}

}  // namespace dispatcher::queue