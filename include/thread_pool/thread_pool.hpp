#pragma once

#include "queue/priority_queue.hpp"  // интерфейс класса приоритетной очереди задач

#include <memory>  // подключение стандартного шаблона разделяемого невладеющего указателя
#include <thread>  // подключение стандартного RAII-потока (std::jthread)
#include <vector>  // подключение стандартного шаблона динамического массива

namespace dispatcher::thread_pool {

// Класс пула потоков-воркеров
class ThreadPool {
public:
    using pq = queue::PriorityQueue;  // псевдоним для приоритетной очереди

    // Явный параметрический конструктор, инициализирующий пул потоков
    // Принимает общую приоритетную очередь задач и число потоков (по умолчанию - поддерживающих аппаратный параллелизм)
    explicit ThreadPool(std::shared_ptr<pq> task_queue, size_t num_threads = hw_threads());

    // Деструктор (сигнализирует о завершении работы и дожидается выполнения всех потоков)
    ~ThreadPool();

    // Запрещаем копирование и перемещение
    ThreadPool(const ThreadPool &) = delete;
    ThreadPool &operator=(const ThreadPool &) = delete;
    ThreadPool(ThreadPool &&) = delete;
    ThreadPool &operator=(ThreadPool &&) = delete;

    // Возвращает количество аппаратных потоков (вычисляется один раз при первом вызове)
    static size_t hw_threads() {
        static const size_t count = std::thread::hardware_concurrency();
        return count;  // может вернуть 0, если инфа не доступна)
    }

private:
    // Рабочий метод потока-воркера (постоянно извлекает задачи из очереди и выполняет их)
    void worker();

    static constexpr size_t MAX_THREADS = 256;  // max допустимое количество потоков
    std::shared_ptr<pq> task_queue_;            // общая приоритетная очередь задач
    std::vector<std::jthread> workers_;         // массив потоков-воркеров
};

}  // namespace dispatcher::thread_pool
