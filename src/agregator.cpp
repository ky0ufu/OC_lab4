#include "agregator.hpp"

// Конструктор запоминает функцию округления времени
Aggregator::Aggregator(timeutil::TP (*period_floor)(const timeutil::TP&))
    : m_floor(period_floor) {}

// Сброс накопителя для нового периода.    
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

    // Если начало периода изменилось это значит мы перешли в новый период
    if (start != m_start) {
        if (m_cnt > 0) {
            finished = AvgOut{m_start, m_sum / (double)m_cnt};
        }
        // Начинаем накапливать значения для нового периода.
        reset(start);
    }

    // текущее значение
    m_sum += value;
    m_cnt++;

    // Если период не сменился вернётся std::nullopt.
    return finished;
}
