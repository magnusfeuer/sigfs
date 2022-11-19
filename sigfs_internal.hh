// (C) 2022 Magnus Feuer
//
//  An attempt to write a fast signal distribution that works between containers and processes.
//  Today we use a FUSE user file system

#include <cstdint>
#include <string.h>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <vector>
#include "sigfs.h"

#ifndef __SIGFS_INTERNAL__
#define __SIGFS_INTERNAL__
namespace sigfs {


    enum class Result {
        ok = 0,
        empty = 1,
        would_block = 2
    };

    class Signal {
    public:
        using id_t = std::uint64_t;
        Signal(void):
            data_(0),
            data_size_(0),
            data_alloc_(0),
            sig_id_(0)
        {
        }

        ~Signal(void)
        {
            if (data_)
                delete[] data_;
        }


        inline const char* const data(void) const
        {
            return data_;
        }

        inline const size_t data_size(void) const
        {
            return data_size_;
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

        inline void set(const id_t sig_id, const char* data, const size_t data_size)
        {
            // First time allocation?
            if (data_size > data_size_) {
                if (data_)
                    delete[] data_;

                data_ = new char[data_size];
                data_alloc_ = data_size;
            }

            data_size_ = data_size;
            memcpy(data_, data, data_size_);
            sig_id_ = sig_id;
        }

    private:
//        std::vector<char> data_;
        char* data_;
        size_t data_size_;
        size_t data_alloc_;
        id_t sig_id_;
    };



    // Circ buffer
    //
    // INDEX 0123
    // SIG_id_t   EBCD
    //
    class Subscriber;
    class Queue {
    public:
        using index_t = std::uint32_t;

        // length has to be a power of 2:
        // 2 4 8 16 32, 64, 128, etc
        Queue(const index_t queue_length);
        ~Queue(void);


        // Queue data as a signal on queue.
        Result queue_signal(const char* data, const size_t data_sz);

        //
        // Retrieve the data of the next signal for us to read.
        //
        // If ok is returned, the
        const Result next_signal(Subscriber* sub,
                                 char * data,
                                 const size_t data_size,
                                 size_t& returned_size,
                                 index_t &lost_signal_count) const;

        const bool would_block(const Subscriber* sub) const;


        inline index_t queue_length(void) const {
            return queue_mask_+1;
        }

        void dump(const char* prefix, const Subscriber* sub);

        inline const Signal::id_t tail_sig_id(void) const {
            std::lock_guard<std::mutex> lock(mutex_);
            return tail_sig_id_();
        }

    private:


        // Not thread safe
        inline const Signal::id_t tail_sig_id_(void) const {
            return queue_[tail()].sig_id();
        }

        // Prerequisite: queue size is always a power of 2.
        inline const index_t index(const Signal::id_t id) const
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

        inline const sigfs_index_t next(const sigfs_index_t index) const
        {
            return (index + 1) & queue_mask_;
        }

        inline const sigfs_index_t prev(const sigfs_index_t index) const
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

        mutable std::mutex mutex_;
        mutable std::condition_variable cond_;
        Signal::id_t next_sig_id_; // Monotonic transaction id.
        std::vector< Signal > queue_;
        index_t queue_mask_;
        index_t head_;
        index_t tail_;
    };


    class Subscriber {
    public:
        Subscriber(Queue& queue):
            queue_(queue),
            sig_id_(queue.tail_sig_id())
        {
            static std::mutex mutex_;
            static int next_sub_id = 0;

            std::lock_guard<std::mutex> lock(mutex_);
            sub_id_ = next_sub_id++;
        };

        const int sub_id(void) const
        {
            return sub_id_;
        }

        // current signal id
        const Signal::id_t sig_id(void) const
        {
            return sig_id_;
        }

        void set_sig_id(const Signal::id_t sig_id)
        {
            sig_id_ = sig_id;
        }

        Queue& queue(void)
        {
            return queue_;
        }


    private:
        Queue& queue_;
        Signal::id_t sig_id_; // The Id of the next signal we are about to read.
        int sub_id_; // Used to color separate logging on a per subscribed basis
    };

}

#endif // __SIGFS_INTERNAL__
