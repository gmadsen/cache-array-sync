//
// Created by garrett on 2/23/25.
//

#ifndef CONFIGURATION_HPP
#define CONFIGURATION_HPP
#include <cstdint>


class Configuration {
public:

    Configuration();

    int num_threads{1}; // number of threads to use for synchronization

private:
};

struct inotify_event {
    int      wd;       /* Watch descriptor */
    uint32_t mask;     /* Mask describing event */
    uint32_t cookie;   /* Unique cookie associating related
                          events (for rename(2)) */
    uint32_t len;      /* Size of name field */
    char     name[];   /* Optional null-terminated name */
};

#endif //CONFIGURATION_HPP
