#include "thread_pool/thread_pool.hpp"  // интерфейс класса пула потоков-воркеров

#include "logger.hpp"  // интерфейс класса-синглтона для потокобезопасного логирования

#include <format>     // подключение стандартного шаблона форматированного вывода
#include <stdexcept>  // подключение стандартных объектов обработки исключений

namespace dispatcher::thread_pool {

// Параметрический конструктор, инициализирующий пул потоков
ThreadPool::ThreadPool(std::shared_ptr<pq> task_queue, size_t num_threads) : task_queue_(std::move(task_queue)) {
    // Проверяем корректность количества потоков
    if (num_threads < 1 || num_threads > MAX_THREADS) {
        throw std::invalid_argument(
            std::format("Failed to create ThreadPool: num_threads={} out of range [1, {}]", num_threads, MAX_THREADS));
    }

    // Резервируем память под массив потоков-воркеров
    workers_.reserve(num_threads);

    // Заполняем массив потоков-воркеров
    for (size_t i = 0; i < num_threads; ++i) {
        workers_.emplace_back(&ThreadPool::worker, this);  // запускаем метод worker в отдельном потоке
    }
}

// Деструктор (сигнализирует о завершении работы и дожидается выполнения всех потоков)
ThreadPool::~ThreadPool() {
    // Сигнализируем очереди о завершении работы (будим все заблокированные pop())
    if (task_queue_) {
        task_queue_->shutdown();
    }
    // Примечание: при разрушении массива потоков-воркеров объекты std::jthread
    // автоматически дожидаются завершения работы своих методов worker
}

// Рабочий метод потока-воркера (постоянно извлекает задачи из очереди и выполняет их)
void ThreadPool::worker() {
    // Цикл крутится, пока pop выдает задачи
    // Как только очереди опустеют после shutdown, то pop вернет nullopt
    while (auto task = task_queue_->pop()) {
        // Задача есть - пытаемся выполнить ее
        try {
            (*task)();
        } catch (const std::exception &e) {
            // Логируем исключение стандартной библиотеки
            Logger::Get().Log(std::string("Task failed with exception: ") + e.what());
        } catch (...) {
            // Логируем неизвестное исключение
            Logger::Get().Log("Task failed with unknown exception");
        }
    }
}

}  // namespace dispatcher::thread_pool