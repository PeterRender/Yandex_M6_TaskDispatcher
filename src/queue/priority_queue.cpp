#include "queue/priority_queue.hpp"

#include "queue/bounded_queue.hpp"
#include "queue/unbounded_queue.hpp"

#include <stdexcept>

namespace dispatcher::queue {

// Параметрический конструктор, принимающий карту конфигураций приоритетной очереди
PriorityQueue::PriorityQueue(const cfgmap &cfg_map) {
    // Проверяем, что карта конфигураций не пуста
    if (cfg_map.empty()) {
        throw std::invalid_argument("Failed to create PriorityQueue: configuration map is empty");
    }

    // Цикл по конфигурациям из карты
    for (const auto &[priority, options] : cfg_map) {
        // Создаем отображение приоритета-ключа на очередь согласно конфигурации
        // (все приоритеты-ключи уникальны по определению)
        if (options.bounded) {                    // в конфигурации указана ограниченная очередь
            if (!options.capacity.has_value()) {  // проверяем, задана ли емкость очереди
                throw std::invalid_argument("Failed to create BoundedQueue: no capacity is specified");
            }
            queues_.emplace(priority, std::make_unique<BoundedQueue>(*options.capacity));
        } else {  // в конфигурации указана неограниченная очередь
            queues_.emplace(priority, std::make_unique<UnboundedQueue>());
        }
    }
}

// Помещает задачу в очередь соответствующего приоритета
void PriorityQueue::push(TaskPriority priority, std::function<void()> task) {
    // Ищем очередь, соответствующую заданному приоритету
    // ( queues_ неизменяем после конструктора -> чтение без мьютекса безопасно)
    auto it = queues_.find(priority);
    if (it == queues_.end()) {
        throw std::runtime_error("Failed to push the task: queue for this priority is not configured");
    }

    // Захватываем мьютекс
    std::lock_guard<std::mutex> lock(mutex_);

    // Помещаем задачу в очередь (выполняется потокобезопасно внутри push)
    it->second->push(std::move(task));

    // Уведомляем один ожидающий поток о появлении задачи
    not_empty_.notify_one();
}

// Извлекает задачу с наивысшим приоритетом (сначала все High-задачи, потом Normal)
std::optional<std::function<void()>> PriorityQueue::pop() {
    // Захватываем мьютекс
    std::unique_lock<std::mutex> lock(mutex_);

    // Локальный объект, в который будет извлечена задача
    std::optional<std::function<void()>> result;

    // Ждем, пока появится задача или произойдет shutdown
    not_empty_.wait(lock, [this, &result]() {
        // Проходим по очередям в порядке приоритета (flat_map уже отсортирован от High к Normal)
        for (const auto &[_, queue] : queues_) {
            if (queue) {
                result = queue->try_pop();  // пробуем извлечь задачу (неблокирующий try_pop)
                if (result.has_value()) {
                    return true;
                }
            }
        }

        // Если задач нет, но пришел сигнал shutdown - тоже выходим из wait
        if (!active_)
            return true;

        return false;  // задач нет и мы активны — продолжаем спать
    });

    // Если мы вышли из wait, и у нас есть задача в result - отдаем её.
    // Если задачи нет и active_ == false - значит, очередь пуста и закрыта.
    if (result.has_value()) {
        // Перемещаем задачу в результат (копирование std::function может быть затратным или невозможным)
        return std::move(result);
    }

    return std::nullopt;  // сигнал воркеру завершить поток
}

// Сигнализирует о завершении работы
void PriorityQueue::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);  // захватываем мьютекс
    active_ = false;                           // опускаем флаг активности очереди
    not_empty_.notify_all();                   // пробуждаем все потоки, ждущие в pop()
}
}  // namespace dispatcher::queue