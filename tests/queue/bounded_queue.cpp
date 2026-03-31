#include <gtest/gtest.h>
#include <thread>  // подключение стандартного RAII-потока (std::jthread)
#include <vector>  // подключение стандартного шаблона динамического массива

#include "queue/bounded_queue.hpp"  // интерфейс класса ограниченной очереди задач

using namespace dispatcher::queue;

// === Однопоточные тесты BoundedQueue ===

// Тест создания очереди с корректной емкостью
TEST(BoundedQueueTest, CreateWithValidCapacity) {
    EXPECT_NO_THROW(BoundedQueue queue(1));
    EXPECT_NO_THROW(BoundedQueue queue(10));
    EXPECT_NO_THROW(BoundedQueue queue(1000));
}

// Тест создания очереди с нулевой емкостью (должно быть исключение)
TEST(BoundedQueueTest, CreateWithZeroCapacityThrows) {
    try {
        BoundedQueue queue(0);
        FAIL() << "Expected std::invalid_argument";
    } catch (const std::invalid_argument &e) {
        EXPECT_NE(std::string(e.what()).find("capacity must be positive"), std::string::npos);
    } catch (...) {
        FAIL() << "Expected std::invalid_argument";
    }
}

// Тест FIFO-порядка (проверка сохранения порядка элементов очереди)
TEST(BoundedQueueTest, FifoOrder) {
    const int TASK_COUNT = 100;
    BoundedQueue queue(TASK_COUNT);

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
TEST(BoundedQueueTest, TryPopChangesReturns) {
    BoundedQueue queue(10);

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

// Параметрический тест заполнения очереди до полной емкости
class BoundedQueueCapacityTest : public ::testing::TestWithParam<size_t> {};

TEST_P(BoundedQueueCapacityTest, PushUpToCapacity) {
    size_t capacity = GetParam();
    BoundedQueue queue(capacity);

    // Заполняем очередь до предела тестовыми задачами
    for (size_t i = 0; i < capacity; ++i) {
        queue.push([]() { return; });
    }

    // Проверяем, что все задачи на месте
    for (size_t i = 0; i < capacity; ++i) {
        auto task = queue.try_pop();
        EXPECT_TRUE(task.has_value());
    }

    // Очередь должна быть пуста
    auto empty = queue.try_pop();
    EXPECT_FALSE(empty.has_value());
}

INSTANTIATE_TEST_SUITE_P(BoundedQueueCapacityTests, BoundedQueueCapacityTest, ::testing::Values(1, 5, 10, 100, 1000));

// === Многопоточные тесты BoundedQueue ===

// Тест push из нескольких потоков (без блокировок, т.к. очередь не заполнена)
TEST(BoundedQueueTest, MultiplePushes) {
    const int TASK_COUNT = 10;
    const int CAPACITY = 100;
    BoundedQueue queue(CAPACITY);

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
TEST(BoundedQueueTest, MultipleTryPops) {
    const int TASK_COUNT = 50;
    const int NUM_THREADS = 10;
    const int CAPACITY = 100;
    BoundedQueue queue(CAPACITY);

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

// Тест блокировки push при заполнении
TEST(BoundedQueueTest, PushBlocksWhenFull) {
    const int DELAY_MS = 100;  // время на выполнение/снятие блокировки
    BoundedQueue queue(1);

    // Заполняем очередь
    queue.push([]() { return; });

    std::atomic<bool> push_done{false};
    std::jthread pusher([&queue, &push_done]() {
        queue.push([]() { return; });  // должен заблокироваться
        push_done = true;
    });

    // Даем время на блокировку и проверяем, что push не выполнен
    std::this_thread::sleep_for(std::chrono::milliseconds(DELAY_MS));
    EXPECT_FALSE(push_done);

    // Освобождаем место в очереди
    auto task = queue.try_pop();
    ASSERT_TRUE(task.has_value());

    // Даем время на снятие блокировки и проверяем, что push выполнен
    std::this_thread::sleep_for(std::chrono::milliseconds(DELAY_MS));
    EXPECT_TRUE(push_done);
}