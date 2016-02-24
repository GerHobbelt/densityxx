// see LICENSE.md for license.
#include "sharcxx/chrono.hpp"

#if defined(_WIN64) || defined(_WIN32)

#define RUSAGE_SELF     0
#define RUSAGE_THREAD   1 

struct rusage {
    struct timeval ru_utime; /* user CPU time used */
    struct timeval ru_stime; /* system CPU time used */
    long   ru_maxrss;        /* maximum resident set size */
    long   ru_ixrss;         /* integral shared memory size */
    long   ru_idrss;         /* integral unshared data size */
    long   ru_isrss;         /* integral unshared stack size */
    long   ru_minflt;        /* page reclaims (soft page faults) */
    long   ru_majflt;        /* page faults (hard page faults) */
    long   ru_nswap;         /* swaps */
    long   ru_inblock;       /* block input operations */
    long   ru_oublock;       /* block output operations */
    long   ru_msgsnd;        /* IPC messages sent */
    long   ru_msgrcv;        /* IPC messages received */
    long   ru_nsignals;      /* signals received */
    long   ru_nvcsw;         /* voluntary context switches */
    long   ru_nivcsw;        /* involuntary context switches */
};

void usage_to_timeval(FILETIME *ft, struct timeval *tv) {
    ULARGE_INTEGER time;
    time.LowPart = ft->dwLowDateTime;
    time.HighPart = ft->dwHighDateTime;

    tv->tv_sec = time.QuadPart / 10000000;
    tv->tv_usec = (time.QuadPart % 10000000) / 10;
}

int getrusage(int who, struct rusage *usage) {
    FILETIME creation_time, exit_time, kernel_time, user_time;
    memset(usage, 0, sizeof(struct rusage));

    if (who == RUSAGE_SELF) {
        if (!GetProcessTimes(GetCurrentProcess(), &creation_time, &exit_time, &kernel_time, &user_time)) {
            return -1;
        }
        usage_to_timeval(&kernel_time, &usage->ru_stime);
        usage_to_timeval(&user_time, &usage->ru_utime);
        return 0;
    } else if (who == RUSAGE_THREAD) {
        if (!GetThreadTimes(GetCurrentThread(), &creation_time, &exit_time, &kernel_time, &user_time)) {
            return -1;
        }
        usage_to_timeval(&kernel_time, &usage->ru_stime);
        usage_to_timeval(&user_time, &usage->ru_utime);
        return 0;
    } else {
        return -1;
    }
}
#endif

namespace density {
    void chrono_t::start(void)
    {
        struct rusage usage;
        getrusage(RUSAGE_SELF, &usage);
        start_ = usage.ru_utime;
    }
    void chrono_t::stop(void)
    {
        struct rusage usage;
        getrusage(RUSAGE_SELF, &usage);
        stop_ = usage.ru_utime;
    }
    double chrono_t::elapsed(void)
    {
        return ((stop_.tv_sec * sharc_chrono_microseconds + stop_.tv_usec) -
                (start_.tv_sec * sharc_chrono_microseconds + start_.tv_usec)) /
            sharc_chrono_microseconds;
    }
}
