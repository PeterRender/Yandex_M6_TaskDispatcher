#pragma once

namespace dispatcher {

// Перечисляемый тип приоритета задачи в системе диспетчеризации
enum class TaskPriority {
    High,   // высокий приоритет (задачи выполняются в первую очередь)
    Normal  // нормальный приоритет (задачи выполняются только при отсутствии High-задач)
};

}  // namespace dispatcher