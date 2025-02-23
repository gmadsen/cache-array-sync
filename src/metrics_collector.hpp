//
// Created by garrett on 2/23/25.
//

#ifndef METRICS_COLLECTOR_HPP
#define METRICS_COLLECTOR_HPP


#include <chrono>
#include <mutex>
#include <string>


class MetricsCollector {
private:
    struct Metric {
        std::string name;
        std::string value;
        std::chrono::system_clock::time_point timestamp;
    };

    std::vector<Metric> m_metrics;
    std::mutex m_metrics_mutex;



public:

    void recordMetric(const std::string& name, const std::string& value);

    void collect();

};



#endif //METRICS_COLLECTOR_HPP
