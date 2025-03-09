// Copyright (C) 2022, Magnus Feuer
// This program is licensed under the terms and conditions of the
// Mozilla Public License, version 2.0.  The full text of the
// Mozilla Public License is at https://www.mozilla.org/MPL/2.0/
//
// Author: Magnus Feuer (magnus@feuerworks.com)
//

//
// File system test of sigfs.
//
// TESTS TO ADD:
// Check that we get an error back if we provide a read buffer too small to read a pending signal.
// Test non blocking mode when it is implemented.
//

#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string>
#include <memory>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <cstdlib>
#include <cassert>
#include <sys/time.h>
#include <sys/resource.h>
#include <thread>
#include <mutex>
#include <poll.h>
#include "../log.h"
#include "../sigfs_common.h"

char* prog_name = 0;
char test_name[256] = {};
void fail(const char* reason)
{
    printf("%s: %s %s. Run debug version of %s with SIGFS_LOG_LEVEL=6 for details.\n", prog_name, test_name, reason, prog_name);
    printf("%s: %s: sigfs file system test - failed\n", prog_name, test_name);
    exit(1);
}

void usage(const char* name)
{
    printf("Usage: %s (-f file-name | --file-name file-name)\n", name);
    puts("        [-p number-of-publishers | --publishers=number-of-publishers]");
    puts("        [-s number-of-subscribers | --subscribers=number-of-subscribers]");
    puts("        [-P bytes | --payload-size=bytes]");
    puts("        [-c signal-count | --count=signal-count]");
    puts("        [-b batch_size | --batch-size=batch_size]\n");
    puts("        [-t test_name | --test-name=test-name]\n");
    puts("        [-u | --use-poll]\n");
    puts("-p number-of-publishers   How many parallel publisher threads to we start. Default: 1");
    puts("-s number-of-subscribers  How many parallel subscribers threads to we start. Default: 1");
    puts("-P payload-size           Number of bytes to send in each signal. Min: 8. Default: 8");
    puts("-c signal-count           How many signals to each publisher send. Default 1000000");
    puts("-b batch-size             How many signals do each publisher pack into a single write operation. Default: 1");
    puts("-t test-name              Label to print on test pass or fail. Default: \"unnamed test\"");
    puts("-u                        Use poll(2) to wait for a signal before reading it.");
}

#ifdef SIGFS_LOG
static std::mutex mutex_;
static int next_log_ind = 0;
#endif

typedef struct test_payload_t_ {
    uint32_t publisher_id;
    uint32_t sequence_nr;
} __attribute__((packed)) test_payload_t;

void publish_signal_sequence(const char* filename, const uint32_t publish_id, uint32_t count, uint32_t batch_size, size_t payload_size)
{
    uint32_t sequence_nr{0};
    char buf[(sizeof(sigfs_payload_t)+payload_size)*batch_size + 256];

#ifdef SIGFS_LOG
    int log_ind{0};
    {
        std::lock_guard<std::mutex> lock(mutex_);
        log_ind = next_log_ind++;
    }
#endif

    SIGFS_LOG_INDEX_DEBUG(log_ind, "Publishing %d signals to %s", count, filename);
    int fd = open(filename, O_WRONLY);

    if (fd == -1) {
        SIGFS_LOG_INDEX_FATAL(log_ind, "Could not open file %s: %s", filename, strerror(errno));
        fail("Could not open file");
    }

    SIGFS_LOG_INDEX_DEBUG(log_ind, "Called. Publishing %d signals to %s", count, filename);

    // Fill buffer with sequence data as dummy payload
    for(uint64_t i = 0; i < sizeof(buf); ++i)
        buf[i]=i%256;

    while(true) {
        uint32_t batch_nr = 0;
        ssize_t tot_size = 0;
#ifdef SIGFS_LOG
        uint32_t start_sequence_nr = sequence_nr;
#endif

        //
        // Create a batch of signals to send.
        //
        while(count && batch_nr < batch_size) {
            sigfs_payload_t* payload = (sigfs_payload_t*) (buf + tot_size);
            test_payload_t tst_payload {
                .publisher_id = publish_id,
                .sequence_nr = sequence_nr
            };
            payload->payload_size = payload_size;

            memcpy((char*) payload->payload, (char*) &tst_payload, sizeof(tst_payload));
            SIGFS_LOG_INDEX_DEBUG(log_ind, "Publishing signal pub_id[%.3u] seq_nr[%.8u]", publish_id, sequence_nr);
            tot_size += SIGFS_PAYLOAD_SIZE(payload);
            ++batch_nr;
            ++sequence_nr;
            --count;
        }

#ifdef SIGFS_LOG
        if (sigfs_log_level_get() == SIGFS_LOG_LEVEL_DEBUG) {
            ssize_t ind1 = 0;
            while(ind1 < tot_size) {
                sigfs_payload_t *payload = (sigfs_payload_t*) &buf[ind1];
                test_payload_t* test_payload = (test_payload_t*) payload->payload;

                SIGFS_LOG_INDEX_DEBUG(log_ind, "write[%d]  payload_len[%lu] pub_id[%u] seq_nr[%u]",
                                      ind1,
                                      payload->payload_size,
                                      test_payload->publisher_id,
                                      test_payload->sequence_nr);

                ind1 += sizeof(sigfs_payload_t) + payload->payload_size;
            }
        }
#endif

        ssize_t res = write(fd, buf, tot_size);
        if (res != tot_size) {
            SIGFS_LOG_INDEX_FATAL(log_ind, "Could not write %lu bytes to file %s. Got %lu bytes written: %s",
                                  tot_size, filename, strerror(errno), res);
            fail("Could not write to file");
        }

        SIGFS_LOG_INDEX_DEBUG(log_ind, "Published %d signals [%.3d][%.8d]-[%.3d][%.8d]", batch_nr, publish_id, start_sequence_nr, publish_id, sequence_nr-1);

        // Are we done?
        if (!count)
            break;
    }
    close(fd);
    SIGFS_LOG_INDEX_DEBUG(log_ind, "Done. Published %d signals to %s", count, filename);
}

int check_payload_integrity(char* buf, ssize_t buf_sz,
                            int pub_count,
                            int signals_processed,
                            int expected_sigid[/* pub_count */],
                            size_t payload_size,
                            int log_ind)
{
    ssize_t bytes_left = buf_sz;
    ssize_t offset = 0;
    int pub_id = 0;
    int sig_id = 0;
    int new_signals_processed = 0;

    char dbg[1024];

    while(bytes_left) {
        sigfs_signal_t *sig = ((sigfs_signal_t*) (buf + offset));

        SIGFS_LOG_INDEX_DEBUG(log_ind, "%ld bytes to validate.", bytes_left);
        if (bytes_left < (ssize_t) sizeof(sigfs_signal_t)) {
            SIGFS_LOG_INDEX_FATAL(log_ind, "Need at least %lu bytes for signal header, got %lu",
                                  sizeof(sigfs_signal_t), bytes_left);
            fail("Could not read signal header");
        }

        if (bytes_left < (ssize_t) SIGFS_SIGNAL_SIZE(sig)) {
            SIGFS_LOG_INDEX_FATAL(log_ind, "Signal header + payload size is %lu, got %d bytes",
                                  bytes_left, sig->payload.payload_size);
            fail("Could not read atomic signal");
        }


        if (sig->payload.payload_size != payload_size) {
            SIGFS_LOG_INDEX_FATAL(log_ind, "Wanted payload size of %lu, got %d",
                                  payload_size, sig->payload.payload_size);
            fail("Incorrect payload size");

        }

        if (sig->lost_signals > 0) {
            SIGFS_LOG_INDEX_FATAL(log_ind, "Lost %d signals after processing %d signals",
                                  sig->lost_signals, signals_processed + new_signals_processed);
            fail("Lost signals");
        }

        //
        // Pub id and sig_id are stored in the first 8 bytes iof payload.
        //
        pub_id = *((int*) sig->payload.payload);
        sig_id = *((int*) (sig->payload.payload + sizeof(int)));
        if (pub_id < 0 || pub_id >= pub_count) {
            SIGFS_LOG_INDEX_FATAL(log_ind, "Publisher id %d is out of range [0-%d]", pub_id, pub_count - 1);
            fail("Publisher out of range");
        }

        SIGFS_LOG_INDEX_DEBUG(log_ind, "SigID[%lu] - pub_id[%.3d] Comparing expected signal ID [%.8d] with received [%.8d]",
                              sig->signal_id, pub_id, expected_sigid[pub_id], sig_id);

        // Check that the rest of the signal payload after prefix matches expectations.
        if (sig_id != expected_sigid[pub_id]) {
            SIGFS_LOG_INDEX_FATAL(log_ind, "pub_id[%.3d] Expected signal ID [%.8d], received [%.8d]. Payoad size[%d] sigfs_signal_id[%lu]. rd_res[%lu]. bytes_left[%lu] offset[%lu]",
                                  pub_id, expected_sigid[pub_id], sig_id, sig->payload.payload_size, sig->signal_id, buf_sz, bytes_left, offset);

            int len = 0;
            for(int ind1 = 0; ind1 < buf_sz; ++ind1) {
                char* ptr = &buf[ind1];

                if ((ind1 % 24) == 0) {
                    SIGFS_LOG_INDEX_FATAL(log_ind, dbg);
                    // Start new line
                    len = sprintf(dbg, "read[%d]: ", ind1);
                }

                switch(ind1 % 24) {
                case 4:
                    len += sprintf(dbg + len, "signal_id[%lu] ", *((uint64_t*) ptr));
                    break;

                case 12:
                    len += sprintf(dbg + len, "payload_len[%u] ", *((uint32_t*) ptr));
                    break;

                case 16:
                    len += sprintf(dbg + len, "pub_id[%d] ", *((int32_t*) ptr));
                    break;
                case 20:
                    len += sprintf(dbg + len, "sig_id[%d]", *((int32_t*) ptr));
                    break;
                }
            }

            fail("Signal sequencing failure.");
        }

        //
        // Payload good. Increase next expected signal ID for the
        // given prefix
        //
        expected_sigid[pub_id]++;
        new_signals_processed++;
        offset += SIGFS_SIGNAL_SIZE(sig);
        bytes_left -= SIGFS_SIGNAL_SIZE(sig);

        // SIGFS_LOG_INDEX_DEBUG(log_ind, "Offset: %lu  rd_res: %lu. bytes_left: %lu", offset, rd_res, bytes_left);
    }
    return new_signals_processed;
}


// Verify that we get a sequence of signals, possibly produced by more
// than one publisher.
// Each created reader thread will call this function to read out signals
// published by the publisher threads.
//
// If two publishers with publish_id 1 and 2 send their own sequence
// of signals at the same time, they will be intermixed.
// This function checks that both signal streams arrive uninterrupted
// and in the right order.
//
// 'filename'          is the file to read signals from.
// 'publisher_count' is the number of publishers.
// 'signal_count'      is the number of signals to expect (starting with
//                     signal 0) from each publisher.
// 'payoad_size'       is the size of each signal's payload
//
// Signals from each publisher are expected to have the format
// <publish_id (4 bytes)><signal_id (4 bytes)>

//
void check_signal_sequence_thread(const char* filename,
                                  int publisher_count,
                                  int signal_count,
                                  size_t payload_size)
{
    int expected_sigid[publisher_count] = {};
    int total_signal_count = signal_count * publisher_count;
    int signals_processed = 0;
    char buf[100*(sizeof(sigfs_signal_t)+payload_size)];

    int log_ind{0};
#ifdef SIGFS_LOG
    {
        std::lock_guard<std::mutex> lock(mutex_);
        log_ind = next_log_ind++;
    }
#endif
    SIGFS_LOG_INDEX_DEBUG(log_ind, "Validating %d signals from %s",
                          total_signal_count, filename);
    int fd = open(filename, O_RDONLY);

    if (fd == -1) {
        SIGFS_LOG_INDEX_FATAL(log_ind, "Could not open file %s: %s",
                              filename, strerror(errno));
        fail("Could not open file");
    }

    while(signals_processed < total_signal_count) {
        SIGFS_LOG_INDEX_DEBUG(log_ind, "Reading %lu bytes signal %d.", sizeof(buf), signal_count * publisher_count - total_signal_count)
        ssize_t rd_res = read(fd, buf, sizeof(buf));

        if (rd_res == -1) {
            SIGFS_LOG_INDEX_FATAL(log_ind, "Could not read from file %s: %s",
                                  filename, strerror(errno));
            fail("Could not read file");
        }

#ifdef SIGFS_LOG
        if (sigfs_log_level_get() == SIGFS_LOG_LEVEL_DEBUG)  {
            for(ssize_t ind1 = 0; ind1 < rd_res; ) {
                sigfs_signal_t *signal = (sigfs_signal_t*) &buf[ind1];
                test_payload_t *payload = (test_payload_t*) signal->payload.payload;

                SIGFS_LOG_INDEX_DEBUG(log_ind, "read[%d]: lost_signals[%lu] signal_id[%lu] payload_size[%u] - publisher_id[%u] sequence_nr[%u]",
                                      ind1,
                                      signal->lost_signals,
                                      signal->signal_id,
                                      signal->payload.payload_size,
                                      payload->publisher_id,
                                      payload->sequence_nr);

                ind1 += sizeof(sigfs_signal_t) + signal->payload.payload_size;
            }
        }
#endif

        signals_processed += check_payload_integrity(buf, rd_res,
                                                     publisher_count,
                                                     signals_processed,
                                                     expected_sigid,
                                                     payload_size,
                                                     log_ind);
    };

    return;
}


// Verify that we get a sequence of signals, possibly produced by more
// than one publisher.
// Each created reader thread will call this function to read out signals
// published by the publisher threads.
//
// If two publishers with publish_id 1 and 2 send their own sequence
// of signals at the same time, they will be intermixed.
// This function checks that both signal streams arrive uninterrupted
// and in the right order.
//
// 'filename'           is the file to read signals from.
// 'publisher_count'  is the number of publishers.
// 'reader_count' is the number of publishers
// 'signal_count'       is the number of signals to expect (starting with
//                      signal 0) from each publisher.
// 'payoad_size'        is the size of each signal's payload
//
// 'reader_count' will indicate how many times this function
// will open 'filename'.
//
// Each file descriptore from the opened file will be fed into poll to
// test the polled funcionality.
//
// When poll returns, we will read signals from all read-ready
// descriptors and verify the integrity of the received signals.
//
// Signals from each publisher are expected to have the format
// <publish_id (4 bytes)><signal_id (4 bytes)>

//
void check_signal_sequence_poll(const char* filename,
                                const int publisher_count,
                                const int reader_count,
                                const int signal_count,
                                const size_t payload_size)
{
    //
    // How many signals do we expect to process from all publishers across all subscribers.
    //
    int total_signal_count = signal_count * publisher_count * reader_count;
    int total_signals_left = total_signal_count;
    struct pollfd pfd[reader_count];

    // Signals left on a per-subscriber basis.
    struct {
        int signals_processed;

        // For each publisher read by this subscriber, what is the
        // expected next signal id?
        int *expected_sigid = 0;
    } readers[reader_count];

    SIGFS_LOG_DEBUG("Validating %d signals from %s", total_signal_count, filename);

    int i = reader_count;

    //
    // Open the single file 'reader_count' times and initialize a
    // poll set. Also store the number of signals left for each file descriptor.
    //
    while(i--) {
        pfd[i].fd = open(filename, O_RDONLY);

        if (pfd[i].fd == -1) {
            SIGFS_LOG_FATAL("Could not open file %s: %s", filename, strerror(errno));
            fail("Could not open file");
        }

        pfd[i].events = POLLIN | POLLOUT;
        pfd[i].revents = 0;

        readers[i].signals_processed = 0;

        // Initialize the array of nexst signal id expected from each
        // publisher to 0.
        //
        readers[i].expected_sigid = new int[publisher_count];
    }

    char dbg[1024];
    char buf[100*(sizeof(sigfs_signal_t)+payload_size)];


    while(total_signals_left > 0) {
        SIGFS_LOG_DEBUG("Polling on %d descriptors", reader_count);

        int ready_cnt = poll(pfd, reader_count, 100);

        // Timeout?
        if (!ready_cnt)
            fail("Time out after 100 msec at poll()");

        // Start at random place to read poll vector so that
        // we don't always start at the lowest descriptor.
        //
        int start_pos = rand() % reader_count;
        int cur_pos = start_pos;

        //
        // Read as many signals as we can for the subscriber sat cur_pos.
        //
        do {
            int signals_processed = 0;
            ssize_t rd_res = 0;

            // Not ready? Move to the next descriptor.
            if (pfd[cur_pos].revents == 0)
                goto next_reader;


            // Did we get anything else than POLLIN
            if (pfd[cur_pos].revents != POLLIN) {
                SIGFS_LOG_FATAL("poll return event for descriptor %d was not POLLIN: 0x%.4X",
                                pfd[cur_pos].fd, pfd[cur_pos].revents);
                fail("Poll returned error");
            }

            // pfd[cur_pos].fd can be read.
            rd_res = read(pfd[cur_pos].fd, buf, sizeof(buf));
            if (rd_res == -1) {
                SIGFS_LOG_FATAL("Could not read from file %s. descriptor %sd: %s",
                                filename, pfd[cur_pos].fd, strerror(errno));
                fail("Could not read file");
            }

#ifdef SIGFS_LOG
            if (sigfs_log_level_get() == SIGFS_LOG_LEVEL_DEBUG)  {
                dbg[0] = 0;
                int len = 0;
                for(int ind1 = 0; ind1 < rd_res; ++ind1) {
                    char* ptr = &buf[ind1];

                    if ((ind1 % 24) == 0) {
                        SIGFS_LOG_DEBUG(dbg);
                        // Start new line
                        len = sprintf(dbg, "read[%d]: ", ind1);
                    }

                    switch(ind1 % 24) {
                    case 4:
                        len += sprintf(dbg + len, "signal_id[%lu]", *((uint64_t*) ptr));
                        break;

                    case 12:
                        len += sprintf(dbg + len, "payload_len[%u] ", *((uint32_t*) ptr));
                        break;

                    case 16:
                        len += sprintf(dbg + len, "pub_id[%d] ", *((int32_t*) ptr));
                        break;
                    case 20:
                        len += sprintf(dbg + len, "sig_id[%d]", *((int32_t*) ptr));
                        break;
                    }
                }
            }
#endif
            signals_processed = check_payload_integrity(buf, rd_res,
                                                        publisher_count,
                                                        readers[cur_pos].signals_processed,
                                                        readers[cur_pos].expected_sigid,
                                                        payload_size,
                                                        SIGFS_NIL_INDEX);

            // Book keeping.
            readers[cur_pos].signals_processed += signals_processed;
            total_signals_left -= signals_processed;

            // Reset returned events.
            pfd[cur_pos].revents = 0;

        next_reader:
            cur_pos++;
            cur_pos %= reader_count;
        } while(cur_pos != start_pos);
    }
    return;
}


int main(int argc,  char *const* argv)
{
    int ch = 0;
    static struct option long_options[] =  {
        {"file-name", required_argument, NULL, 'f'},
        {"publishers", optional_argument, NULL, 'p'},
        {"subscribers", optional_argument, NULL, 's'},
        {"count", required_argument, NULL, 'c'},
        {"batch-size", optional_argument, NULL, 'b'},
        {"test-name", optional_argument, NULL, 't'},
        {"payload-size", optional_argument, NULL, 'P'},
        {"use-poll", optional_argument, NULL, 'u'},
        {NULL, 0, NULL, 0}
    };

    int signal_count{1000000};
    int publisher_count{1};
    int subscriber_count{1};
    int batch_size{1};
    char filename[256] = {};
    size_t payload_size{8};
    bool use_poll{false};

    prog_name = argv[0];

    strcpy(test_name, "unnamed test");
    // loop over all of the options
    while ((ch = getopt_long(argc, argv, "p:s:c:f:b:P:t:u", long_options, NULL)) != -1) {
        // check to see if a single character or long option came through
        switch (ch)
        {
        case 'f':
            strcpy(filename, optarg);
            break;
        case 't':
            strcpy(test_name, optarg);
            break;
        case 'p':
            publisher_count = atoi(optarg);
            break;

        case 's':
            subscriber_count = atoi(optarg);
            break;

        case 'c':
            signal_count = std::atoi(optarg);
            break;

        case 'P':
            payload_size = std::atoi(optarg);
            break;

        case 'u':
            use_poll = true;
            break;

        case 'b':
            batch_size = std::atoi(optarg);
            break;

        default:
            usage(argv[0]);
            exit(255);
        }
    }


    if (!filename[0]) {
        printf("Missing argument -f <file-name> | --file-name=<file-name>\n");
        usage(argv[0]);
        exit(1);
    }

    if (payload_size < 8) {
        printf("payload size must be at least 8 bytes.\n");
        usage(argv[0]);
        exit(1);
    }

    sigfs_log_set_start_time();
    SIGFS_LOG_INFO("nr-publishers: %d, nr-subscribers: %d, total-signal-count: %d",
                   publisher_count,
                   subscriber_count,
                   signal_count * publisher_count);


    srand(time(0));
    //
    // Launch a bunch of subscriber threads.
    //
    std::unique_ptr<std::thread> sub_thr[subscriber_count];
    std::unique_ptr<std::thread> pub_thr[publisher_count];

    //
    // Are the readers going to get  their own thread or used poll mode?
    //
    if (!use_poll) {
        for (int i=0; i < subscriber_count; ++i) {
            sub_thr[i] = std::make_unique<std::thread>(check_signal_sequence_thread,
                                                       filename,
                                                       publisher_count,
                                                       signal_count,
                                                       payload_size);
        }
    }
    else {
        sub_thr[0] = std::make_unique<std::thread>(check_signal_sequence_poll,
                                                   filename,
                                                   publisher_count,
                                                   subscriber_count,
                                                   signal_count,
                                                   payload_size);
        // Only one thread to join below.
        subscriber_count = 1;
    }
    //
    // Launch a bunch of publisher threads
    //
    usleep(200000);
    for (int i=0; i < publisher_count; ++i) {
        pub_thr[i] = std::make_unique<std::thread>(publish_signal_sequence, filename, i, signal_count, batch_size, payload_size);
    }


    for (int i=0; i < publisher_count; ++i) {
        pub_thr[i]->join();
    }

    for (int i=0; i < subscriber_count; ++i) {
        sub_thr[i]->join();
    }

#ifdef SIGFS_LOG
    auto done = sigfs_usec_since_start();
#endif

    SIGFS_LOG_INFO("nr-publishers: %d, nr-subscribers: %d, total-signal-count: %d",
                   publisher_count,
                   subscriber_count,
                   signal_count * publisher_count);

    SIGFS_LOG_INFO("payload size   usec/signal   signals/sec   mbyte/sec/subscriber   signals received");
    SIGFS_LOG_INFO("%12lu %13.2f %13.0f %11.3f %18d",
                   payload_size,
                   (float) done / (float) (signal_count * publisher_count),
                   (float) (signal_count*publisher_count) / (float) (done / 1000000.0),
                   (float) (payload_size * signal_count * publisher_count) / ((float) done / 1000000.0) / (float) subscriber_count / (float) (1024*1024),
                   signal_count * publisher_count);

    // printf("nr-publishers: %d, nr-subscribers: %d, signal-count: %d, payload_size: %lu, execution-time: %ld usec, %.0f signals/sec, %f nsec/signal\n",
    //        publisher_count,
    //        subscriber_count,
    //        signal_count * publisher_count,
    //        payload_size,
    //        done,
    //        (float) (signal_count*publisher_count) / (float) (done / 1000000.0),
    //        (float) done*1000 / (float) (signal_count*publisher_count));


    printf("%s: sigfs filesystem test - passed\n", prog_name);
    exit(0);
}
