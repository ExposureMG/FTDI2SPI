#ifndef LIBUSB_CONFIG_H
#define LIBUSB_CONFIG_H

#ifdef _WIN32
#define PLATFORM_WINDOWS 1
#define DEFAULT_VISIBILITY
#define PRINTF_FORMAT(a, b)
#define ENABLE_LOGGING 1
#else
#define PLATFORM_POSIX 1
#define _GNU_SOURCE 1
#define HAVE_CLOCK_GETTIME 1
#define HAVE_TIMERFD 1
#define HAVE_EVENTFD 1
#define HAVE_SYS_TIME_H 1
#define HAVE_UNISTD_H 1
#define HAVE_PTHREAD_CONDATTR_SETCLOCK 1
#define HAVE_NFDS_T 1
#define DEFAULT_VISIBILITY __attribute__((visibility("default")))
#define PRINTF_FORMAT(a, b) __attribute__ ((__format__ (__printf__, a, b)))
#define ENABLE_LOGGING 1
#endif

#endif
