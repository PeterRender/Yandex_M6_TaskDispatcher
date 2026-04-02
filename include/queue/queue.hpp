#pragma once
#include <functional>
#include <optional>

namespace dispatcher::queue {

// Структура конфигурации очереди задач
struct QueueOptions {
    bool bounded;                    // true - ограниченная, false - неограниченная очередь
    std::optional<size_t> capacity;  // max размер очереди (только для bounded-очереди)
};

// Интерфейсный класс очереди задач
// (реализации должны обеспечивать потокобезопасность методов push/try_pop)
class IQueue {
public:
    // Виртуальный деструктор по умолчанию (для корректного удаления через интерфейс)
    virtual ~IQueue() = default;

    // Помещает задачу в очередь
    virtual void push(std::function<void()> task) = 0;

    // Пытается извлечь задачу из очереди (возвращает std::nullopt, если очередь пуста)
    virtual std::optional<std::function<void()>> try_pop() = 0;
};

}  // namespace dispatcher::queue