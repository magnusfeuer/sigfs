// Copyright (C) 2022, Magnus Feuer
// This program is licensed under the terms and conditions of the
// Mozilla Public License, version 2.0.  The full text of the
// Mozilla Public License is at https://www.mozilla.org/MPL/2.0/
//
// Author: Magnus Feuer (magnus@feuerworks.com)
//

#ifndef __SIGFS_QUEUE__
#define __SIGFS_QUEUE__
#include "sigfs_common.hh"

namespace sigfs {

    class Subscriber;


    enum class Result {
        ok = 0,
        empty = 1,
        would_block = 2
    };


    class Queue {
    public:
        using index_t = std::uint32_t;

        //
        // The struct returned by each read operation.
        // Reads should continue until you have received sizeof(sigfs_signal_t) + data_size bytes.
        //
        typedef struct payload_t_ {
            uint32_t payload_size = 0; // Number of bytes in data.
            char payload[];
        } __attribute__((packed)) payload_t;

        template<typename T>
        using signal_callback_t = std::function<void(T userdata,
                                                     signal_id_t signal_id,
                                                     const char* payload,
                                                     std::uint32_t payload_size,
                                                     sigfs::signal_count_t lost_signals)>;


        // length has to be a power of 2:
        // 2 4 8 16 32, 64, 128, etc
        Queue(const index_t queue_length);
        ~Queue(void);


        // Queue data as a signal on queue.
        Result queue_signal(const char* data, const size_t data_sz);

        //
        // Retrieve the data of the next signal for us to read.
        //
        //
        template<typename CallbackT=void*> 
        const Result dequeue_signal(Subscriber * sub,
                                   CallbackT userdata,
                                   signal_callback_t<CallbackT>& cb) const;

        const bool signal_available(const Subscriber* sub) const;


        inline index_t queue_length(void) const {
            return queue_mask_+1;
        }

        void dump(const char* prefix, const Subscriber* sub);

        inline const signal_id_t tail_sig_id(void) const {
            std::lock_guard<std::mutex> lock(mutex_);
            return tail_sig_id_();
        }

    private:


        // Not thread safe
        inline const signal_id_t tail_sig_id_(void) const {
            return queue_[tail()].sig_id();
        }

        // Prerequisite: queue size is always a power of 2.
        inline const index_t index(const signal_id_t id) const
        {
            return id & queue_mask_;
        }

        // Empty will only return true before the first signal
        // has been installed.
        // After that we will continue around the circular buffer
        // and overwrite the oldest signal as we move ahead.
        // There will always be queue_mask_ - 1 signals available,
        // with the last one being queue_[head_], which is the next
        // one to be set.
        //
        inline bool empty(void) const { return head_ == tail_; }

        inline const index_t next(const index_t index) const
        {
            return (index + 1) & queue_mask_;
        }

        inline const index_t prev(const index_t index) const
        {
            return (index - 1) & queue_mask_;
        }

        // Return the next signal to be filled.
        //
        // This psignal should be waited on to be set using a
        // conditional wait as seen in next_signal()
        //
        inline const index_t head(void) const
        {
           return head_;
        }

        // Return the oldest signal in the queue.
        //
        // This signal is also the next signal to get
        // overwritten if the queue is full.
        //
        inline const index_t tail(void) const
        {
           return tail_;
        }

    private:

        class Signal {
        public:
            Signal(void):
                payload_alloc_(0),
                sig_id_(0),
                payload_{}
            {
            }

            ~Signal(void)
            {
                if (payload_)
                    delete[] (char*) payload_;
            }


            inline const payload_t* payload(void) const
            {
                return payload_;
            }


            // Not thread safe! Caller must manage locks
            inline const id_t sig_id(void) const
            {
                return sig_id_;
            }

            // Not thread safe! Caller must manage locks
            inline void set_sig_id(const id_t id)
            {
                sig_id_ = id;
            }

            inline void set(const id_t sig_id, const char* payload, const size_t payload_size)
            {
                // First time allocation?
                if (payload_size + sizeof(signal_t) > payload_alloc_) {
                    if (payload_)
                        delete[] (char*) payload_;

                    payload_ = (payload_t*) new  char[sizeof(payload_t) + payload_size];
                    payload_alloc_ = payload_size + sizeof(payload_t);
                }

                payload_->payload_size = payload_size;
                memcpy(payload_->payload, payload, payload_size);
                sig_id_ = sig_id;
            }

        private:
            size_t payload_alloc_;
            id_t sig_id_;
            payload_t* payload_;
        };


        mutable std::mutex mutex_;
        mutable std::condition_variable cond_;


        // Conditional variable setup used to
        // ensure that subscriber threads always have priority
        //
        mutable std::mutex prio_mutex_;
        mutable std::condition_variable prio_cond_;
        mutable int active_subscribers_;

        signal_id_t next_sig_id_; // Monotonic transaction id.
        std::vector< Signal > queue_;
        index_t queue_mask_;
        index_t head_;
        index_t tail_;
    };
}
#endif // __SIGFS_QUEUE__
