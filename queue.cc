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

#include "sigfs.hh"
#include "log.h"
using namespace sigfs;

Queue::Queue(const std::uint32_t queue_size):
    active_subscribers_(0),
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

#ifdef SIGFS_LOG
    index_t ind = 0;
    char suffix[512];
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

        SIGFS_LOG_DEBUG("%s: [%d] SigID[%lu] [%-*s]%s ", prefix, ind, queue_[ind].sig_id(), queue_[ind].signal()->data_size, queue_[ind].signal()->data, suffix);
        ++ind;
    }
#endif
}


Result Queue::queue_signal(const char* data, const size_t data_size)
{

    SIGFS_LOG_DEBUG("queue_signal(): Called");
    //
    // Do we have active subscribers?
    //

    //
    // Create a conditional wait that will wait for no active subscribers to
    // be calling next_signal() and a successful lock of mutex_()
    //
    /*
    {
        const Queue& self(*this);
        std::unique_lock prio_lock(prio_mutex_);
        SIGFS_LOG_DEBUG("queue_signal(): Prio lock acquired. ");

        prio_cond_.wait(prio_lock, [&self]
                                   {
                                       //
                                       // If there are active subscribers, or
                                       // we would fail to grab the lock, continue to wait.
                                       //
                                       if (self.active_subscribers_ || !self.mutex_.try_lock()) {
                                           return false;
                                       }

                                       return true;
                                   });
    }
    */
    //
    // No active subscribers are in next_signal, and mutex_.try_lock() gave
    // is the lock.
    //
    {
        std::unique_lock lock(mutex_);

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

        // Notify other queue_signal() callers waiting on conditional lock above
        cond_.notify_one();
    }

    return Result::ok;
}




const bool Queue::signal_available(const Subscriber* sub) const
{
    SIGFS_LOG_INDEX_DEBUG(sub->sub_id(), "signal_available(): Called");
    std::unique_lock lock(mutex_);

    if (head() == tail() || index(sub->sig_id()) == head()) {
        SIGFS_LOG_INDEX_DEBUG(sub->sub_id(),
                              "signal_available(): head{%u} %s tail{%u} --- index(sub->sig_id{%lu}){%u} %s head{%u} -> SIgnal not available.",
                              head(),
                              ((head() == tail())?"==":"!="),
                              tail(),

                              sub->sig_id(),
                              index(sub->sig_id()),
                              ((index(sub->sig_id()) == head())?"==":"!="),
                              head());
        return false;
    }

    SIGFS_LOG_INDEX_DEBUG(sub->sub_id(),
                          "signal_available(): head{%u} %s tail{%u} --- index(sub->sig_id{%lu}){%u} %s head{%u} -> Signal available.",
                          head(),
                          ((head() == tail())?"==":"!="),
                          tail(),

                          sub->sig_id(),
                          index(sub->sig_id()),
                          ((index(sub->sig_id()) == head())?"==":"!="),
                          head());
    return true;
}

