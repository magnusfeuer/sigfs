// Copyright (C) 2022, Magnus Feuer
// This program is licensed under the terms and conditions of the
// Mozilla Public License, version 2.0.  The full text of the
// Mozilla Public License is at https://www.mozilla.org/MPL/2.0/
//
// Author: Magnus Feuer (magnus@feuerworks.com)
//

//
// Test code for signal queue.
//

#include <getopt.h>
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
#include <thread>
#include <sys/time.h>
#include <sys/resource.h>

#include "../queue_impl.hh"
void usage(const char* name)
{
    std::cout << "Usage: " << name << " -d <data> | --data=<data>" << std::endl;
    std::cout << "        -f <file> | --file=<file>" << std::endl;
    std::cout << "        -c <signal-count> | --count=<signal-count>" << std::endl;
    std::cout << "        -s <usec> | --sleep=<usec>" << std::endl;
}

void check_signal(sigfs::Queue& queue,
                  const char* prefix,
                  sigfs::Subscriber& sub,
                  const char* wanted_data,
                  int wanted_res,
                  int wanted_lost_signals)
{
    char buf[1024];

    memset(buf, 0, sizeof(buf));

    sigfs::Queue::signal_callback_t<void*> cb =
        [&queue, prefix, wanted_res, &sub, wanted_data, wanted_lost_signals]
        (void* userdata,
         signal_id_t signal_id,
         const char* payload,
         std::uint32_t payload_size,
         signal_count_t lost_signals,
         signal_count_t remaining_signal_count) -> sigfs::Queue::cb_result_t {
            if (!payload) {
                SIGFS_LOG_INDEX_FATAL(sub.sub_id(), "%s: Wanted %lu bytes. Got interrupted!", prefix, wanted_res);
                queue.dump(prefix, sub);
                exit(1);
            }

            if ((int) payload_size != wanted_res) {
                SIGFS_LOG_INDEX_FATAL(sub.sub_id(), "%s: Wanted %lu bytes. Got %lu bytes", prefix, wanted_res, payload_size);
                queue.dump(prefix, sub);
                exit(1);
            }

            if (memcmp(payload, wanted_data, payload_size)) {
                SIGFS_LOG_INDEX_FATAL(sub.sub_id(), "%s: Wanted data [%-*s]. Got [%-*s]", prefix, wanted_res, wanted_data, (int) payload_size, payload);
                queue.dump(prefix, sub);
                exit(1);
            }

            if ((int) lost_signals != wanted_lost_signals) {
                SIGFS_LOG_INDEX_FATAL(sub.sub_id(), "%s: Wanted lost signals %lu. Got %lu", prefix, wanted_lost_signals, lost_signals);
                queue.dump(prefix, sub);
                exit(1);
            }
            return sigfs::Queue::cb_result_t::processed_dont_call_again;

        };

    queue.dequeue_signal<void*>(sub, 0, cb);
}


// Verify that we get a sequence of signals, possibly produced by more
// than one publisher.
//
// If publisher with prefix "A" and "B" send their own sequence
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
void check_signal_sequence(const char* test_id,
                           sigfs::Subscriber& sub,
                           const int* prefix_ids,
                           int prefix_count,
                           int count)
{
    int expect_sigid[prefix_count] = {};

    while(count) {
        //
        // We do the callback since signal is a part of a locked context in queue.cc
        // Once this lambda returns, signal will be undefined.
        //
        sigfs::Queue::signal_callback_t<void*> cb =
            [test_id, &count, sub, prefix_count, prefix_ids, &expect_sigid]
            (void* x,
             signal_id_t signal_id,
             const char* payload,
             std::uint32_t payload_size,
             signal_count_t lost_signals,
             signal_count_t remaining_signal_count) -> sigfs::Queue::cb_result_t {

                int prefix_ind{0};
                (void) x;

                if (payload_size != 2*sizeof(int)) {
                    SIGFS_LOG_INDEX_FATAL(sub.sub_id(), "%s: Expected %d bytes, got %d bytes,",
                                          test_id, 2*sizeof(int), payload_size);
                    exit(0);
                }

                // Did we lose signals?
                if (lost_signals > 0) {
                    SIGFS_LOG_INDEX_FATAL(sub.sub_id(), "%s: Lost %d signals,", test_id, lost_signals);
                    exit(0);
                }

                // for(std::uint32_t ind = 0; ind < payload_size; ++ind) {
                //     SIGFS_LOG_INDEX_DEBUG(sub.sub_id(),
                //                           "%s: Byte %d: %.2X", test_id, ind, (char) payload[ind]);
                // }
                // Find correct prefix
                for (prefix_ind = 0; prefix_ind < prefix_count; ++prefix_ind) {
                    SIGFS_LOG_INDEX_DEBUG(sub.sub_id(),
                                          "%s: Checking payload first four bytes [%.8X] bytes against prefix [%.8X]",
                                          test_id,
                                          *((int*) payload),
                                          prefix_ids[prefix_ind]);

                    if (*((int*)payload) == prefix_ids[prefix_ind])
                        break;
                }

                // Did we not recognize prefix?
                if (prefix_ind == prefix_count) {
                    SIGFS_LOG_INDEX_FATAL(sub.sub_id(),
                                          "%s: No prefix matched first four payload bytes [%.8X]",
                                          test_id,
                                          *((int*) payload));

                    SIGFS_LOG_INDEX_FATAL(sub.sub_id(),  "%s: Available prefixes are:", test_id);

                    for (prefix_ind = 0; prefix_ind < prefix_count; ++prefix_ind) {
                        SIGFS_LOG_INDEX_FATAL(sub.sub_id(),
                                              "%s:   [%.8X]",
                                              test_id, prefix_ids[prefix_ind]);
                    }
                    exit(1);
                }

                SIGFS_LOG_INDEX_DEBUG(sub.sub_id(),
                                      "%s: Comparing expected signal ID [%.3d][%.8d] with received [%.3d][%.8d]. %d signals left",
                                      test_id,
                                      prefix_ids[prefix_ind], expect_sigid[prefix_ind],
                                      *((int*) payload),
                                      *((int*) (payload + sizeof(int))),
                                      remaining_signal_count);

                // Check that the rest of the signal payload after prefix matches expectations.
                if (*((int*) (payload + sizeof(int))) != expect_sigid[prefix_ind]) {
                    SIGFS_LOG_INDEX_FATAL(sub.sub_id(),
                                          "%s: Expected signal ID [%.3d][%.8d], received [%.3d][%.8d]",
                                          test_id, prefix_ids[prefix_ind], expect_sigid[prefix_ind],
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


        //
        // Do multiple callbacks.
        //
        sub.queue()->dequeue_signal<void*>(sub, (void*) 0, cb);

    }

    return;
}



void validate_signal(std::shared_ptr<sigfs::Queue> queue,
                     int sub_count,
                     const char* test_id,
                     const char* data,
                     int lost_signals)
{
    std::unique_ptr<std::thread> threads[sub_count];

    for (int ind=0; ind < sub_count; ++ind)
        threads[ind] = std::make_unique<std::thread> (
            [test_id, data, &queue, lost_signals]() {
                sigfs::Subscriber sub(queue);
                check_signal(*queue, test_id, sub, data, strlen(data) + 1, lost_signals);
            });

    // Wait for thread to fire up.
    // Non-dermenistic, but this is not a problem here.
    usleep(10000);
    queue->queue_signal(data, strlen(data)+1);

    for (int ind=0; ind < sub_count; ++ind)
        threads[ind]->join();
}

void publish_signal_sequence(const char* test_id, sigfs::Queue& queue, const int publish_id, int count)
{
    int sig_id{0};
    char buf[256];

    SIGFS_LOG_DEBUG("%s: Called. Publishing %d signals", test_id, count);

    for(sig_id = 0; sig_id < count; ++sig_id) {
        *((int*) buf) = publish_id;
        *((int*) (buf + sizeof(int))) = sig_id;
        SIGFS_LOG_DEBUG("%s: Publishing signal [%.3d][%.8d] (%.3d %.8d)",
                        test_id,
                        *((int*) buf),
                        *((int*) (buf + sizeof(int))),
                        publish_id, sig_id);

        queue.queue_signal(buf, 2*sizeof(int));
    }
    SIGFS_LOG_DEBUG("%s: Done. Published %d signals", test_id, count);
}


int main(int argc,  char *const* argv)
{
    using namespace sigfs;

    int ch = 0;
    static struct option long_options[] =  {
        {"data", required_argument, NULL, 'd'},
        {"file", required_argument, NULL, 'f'},
        {"count", optional_argument, NULL, 'c'},
        {"sleep", optional_argument, NULL, 's'},
        {NULL, 0, NULL, 0}
    };
    char fmt_string[2048];
    std::string file{""};
    int count{1};
    int usec_sleep{0};
    int fd{-1};
    // loop over all of the options
    fmt_string[0] = 0;
    while ((ch = getopt_long(argc, argv, "d:f:c:s:", long_options, NULL)) != -1) {
        // check to see if a single character or long option came through
        switch (ch)
        {
        case 'd':
            strcpy(fmt_string, optarg);
            break;

        case 'f':
            file = optarg;
            break;

        case 'c':
            count = std::atoi(optarg);
            break;

        case 's':
            usec_sleep = std::atoi(optarg);
            break;

        default:
            usage(argv[0]);
            exit(255);
        }
    }
    (void) count;
    (void) fd;
    (void) usec_sleep;

    // if (!fmt_string[0]) {
    //     std::cout << std::endl << "Missing argument: -d <data>" << std::endl << std::endl;
    //     usage(argv[0]);
    //     exit(255);
    // }

    // if (file.empty()) {
    //     std::cout << std::endl << "Missing argument: -f <file>" << std::endl << std::endl;
    //     usage(argv[0]);
    //     exit(255);
    // }

    if (sigfs_log_level_get() == SIGFS_LOG_LEVEL_NONE)
        sigfs_log_level_set(SIGFS_LOG_LEVEL_INFO);



    puts("Start");
    sigfs_log_set_start_time();

    //
    // We use a shared g_queu/sub1/sub2 scope for 1.0-1.4 since
    // the tests build on each other.
    //
    {
        // TEST 1.0
        // One signal published. One signal read
        //

        std::shared_ptr<Queue> g_queue(std::make_shared<Queue>(4));

        {
            SIGFS_LOG_DEBUG("START: 1.0");
            Subscriber sub(g_queue);
            g_queue->queue_signal("SIG001", 7);

            assert(g_queue->signal_available(sub));
            check_signal(*g_queue, "1.0.1", sub, "SIG001", 7, 0);
            assert(!g_queue->signal_available(sub));

            SIGFS_LOG_INFO("PASS: 1.0");
        }

        {

            // TEST 1.1
            // Two signals published, two signals read
            //
            SIGFS_LOG_DEBUG("START: 1.1");
            Subscriber sub1(g_queue);
            Subscriber sub2(g_queue);

            g_queue->queue_signal("SIG001", 7);
            g_queue->queue_signal("SIG002", 7);

            assert(g_queue->signal_available(sub1));
            check_signal(*g_queue, "1.1.1", sub1, "SIG001", 7, 0);

            assert(g_queue->signal_available(sub1));
            check_signal(*g_queue, "1.1.2", sub1, "SIG002", 7, 0);

            assert(!g_queue->signal_available(sub1));

            assert(g_queue->signal_available(sub2));
            check_signal(*g_queue, "1.1.1", sub2, "SIG001", 7, 0);

            assert(g_queue->signal_available(sub2));
            check_signal(*g_queue, "1.1.2", sub2, "SIG002", 7, 0);

            assert(!g_queue->signal_available(sub2));
            SIGFS_LOG_INFO("PASS: 1.1");


            // TEST 1.1
            // Add two more signals and ensure that we can read them.
            //
            SIGFS_LOG_DEBUG("START: 1.2");

            // Add two additional signals and make sure that we can read them.
            g_queue->queue_signal("SIG003", 7);
            g_queue->queue_signal("SIG004", 7);

            assert(g_queue->signal_available(sub1));
            check_signal(*g_queue, "1.2.1", sub1, "SIG003", 7, 0);

            assert(g_queue->signal_available(sub1));
            check_signal(*g_queue, "1.2.2", sub1, "SIG004", 7, 0);

            assert(!g_queue->signal_available(sub1));

            SIGFS_LOG_DEBUG("PASS: 1.2");
        }

        {
            // TEST 1.3
            // Lost signals by overwrapping the queue
            //
            SIGFS_LOG_DEBUG("START: 1.3");
            Subscriber sub(g_queue);
            g_queue->queue_signal("SIG005", 7);
            g_queue->queue_signal("SIG006", 7);
            g_queue->queue_signal("SIG007", 7);
            g_queue->queue_signal("SIG008", 7);
            g_queue->queue_signal("SIG009", 7);
            g_queue->queue_signal("SIG010", 7);
            // Only the last three signals will be available.

            check_signal(*g_queue, "1.3.1", sub, "SIG008", 7, 3);
            assert(g_queue->signal_available(sub));
            check_signal(*g_queue, "1.3.2", sub, "SIG009", 7, 0);
            assert(g_queue->signal_available(sub));
            check_signal(*g_queue, "1.3.3", sub, "SIG010", 7, 0);
            assert(!g_queue->signal_available(sub));
            SIGFS_LOG_INFO("PASS: 1.3");
        }
        {
            // TEST 1.4
            // Double overwrap of queue
            //
            SIGFS_LOG_DEBUG("START: 1.4");
            Subscriber sub1(g_queue);
            Subscriber sub2(g_queue);
            g_queue->queue_signal("SIG011", 7);
            g_queue->queue_signal("SIG012", 7);
            g_queue->queue_signal("SIG013", 7);
            g_queue->queue_signal("SIG014", 7);
            g_queue->queue_signal("SIG015", 7);
            g_queue->queue_signal("SIG016", 7);
            g_queue->queue_signal("SIG017", 7);

            // Check that we have the last three signals available.
            assert(g_queue->signal_available(sub1));
            check_signal(*g_queue, "1.4.1", sub1, "SIG015", 7, 4);
            assert(g_queue->signal_available(sub1));
            check_signal(*g_queue, "1.4.2", sub1, "SIG016", 7, 0);
            assert(g_queue->signal_available(sub1));
            check_signal(*g_queue, "1.4.3", sub1, "SIG017", 7, 0);
            assert(!g_queue->signal_available(sub1));
            SIGFS_LOG_INFO("PASS: 1.4");



            // 1.5
            //  Have second subscriber, who has read no signals,
            //  catch up even further after adding to more signals.
            //
            SIGFS_LOG_DEBUG("START: 1.5");
            g_queue->queue_signal("SIG018", 7);
            g_queue->queue_signal("SIG019", 7);

            check_signal(*g_queue, "1.5.1", sub2, "SIG017", 7, 6);
            check_signal(*g_queue, "1.5.2", sub2, "SIG018", 7, 0);
            check_signal(*g_queue, "1.5.3", sub2, "SIG019", 7, 0);
            SIGFS_LOG_INFO("PASS: 1.5");
        }
    }

    //
    // THREADED TESTS
    //

    //
    // Nori talar med en lagre rost. Had to listen more. Nothing strange with her.
    // Fully non-dramatic.
    //

    // TEST 2.0 - Two publishers, single subscriber.
    //
    // Create two publisher threads that wait for a bit and
    // then pump two separate sequence of signals as fast as they can.
    // Have a single subscriber check for signal consistency
    {
        //
        // Make queue length fairly small to ensure wrapping.
        //
        SIGFS_LOG_DEBUG("START: 2.0");
        std::shared_ptr<Queue> g_queue(std::make_shared<Queue>(2048));

        Subscriber sub1(g_queue);
        // // Create publisher thread A
        std::thread pub_thr_a (
            [&g_queue]() {
                publish_signal_sequence("2.0.1", *g_queue, 1, 1200);
            });


        // Create publisher thread B
        std::thread pub_thr_b (
            [&g_queue]() {
                publish_signal_sequence("2.0.2", *g_queue, 2, 1200);
            });


        // Create the subscriber and check that all the combined 2400
        // signals can be read in sequence without loss by
        // the single subscriber.
        //
        const int prefixes[]= { 1, 2 };
        check_signal_sequence("2.0.3", sub1, prefixes, 2, 2400);

        // Check that we got all signals and no more are ready to be read
        assert(!g_queue->signal_available(sub1));

        // Join threads.
        pub_thr_a.join();
        pub_thr_b.join();

        SIGFS_LOG_INFO("PASS: 2.0");
    }

    // TEST 2.1 - Two publishers, three subscribers.
    //
    // Create two publisher threads that wait for a bit and
    // then pump two separate sequence of signals as fast as they can.
    // Have a single subscriber check for signal consistency
    {
        std::shared_ptr<Queue> g_queue(std::make_shared<Queue>(131072));
        Subscriber sub1(g_queue);
        Subscriber sub2(g_queue);
        const int prefixes[]= { 1,2 };

        // Create subscriber thread 1
        std::thread sub_thr_1 (
            [&g_queue, &sub1, prefixes]() {
                check_signal_sequence("2.1.3", sub1, prefixes, 2, 200000);
            });

        // Create subscriber thread 2
        std::thread sub_thr_2 (
            [&g_queue, &sub2, prefixes]() {
                check_signal_sequence("2.1.4", sub2, prefixes, 2, 200000);
            });

        // Create publisher thread a
        std::thread pub_thr_a (
            [&g_queue]() {
                publish_signal_sequence("2.1.1", *g_queue, 1, 100000);
            });


        // Create publisher thread b
        std::thread pub_thr_b (
            [&g_queue]() {
                publish_signal_sequence("2.1.2", *g_queue, 2, 100000);
            });


        // Join threads.
        pub_thr_a.join();
        pub_thr_b.join();

        sub_thr_1.join();
        sub_thr_2.join();

        SIGFS_LOG_INFO("PASS: 2.1");
    }
    usec_timestamp_t done = sigfs_usec_since_start();
    printf("Done. Execution time: %ld microseconds\n", done);

    exit(0);
}
