// Copyright (C) 2022, Magnus Feuer
// This program is licensed under the terms and conditions of the
// Mozilla Public License, version 2.0.  The full text of the
// Mozilla Public License is at https://www.mozilla.org/MPL/2.0/
//
// Author: Magnus Feuer (magnus@feuerworks.com)
//

#ifndef __SIGFS_COMMON__
#define __SIGFS_COMMON__

#include <stdint.h>
typedef uint64_t signal_id_t;
typedef uint32_t signal_count_t;


#ifdef __cplusplus
extern "C" {
#endif
typedef struct sigfs_payload_t_ {
    uint32_t payload_size; // Number of bytes in data.
    char payload[];
} __attribute__((packed)) sigfs_payload_t;

#define SIGFS_PAYLOAD_SIZE(payload) (sizeof(sigfs_payload_t) + payload->payload_size)

//
// Single signal as reported by Queue::next_signal()
//
typedef struct sigfs_signal_t_ {
    // Number of signals lost before
    // this signal was returned.
    // Happens when the subscriber is too slow to call Queue::next_signal()
    // and the publishers overwrites the circular buffer.
    //
    signal_count_t lost_signals;

    //
    // Unique and incrementally greater for each signal in a given Queue object.
    // Starts at 1, and then is incremented by one for each new signal by
    // Queue::queue_signal().
    //
    signal_id_t signal_id;

    //
    // Signal payload
    //
    sigfs_payload_t payload;
} __attribute__((packed)) sigfs_signal_t;

#define SIGFS_SIGNAL_SIZE(signal) (sizeof(sigfs_signal_t) + signal->payload.payload_size)

#ifdef __cplusplus
}
#endif

#endif // __SIGFS_COMMON__
