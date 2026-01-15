#include "agregator.hpp"
#include "line_reader.hpp"
#include "retention.hpp"
#include "timeutil.hpp"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <algorithm>

static void usage() {
    std::cerr <<
      "temp_logger:\n"
      "  --source stdin|serial\n"
      "  [--port COM3|/dev/ttyUSB0] [--baud 9600]\n"
      "  [--raw measurements.log] [--hour hourly_avg.log] [--day daily_avg.log]\n"
      "  [--compact-min 5]\n";
}

static bool parse_temp_line(const std::string& line, double& out) {
    std::string s = line;

    s.erase(std::remove(s.begin(), s.end(), '\0'), s.end());

    if (s.rfind("TEMP=", 0) == 0) s = s.substr(5);

    // на всякий случай: запятая вместо точки
    for (char& c : s) if (c == ',') c = '.';

    try {
        out = std::stod(s);
        return true;
    } catch (...) {
        return false;
    }
}


int main(int argc, char** argv) {
    std::string source = "stdin";
    std::string port;
    int baud = 9600;

    std::string rawPath  = "measurements.log";
    std::string hourPath = "hourly_avg.log";
    std::string dayPath  = "daily_avg.log";

    int compactMin = 5;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto need = [&](const char* name) -> std::string {
            if (i + 1 >= argc) { std::cerr << "Missing value for " << name << "\n"; std::exit(2); }
            return argv[++i];
        };

        if (a == "--source") source = need("--source");
        else if (a == "--port") port = need("--port");
        else if (a == "--baud") baud = std::stoi(need("--baud"));
        else if (a == "--raw") rawPath = need("--raw");
        else if (a == "--hour") hourPath = need("--hour");
        else if (a == "--day") dayPath = need("--day");
        else if (a == "--compact-min") compactMin = std::stoi(need("--compact-min"));
        else if (a == "-h" || a == "--help") { usage(); return 0; }
        else { std::cerr << "Unknown arg: " << a << "\n"; usage(); return 2; }
    }

    using clock = std::chrono::system_clock;
    auto now = clock::now();

    // raw: последние 24 часа
    RetentionLog rawLog(rawPath, [](auto n){ return n - std::chrono::hours(24); });

    // hourly: последние 30 дней (упрощение для лабы)
    RetentionLog hourLog(hourPath, [](auto n){ return n - std::chrono::hours(24 * 30); });

    // daily: текущий год (с 1 января)
    RetentionLog dayLog(dayPath, [](auto n){ return timeutil::start_of_current_year(n); });

    // при старте обрежем уже существующие файлы
    rawLog.load_and_compact(now);
    hourLog.load_and_compact(now);
    dayLog.load_and_compact(now);

    std::unique_ptr<LineReader> reader;
    if (source == "stdin") {
        reader = make_stdin_reader();
    } else if (source == "serial") {
        if (port.empty()) { std::cerr << "Error: --port required for serial\n"; return 2; }
        bool ok = false;
        reader = make_serial_reader(port, baud, ok);
        if (!ok) { std::cerr << "Error: can't open serial port " << port << "\n"; return 2; }
    } else {
        std::cerr << "Error: unknown --source\n";
        return 2;
    }

    Aggregator hourAgg(timeutil::floor_to_hour);
    Aggregator dayAgg(timeutil::floor_to_day);

    auto nextCompact = clock::now() + std::chrono::minutes(compactMin);

    std::cerr << "temp_logger started. source=" << source << "\n";

    std::string line;
    while (reader->readLine(line)) {
        double temp = 0.0;
        if (!parse_temp_line(line, temp)) continue;
        auto ts = clock::now();

        // 1) все измерения
        rawLog.append({ts, temp});

        // 2) среднее за час
        if (auto fin = hourAgg.push(ts, temp)) {
            hourLog.append({fin->period_start, fin->avg});
        }

        // 3) среднее за день
        if (auto fin = dayAgg.push(ts, temp)) {
            dayLog.append({fin->period_start, fin->avg});

            // простое решение на случай смены года:
            // при смене дня ещё раз подрежем дневной лог
            dayLog.compact_to_disk(clock::now());
        }

        // компактация раз в N минут (удаляем старые строки)
        auto n = clock::now();
        if (n >= nextCompact) {
            rawLog.compact_to_disk(n);
            hourLog.compact_to_disk(n);
            dayLog.compact_to_disk(n);
            nextCompact = n + std::chrono::minutes(compactMin);
        }
    }

    std::cerr << "temp_logger finished (input closed)\n";
    return 0;
}
