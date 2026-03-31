#pragma once

#include <functional>  // подключение стандартного функционального объекта (типа задачи)
#include <memory>      // подключение стандартных шаблонов умных указателей unique_ptr и shared_ptr

#include "queue/priority_queue.hpp"     // интерфейс класса приоритетной очереди задач
#include "thread_pool/thread_pool.hpp"  // интерфейс класса пула потоков-воркеров
#include "types.hpp"                    // перечисляемый тип приоритета задачи в системе диспетчеризации

namespace dispatcher {

// Класс диспетчера задач
// (использует приоритетную очередь для планирования задач и пул потоков для их выполнения)
class TaskDispatcher {
public:
    using pq = queue::PriorityQueue;     // псевдоним для приоритетной очереди
    using tp = thread_pool::ThreadPool;  // псевдоним для пула потоков-воркеров

    // Явный параметрический конструктор, инициализирующий диспетчер задач
    // - num_threads - число потоков (по умолчанию - поддерживающих аппаратный параллелизм)
    // - cfg_map - карта конфигураций приоритетной очереди (по умолчанию см. def_map())
    explicit TaskDispatcher(size_t num_threads = tp::hw_threads(), const pq::cfgmap &cfg_map = pq::def_map());

    // Деструктор по умолчанию
    // Важен порядок разрушения данных членов:
    // 1. Пул потоков ThreadPool (при разрушении ждет завершения всех задач)
    // 2. Общая приоритетная очередь задач PriorityQueue (при разрушении удаляются вложенные очереди)
    ~TaskDispatcher() = default;

    // Запрещаем копирование и перемещение
    TaskDispatcher(const TaskDispatcher &) = delete;
    TaskDispatcher &operator=(const TaskDispatcher &) = delete;
    TaskDispatcher(TaskDispatcher &&) = delete;
    TaskDispatcher &operator=(TaskDispatcher &&) = delete;

    // Планирует выполненение задачи с заданным приоритетом
    void schedule(TaskPriority priority, std::function<void()> task);

private:
    std::shared_ptr<pq> task_queue_;   // общая приоритетная очередь (владеет вложенными очередьми)
    std::unique_ptr<tp> thread_pool_;  // пул потоков-воркеров
};

}  // namespace dispatcher