//
// Created by garrett on 2/23/25.
//

#include "metrics_collector.hpp"

#include <iostream>

auto MetricsCollector::recordMetric(const std::string &name, const std::string &value) -> void {
    std::lock_guard lock(m_metrics_mutex);
    m_metrics.push_back({name, value, std::chrono::system_clock::now()});
}

auto MetricsCollector::collect() -> void {
    std::lock_guard lock(m_metrics_mutex);
    for (const auto &metric : m_metrics) {
        std::cout << metric.name << ": " << metric.value << std::endl;
    }
    m_metrics.clear();
}