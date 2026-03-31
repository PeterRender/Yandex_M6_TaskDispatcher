#include "task_dispatcher.hpp"  // интерфейс класса диспетчера задач

namespace dispatcher {

// Параметрический конструктор, инициализирующий диспетчер задач
TaskDispatcher::TaskDispatcher(size_t num_threads, const pq::cfgmap &cfg_map) {
    // Пытаемся создать компоненты диспетчера задач
    try {
        // Создаем общую приоритетную очередь с заданной конфигурацией
        task_queue_ = std::make_shared<pq>(cfg_map);

        // Создаем пул потоков-воркеров, забирающих задачи из общей приоритетной очереди
        thread_pool_ = std::make_unique<tp>(task_queue_, num_threads);
    }
    // Обрабатываем штатные исключения
    catch (const std::exception &e) {
        throw std::runtime_error(std::format("Failed to create TaskDispatcher: {}", e.what()));
    }
    // Обрабатываем все остальные исключения
    catch (...) {
        throw std::runtime_error("Failed to create TaskDispatcher: unknown error");
    }
}

// Планирует выполненение задачи с заданным приоритетом
void TaskDispatcher::schedule(TaskPriority priority, std::function<void()> task) {
    task_queue_->push(priority, std::move(task));  // добавляем задачу в общую приоритетную очередь
}

}  // namespace dispatcher