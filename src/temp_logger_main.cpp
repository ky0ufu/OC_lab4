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
      "  [--raw-keep-sec 86400] [--hour-keep-sec 2592000]\n"
      "  [--compact-sec 300]\n"
      "  (old) [--compact-min 5]\n";
}

static bool parse_temp_line(const std::string& line, double& out) {
    std::string s = line;

    s.erase(std::remove(s.begin(), s.end(), '\0'), s.end());

    if (s.rfind("TEMP=", 0) == 0) s = s.substr(5);

    for (char& c : s) if (c == ',') c = '.';

    try {
        out = std::stod(s);
        return true;
    } catch (...) {
        return false;
    }
}


int main(int argc, char** argv) {
    std::string source = "serial";
    std::string port;
    int baud = 9600;

    std::string rawPath  = "measurements.log";
    std::string hourPath = "hourly_avg.log";
    std::string dayPath  = "daily_avg.log";

    int compactMin = 5;

    long long rawKeepSec  = 24 * 3600;         // 24 часа
    long long hourKeepSec = 30 * 24 * 3600;    // 30 дней
    long long compactSec  = 5 * 60;            // 5 минут

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

        else if (a == "--raw-keep-sec")  rawKeepSec  = std::stoll(need("--raw-keep-sec"));
        else if (a == "--hour-keep-sec") hourKeepSec = std::stoll(need("--hour-keep-sec"));
        else if (a == "--compact-sec")   compactSec  = std::stoll(need("--compact-sec"));

        else if (a == "--compact-min") compactMin = std::stoi(need("--compact-min"));
        else if (a == "-h" || a == "--help") { usage(); return 0; }
        else { std::cerr << "Unknown arg: " << a << "\n"; usage(); return 2; }
    }

    using clock = std::chrono::system_clock;
    auto now = clock::now();

    // raw: последние 24 часа
    RetentionLog rawLog(rawPath, [rawKeepSec](auto n){ return n - std::chrono::seconds(rawKeepSec); });

    // hourly: последние 30 дней
    RetentionLog hourLog(hourPath, [hourKeepSec](auto n){ return n - std::chrono::seconds(hourKeepSec); });

    // daily: текущий год
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

    auto nextCompact = clock::now() + std::chrono::seconds(compactSec);

    std::cerr << "temp_logger started. source=" << source << "\n";
    std::cerr << "retention: rawKeepSec=" << rawKeepSec
              << " hourKeepSec=" << hourKeepSec
              << " compactSec=" << compactSec << "\n";

    std::string line;
    while (reader->readLine(line)) {
        double temp = 0.0;
        if (!parse_temp_line(line, temp)) continue;
        auto ts = clock::now();

        // все измерения
        rawLog.append({ts, temp});

        // среднее за час
        if (auto fin = hourAgg.push(ts, temp)) {
            hourLog.append({fin->period_start, fin->avg});
        }

        // среднее за день
        if (auto fin = dayAgg.push(ts, temp)) {
            dayLog.append({fin->period_start, fin->avg});

            // на случай смены года
            dayLog.compact_to_disk(clock::now());
        }

        // компактация раз в compactSec секунд
        auto n = clock::now();
        if (n >= nextCompact) {
            rawLog.compact_to_disk(n);
            hourLog.compact_to_disk(n);
            dayLog.compact_to_disk(n);
            nextCompact = n + std::chrono::seconds(compactSec);
        }
    }

    std::cerr << "temp_logger finished (input closed)\n";
    return 0;
}
