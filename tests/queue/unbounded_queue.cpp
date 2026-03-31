#include <gtest/gtest.h>
#include <thread>  // подключение стандартного RAII-потока (std::jthread)
#include <vector>  // подключение стандартного шаблона динамического массива

#include "queue/unbounded_queue.hpp"  // интерфейс класса неограниченной очереди задач

using namespace dispatcher::queue;

// === Однопоточные тесты UnboundedQueue ===

// Тест создания очереди
TEST(UnboundedQueueTest, CreateQueue) { EXPECT_NO_THROW(UnboundedQueue queue); }

// Тест FIFO-порядка (проверка сохранения порядка элементов очереди)
TEST(UnboundedQueueTest, FifoOrder) {
    const int TASK_COUNT = 1000;
    UnboundedQueue queue;

    std::vector<int> execution_order;
    execution_order.reserve(TASK_COUNT);

    // Добавляем задачи с ожидаемым порядком
    for (int i = 0; i < TASK_COUNT; ++i) {
        queue.push([&execution_order, i]() { execution_order.push_back(i); });
    }

    // Извлекаем и выполняем все задачи
    for (int i = 0; i < TASK_COUNT; ++i) {
        auto task = queue.try_pop();
        ASSERT_TRUE(task.has_value()) << "Failed to pop task " << i;
        (*task)();
    }

    // Проверяем, что порядок выполнения соответствует порядку добавления
    for (int i = 0; i < TASK_COUNT; ++i) {
        EXPECT_EQ(execution_order[i], i) << "Order mismatch at index " << i;
    }
}

// Тест изменения возвращаемого значения try_pop
TEST(UnboundedQueueTest, TryPopChangesReturns) {
    UnboundedQueue queue;

    // Очередь пуста (try_pop должен вернуть nullopt)
    auto empty_result = queue.try_pop();
    EXPECT_FALSE(empty_result.has_value());

    // Добавляем задачу
    bool executed = false;
    queue.push([&executed]() { executed = true; });

    // Очередь не пуста (try_pop должен вернуть задачу)
    auto task = queue.try_pop();
    ASSERT_TRUE(task.has_value());
    (*task)();
    EXPECT_TRUE(executed);

    // После извлечения очередь снова пуста (должен вернуться nullopt)
    auto again_empty = queue.try_pop();
    EXPECT_FALSE(again_empty.has_value());
}

// Параметрический тест большого количества задач
class UnboundedQueueLargeTest : public ::testing::TestWithParam<size_t> {};

TEST_P(UnboundedQueueLargeTest, PushManyTasks) {
    size_t count = GetParam();
    UnboundedQueue queue;

    // Добавляем count тестовых задач
    for (size_t i = 0; i < count; ++i) {
        queue.push([]() { return; });
    }

    // Проверяем, что все задачи на месте
    size_t popped_cnt = 0;
    while (queue.try_pop().has_value()) {
        popped_cnt++;
    }

    EXPECT_EQ(popped_cnt, count);
}

INSTANTIATE_TEST_SUITE_P(UnboundedQueueLargeTests, UnboundedQueueLargeTest, ::testing::Values(10, 100, 1000, 10000));

// === Многопоточные тесты UnboundedQueue ===

// Тест push из нескольких потоков
TEST(UnboundedQueueTest, MultiplePushes) {
    const int TASK_COUNT = 10;
    UnboundedQueue queue;

    // Создаем массив RAII-потоков (с автоожиданием при разрушении)
    std::vector<std::jthread> threads;
    std::atomic<int> pushed_cnt{0};

    // Запускаем по потоку на каждую задачу
    for (int i = 0; i < TASK_COUNT; ++i) {
        threads.emplace_back([&queue, &pushed_cnt]() { queue.push([&pushed_cnt]() { pushed_cnt++; }); });
    }
    threads.clear();  // автоожидание завершения всех потоков при разрушении

    // Проверяем, что все TASK_COUNT задач в очереди
    int popped_cnt = 0;
    while (queue.try_pop().has_value()) {
        popped_cnt++;
    }
    EXPECT_EQ(popped_cnt, TASK_COUNT);
}

// Тест try_pop из нескольких потоков
TEST(UnboundedQueueTest, MultipleTryPops) {
    const int TASK_COUNT = 100;
    const int NUM_THREADS = 10;
    UnboundedQueue queue;

    // Добавляем TASK_COUNT тестовых задач
    for (int i = 0; i < TASK_COUNT; ++i) {
        queue.push([]() { return; });
    }

    // Создаем массив RAII-потоков (с автоожиданием при разрушении)
    std::vector<std::jthread> threads;
    std::atomic<int> popped_cnt{0};

    // NUM_THREADS потоков пытаются извлечь задачи
    for (int i = 0; i < NUM_THREADS; ++i) {
        threads.emplace_back([&queue, &popped_cnt]() {
            while (auto task = queue.try_pop()) {
                popped_cnt++;
            }
        });
    }
    threads.clear();  // автоожидание завершения всех потоков при разрушении

    // Проверяем, что все TASK_COUNT задач извлечены
    EXPECT_EQ(popped_cnt, TASK_COUNT);
}

// Тест параллельных push и try_pop (потоки producers и consumers)
TEST(UnboundedQueueTest, ConcurrentPushAndPop) {
    const int PRODUCERS = 4;
    const int CONSUMERS = 4;
    const int TASKS_PER_PRODUCER = 250;  // всего задач = 1000
    UnboundedQueue queue;

    std::atomic<int> tasks_completed{0};
    std::atomic<bool> producers_done{false};

    // Потоки-producers добавляют задачи в очередь
    std::vector<std::jthread> producers;
    for (int i = 0; i < PRODUCERS; ++i) {
        producers.emplace_back([&queue, &tasks_completed, TASKS_PER_PRODUCER]() {
            for (int j = 0; j < TASKS_PER_PRODUCER; ++j) {
                queue.push([&tasks_completed]() { tasks_completed++; });
            }
        });
    }

    // Потоки-consumers (работают, пока producers не завершились или есть задачи)
    std::vector<std::jthread> consumers;
    for (int i = 0; i < CONSUMERS; ++i) {
        consumers.emplace_back([&queue, &producers_done]() {
            while (!producers_done || queue.try_pop().has_value()) {
                if (auto task = queue.try_pop()) {
                    (*task)();
                }
            }
        });
    }

    producers.clear();      // автоожидание завершения всех потоков-producers при разрушении
    producers_done = true;  // сигнализируем consumers, что producers закончили
    consumers.clear();      // автоожидание завершения всех потоков-consumers при разрушении

    // Проверяем, что все задачи выполнены
    EXPECT_EQ(tasks_completed, PRODUCERS * TASKS_PER_PRODUCER);
}

// Тест множества потоков с интенсивным push/pop (стресс-тест)
TEST(UnboundedQueueTest, StressTestManyThreads) {
    const int NUM_THREADS = 20;
    const int OPS_PER_THREAD = 500;  // каждый поток делает 500 операций
    UnboundedQueue queue;

    std::atomic<int> pushes{0};       // количество выполненных push
    std::atomic<int> good_pops{0};    // количество успешных try_pop
    std::atomic<int> failed_pops{0};  // количество неудачных try_pop (очередь пуста)

    // Каждый поток чередует push и try_pop
    std::vector<std::jthread> threads;
    for (int i = 0; i < NUM_THREADS; ++i) {
        threads.emplace_back([&queue, &pushes, &good_pops, &failed_pops, OPS_PER_THREAD]() {
            for (int j = 0; j < OPS_PER_THREAD; ++j) {
                // Четные итерации - push, нечетные - try_pop
                if (j % 2 == 0) {
                    queue.push([&pushes]() { pushes++; });
                } else {
                    auto task = queue.try_pop();
                    if (task.has_value()) {
                        (*task)();  // выполняем задачу (увеличивает pushes)
                        good_pops++;
                    } else {
                        failed_pops++;
                    }
                }
            }
        });
    }

    threads.clear();  // дожидаемся завершения всех потоков

    // После завершения всех потоков извлекаем и выполняем оставшиеся задачи
    while (auto task = queue.try_pop()) {
        (*task)();
        good_pops++;
    }

    // Проверяем, что все добавленные задачи выполнены
    // Количество успешных pop должно равняться количеству push
    EXPECT_EQ(good_pops, pushes);

    // Очередь должна быть пуста
    auto empty_check = queue.try_pop();
    EXPECT_FALSE(empty_check.has_value());
}