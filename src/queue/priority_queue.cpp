#include "queue/priority_queue.hpp"  // интерфейс класса очереди с приоритетами

#include "queue/bounded_queue.hpp"    // интерфейс класса ограниченной очереди задач (для High-приоритета)
#include "queue/unbounded_queue.hpp"  // интерфейс класса неограниченной очереди задач (для Normal-приоритета)

#include <stdexcept>  // подключение стандартных объектов для обработки исключений

namespace dispatcher::queue {

// Параметрический конструктор, принимающий карту конфигураций приоритетной очереди
PriorityQueue::PriorityQueue(const cfgmap &cfg_map) {
    // Проверяем, что карта конфигураций не пуста
    if (cfg_map.empty()) {
        throw std::invalid_argument("Failed to create PriorityQueue: configuration map is empty");
    }

    // Цикл по конфигурациям из карты
    for (const auto &[priority, options] : cfg_map) {
        // Преобразуем тип приоритета в индекс
        size_t idx = static_cast<size_t>(priority);

        // Проверяем выход за границы массива
        if (idx >= queues_.size()) {
            throw std::runtime_error("Failed to create PriorityQueue: invalid priority value");
        }

        // Если в конфигурации указана ограниченная очередь
        if (options.bounded) {
            // Проверяем, задан ли max размер (емкость) очереди
            if (!options.capacity.has_value()) {
                throw std::invalid_argument("Failed to create bounded queue: no capacity is specified");
            }
            queues_[idx] = std::make_unique<BoundedQueue>(*options.capacity);
        }
        // В конфигурации указана неограниченная очередь
        else {
            queues_[idx] = std::make_unique<UnboundedQueue>();
        }
    }
}

// Помещает задачу в очередь соответствующего приоритета
void PriorityQueue::push(TaskPriority priority, std::function<void()> task) {
    // Преобразуем тип приоритета в индекс
    size_t idx = static_cast<size_t>(priority);  // локальная переменная (существует только в стеке текущего потока)

    // Проверяем выход за границы массива (защита от неожиданных значений)
    // Массив queues_ неизменяем после конструктора, поэтому чтение без мьютекса безопасно
    if (idx >= queues_.size()) {
        throw std::runtime_error("Failed to push the task: invalid priority value");
    }

    // Проверяем, что очередь для этого приоритета сконфигурирована
    if (!queues_[idx]) {
        throw std::runtime_error("Failed to push the task: queue for this priority is not configured");
    }

    // Захватываем мьютекс
    std::lock_guard<std::mutex> lock(mutex_);

    // Помещаем задачу в очередь (выполняется потокобезопасно внутри push)
    queues_[idx]->push(std::move(task));

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
        // Сначала всегда проверяем, нет ли чего-нибудь в очередях
        for (auto &queue : queues_) {
            // Проверяем, что очередь существует
            if (queue) {
                // Пробуем извлечь задачу (неблокирующий try_pop)
                result = queue->try_pop();  // перемещаем задачу в локальный объект result
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