// Copyright (C) 2022, Magnus Feuer
// This program is licensed under the terms and conditions of the
// Mozilla Public License, version 2.0.  The full text of the
// Mozilla Public License is at https://www.mozilla.org/MPL/2.0/
//
// Author: Magnus Feuer (magnus@feuerworks.com)
//

#ifndef __SIGFS__
#define __SIGFS__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
    typedef int32_t sigfs_index_t;

    // The struct returned by each read operation.
    // Reads should continue until you have received sizeof(sigfs_signal_t) + data_size bytes.
    //
    typedef struct sigfs_signal_t_ {
        uint32_t lost_signals; // How many signals did we lose since  last read.
        uint32_t data_size;
        char data[];
    } __attribute__((packed)) sigfs_signal_t;

#ifdef __cplusplus
}
#endif

#endif // __SIGFS__
