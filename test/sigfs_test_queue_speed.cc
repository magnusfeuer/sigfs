// Copyright (C) 2022, Magnus Feuer
// This program is licensed under the terms and conditions of the
// Mozilla Public License, version 2.0.  The full text of the
// Mozilla Public License is at https://www.mozilla.org/MPL/2.0/
//
// Author: Magnus Feuer (magnus@feuerworks.com)
//

//
// Speed test code for signal queue
//

#include <getopt.h>
#include "../queue_impl.hh"
#include "../subscriber.hh"
#include <string>
#include <iostream>
#include <memory.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <cstdlib>
#include <cassert>
#include <sys/time.h>
#include <sys/resource.h>
#include <thread>

int queue_length{131072};

void usage(const char* name)
{
    std::cout << "Usage: " << name << " [-p <number-of-publishers> | --publishers=<number-of-publishers>]" << std::endl;
    std::cout << "        [-s <number-of-subscribers> | --subscribers=<number-of-subscribers>]" << std::endl;
    std::cout << "        [-c <signal-count> | --count=<signal-count>]" << std::endl;
    std::cout << "        [-q <queue-length> | --queue-length=<queue-length>" << std::endl;
}



void publish_signal_sequence(std::shared_ptr<sigfs::Queue> queue, const int publish_id, int count)
{
    int sig_id{0};
    char buf[256];

    SIGFS_LOG_DEBUG("Called. Publishing %d signals", count);

    for(sig_id = 0; sig_id < count; ++sig_id) {
        *((int*) buf) = publish_id;
        *((int*) (buf + sizeof(int))) = sig_id;
        SIGFS_LOG_DEBUG("Publishing signal [%.3d][%.8d] (%.8X %.8X)",
                        *((int*) buf),
                        *((int*) (buf + sizeof(int))),
                        publish_id, sig_id);

        queue->queue_signal(buf, 2*sizeof(int));
    }
    SIGFS_LOG_DEBUG("Done. Published %d signals", count);
}


// Verify that we get a sequence of signals, possibly produced by more
// than one publisher.
//
// If publisher with prefix 1 and 2 send their own sequence
// of signals at the same time, they will be intermixed.
// This signal checks that both signal streams arrive uninterrupted
// and in the right order.
//
// 'test_id' is the test ID to use when logging
// 'sub' is the subscriber that reads the signal sequence.
// 'prefix' contains an array of pointers to signal prefixes ("A", "B", "thread-1", etc)
// 'count' is the number of signals to expect (starting with signal 0).
//
// Signals from each publisher are expected to have the format
// <prefix_id (4 bytes)><sequence_id (4 bytes)>

//
void check_signal_sequence(std::shared_ptr<sigfs::Queue> queue,
                           sigfs::Subscriber* sub,
                           const int* prefix_ids,
                           int prefix_count,
                           int count)
{
    int expect_sigid[prefix_count] = {};
    int ind = 0;

    sigfs::Queue::signal_callback_t<void*> cb =
        [&expect_sigid, &count, sub, ind, prefix_count, prefix_ids]
            (void* userdata,
             signal_id_t signal_id,
             const char* payload,
             std::uint32_t payload_size,
             signal_count_t lost_signals,
             signal_count_t remaining_signal_count) -> sigfs::Queue::cb_result_t {

            // Did we lose signals?
            if (lost_signals > 0) {
                printf("Lost %d signals after processing %d signals. Maybe increase with --queue-length=%d\n", lost_signals, ind, ((queue_length << 1) | 1) - 1);
                exit(0);
            }

            int prefix_ind = 0;

            // Find correct prefix
            for (prefix_ind = 0; prefix_ind < prefix_count; ++prefix_ind) {
                SIGFS_LOG_DEBUG("Checking payload first four bytes [%.8X] bytes against prefix [%.8X]",
                                *((int*)payload),
                                prefix_ids[prefix_ind]);

                if (*((int*)payload) == prefix_ids[prefix_ind])
                    break;
            }

            // Did we not recognize prefix?
            if (prefix_ind == prefix_count) {
                SIGFS_LOG_FATAL("No prefix matched first four payload bytes [%.8X]",
                                *((int*) payload));

                SIGFS_LOG_FATAL( "Available prefixes are:");

                for (prefix_ind = 0; prefix_ind < prefix_count; ++prefix_ind) {
                    SIGFS_LOG_FATAL("   [%.8X]",
                                    prefix_ids[prefix_ind]);
                }
                exit(1);
            }

            SIGFS_LOG_DEBUG("Comparing expected signal ID [%.3d][%.8d] with received [%.3d][%.8d]. %d signals left.",
                                  prefix_ids[prefix_ind], expect_sigid[prefix_ind],
                                  *((int*) payload),
                                  *((int*) (payload + sizeof(int))),
                                  remaining_signal_count);

            // Check that the rest of the signal payload after prefix matches expectations.
            if (*((int*) (payload + sizeof(int))) != expect_sigid[prefix_ind]) {
                SIGFS_LOG_FATAL(      "Expected signal ID [%.3d][%.8d], received [%.3d][%.8d]",
                                      prefix_ids[prefix_ind], expect_sigid[prefix_ind],
                                      *((int*) payload),
                                      *((int*) (payload + sizeof(int))));

                exit(1);
            }

            // Payload good. Increase next expected signal ID for the
            // given prefix
            expect_sigid[prefix_ind]++;
            count--;
            return sigfs::Queue::cb_result_t::processed_call_again;
        };

    while(count)
        queue->dequeue_signal<void*>(*sub, 0, cb);


    return;
}

int main(int argc,  char *const* argv)
{
    using namespace sigfs;

    int ch = 0;
    static struct option long_options[] =  {
        {"publishers", optional_argument, NULL, 'p'},
        {"subscribers", optional_argument, NULL, 's'},
        {"count", required_argument, NULL, 'c'},
        {"queue-length", optional_argument, NULL, 'q'},
        {NULL, 0, NULL, 0}
    };
    int signal_count{1000000};
    int nr_publishers{1};
    int nr_subscribers{1};

    // loop over all of the options
    while ((ch = getopt_long(argc, argv, "p:s:c:q:", long_options, NULL)) != -1) {
        // check to see if a single character or long option came through
        switch (ch)
        {
        case 'p':
            nr_publishers = atoi(optarg);
            break;

        case 's':
            nr_subscribers = atoi(optarg);
            break;

        case 'c':
            signal_count = std::atoi(optarg);
            break;

        case 'q':
            queue_length = std::atoi(optarg);
            break;

        default:
            usage(argv[0]);
            exit(255);
        }
    }

    if (sigfs_log_level_get() == SIGFS_LOG_LEVEL_NONE)
        sigfs_log_level_set(SIGFS_LOG_LEVEL_INFO);


    if (queue_length & (queue_length - 1)) {
        printf("queue-length %d is not a power of 2\n", queue_length);
        exit(255);
    }

    sigfs_log_set_start_time();

    // TEST 1.0
    // One signal published. One signal read
    //

    printf("queue-length: %d, nr-publishers: %d, nr-subscribers: %d, total-nr-signals: %d\n",
           queue_length, nr_publishers, nr_subscribers, signal_count * nr_publishers);

    std::shared_ptr<Queue> g_queue(std::make_shared<Queue>(queue_length));

    Subscriber *subs[nr_subscribers] = {};
    std::thread *sub_thr[nr_subscribers] = {};
    std::thread *pub_thr[nr_publishers] = {};
    int prefix_ids[nr_publishers];

    for (int i=0; i < nr_publishers; ++i) {
        prefix_ids[i] = i + 1;
    }

    //
    // Launch a bunch of subscriber threads.
    //
    for (int i=0; i < nr_subscribers; ++i) {
        subs[i] = new Subscriber(g_queue);
        sub_thr[i] = new std::thread(check_signal_sequence, g_queue, subs[i], (int*) prefix_ids, nr_publishers, signal_count * nr_publishers);
    }

    //
    // Launch a bunch of publisher threads
    //
    for (int i=0; i < nr_publishers; ++i) {
        pub_thr[i] = new std::thread (publish_signal_sequence, g_queue, i+1, signal_count);
    }


    for (int i=0; i < nr_publishers; ++i) {
        pub_thr[i]->join();
    }

    for (int i=0; i < nr_subscribers; ++i) {
        sub_thr[i]->join();
        delete subs[i];
    }

    auto done = sigfs_usec_since_start();
    printf("queue-length: %d, nr-publishers: %d, nr-subscribers: %d, signal-count: %d, execution-time: %ld usec, %.0f signals/sec, %f nsec/signal\n",
           queue_length, nr_publishers, nr_subscribers, signal_count * nr_publishers, done,
           (float) (signal_count*nr_publishers) / (float) (done / 1000000.0),
           (float) done*1000 / (float) (signal_count*nr_publishers));

    exit(0);
}
