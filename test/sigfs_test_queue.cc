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
#include "../sigfs.h"
#include "../sigfs_internal.hh"
#include "../log.h"
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

void usage(const char* name)
{
    std::cout << "Usage: " << name << " -d <data> | --data=<data>" << std::endl;
    std::cout << "        -f <file> | --file=<file>" << std::endl;
    std::cout << "        -c <signal-count> | --count=<signal-count>" << std::endl;
    std::cout << "        -s <usec> | --sleep=<usec>" << std::endl;
}

#define check_signal(prefix, sub, wanted_data, wanted_res, wanted_lost_signals) { \
    char buf[1024];                                                     \
    size_t res{0};                                                      \
    Queue::index_t lost_signals{0};                                     \
    memset(buf, 0, sizeof(buf));                                        \
    assert(g_queue->next_signal(sub, buf, sizeof(buf), res, lost_signals) == sigfs::Result::ok); \
    if (res != wanted_res) {                                            \
        SIGFS_LOG_FATAL("%s: Wanted result %lu. Got %lu", prefix, wanted_res, res); \
        g_queue->dump(prefix, sub);                                      \
        exit(1);                                                        \
    }                                                                   \
    if (memcmp(buf, wanted_data, res)) {                                \
        SIGFS_LOG_FATAL("%s: Wanted data [%-*s]. Got [%-*s]", prefix, (int) res, wanted_data, (int) res, buf); \
        g_queue->dump(prefix, sub);                                      \
        exit(1);                                                        \
    }                                                                   \
    if (lost_signals != wanted_lost_signals) {                          \
        SIGFS_LOG_FATAL("%s: Wanted lost signals %lu. Got %lu", prefix, wanted_lost_signals, lost_signals); \
        g_queue->dump(prefix, sub);                                     \
        exit(1);                                                        \
    }                                                                   \
}


void publish_signal_sequence(const char* test_id, sigfs::Queue* queue, const char* prefix, int count)
{
    int ind{0};
    char buf[256];
    int prefix_len {(int) strlen(prefix)};

    strcpy(buf, prefix);

    SIGFS_LOG_DEBUG("%s: Called. Publishing %d signals", test_id, count);

    for(ind = 0; ind < count; ++ind) {

        sprintf(buf + prefix_len, "%d", ind);
        assert(queue->queue_signal(buf, strlen(buf) + 1) == sigfs::Result::ok);
    }
    SIGFS_LOG_DEBUG("%s: Done. Published %d signals", test_id, count);
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
// ("%s%d", prefix, signal_id), where signal_id is 0..(count-1)
//
void check_signal_sequence(const char* test_id,
                           sigfs::Subscriber* sub,
                           const char* const*prefixes,
                           int prefix_count,
                           int count)
{
    int prefix_length[prefix_count];
    int expect_sigid[prefix_count];
    char payload[256];
    size_t res{0};
    sigfs::Queue::index_t lost_signals{0};
    int prefix_ind = 0;

    // Setup prefix length and expect_sigid
    prefix_ind = 0;
    while(prefix_ind < prefix_count) {
        prefix_length[prefix_ind] = strlen(prefixes[prefix_ind]);
        expect_sigid[prefix_ind] = 0;
        ++prefix_ind;
    }

    while(count--) {
        assert(sub->queue().next_signal(sub, payload, sizeof(payload), res, lost_signals) == sigfs::Result::ok);

        // Did we lose signals?
        if (lost_signals > 0) {
            SIGFS_LOG_INDEX_FATAL(sub->sub_id(), "%s: Lost %d signals,", test_id, lost_signals);
            exit(0);
        }

        // Find correct prefix
        for (prefix_ind = 0; prefix_ind < prefix_count; ++prefix_ind) {
            SIGFS_LOG_INDEX_DEBUG(sub->sub_id(),
                                  "%s: Checking payload [%-*s] first %d bytes against  prefix [%s]",
                                  test_id,
                                  (int) res-1, payload,
                                  prefix_length[prefix_ind],
                                  prefixes[prefix_ind]);

            if (!memcmp(payload, prefixes[prefix_ind], prefix_length[prefix_ind]))
                break;
        }

        // Did we not recognize prefix?
        if (prefix_ind == prefix_count) {
            SIGFS_LOG_INDEX_FATAL(sub->sub_id(),
                                  "%s: Did not recognize any prefix in received signal %-*s",
                                  test_id, (int) res-1, payload); 

            SIGFS_LOG_INDEX_FATAL(sub->sub_id(),  "%s: Available prefixes are:", test_id);

            for (prefix_ind = 0; prefix_ind < prefix_count; ++prefix_ind) {
                SIGFS_LOG_INDEX_FATAL(sub->sub_id(),
                                      "%s:   [%s]",
                                      test_id, prefixes[prefix_ind]);
            }
            exit(1);
        }

        SIGFS_LOG_INDEX_DEBUG(sub->sub_id(),
                              "%s: Comparing expected [%s][%d] with payload [%-*s]. Prefix length[%d]",
                              test_id, prefixes[prefix_ind],
                              expect_sigid[prefix_ind], (int) res-1, payload,prefix_length[prefix_ind]);

        // Check that the rest of the signal payload after prefix matches expectations.
        if (atoi(&payload[prefix_length[prefix_ind]]) != expect_sigid[prefix_ind]) {
            SIGFS_LOG_INDEX_FATAL(sub->sub_id(),
                                   "%s: Expected signal [%s%d], got [%-*s]",
                            test_id, prefixes[prefix_ind], expect_sigid[prefix_ind], (int) res-1, payload);
            exit(1);
        }

        // Payload good. Increase next expected signal ID for the
        // given prefix
        expect_sigid[prefix_ind]++;
    }

    return;
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



    //
    // We use a shared g_queu/sub1/sub2 scope for 1.0-1.4 since
    // the tests build on each other.
    //
    {
        // TEST 1.0
        // One signal published. One signal read
        //

        Queue* g_queue(new Queue(4));
        Subscriber *sub1{new Subscriber(*g_queue)};
        Subscriber *sub2{new Subscriber(*g_queue)};
        SIGFS_LOG_DEBUG("START: 1.0");
        assert(g_queue->queue_signal( "SIG000", 7) == sigfs::Result::ok);
        check_signal("1.0.1", sub1, "SIG000", 7, 0);
        SIGFS_LOG_INFO("PASS: 1.0");

        // Have second subscriber read signal
        check_signal("1.0.2", sub2, "SIG000", 7, 0);
        SIGFS_LOG_INFO("PASS: 1.0");


        // TEST 1.1
        // Two signals published, two signals read
        //
        SIGFS_LOG_DEBUG("START: 1.1");
        assert(g_queue->queue_signal("SIG001", 7) == sigfs::Result::ok);
        assert(g_queue->queue_signal("SIG002", 7) == sigfs::Result::ok);

        check_signal("1.1.1", sub1, "SIG001", 7, 0);
        check_signal("1.1.2", sub1, "SIG002", 7, 0);

        SIGFS_LOG_INFO("PASS: 1.1");

        // TEST 1.2
        // Test blocking functionality

        SIGFS_LOG_DEBUG("START: 1.2");

        // Check that sub1, which have read all signals up to and
        // including SIG002, would block.
        assert(g_queue->would_block(sub1));

        // Check that sub2, which still has SIG001 and SIG002 to read,
        // would not block
        //
        assert(!g_queue->would_block(sub2));
        check_signal("1.2.1", sub2, "SIG001", 7, 0);
        assert(!g_queue->would_block(sub2));
        check_signal("1.2.2", sub2, "SIG002", 7, 0);
        assert(g_queue->would_block(sub2));


        // TEST 1.3
        // Lost signals by overflowing the queue
        //
        SIGFS_LOG_DEBUG("START: 1.3");
        assert(g_queue->queue_signal("SIG003", 7) == sigfs::Result::ok);
        assert(g_queue->queue_signal("SIG004", 7) == sigfs::Result::ok);
        assert(g_queue->queue_signal("SIG005", 7) == sigfs::Result::ok);
        assert(g_queue->queue_signal("SIG006", 7) == sigfs::Result::ok);
        assert(g_queue->queue_signal("SIG007", 7) == sigfs::Result::ok);
        assert(g_queue->queue_signal("SIG008", 7) == sigfs::Result::ok);
        check_signal("1.3.1", sub1, "SIG006", 7, 3);
        check_signal("1.3.2", sub1, "SIG007", 7, 0);
        check_signal("1.3.3", sub1, "SIG008", 7, 0);
        SIGFS_LOG_INFO("PASS: 1.3");

        // TEST 1.4
        // Double overwrap of queue
        //
        SIGFS_LOG_DEBUG("START: 1.4");
        assert(g_queue->queue_signal("SIG009", 7) == sigfs::Result::ok);
        assert(g_queue->queue_signal("SIG010", 7) == sigfs::Result::ok);
        assert(g_queue->queue_signal("SIG011", 7) == sigfs::Result::ok);
        assert(g_queue->queue_signal("SIG012", 7) == sigfs::Result::ok);
        assert(g_queue->queue_signal("SIG013", 7) == sigfs::Result::ok);
        assert(g_queue->queue_signal("SIG014", 7) == sigfs::Result::ok);
        assert(g_queue->queue_signal("SIG015", 7) == sigfs::Result::ok);
        assert(g_queue->queue_signal("SIG016", 7) == sigfs::Result::ok);
        assert(g_queue->queue_signal("SIG017", 7) == sigfs::Result::ok);

        check_signal("1.4.1", sub1, "SIG015", 7, 6);
        check_signal("1.4.2", sub1, "SIG016", 7, 0);
        check_signal("1.4.3", sub1, "SIG017", 7, 0);
        SIGFS_LOG_INFO("PASS: 1.4");

        // 1.5
        //  Have second subscriber, who only read one signal, catch up
        //
        SIGFS_LOG_DEBUG("START: 1.5");
        check_signal("1.5.1", sub2, "SIG015", 7, 12);
        check_signal("1.5.2", sub2, "SIG016", 7, 0);
        check_signal("1.5.3", sub2, "SIG017", 7, 0);
        SIGFS_LOG_INFO("PASS: 1.5");

        //
        // Free memory for a clean valgrind run
        //
        delete g_queue;
        delete sub1;
        delete sub2;
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
        Queue* g_queue{new Queue(4096)};
        SIGFS_LOG_INFO("START: 2.0");

        // Create publisher thread A
        publish_signal_sequence("2.0.1", g_queue, "A", 1200);
        std::thread pub_thr_a (
            [g_queue]() {
                usleep(300000);
                publish_signal_sequence("2.0.1", g_queue, "A", 1200);
            });


        // Create publisher thread B
        std::thread pub_thr_b (
            [=]() {
                usleep(300000);
                publish_signal_sequence("2.0.2", g_queue, "B", 1200);
            });


        // Create the subscriber and check that all the combined 2400
        // signals can be read in sequence without loss by
        // the single subscriber.
        //
        const char* prefixes[]= { "A", "B" };
        Subscriber *sub1{new Subscriber(*g_queue)};
        check_signal_sequence("2.0.3", sub1, prefixes, 2, 2400);

        // Check that we got all signals and no more are ready to be read
        assert(g_queue->would_block(sub1));

        // Join threads.
        pub_thr_a.join();
        pub_thr_b.join();

        SIGFS_LOG_INFO("PASS: 2.0");

        delete sub1;
        delete g_queue;
    }

    // TEST 2.1 - Two publishers, three subscribers.
    //
    // Create two publisher threads that wait for a bit and
    // then pump two separate sequence of signals as fast as they can.
    // Have a single subscriber check for signal consistency
    {
        SIGFS_LOG_INFO("START: 2.1");
        Queue* g_queue(new Queue(1024));
        Subscriber* sub1(new Subscriber(*g_queue));
        Subscriber* sub2(new Subscriber(*g_queue));
        Subscriber* sub3(new Subscriber(*g_queue));

        // Create publisher thread a
        std::thread pub_thr_a (
            [g_queue]() {
                usleep(300000);
                publish_signal_sequence("2.0.1", g_queue, "A", 1000);
            });


        // Create publisher thread b
        std::thread pub_thr_b (
            [g_queue]() {
                usleep(300000);
                publish_signal_sequence("2.0.2", g_queue, "B", 1000);
            });


        const char* prefixes[]= { "A", "B" };
        // Create subscriber thread 1
        std::thread sub_thr_1 (
            [g_queue, sub1, prefixes]() {
                check_signal_sequence("2.0.3", sub1, prefixes, 2, 2000);
            });

        // Create subscriber thread 2
        std::thread sub_thr_2 (
            [g_queue, sub2, prefixes]() {
                check_signal_sequence("2.0.3", sub2, prefixes, 2, 2000);
            });

        // Create subscriber thread 3
        std::thread sub_thr_3 (
            [g_queue, sub3, prefixes]() {
                check_signal_sequence("2.0.3", sub3, prefixes, 2, 2000);
            });


        // Join threads.
        pub_thr_a.join();
        pub_thr_b.join();
        sub_thr_1.join();
        sub_thr_2.join();
        sub_thr_3.join();

        // Clean up for a clean valgrind run.
        delete sub1;
        delete sub2;
        delete g_queue;
    }
    exit(0);
}
