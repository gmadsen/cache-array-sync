//
// Created by garrett on 2/23/25.
//

#include <iostream>


#include "sync_manager.hpp"

SyncManager::SyncManager() {
    // constructor
}

SyncManager::SyncManager(const std::shared_ptr<Configuration>& config, std::unique_ptr<MetricsCollector> metrics) : config(config), metrics(std::move(metrics)) {
    // constructor
}

void SyncManager::syncData() {
    std::cout << "Syncing data" << std::endl;
}

void SyncManager::syncData(const std::string& data) {
    std::cout << "Syncing data: " << data << std::endl;
}

void SyncManager::syncData(const std::vector<std::string>& data) {
    std::cout << "Syncing data: ";
    for (const auto& d : data) {
        std::cout << d << " ";
    }
    std::cout << std::endl;
}

void SyncManager::syncFile(const std::string& file) {
    std::cout << "Syncing file: " << file << std::endl;
}

void SyncManager::batchSync(const std::vector<std::string>& paths) {
    std::cout << "Batch syncing files: ";
    for (const auto& p : paths) {
        std::cout << p << " ";
    }
    std::cout << std::endl;
}

void SyncManager::performConsistencyCheck() {
    std::cout << "Performing consistency check" << std::endl;
}

// Created by garrett on 2/23/25.