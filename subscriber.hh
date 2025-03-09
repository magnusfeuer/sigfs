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

    // We will create one subscriber for each call made to
    // sigfs.cc::do_open(), which is done when a process calls open(2)
    // to open a file.
    //
    // The opened file, described by fs.hh::FileSystem::File, will
    // have a single queue, subscribed to by multiple Subscribers.
    //
    // When open(2) is called, a file descriptor is assigned, and libfuse
    // will map that descriptor to a fuse_file_info struct that is forwarded
    // to sigfs.cc::do_open().
    //
    // do_open() will create a new Subscriber instance bound to the
    // File's fs.hh::FileSystem::File::queue_ and assign its pointer
    // to fuse_file_info::fh.
    //
    // In other words, each opened file descriptor will have their own
    // own Subscriber instance that, in its turn, is mapped to the
    // File's queue.
    //
    // This is also true when a single file is opened multiple times
    // (for reading and writing), where each open() operation is assigned
    // their own file descriptor / Subscriber pair.
    //
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

        virtual ~Subscriber(void)
        {}


        void subscribe_read_ready_notifications(void) {
//            std::make_shared<Directory>(owner, inode(), entry);
            //https://en.cppreference.com/w/cpp/memory/enable_shared_from_this
//            queue_->subscribe_read_ready_notifications(std::make_shared<Subscriber>(this));
        }

        void subscribe_write_ready_notifications(void) {
//            queue_->subscribe_write_ready_notifications(*this);
        }

        void unsubscribe_read_ready_notifications(void) {
//            queue_->unsubscribe_read_ready_notifications(*this);
        }

        void unsubscribe_write_ready_notifications(void) {
//            queue_->unsubscribe_write_ready_notifications(*this);
        }

        // Called by Queue when dequeue() has been called and freed up
        // space for others to call queue()
        //
        virtual void queue_write_ready(void) {};

        // Called by Queue when queue() has been called and installed
        // data for others to retreive with dequeue()
        //
        virtual void queue_read_ready(void) {};

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
