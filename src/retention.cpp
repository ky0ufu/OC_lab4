#include "retention.hpp"
#include <filesystem>
#include <fstream>
#include <iomanip>

// Парсит одну строку из лог-файла в структуру LogRecord.
// Формат строки: "YYYY-MM-DDTHH:MM:SS value"
static bool parse_line(const std::string& line, LogRecord& out) {
    auto sp = line.find(' ');
    if (sp == std::string::npos) return false; // неправильная строка

    auto tsStr = line.substr(0, sp);
    auto valStr = line.substr(sp + 1);

    timeutil::TP tp;
    if (!timeutil::parse_iso_local(tsStr, tp)) return false;

    try {
        out.ts = tp;
        out.value = std::stod(valStr);
        return true;
    } catch (...) {
        // Если stod не смог распарсить то пропускаем строку
        return false;
    }
}

// Конструктор: path - путь к лог-файлу,
// cutoff_fn(now) возвращает минимально допустимую дату.
RetentionLog::RetentionLog(std::string path, std::function<timeutil::TP(timeutil::TP)> cutoff_fn)
    : m_path(std::move(path)), m_cutoff(std::move(cutoff_fn)) {}


// Загрузка существующего файла в память + обрезка старых записей
void RetentionLog::load_and_compact(timeutil::TP now) {
    // очищаем текущие данные в памяти
    m_data.clear();

    // читаем файл, если он существует
    std::ifstream in(m_path);
    if (in) {
        std::string line;
        while (std::getline(in, line)) {
            LogRecord r{};
            // если строка корректная то добавляем
            if (parse_line(line, r)) m_data.push_back(r);
        }
    }

    // вычисляем границу хранения
    auto cut = m_cutoff(now);
    // удаляем из начала очереди все записи, которые старее границы
    while (!m_data.empty() && m_data.front().ts < cut) m_data.pop_front();

    // переписываем файл целиком уже без старых строк
    rewrite_file();
}

// Добавление одной записи
void RetentionLog::append(const LogRecord& r) {
    m_data.push_back(r);

    std::ofstream out(m_path, std::ios::app);
    out << timeutil::format_iso_local(r.ts) << " "
        << std::fixed << std::setprecision(3) << r.value << "\n";
}

// Компактация в процессе: удаляем старые записи и переписываем файл.
void RetentionLog::compact_to_disk(timeutil::TP now) {
    auto cut = m_cutoff(now);
    while (!m_data.empty() && m_data.front().ts < cut) m_data.pop_front();
    rewrite_file();
}

// Перезаписывает файл полностью из m_data.
// Пишем во временный файл *.tmp, а потом заменяет основной файл.
void RetentionLog::rewrite_file() {
    namespace fs = std::filesystem;

    fs::path p(m_path);
    fs::path tmp = p;
    tmp += ".tmp";

    {
        std::ofstream out(tmp.string(), std::ios::trunc);
        for (const auto& r : m_data) {
            out << timeutil::format_iso_local(r.ts) << " "
                << std::fixed << std::setprecision(3) << r.value << "\n";
        }
    }

    std::error_code ec;

#ifdef _WIN32
    // удаляем старый файл
    fs::remove(p, ec);
    ec.clear();
#endif
    // переименновываем tmp в основной файл
    fs::rename(tmp, p, ec);
    if (ec) {
        // fallback: скопировать и удалить tmp
        ec.clear();
        fs::copy_file(tmp, p, fs::copy_options::overwrite_existing, ec);
        fs::remove(tmp, ec);
    }
}

    