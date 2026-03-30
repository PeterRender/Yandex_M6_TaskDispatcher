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

// Класс очереди с приоритетами (сначала все High-задачи, потом Normal)
class PriorityQueue {
public:
    // Явный параметрический конструктор, создающий очереди согласно карте конфигураций
    explicit PriorityQueue(const std::map<TaskPriority, QueueOptions> &cfg_map);

    // Деструктор по умолчанию
    ~PriorityQueue() = default;

    // Помещает задачу в очередь соответствующего приоритета
    void push(TaskPriority priority, std::function<void()> task);

    // Извлекает задачу с наивысшим приоритетом (сначала все High-задачи, потом Normal)
    // Блокируется, если нет задач. При shutdown() начинает возвращать std::nullopt
    std::optional<std::function<void()>> pop();

    // Сигнализирует о завершении работы
    void shutdown();

private:
    std::array<std::unique_ptr<IQueue>, 2> queues_;  // отображение приоритета на очередь ([0] - High, [1] - Normal)
    bool active_{true};                              // флаг активности очереди
    std::condition_variable not_empty_;              // условная переменнная для блокировки pop() при отсутствии задач
    std::mutex mutex_;                               // мьютекс для защиты условной переменной и флага active_
};

}  // namespace dispatcher::queue