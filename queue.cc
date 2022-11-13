// Copyright (C) 2022, Magnus Feuer
// This program is licensed under the terms and conditions of the
// Mozilla Public License, version 2.0.  The full text of the
// Mozilla Public License is at https://www.mozilla.org/MPL/2.0/
//
// Author: Magnus Feuer (magnus@feuerworks.com)
//
//

//
// Thread safe circular queue with loss detection
//

#include "sigfs_internal.hh"
#include "log.h"
using namespace sigfs;

Queue::Queue(const std::uint32_t queue_size):
    next_sig_id_(0),
    queue_(queue_size),
    queue_mask_(queue_size-1),
    head_(0),
    tail_(0)
{
    if (queue_size < 4) {
        SIGFS_LOG_FATAL("Queue::Queue(): queue_size < 4");
        exit(255);
    }

    if (queue_size & (queue_size - 1)) {
        SIGFS_LOG_FATAL("Queue::Queue(): queue_size[%u] is not a power of 2", queue_size);
        exit(255);
    }
    SIGFS_LOG_DEBUG("Queue::Queue(): queue_size_[%u]", queue_size);
}


Queue::~Queue(void)
{
}


void Queue::dump(const char* prefix, const Subscriber* sub)
{
    index_t ind = 0;
    char suffix[512];

#ifdef SIGFS_LOG
    while(ind < queue_.size()) {
        strcpy(suffix, "<-- ");
        if (ind == tail())
            strcat(suffix, "tail ");

        if (ind == head())
            strcat(suffix, "head ");

        if (index(sub->sig_id()) == ind)
                sprintf(suffix+strlen(suffix), "Sub[%.3d]", sub->sub_id());

        if (strlen(suffix) == 4)
            suffix[0] = 0;

        SIGFS_LOG_DEBUG("%s: [%d] SigID[%lu] [%-*s]%s ", prefix, ind, queue_[ind].sig_id(), queue_[ind].data_size(), queue_[ind].data(), suffix);
        ++ind;
    }
#endif
}

Result Queue::queue_signal(const char* data, const size_t data_size)
{
    SIGFS_LOG_DEBUG("queue_signal(): Called");

    {
        std::lock_guard<std::mutex> lock(mutex_);
        SIGFS_LOG_DEBUG("queue_signal(): Lock guard acquired. ");

        SIGFS_LOG_DEBUG("queue_signal(): Assigned signal ID [%lu]", next_sig_id_);
        queue_[head_].set(next_sig_id_, data, data_size);
        next_sig_id_++;
        // Clear write lock so that pending readers can process.

        // Unlock queue element so that readers will get their read lock and can
        // retrieve and return their data in do_read)(


        // Move tail if we have bumped into it
        head_ = next(head_);
        if (head_ == tail_)
            tail_ = next(tail_);

        // Nil Sig ID for clarity. No functionality is associated with this.
        queue_[head_].set_sig_id(0);
    }


    // Wake up all readers hanging on this slot.
    cond_.notify_all();

    return Result::ok;
}

const bool Queue::would_block(const Subscriber* sub) const
{
    SIGFS_LOG_INDEX_DEBUG(sub->sub_id(), "would_block(): Called");
    std::unique_lock lock(mutex_);

    if (head() == tail() || index(sub->sig_id()) == head()) {
        SIGFS_LOG_INDEX_DEBUG(sub->sub_id(),
                              "would_block(): head{%u} %s tail{%u} --- index(sub->sig_id{%lu}){%u} %s head{%u} -> Would block.",
                              head(),
                              ((head() == tail())?"==":"!="),
                              tail(),

                              sub->sig_id(),
                              index(sub->sig_id()),
                              ((index(sub->sig_id()) == head())?"==":"!="),
                              head());
        return true;
    }
    SIGFS_LOG_INDEX_DEBUG(sub->sub_id(),
                          "would_block(): head{%u} %s tail{%u} --- index(sub->sig_id{%lu}){%u} %s head{%u} -> Would not block.",
                          head(),
                          ((head() == tail())?"==":"!="),
                          tail(),

                          sub->sig_id(),
                          index(sub->sig_id()),
                          ((index(sub->sig_id()) == head())?"==":"!="),
                          head());
    return false;
}


const Result Queue::next_signal(Subscriber* sub,
                                char*  data,
                                const size_t data_size,
                                size_t& returned_size,
                                index_t &lost_signal_count) const
{
    SIGFS_LOG_INDEX_DEBUG(sub->sub_id(), "next_signal(): Called. sub->sig_id(): %lu", sub->sig_id());
    std::unique_lock lock(mutex_);
    SIGFS_LOG_INDEX_DEBUG(sub->sub_id(), "next_signal(): Lock acquired", sub->sig_id());


    // Have we lost signals?
    // We have the sig_id of currently stored signal in queue[index(sub->sig_id())].sig_id()
    // We have the ID of the next signal to process is in sub->sig_id()
    // If the current id < next id then we are waiting for a new signal to fill the slot
    // If the current id == next_id, then the signal is ready to read
    // If the current id > next_id then we have lost signals.
    //
    lost_signal_count = 0;

    //
    // Wait for a signal to become ready.
    //
    const Queue& self(*this);

    cond_.wait(lock, [&self, &sub] {
                         SIGFS_LOG_INDEX_DEBUG(sub->sub_id(),
                                               "next_signal(): head(%lu) %s tail(%lu) --- self.queue_[self.index(sub->sig_id(%u))].sig_id(%lu) - sub->sig_id(%lu) = %ld",
                                               self.head(),
                                               ((self.head() == self.tail())?"==":"!="),
                                               self.tail(),
                                               sub->sig_id(),
                                               self.queue_[self.index(sub->sig_id())].sig_id(),
                                               sub->sig_id(),
                                               (self.queue_[self.index(sub->sig_id())].sig_id() - sub->sig_id()));

                         //
                         // If current id < next id, then we are still waiting for
                         // a new signal to arrive. Continue waiting
                         //
                         if (self.head() == self.tail() || self.queue_[self.index(sub->sig_id())].sig_id() < sub->sig_id())
                             return false;

                         // We either have:
                         // current id == next_id (Signal ready)
                         //   or
                         // current_id > next_id (Signals lost)
                         //
                         // In either case, exit conditional wait.
                         //
                         return true;
                     });

    SIGFS_LOG_INDEX_DEBUG(sub->sub_id(), "next_signal(): Conditional wait exit on signal %lu", sub->sig_id());

    //
    // If the ID of the oldest signal in the queue (tail()) is
    // greater than the next signal we expect to read (sub->sig_id()), then
    // we have lost signals
    //
    // Update subscriber's sig_id to the oldest signal in the queue.
    //
    if (self.queue_[tail()].sig_id() > sub->sig_id()) {
        SIGFS_LOG_INDEX_DEBUG(sub->sub_id(),
                              "next_signal(): Tail catchup for [%lu] lost signals [%lu]->[%lu]",
                              queue_[tail()].sig_id() - sub->sig_id(),
                              sub->sig_id(),
                              queue_[tail()].sig_id());
        //
        // sub->sig_id() is for the next signal we are expecting.
        // queue_[tail()].sig_id() is the signal currently stored in the queue slot
        // we are about to read the signal from.
        //
        lost_signal_count = self.queue_[tail()].sig_id() - sub->sig_id();
        sub->set_sig_id(tail_sig_id_());
    }

    //
    // We are now ready to read out the signal stored by sub->sig_id()
    //

    //
    // Do we have enough storage to copy out signal data?

    if (queue_[index(sub->sig_id())].data_size() > data_size) {
        SIGFS_LOG_INDEX_FATAL(sub->sub_id(),
                              "next_signal(): Signal %lu too large for provided byffer. Needed %lu bytes. Have %lu",
                              sub->sig_id(),
                              queue_[index(sub->sig_id())].data_size(),
                              data_size);
        exit(255);
    }

    returned_size = queue_[index(sub->sig_id())].data_size();

    memcpy(data, queue_[index(sub->sig_id())].data(), returned_size);

    // Bump signal id in preparation for the next signal.
    sub->set_sig_id(sub->sig_id() + 1);
    SIGFS_LOG_INDEX_DEBUG(sub->sub_id(), "next_signal(): Done.");

    return Result::ok;
}

