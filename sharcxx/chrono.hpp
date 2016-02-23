// see LICENSE.md for license.
#pragma once

#include "densityxx/globals.hpp"

#if defined(_WIN64) || defined(_WIN32)
#include <windows.h>
#include <time.h>
#else
#include <sys/resource.h>
#endif

namespace density {
    class chrono_t {
    private:
        struct timeval start_;
        struct timeval stop_;
    public:
        void start(void);
        void stop(void);
        double elapsed(void);
    };
    const double sharc_chrono_microseconds = 1000000.0;
}
