#include "retention.hpp"
#include <filesystem>
#include <fstream>
#include <iomanip>

static bool parse_line(const std::string& line, LogRecord& out) {
    // формат: "YYYY-MM-DDTHH:MM:SS value"
    auto sp = line.find(' ');
    if (sp == std::string::npos) return false;

    auto tsStr = line.substr(0, sp);
    auto valStr = line.substr(sp + 1);

    timeutil::TP tp;
    if (!timeutil::parse_iso_local(tsStr, tp)) return false;

    try {
        out.ts = tp;
        out.value = std::stod(valStr);
        return true;
    } catch (...) {
        return false;
    }
}

RetentionLog::RetentionLog(std::string path, std::function<timeutil::TP(timeutil::TP)> cutoff_fn)
    : m_path(std::move(path)), m_cutoff(std::move(cutoff_fn)) {}

void RetentionLog::load_and_compact(timeutil::TP now) {
    m_data.clear();

    std::ifstream in(m_path);
    if (in) {
        std::string line;
        while (std::getline(in, line)) {
            LogRecord r{};
            if (parse_line(line, r)) m_data.push_back(r);
        }
    }

    auto cut = m_cutoff(now);
    while (!m_data.empty() && m_data.front().ts < cut) m_data.pop_front();

    rewrite_file();
}

void RetentionLog::append(const LogRecord& r) {
    m_data.push_back(r);

    std::ofstream out(m_path, std::ios::app);
    out << timeutil::format_iso_local(r.ts) << " "
        << std::fixed << std::setprecision(3) << r.value << "\n";
}

void RetentionLog::compact_to_disk(timeutil::TP now) {
    auto cut = m_cutoff(now);
    while (!m_data.empty() && m_data.front().ts < cut) m_data.pop_front();
    rewrite_file();
}

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
    fs::remove(p, ec);
    ec.clear();
#endif

    fs::rename(tmp, p, ec);
    if (ec) {
        // fallback: скопировать и удалить tmp
        ec.clear();
        fs::copy_file(tmp, p, fs::copy_options::overwrite_existing, ec);
        fs::remove(tmp, ec);
    }
}

