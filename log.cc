// Copyright (C) 2018, Jaguar Land Rover
// This program is licensed under the terms and conditions of the
// Mozilla Public License, version 2.0.  The full text of the
// Mozilla Public License is at https://www.mozilla.org/MPL/2.0/
//
// Author: Magnus Feuer (mfeuer1@jaguarlandrover.com)
//
// Acquired from:
// https://github.com/PDXostc/reliable_multicast/blob/master/rmc_log.c

// Simple logging

#include "log.h"
#include "string.h"
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>

static usec_timestamp_t start_time = 0;

int _sigfs_log_level = SIGFS_LOG_LEVEL_NONE;
int _sigfs_log_use_color = -1;
int _sigfs_log_use_color_calculated = 0;
FILE *_sigfs_log_file = 0;


// Run when the library is loaded
static void __attribute__((constructor)) log_level_set_on_env(void)
{
    char* log_level = getenv("SIGFS_LOG_LEVEL");

    if (!log_level)
        return;

    sigfs_log_level_set(atoi(log_level));
}


usec_timestamp_t sigfs_usec_monotonic_timestamp(void)
{
    struct timespec res;

    clock_gettime(CLOCK_BOOTTIME, &res);

    return (usec_timestamp_t) res.tv_sec * 1000000 + res.tv_nsec / 1000;
}

void sigfs_log_set_start_time(void)
{
    start_time = sigfs_usec_monotonic_timestamp();
}

usec_timestamp_t sigfs_log_get_start_time(void)
{
    return start_time;
}

void sigfs_log_use_color(int use_color)
{
    _sigfs_log_use_color = use_color;
    _sigfs_log_use_color_calculated = 0;
}

int sigfs_log_level_set(int log_level)
{
    if (log_level < SIGFS_LOG_LEVEL_NONE || log_level > SIGFS_LOG_LEVEL_DEBUG) {

        sigfs_log(SIGFS_LOG_LEVEL_WARNING, __FUNCTION__, __FILE__, __LINE__, SIGFS_NIL_INDEX,
                "Illegal log level: %d. Legal values [%d-%d]", log_level, SIGFS_LOG_LEVEL_NONE, SIGFS_LOG_LEVEL_DEBUG);
        return 1;
    }

    _sigfs_log_level = log_level;
    return 0;
}

int sigfs_log_level_get(void)
{
    return _sigfs_log_level;
}

void sigfs_log_set_file(FILE* file)
{
    _sigfs_log_file = file;

    // If necessary, recalculate if we should use colors or not.

    if (_sigfs_log_use_color_calculated) {
        if (isatty(fileno(_sigfs_log_file)))
            _sigfs_log_use_color = 1;
        else
            _sigfs_log_use_color = 0;
    }
}

const char* sigfs_log_color_flashing_red()
{
    return _sigfs_log_use_color?"\033[5;38;2;192;0;0m":"";
}

const char* sigfs_log_color_light_red()
{
    return _sigfs_log_use_color?"\033[38;2;255;204;204m":"";
}

const char* sigfs_log_color_red()
{
    return _sigfs_log_use_color?"\033[38;2;192;0;0m":"";
}

const char* sigfs_log_color_dark_red()
{
    return _sigfs_log_use_color?"\033[38;2;255;0;0m":"";
}

const char* sigfs_log_color_orange()
{
    return _sigfs_log_use_color?"\033[38;2;255;128;0m":"";
}

const char* sigfs_log_color_yellow()
{
    return _sigfs_log_use_color?"\033[38;2;255;255;0m":"";
}

const char* sigfs_log_color_light_blue()
{
    return _sigfs_log_use_color?"\033[38;2;0;255;255m":"";
}

const char* sigfs_log_color_blue()
{
    return _sigfs_log_use_color?"\033[38;2;0;128;255m":"";
}

const char* sigfs_log_color_dark_blue()
{
    return _sigfs_log_use_color?"\033[38;2;0;0;255m":"";
}

const char* sigfs_log_color_light_green()
{
    return _sigfs_log_use_color?"\033[38;2;153;255;153m":"";
}


const char* sigfs_log_color_green()
{
    return _sigfs_log_use_color?"\033[38;2;0;255;0m":"";
}


const char* sigfs_log_color_dark_green()
{
    return _sigfs_log_use_color?"\033[38;2;0;204;0m":"";
}

const char* sigfs_log_color_faint()
{
    return _sigfs_log_use_color?"\033[2m":"";
}

const char* sigfs_log_color_none()
{
    return _sigfs_log_use_color?"\033[0m":"";
}


const char* sigfs_index_color(int index)
{
    switch(index) {
    case -1:
        return sigfs_log_color_faint();

    case 0:
        return sigfs_log_color_dark_blue();

    case 1:
        return sigfs_log_color_dark_green();

    case 2:
        return sigfs_log_color_light_blue();

    case 3:
        return sigfs_log_color_light_green();

    case 4:
        return sigfs_log_color_light_red();

    case 5:
        return sigfs_log_color_green();

    case 6:
        return sigfs_log_color_blue();

    case 7:
        return sigfs_log_color_dark_red();

    case 8:
        return sigfs_log_color_red();

    case 9:
        return sigfs_log_color_orange();

    case 10:
        return sigfs_log_color_orange();

    default:
        return sigfs_log_color_none();
    }
}

void sigfs_log(int log_level, const char* func, const char* file, int line, int index, const char* fmt, ...)
{
    const char* color = 0;
    const char* tag = 0;
    char index_str[32];
    va_list ap;

    // Set start time, if necessary
    if (!sigfs_log_get_start_time())
        sigfs_log_set_start_time();

    // Default sigfs_log_file, if not set.
    if (!_sigfs_log_file)
        _sigfs_log_file = stdout;

    // If use color is -1, then check if we are on a tty or not
    if (_sigfs_log_use_color == -1) {
        _sigfs_log_use_color_calculated = 1;

        if (isatty(fileno(_sigfs_log_file)))
            _sigfs_log_use_color = 1;
        else
            _sigfs_log_use_color = 0;
    }

    if (index != SIGFS_NIL_INDEX) 
        sprintf(index_str, "%s[%.3d]%s", sigfs_index_color(index), index, sigfs_log_color_none());
    else
        strcpy(index_str, "     ");

    switch(log_level) {
    case SIGFS_LOG_LEVEL_DEBUG:
        color = sigfs_log_color_none();
        tag = "D";
        break;

    case SIGFS_LOG_LEVEL_COMMENT:
        color = sigfs_log_color_green();
        tag = "C";
        break;

    case SIGFS_LOG_LEVEL_INFO:
        color = sigfs_log_color_blue();
        tag = "I";
        break;

    case SIGFS_LOG_LEVEL_WARNING:
        color = sigfs_log_color_orange();
        tag = "W";
        break;

    case SIGFS_LOG_LEVEL_ERROR:
        color = sigfs_log_color_red();
        tag = "E";
        break;

    case SIGFS_LOG_LEVEL_FATAL:
        color = sigfs_log_color_flashing_red();
        tag = "F";
        break;

    default:
        color = sigfs_log_color_none();
        tag = "?";
        break;
    }


    fprintf(_sigfs_log_file, "%s%s%s %lld %s %s%s:%d%s ",
            color,
            tag,
            sigfs_log_color_none(),
            (long long int) (start_time?((sigfs_usec_monotonic_timestamp() - start_time)/1000):0) ,
            index_str,
            sigfs_log_color_faint(),
            file,
            line,
            sigfs_log_color_none());

    va_start(ap, fmt);
    vfprintf(_sigfs_log_file, fmt, ap);
    va_end(ap);
    fputc('\n', _sigfs_log_file);
}

