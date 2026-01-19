#pragma once
#include <chrono>
#include <string>

namespace timeutil {
using TP = std::chrono::system_clock::time_point;

// Форматирование времени в локальном времени: "YYYY-MM-DDTHH:MM:SS"
std::string format_iso_local(const TP& tp);

// Парсинг ISO локального времени
bool parse_iso_local(const std::string& s, TP& out);

// Округление времени к началу часа/дня
TP floor_to_hour(const TP& tp);
TP floor_to_day(const TP& tp);

// 1 января текущего года
TP start_of_current_year(const TP& now);
} // namespace timeutil
