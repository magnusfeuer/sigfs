// Copyright (C) 2022, Magnus Feuer
// This program is licensed under the terms and conditions of the
// Mozilla Public License, version 2.0.  The full text of the
// Mozilla Public License is at https://www.mozilla.org/MPL/2.0/
//
// Author: Magnus Feuer (magnus@feuerworks.com)
//

#ifndef __SIGFS_COMMON__
#define __SIGFS_COMMON__

#include <cstdint>
namespace sigfs {
    using signal_id_t = std::uint64_t;
    using signal_count_t = std::uint32_t;


    //
    // Single signal as reported by Queue::next_signal()
    //
    struct signal_t {
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
        uint32_t payload_size; // Number of bytes in data.
        char payload[];
    } __attribute__((packed));
}
#endif // __SIGFS_COMMON__
