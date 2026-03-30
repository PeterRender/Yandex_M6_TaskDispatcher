#pragma once

#include "queue/priority_queue.hpp"  // приоритетная очередь задач

#include <atomic>  // подключение стандартного шаблона атомарной переменной
#include <memory>  // подключение стандартного шаблона разделяемого невладеющего указателя
#include <thread>  // подключение стандартного RAII-потока (std::jthread)
#include <vector>  // подключение стандартного шаблона динамического массива

namespace dispatcher::thread_pool {

// Класс пула потоков-воркеров
class ThreadPool {
public:
    // Явный параметрический конструктор, инициализирующий пул потоков
    // Принимает общую очередь задач и число потоков (по умолчанию - число потоков с аппаратным параллелизмом)
    explicit ThreadPool(std::shared_ptr<queue::PriorityQueue> task_queue,
                        size_t num_threads = std::thread::hardware_concurrency());

    // Деструктор (сигнализирует о завершении работы и дожидается выполнения всех потоков)
    ~ThreadPool();

    // Запрещаем копирование и перемещение
    ThreadPool(const ThreadPool &) = delete;
    ThreadPool &operator=(const ThreadPool &) = delete;
    ThreadPool(ThreadPool &&) = delete;
    ThreadPool &operator=(ThreadPool &&) = delete;

private:
    // Рабочий метод потока-воркера (постоянно извлекает задачи из очереди и выполняет их)
    void worker();

    std::shared_ptr<queue::PriorityQueue> task_queue_;  // общая очередь задач с приоритетами
    std::vector<std::jthread> workers_;                 // массив потоков-воркеров
    std::atomic<bool> stop_{false};                     // атомарный флаг прекращения работы
};

}  // namespace dispatcher::thread_pool
