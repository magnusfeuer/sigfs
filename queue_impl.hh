// Copyright (C) 2022, Magnus Feuer
// This program is licensed under the terms and conditions of the
// Mozilla Public License, version 2.0.  The full text of the
// Mozilla Public License is at https://www.mozilla.org/MPL/2.0/
//
// Author: Magnus Feuer (magnus@feuerworks.com)
//

#ifndef _SIGFS_QUEUE_IMPL__
#define _SIGFS_QUEUE_IMPL__
#include "log.h"
#include "queue.hh"
#include "subscriber.hh"

namespace sigfs {
    template<typename CallbackT>

    // Return true if we were not interrupted.
    // Returnf alse if we were interrupted.
    bool Queue::dequeue_signal(Subscriber& sub,
                               CallbackT userdata,
                               signal_callback_t<CallbackT>& cb) const
    {

        SIGFS_LOG_INDEX_DEBUG(sub.sub_id(), "dequeue_signal(): Called", sub.sig_id());

        // Have we lost signals?
        // We have the sig_id of currently stored signal in queue[index(sub->sig_id())].sig_id()
        // We havne the ID of the next signal to process is in sub->sig_id()
        // If the current id < next id then we are waiting for a new signal to fill the slot
        // If the current id == next_id, then the signal is ready to read
        // If the current id > next_id then we have lost signals.
        //
        signal_count_t lost_signal_count = 0;

        /*
          {
          std::unique_lock prio_lock(prio_mutex_);
          SIGFS_LOG_INDEX_DEBUG(sub->sub_id(), "dequeue_signal(): Prio lock acquired", sub->sig_id());
          active_subscribers_++;
          }
        */
        //
        // Wait for a signal to become ready.
        //

        const Queue& self(*this);
        auto check =
            [&self, &sub] {

                //
                // Are we interrupted?
                //
                if (sub.is_interrupted())
                    return true;

                //
                // If current id < next id, then we are still waiting for
                // a new signal to arrive. Continue waiting
                //
                if (self.head() == self.tail() || self.queue_[self.index(sub.sig_id())].sig_id() < sub.sig_id()) {
                    // SIGFS_LOG_INDEX_DEBUG(sub.sub_id(),
                    //                       "dequeue_signal(): head(%lu) %s tail(%lu) --- self.queue_[self.index(sub.sig_id(%u))].sig_id(%lu) - sub.sig_id(%lu) = %ld -> Do not exit wait",
                    //                       self.head(),
                    //                       ((self.head() == self.tail())?"==":"!="),
                    //                       self.tail(),
                    //                       sub.sig_id(),
                    //                       self.queue_[self.index(sub.sig_id())].sig_id(),
                    //                       sub.sig_id(),
                    //                       (self.queue_[self.index(sub.sig_id())].sig_id() - sub.sig_id()));
                    return false;
                }

                // We either have:
                // current id == next_id (Signal ready)
                //   or
                // current_id > next_id (Signals lost)
                //
                // In either case, exit conditional wait.
                //
                // SIGFS_LOG_INDEX_DEBUG(sub.sub_id(),
                //                       "dequeue_signal(): head(%lu) %s tail(%lu) --- self.queue_[self.index(sub.sig_id(%u))].sig_id(%lu) - sub.sig_id(%lu) = %ld -> Exit wait",
                //                       self.head(),
                //                       ((self.head() == self.tail())?"==":"!="),
                //                       self.tail(),
                //                       sub.sig_id(),
                //                       self.queue_[self.index(sub.sig_id())].sig_id(),
                //                       sub.sig_id(),
                //                       (self.queue_[self.index(sub.sig_id())].sig_id() - sub.sig_id()));

                return true;
            };

        {
            std::unique_lock<std::mutex> lock(mutex_);
            SIGFS_LOG_INDEX_DEBUG(sub.sub_id(), "dequeue_signal(): Lock acquired", sub.sig_id());

            // Wait for condition to be fulfilled.
            cond_.wait(lock, check);

            // Were we interrupted?
            if (sub.is_interrupted()) {
                (void) cb( userdata, 0, 0, 0, 0, 0);
                return false;
            }

            //
            // If the ID of the oldest signal in the queue (tail()) is
            // greater than the next signal we expect to read (sub.sig_id()), then
            // we have lost signals
            //
            // Update subscriber's sig_id to the oldest signal in the queue.
            //
            if (self.queue_[tail()].sig_id() > sub.sig_id()) {
                SIGFS_LOG_INDEX_DEBUG(sub.sub_id(),
                                      "dequeue_signal(): Tail catchup for [%lu] lost signals [%lu]->[%lu]",
                                      queue_[tail()].sig_id() - sub.sig_id(),
                                      sub.sig_id(),
                                      queue_[tail()].sig_id());
                //
                // sub.sig_id() is for the next signal we are expecting.
                // queue_[tail()].sig_id() is the signal currently stored in the queue slot
                // we are about to read the signal from.
                //
                lost_signal_count = self.queue_[tail()].sig_id() - sub.sig_id();
                sub.set_sig_id(tail_sig_id_());
            }

            while(true) {
                //
                // We do the callback since signal is protected by mutex_.
                // Once this callback returns, the mutex will unlock when lock is detroyed
                // and the signal provded as an argument becomes unprotected.
                //
                // There seems to be no easy way to keep the mutex_ locked
                // after a unique_lock() is detroyed, which is what we would need
                // if we were to continue to have mutex_ locked after the conditional
                // wait cycle is over.
                //
                // We could do recursive locks and lock it one extra time, but that
                // is a slower mutex and implementation in a very critical code path.
                //
                SIGFS_LOG_INDEX_DEBUG(sub.sub_id(), "dequeue_signal(): Doing callback with %lu bytes.",
                                      queue_[index(sub.sig_id())].payload()->payload_size);
                cb_result_t cb_res = cb( userdata,
                                         sub.sig_id(),
                                         queue_[index(sub.sig_id())].payload()->payload,
                                         queue_[index(sub.sig_id())].payload()->payload_size,
                                         lost_signal_count,
                                         self.next_sig_id_ - sub.sig_id() - 1);


                //
                // Did we successfully process the signal?
                //
                if (cb_res != cb_result_t::not_processed) {
                    sub.set_sig_id(sub.sig_id() + 1);
                }

                //
                // Do we process more signals or do we break out of the loop?
                //
                if (cb_res != cb_result_t::processed_call_again ||
                    !signal_available_(sub)) {
                    break;
                }
            }
        }
        //
        // Decrease active subscribers and check if there are no other subscribers waiting.
        // If that is the case, signal all threads waiting in queue_signal() that they
        // may proceed.
        //
        /*
          {
          std::unique_lock prio_lock(prio_mutex_);
          active_subscribers_--;

          // No more active subscribers.
          // Ping any queue_signal() callers to get them going
          if (!active_subscribers_) {
          prio_cond_.notify_one();
          }
          }
        */
        return true; // Not interrupted.
    }
}
#endif // __SIGFS_QUEUE__
