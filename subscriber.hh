// Copyright (C) 2022, Magnus Feuer
// This program is licensed under the terms and conditions of the
// Mozilla Public License, version 2.0.  The full text of the
// Mozilla Public License is at https://www.mozilla.org/MPL/2.0/
//
// Author: Magnus Feuer (magnus@feuerworks.com)
//

#ifndef __SIGFS_SUBSCRIBER__
#define __SIGFS_SUBSCRIBER__

#include <cstdint>
#include <functional>
#include <mutex>
#include <vector>
#include <string.h>
#include "queue.hh"

namespace sigfs {
    using index_t = std::int32_t;

    class Subscriber {
    public:
        Subscriber(std::shared_ptr<Queue> queue):
            queue_(queue),
            sig_id_(0),
            interrupted_(false)
        {
            static std::mutex mutex_;
            static int next_sub_id = 0;

            std::lock_guard<std::mutex> lock(mutex_);
            sub_id_ = next_sub_id++;
            queue_->initialize_subscriber(*this);
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

        inline void interrupt_dequeue(void)
        {
            queue_->interrupt_dequeue(*this);
        }

        inline std::shared_ptr<Queue> queue(void)
        {
            return queue_;
        }

        inline bool is_interrupted(void) const
        {
            return interrupted_;
        }

        inline void set_interrupted(bool interrupted)
        {
            interrupted_ = interrupted;
        }

        const signal_count_t signal_available(void) const
        {
            return queue_->signal_available(*this);
        }


    private:
        std::shared_ptr<Queue> queue_;
        signal_id_t sig_id_; // The Id of the next signal we are about to read.
        int sub_id_; // Used to color separate logging on a per subscribed basis
        bool interrupted_; // Set to true to indicate that a dequeue_signal() has been interrupted.
    };
}
#endif // __SIGFS_SUBSCRIBER__
