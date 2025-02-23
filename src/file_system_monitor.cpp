//
// Created by garrett on 2/23/25.
//
#include "src/file_system_monitor.hpp"
#include <linux/limits.h>
#include <sys/inotify.h>

#include "thread_pool.hpp"

//// from Inotify API documentation
////
/*
       If monitoring an entire directory subtree, and a new subdirectory
       is created in that tree or an existing directory is renamed into
       that tree, be aware that by the time you create a watch for the
       new subdirectory, new files (and subdirectories) may already exist
       inside the subdirectory.  Therefore, you may want to scan the
       contents of the subdirectory immediately after adding the watch
       (and, if desired, recursively add watches for any subdirectories
       that it contains).

       Note that the event queue can overflow.  In this case, events are
       lost.  Robust applications should handle the possibility of lost
       events gracefully.  For example, it may be necessary to rebuild
       part or all of the application cache.  (One simple, but possibly
       expensive, approach is to close the inotify file descriptor, empty
       the cache, create a new inotify file descriptor, and then re-
       create watches and cache entries for the objects to be monitored.)
*/
////
FileSystemMonitor::FileSystemMonitor() : m_inotifyFd(inotify_init()) {
    // constructor
}

void FileSystemMonitor::removeWatch(const std::string& path) {
}


void FileSystemMonitor::stop() {

}
void FileSystemMonitor::setCallback(std::function<void(const std::string&)> cb) {
    m_callback = cb;
}

void FileSystemMonitor::addWatch(const std::string& path) {
    const char *filename = path.c_str();

    if (int watch_desc = inotify_add_watch(m_inotifyFd, filename, IN_MODIFY); watch_desc == -1) {
        perror("inotify_add_watch");
        exit(EXIT_FAILURE);
    }

    size_t bufsiz = sizeof(struct inotify_event) + PATH_MAX + 1;
    struct inotify_event* event = malloc(bufsiz);

    /* wait for an event to occur */
    read(m_inotifyFd, event, bufsiz);

    /* process event struct here */
}

std::optional<FileSystemMonitor::FSEvent> FileSystemMonitor::getNextEvent() {
    return std::nullopt;
}

bool FileSystemMonitor::empty() {
    return event_queue.empty();
}