#pragma once
#include "timeutil.hpp"
#include <optional>

struct AvgOut {
    timeutil::TP period_start{};
    double avg{};
};

// Накопитель среднего значения по периодам.
class Aggregator {
public:
    // period_floor округляет timestamp к началу часа/дня
    explicit Aggregator(timeutil::TP (*period_floor)(const timeutil::TP&));

    // Добавить измерение. Если период сменился - возвращает среднее за прошлый период.
    std::optional<AvgOut> push(timeutil::TP ts, double value);

private:
    timeutil::TP (*m_floor)(const timeutil::TP&);
    bool m_inited = false;
    timeutil::TP m_start{};
    double m_sum = 0.0;
    long long m_cnt = 0;

    void reset(timeutil::TP newStart);
};
