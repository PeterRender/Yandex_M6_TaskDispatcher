#pragma once

#include <cstdio>
#include <string>

// Класс-синглтон для потокобезопасного логирования
// Потокобезопасность достигается отключением буферизации stdout
// (fprintf без буфера является потокобезопасным на уровне POSIX)
class Logger {
public:
    // Возвращает единственный экземпляр логгера
    static Logger &Get() {
        static Logger instance;  // инициализируется один раз при первом вызове
        return instance;
    }

    // Деструктор по умолчанию
    ~Logger() = default;

    // Запрещаем копирование и перемещение (т.к. синглтон)
    Logger(const Logger &) = delete;
    Logger &operator=(const Logger &) = delete;
    Logger(Logger &&) = delete;
    Logger &operator=(Logger &&) = delete;

    // Записывает сообщение в stdout (потокобезопасно, т.к. буферизация отключена)
    void Log(const std::string &message) { fprintf(file_, "%s\n", message.c_str()); }

private:
    // Приватный конструктор (никто не может создать объект напрямую)
    Logger() {
        file_ = stdout;                      // выводим в стандартный поток вывода
        setvbuf(file_, nullptr, _IONBF, 0);  // отключаем буферизацию -> потокобезопасность
    }

    FILE *file_;  // указатель на файл/поток вывода (stdout)
};
