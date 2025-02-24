

#ifndef SYNC_MANAGER_HPP
#define SYNC_MANAGER_HPP

#include <memory>
#include <vector>


class MetricsCollector;
class Configuration;

/// class that will manage the synchronization of the data
class SyncManager
{
public:
    SyncManager(std::shared_ptr<Configuration> config, std::unique_ptr<MetricsCollector> metrics);
    SyncManager(const SyncManager&) = delete;
    SyncManager& operator=(const SyncManager&) = delete;
    SyncManager(SyncManager&&) = delete;
    SyncManager& operator=(SyncManager&&) = delete;

    void syncData();
    void syncData(const std::string& data);
    void syncData(const std::vector<std::string>& data);

    void syncFile(const std::string& file);

    void batchSync(const std::vector<std::string>& paths);

    void performConsistencyCheck();


private:
    std::shared_ptr<Configuration> config;
    std::unique_ptr<MetricsCollector> metrics;
};


#endif //SYNC_MANAGER_HPP

//