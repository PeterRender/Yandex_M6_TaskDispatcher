#pragma once
#include "queue/queue.hpp"  // интерфейсный класс очереди задач
#include "types.hpp"        // перечисляемый тип приоритета задачи в системе диспетчеризации

#include <array>               // подключение стандартного шаблона статического массива
#include <condition_variable>  // подключение стандартной условной переменной (примитива синхронизации)
#include <functional>          // подключение стандартного функционального объекта (типа задачи)
#include <map>                 // подключение стандартного контейнера упорядоченного отображения
#include <memory>              // подключение стандартного шаблона умного владеющего указателя
#include <mutex>               // подключение стандартного мьютекса
#include <optional>            // подключение словарного типа опционального результата

namespace dispatcher::queue {

// Класс приоритетной очереди задач (сначала все High-задачи, потом Normal)
class PriorityQueue {
public:
    // Псевдоним для карты конфигураций приоритетной очереди
    using cfgmap = std::map<TaskPriority, queue::QueueOptions>;

    // Явный параметрический конструктор, принимающий карту конфигураций приоритетной очереди
    explicit PriorityQueue(const cfgmap &cfg_map = def_map());

    // Деструктор по умолчанию
    ~PriorityQueue() = default;

    // Помещает задачу в очередь соответствующего приоритета
    void push(TaskPriority priority, std::function<void()> task);

    // Извлекает задачу с наивысшим приоритетом (сначала все High-задачи, потом Normal)
    // Блокируется, если нет задач. При shutdown() начинает возвращать std::nullopt
    std::optional<std::function<void()>> pop();

    // Сигнализирует о завершении работы
    void shutdown();

    // Создает карту конфигурации приоритетной очереди по умолчанию (один раз при первом вызове):
    // для High-задач - ограниченная очередь на 1000 элементов,
    // для Normal-задач - неограниченная очередь
    static const cfgmap &def_map() {
        static const cfgmap cfg_map = {{TaskPriority::High, {.bounded = true, .capacity = 1000}},
                                       {TaskPriority::Normal, {.bounded = false, .capacity = std::nullopt}}};
        return cfg_map;
    }

private:
    std::array<std::unique_ptr<IQueue>, 2> queues_;  // отображение приоритета на очередь ([0] - High, [1] - Normal)
    bool active_{true};                              // флаг активности очереди
    std::condition_variable not_empty_;              // условная переменнная для блокировки pop() при отсутствии задач
    std::mutex mutex_;                               // мьютекс для защиты условной переменной и флага active_
};

}  // namespace dispatcher::queue