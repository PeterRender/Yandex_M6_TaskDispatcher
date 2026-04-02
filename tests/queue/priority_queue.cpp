#include <gtest/gtest.h>

#include <atomic>
#include <thread>
#include <vector>

#include "queue/priority_queue.hpp"
#include "types.hpp"

using namespace dispatcher;
using namespace dispatcher::queue;

// Псевдоним для карты конфигураций приоритетной очереди
using cfgmap = std::map<TaskPriority, queue::QueueOptions>;

// === Однопоточные тесты PriorityQueue ===

// Тест создания очереди с конфигурацией по умолчанию
TEST(PriorityQueueTest, CreateWithDefaultCfg) { EXPECT_NO_THROW(PriorityQueue queue); }

// Тест создания очереди с пользовательской конфигурацией
TEST(PriorityQueueTest, CreateWithCustomCfg) {
    cfgmap cfg = {
        {TaskPriority::High, {true, 100}},             // High - ограниченная на 100 элементов
        {TaskPriority::Normal, {false, std::nullopt}}  // Normal - неограниченная
    };
    EXPECT_NO_THROW(PriorityQueue queue(cfg));
}

// Тест push и pop для High-задачи
TEST(PriorityQueueTest, PushAndPopHighPriority) {
    PriorityQueue queue;

    bool executed = false;
    queue.push(TaskPriority::High, [&executed]() { executed = true; });

    auto task = queue.pop();
    ASSERT_TRUE(task.has_value());
    (*task)();
    EXPECT_TRUE(executed);
}

// Тест push и pop для Normal-задачи
TEST(PriorityQueueTest, PushAndPopNormalPriority) {
    PriorityQueue queue;

    bool executed = false;
    queue.push(TaskPriority::Normal, [&executed]() { executed = true; });

    auto task = queue.pop();
    ASSERT_TRUE(task.has_value());
    (*task)();
    EXPECT_TRUE(executed);
}

// Тест выполнения High-задач перед Normal-задачами
TEST(PriorityQueueTest, HighPriorityFirst) {
    PriorityQueue queue;
    std::vector<std::string> execution_order;
    const int TOTAL_TASKS = 4;

    // Добавляем TOTAL_TASKS задач в разном порядке
    queue.push(TaskPriority::Normal, [&execution_order]() { execution_order.push_back("norm1"); });
    queue.push(TaskPriority::High, [&execution_order]() { execution_order.push_back("high1"); });
    queue.push(TaskPriority::Normal, [&execution_order]() { execution_order.push_back("norm2"); });
    queue.push(TaskPriority::High, [&execution_order]() { execution_order.push_back("high2"); });

    // Извлекаем и выполняем все задачи
    for (int i = 0; i < TOTAL_TASKS; ++i) {
        auto task = queue.pop();
        ASSERT_TRUE(task.has_value()) << "Failed to pop task " << i;
        (*task)();
    }

    // Ожидаемый порядок: все High, затем все Normal
    std::vector<std::string> expected_order = {"high1", "high2", "norm1", "norm2"};
    EXPECT_EQ(execution_order, expected_order);
}

// === Многопоточные тесты PriorityQueue ===

// Тест блокировки pop при пустой очереди
TEST(PriorityQueueTest, PopBlocksWhenEmpty) {
    const int DELAY_MS = 100;  // время на выполнение/снятие блокировки

    PriorityQueue queue;
    std::atomic<bool> pop_done{false};
    std::atomic<bool> pop_started{false};

    // Запускаем поток, обрабатывающий очередь
    std::jthread worker([&queue, &pop_done, &pop_started]() {
        pop_started = true;
        auto task = queue.pop();  // должен заблокироваться
        pop_done = true;
    });

    // Ждем, пока поток начнет ожидание
    while (!pop_started) {
        std::this_thread::yield();
    }

    // Даем время на блокировку и проверяем, что pop заблокирован
    std::this_thread::sleep_for(std::chrono::milliseconds(DELAY_MS));
    EXPECT_FALSE(pop_done);

    // Добавляем задачу - разблокируем pop
    queue.push(TaskPriority::High, []() { return; });

    // Даем время на разблокировку и проверяем, что pop выполнен
    std::this_thread::sleep_for(std::chrono::milliseconds(DELAY_MS));
    EXPECT_TRUE(pop_done);
}

// Тест корректного shutdown пустой очереди (разблокировка pop и возвращение nullopt)
TEST(PriorityQueueTest, ShutdownEmptyQueue) {
    const int DELAY_MS = 100;  // время на выполнение/снятие блокировки

    PriorityQueue queue;
    std::atomic<bool> pop_done{false};
    std::atomic<bool> got_nullopt{false};

    // Запускаем поток, обрабатывающий очередь
    std::jthread worker([&queue, &pop_done, &got_nullopt]() {
        auto task = queue.pop();
        pop_done = true;
        if (!task.has_value()) {
            got_nullopt = true;
        }
    });

    // Даем время на блокировку и проверяем, что pop заблокирован
    std::this_thread::sleep_for(std::chrono::milliseconds(DELAY_MS));
    EXPECT_FALSE(pop_done);

    // Вызываем shutdown
    queue.shutdown();

    // Даем время на разблокировку и проверяем, что pop выполнен и получен nullopt
    std::this_thread::sleep_for(std::chrono::milliseconds(DELAY_MS));
    EXPECT_TRUE(pop_done);
    EXPECT_TRUE(got_nullopt);
}

// Тест корректного shutdown заполненной очереди (отсутствие потерь уже добавленных задач)
TEST(PriorityQueueTest, ShutdownFilledQueue) {
    const int TASKS_OF_ONE_PRIORITY = 50;
    const int ALL_TASKS = TASKS_OF_ONE_PRIORITY * 2;
    const int NUM_THREADS = 10;
    const int DELAY_MS = 100;  // время на выполнение/снятие блокировки

    PriorityQueue queue;
    std::atomic<int> high_executed{0};
    std::atomic<int> normal_executed{0};
    std::atomic<int> workers_finished{0};

    // Добавляем разные задачи до shutdown
    for (int i = 0; i < ALL_TASKS; ++i) {
        if (i % 2 == 0) {
            // Четные - High-приоритет
            queue.push(TaskPriority::High, [&high_executed]() { high_executed++; });
        } else {
            // Нечетные - Normal-приоритет
            queue.push(TaskPriority::Normal, [&normal_executed]() { normal_executed++; });
        }
    }

    // Запускаем несколько потоков-воркеров
    std::vector<std::jthread> workers;
    for (int i = 0; i < NUM_THREADS; ++i) {
        workers.emplace_back([&queue, &workers_finished]() {
            while (true) {
                auto task = queue.pop();
                if (!task.has_value()) {
                    break;  // shutdown и очередь пуста
                }
                (*task)();
            }
            workers_finished++;
        });
    }

    // Даем потокам время начать работу
    std::this_thread::sleep_for(std::chrono::milliseconds(DELAY_MS));

    // Вызываем shutdown
    queue.shutdown();

    // Ждем завершения всех потоков
    workers.clear();

    // Проверяем, что все High-задачи выполнены
    EXPECT_EQ(high_executed, TASKS_OF_ONE_PRIORITY);

    // Проверяем, что все Normal-задачи выполнены
    EXPECT_EQ(normal_executed, TASKS_OF_ONE_PRIORITY);

    // Проверяем, что все потоки-воркеры завершились
    EXPECT_EQ(workers_finished, NUM_THREADS);

    // Очередь должна быть пуста
    auto empty_check = queue.pop();
    EXPECT_FALSE(empty_check.has_value());
}