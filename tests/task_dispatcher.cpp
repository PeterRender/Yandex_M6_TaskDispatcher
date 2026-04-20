#include <gtest/gtest.h>

#include <atomic>
#include <thread>
#include <vector>

#include "task_dispatcher.hpp"

using namespace dispatcher;

// Тест создания диспетчера с конфигурацией по умолчанию
TEST(TaskDispatcherTest, CreateWithDefaultConfig) { EXPECT_NO_THROW(TaskDispatcher td); }

// Тест создания диспетчера с пользовательским количеством потоков
TEST(TaskDispatcherTest, CreateWithCustomThreadCount) {
    EXPECT_NO_THROW(TaskDispatcher td(1));
    EXPECT_NO_THROW(TaskDispatcher td(4));
    EXPECT_NO_THROW(TaskDispatcher td(8));
}

// Тест выполнения одной High-задачи
TEST(TaskDispatcherTest, ScheduleSingleHighTask) {
    std::atomic<bool> executed{false};

    {
        TaskDispatcher td(1);  // в пуле 1 поток-воркер
        td.schedule(TaskPriority::High, [&executed]() { executed = true; });
    }  // деструктор TaskDispatcher дожидается выполнения задачи

    EXPECT_TRUE(executed);
}

// Тест выполнения многих Normal-задач
TEST(TaskDispatcherTest, ScheduleManyNormalTasks) {
    const int TASK_COUNT = 100;
    std::atomic<int> executed{0};

    {
        TaskDispatcher td(2);  // в пуле 2 потока-воркера
        for (int i = 0; i < TASK_COUNT; ++i) {
            td.schedule(TaskPriority::Normal, [&executed]() { executed++; });
        }
    }  // деструктор TaskDispatcher дожидается выполнения всех задач

    EXPECT_EQ(executed, TASK_COUNT);
}

// Тест выполнения задач с разными приоритетами (порядок выполнения задач недетерминированный из-за планировщика ОС)
TEST(TaskDispatcherTest, ScheduleMixedTasks) {
    std::atomic<int> high_cnt{0};
    std::atomic<int> normal_cnt{0};
    const int TASKS_PER_PRIORITY = 50;

    {
        TaskDispatcher td(2);  // в пуле 2 потока-воркера
        for (int i = 0; i < TASKS_PER_PRIORITY; ++i) {
            td.schedule(TaskPriority::High, [&high_cnt]() { high_cnt++; });
            td.schedule(TaskPriority::Normal, [&normal_cnt]() { normal_cnt++; });
        }
    }  // деструктор TaskDispatcher дожидается выполнения всех задач

    EXPECT_EQ(high_cnt, TASKS_PER_PRIORITY);
    EXPECT_EQ(normal_cnt, TASKS_PER_PRIORITY);
}

// Тест отправки задач из нескольких потоков (проверка потокобезопасности schedule())
TEST(TaskDispatcherTest, ScheduleFromMultipleThreads) {
    std::atomic<int> executed{0};
    const int TASKS_PER_THREAD = 100;
    const int NUM_SENDER_THREADS = 4;
    const int NUM_WORKER_THREADS = 8;

    {
        TaskDispatcher td(NUM_WORKER_THREADS);  // в пуле NUM_WORKER_THREADS потоков-воркеров
        std::vector<std::jthread> sender_threads;

        // Запускаем NUM_SENDER_THREADS потоков-отправителей
        for (int i = 0; i < NUM_SENDER_THREADS; ++i) {
            sender_threads.emplace_back([&td, &executed, TASKS_PER_THREAD]() {
                for (int j = 0; j < TASKS_PER_THREAD; ++j) {
                    // Вызываем schedule() из разных потоков-отправителей
                    td.schedule(TaskPriority::Normal, [&executed]() { executed++; });
                }
            });
        }

        sender_threads.clear();  // дожидаемся завершения всех потоков-отправителей
    }  // деструктор TaskDispatcher дожидается выполнения всех потоков-воркеров

    EXPECT_EQ(executed, NUM_SENDER_THREADS * TASKS_PER_THREAD);
}

// Тест устойчивости работы потоков-воркеров к исключениям в задачах
TEST(TaskDispatcherTest, ExceptionStability) {
    std::atomic<int> executed{0};
    const int CORRECT_HIGH_TASKS = 5;
    const int CORRECT_NORM_TASKS = 10;

    {
        TaskDispatcher td(2);  // в пуле 2 потока-воркера

        // Ошибочная High-задача, выбрасывающая исключение
        td.schedule(TaskPriority::High, []() { throw std::runtime_error("Task exception"); });

        // Добавляем несколько корректных High-задач после ошибочной High-задачи
        for (int i = 0; i < CORRECT_HIGH_TASKS; ++i) {
            td.schedule(TaskPriority::High, [&executed]() { executed++; });
        }

        // Добавляем еще несколько корректных Normal-задач
        for (int i = 0; i < CORRECT_NORM_TASKS; ++i) {
            td.schedule(TaskPriority::Normal, [&executed]() { executed++; });
        }
    }  // деструктор TaskDispatcher дожидается выполнения всех потоков-воркеров

    // Все корректные задачи должны выполниться
    EXPECT_EQ(executed, CORRECT_HIGH_TASKS + CORRECT_NORM_TASKS);
}

// Тест параллельного добавления задач с разными приоритетами из множества потоков
TEST(TaskDispatcherTest, ConcurrentMixedPriorities) {
    const int THREADS_PER_PRIORITY = 4;
    const int TASKS_PER_THREAD = 250;
    std::atomic<int> high_cnt{0};
    std::atomic<int> norm_cnt{0};

    {
        TaskDispatcher td(4);  // в пуле 4 потока-воркера
        std::vector<std::jthread> threads;

        // Потоки, добавляющие High-задачи
        for (int i = 0; i < THREADS_PER_PRIORITY; ++i) {
            threads.emplace_back([&td, &high_cnt, TASKS_PER_THREAD]() {
                for (int j = 0; j < TASKS_PER_THREAD; ++j) {
                    td.schedule(TaskPriority::High, [&high_cnt]() { high_cnt++; });
                }
            });
        }

        // Потоки, добавляющие Normal-задачи
        for (int i = 0; i < THREADS_PER_PRIORITY; ++i) {
            threads.emplace_back([&td, &norm_cnt, TASKS_PER_THREAD]() {
                for (int j = 0; j < TASKS_PER_THREAD; ++j) {
                    td.schedule(TaskPriority::Normal, [&norm_cnt]() { norm_cnt++; });
                }
            });
        }

        threads.clear();  // дожидаемся добавления всех задач
    }  // деструктор TaskDispatcher дожидается выполнения всех потоков-воркеров

    // Проверяем, что все задачи выполнены
    EXPECT_EQ(high_cnt, THREADS_PER_PRIORITY * TASKS_PER_THREAD);
    EXPECT_EQ(norm_cnt, THREADS_PER_PRIORITY * TASKS_PER_THREAD);
}