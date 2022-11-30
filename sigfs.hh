// Copyright (C) 2022, Magnus Feuer
// This program is licensed under the terms and conditions of the
// Mozilla Public License, version 2.0.  The full text of the
// Mozilla Public License is at https://www.mozilla.org/MPL/2.0/
//
// Author: Magnus Feuer (magnus@feuerworks.com)
//

#ifndef __SIGFS__
#define __SIGFS__

#include <cstdint>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <string.h>
#include "queue.hh"

namespace sigfs {
    using index_t = std::int32_t;

    class Subscriber {
    public:
        typedef  std::function<bool(void)>  check_interrupt_cb_t;
        Subscriber(Queue& queue,
                   check_interrupt_cb_t check_interrupt = []() -> bool { return false; } ):
            queue_(queue),
            check_interrupt_(check_interrupt),
            sig_id_(queue.tail_sig_id())
        {
            static std::mutex mutex_;
            static int next_sub_id = 0;

            std::lock_guard<std::mutex> lock(mutex_);
            sub_id_ = next_sub_id++;
        };

        inline const int sub_id(void) const
        {
            return sub_id_;
        }

        // current signal id
        inline const signal_id_t sig_id(void) const
        {
            return sig_id_;
        }

        inline void set_sig_id(const signal_id_t sig_id)
        {
            sig_id_ = sig_id;
        }

        inline Queue& queue(void)
        {
            return queue_;
        }


        inline bool check_interrupt(void)
        {
            return check_interrupt_();
        }

    private:
        Queue& queue_;

        std::function<bool(void)> check_interrupt_;

        signal_id_t sig_id_; // The Id of the next signal we are about to read.
        int sub_id_; // Used to color separate logging on a per subscribed basis
    };
}
#endif // __SIGFS__
