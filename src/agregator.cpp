#include "agregator.hpp"

Aggregator::Aggregator(timeutil::TP (*period_floor)(const timeutil::TP&))
    : m_floor(period_floor) {}

void Aggregator::reset(timeutil::TP newStart) {
    m_inited = true;
    m_start = newStart;
    m_sum = 0.0;
    m_cnt = 0;
}

std::optional<AvgOut> Aggregator::push(timeutil::TP ts, double value) {
    auto start = m_floor(ts);

    if (!m_inited) {
        reset(start);
    }

    std::optional<AvgOut> finished;

    // если период сменился — выдаём среднее за прошлый период
    if (start != m_start) {
        if (m_cnt > 0) finished = AvgOut{m_start, m_sum / (double)m_cnt};
        reset(start);
    }

    // текущее значение
    m_sum += value;
    m_cnt++;
    return finished;
}
