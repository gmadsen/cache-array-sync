//
// Created by garrett on 2/23/25.
//
#include "src/file_system_monitor.hpp"

#include <sys/inotify.h>


FileSystemMonitor::FileSystemMonitor() : inotifyFd(inotify_init()) {
    // constructor
}

void FileSystemMonitor::monitor(const std::string& path) {
}


void FileSystemMonitor::stop() {

}
void FileSystemMonitor::setCallback(std::function<void(const std::string&)> cb) {
    callback = cb;
}

void FileSystemMonitor::addWatch(const std::string& path) {
}

std::optional<FileSystemMonitor::FSEvent> FileSystemMonitor::getNextEvent() {
    return std::nullopt;
}