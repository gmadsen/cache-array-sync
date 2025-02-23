//
// Created by garrett on 2/23/25.
//

#include "metrics_collector.hpp"

auto MetricsCollector::recordMetric(const std::string &name, const std::string &value) -> void {
    std::lock_guard lock(m_metrics_mutex);
    m_metrics.push_back({name, value, std::chrono::system_clock::now()});
}