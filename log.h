// Copyright (C) 2018, Jaguar Land Rover
// This program is licensed under the terms and conditions of the
// Mozilla Public License, version 2.0.  The full text of the
// Mozilla Public License is at https://www.mozilla.org/MPL/2.0/
//
// Author: Magnus Feuer (mfeuer1@jaguarlandrover.com)
//
// Acquired from:
// https://github.com/PDXostc/reliable_multicast/blob/master/rmc_log.h


#ifndef __SIGFS_LOG_H__
#define __SIGFS_LOG_H__
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
#define SIGFS_LOG_LEVEL_NONE 0
#define SIGFS_LOG_LEVEL_FATAL 1
#define SIGFS_LOG_LEVEL_ERROR 2
#define SIGFS_LOG_LEVEL_WARNING 3
#define SIGFS_LOG_LEVEL_INFO 4
#define SIGFS_LOG_LEVEL_COMMENT 5
#define SIGFS_LOG_LEVEL_DEBUG 6

    typedef int64_t usec_timestamp_t;
    extern usec_timestamp_t rmc_usec_monotonic_timestamp(void);

    extern char* sigfs_log_timestamp(char* target);
    extern void sigfs_log_set_start_time(void);
    extern usec_timestamp_t sigfs_usec_since_start(void);
    extern usec_timestamp_t sigfs_usec_monotonic_timestamp(void);
    extern usec_timestamp_t sigfs_log_get_start_time(void);
    extern void sigfs_log_use_color(int use_color);
    extern int sigfs_log_level_set(int log_level);
    extern int sigfs_log_level_get(void);
    extern void sigfs_log(int log_level, const char* func, const char* file, int line, int index, const char* fmt, ...);
    extern const char* sigfs_log_color_none();
    extern const char* sigfs_log_color_faint();
    extern const char* sigfs_log_color_green();
    extern const char* sigfs_log_color_blue();
    extern const char* sigfs_log_color_orange();
    extern const char* sigfs_log_color_red();
    extern const char* sigfs_log_color_flashing_red();
    extern const char* sigfs_index_color(int index);

    extern int _sigfs_log_level;

#ifndef SIGFS_NIL_INDEX
#define SIGFS_NIL_INDEX 0x7FFF
#endif

#ifndef SIGFS_INDEX_COUNT
#define SIGFS_INDEX_COUNT 11
#endif

#ifdef SIGFS_LOG
#define SIGFS_LOG_DEBUG(fmt, ...) { if (_sigfs_log_level >= SIGFS_LOG_LEVEL_DEBUG) sigfs_log(SIGFS_LOG_LEVEL_DEBUG, __FUNCTION__, __FILE__, __LINE__, SIGFS_NIL_INDEX, fmt, ##__VA_ARGS__ ); }
#define SIGFS_LOG_COMMENT(fmt, ...) { if (_sigfs_log_level >= SIGFS_LOG_LEVEL_COMMENT) sigfs_log(SIGFS_LOG_LEVEL_COMMENT, __FUNCTION__, __FILE__, __LINE__, SIGFS_NIL_INDEX, fmt, ##__VA_ARGS__ ); }
#define SIGFS_LOG_INFO(fmt, ...) { if (_sigfs_log_level >= SIGFS_LOG_LEVEL_INFO) sigfs_log(SIGFS_LOG_LEVEL_INFO, __FUNCTION__, __FILE__, __LINE__, SIGFS_NIL_INDEX, fmt, ##__VA_ARGS__); }
#define SIGFS_LOG_WARNING(fmt, ...) { if (_sigfs_log_level >= SIGFS_LOG_LEVEL_WARNING) sigfs_log(SIGFS_LOG_LEVEL_WARNING, __FUNCTION__, __FILE__, __LINE__, SIGFS_NIL_INDEX, fmt, ##__VA_ARGS__); }
#define SIGFS_LOG_ERROR(fmt, ...) { if (_sigfs_log_level >= SIGFS_LOG_LEVEL_ERROR) sigfs_log(SIGFS_LOG_LEVEL_ERROR, __FUNCTION__, __FILE__, __LINE__, SIGFS_NIL_INDEX, fmt, ##__VA_ARGS__); }
#define SIGFS_LOG_FATAL(fmt, ...) { if (_sigfs_log_level >= SIGFS_LOG_LEVEL_FATAL) sigfs_log(SIGFS_LOG_LEVEL_FATAL, __FUNCTION__, __FILE__, __LINE__, SIGFS_NIL_INDEX, fmt, ##__VA_ARGS__); }

#define SIGFS_LOG_INDEX_DEBUG(index, fmt, ...) { if (_sigfs_log_level >= SIGFS_LOG_LEVEL_DEBUG) sigfs_log(SIGFS_LOG_LEVEL_DEBUG, __FUNCTION__, __FILE__, __LINE__, index, fmt, ##__VA_ARGS__ ); }
#define SIGFS_LOG_INDEX_COMMENT(index, fmt, ...) { if (_sigfs_log_level >= SIGFS_LOG_LEVEL_COMMENT) sigfs_log(SIGFS_LOG_LEVEL_COMMENT, __FUNCTION__, __FILE__, __LINE__, index, fmt, ##__VA_ARGS__ ); }
#define SIGFS_LOG_INDEX_INFO(index, fmt, ...) { if (_sigfs_log_level >= SIGFS_LOG_LEVEL_INFO) sigfs_log(SIGFS_LOG_LEVEL_INFO, __FUNCTION__, __FILE__, __LINE__, index, fmt, ##__VA_ARGS__); }
#define SIGFS_LOG_INDEX_WARNING(index, fmt, ...) { if (_sigfs_log_level >= SIGFS_LOG_LEVEL_WARNING) sigfs_log(SIGFS_LOG_LEVEL_WARNING, __FUNCTION__, __FILE__, __LINE__, index, fmt, ##__VA_ARGS__); }
#define SIGFS_LOG_INDEX_ERROR(index, fmt, ...) { if (_sigfs_log_level >= SIGFS_LOG_LEVEL_ERROR) sigfs_log(SIGFS_LOG_LEVEL_ERROR, __FUNCTION__, __FILE__, __LINE__, index, fmt, ##__VA_ARGS__); }
#define SIGFS_LOG_INDEX_FATAL(index, fmt, ...) { if (_sigfs_log_level >= SIGFS_LOG_LEVEL_FATAL) sigfs_log(SIGFS_LOG_LEVEL_FATAL, __FUNCTION__, __FILE__, __LINE__, index, fmt, ##__VA_ARGS__); }
#else
#define SIGFS_LOG_DEBUG(fmt, ...) { }
#define SIGFS_LOG_COMMENT(fmt, ...) { }
#define SIGFS_LOG_INFO(fmt, ...) { }
#define SIGFS_LOG_WARNING(fmt, ...) { }
#define SIGFS_LOG_ERROR(fmt, ...) { }
#define SIGFS_LOG_FATAL(fmt, ...) { }

#define SIGFS_LOG_INDEX_DEBUG(index, fmt, ...) { }
#define SIGFS_LOG_INDEX_COMMENT(index, fmt, ...) { }
#define SIGFS_LOG_INDEX_INFO(index, fmt, ...) { }
#define SIGFS_LOG_INDEX_WARNING(index, fmt, ...) { }
#define SIGFS_LOG_INDEX_ERROR(index, fmt, ...) { }
#define SIGFS_LOG_INDEX_FATAL(index, fmt, ...) { }
#endif

#ifdef __cplusplus
}
#endif



#endif // __SIGFS_LOG_H__
