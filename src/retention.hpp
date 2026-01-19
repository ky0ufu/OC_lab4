#pragma once
#include "timeutil.hpp"
#include <deque>
#include <functional>
#include <string>

struct LogRecord {
    timeutil::TP ts{};
    double value{};
};

// Лог-файл: хранит записи в памяти + в файле
class RetentionLog {
public:
    // cutoff_fn(now) -> все записи раньше этого момента удаляем
    RetentionLog(std::string path, std::function<timeutil::TP(timeutil::TP)> cutoff_fn);

    // Прочитать файл, обрезать по cutoff, переписать файл
    void load_and_compact(timeutil::TP now);

    // Добавить запись: append в файл + в память
    void append(const LogRecord& r);

    // Обрезать в памяти и переписать файл целиком
    void compact_to_disk(timeutil::TP now);

private:
    std::string m_path;
    std::deque<LogRecord> m_data;
    std::function<timeutil::TP(timeutil::TP)> m_cutoff;

    void rewrite_file();
};
