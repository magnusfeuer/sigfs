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
#include "../log.h"
#include "../sigfs_common.h"

void usage(const char* name)
{
    printf("Usage: %s (-f file-name | --file-name file-name)\n", name);
    puts("        [-p number-of-publishers | --publishers=number-of-publishers]");
    puts("        [-s number-of-subscribers | --subscribers=number-of-subscribers]");
    puts("        [-P bytes | --payload-size=bytes]");
    puts("        [-c signal-count | --count=signal-count]");
    puts("        [-b batch_size | --batch-size=batch_size]\n");
    puts("-p number-of-publishers   How many parallel publisher threads to we start. Default: 1");
    puts("-s number-of-subscribers  How many parallel subscribers threads to we start. Default: 1");
    puts("-P payload-size           Number of bytes to send in each signal. Min: 8. Default: 8");
    puts("-c signal-count           How many signals to each publisher send. Default 1000000");
    puts("-b batch-size             How many signals do each publisher pack into a single write operation. Default: 1");
}

#ifdef SIGFS_LOG
static std::mutex mutex_;
static int next_log_ind = 0;
#endif

void publish_signal_sequence(const char* filename, const int publish_id, int count, int batch_size, size_t payload_size)
{
    int sig_id{0};
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
        exit(1);
    }

    SIGFS_LOG_INDEX_DEBUG(log_ind, "Called. Publishing %d signals to %s", count, filename);

    // Fill buffer with sequence data as dummy payload
    for(uint64_t i = 0; i < sizeof(buf); ++i)
        buf[i]=i%256;

    while(true) {
        int ind = 0;
        ssize_t tot_size = 0;
#ifdef SIGFS_LOG
        signal_id_t start_sig_id = sig_id;
#endif

        //
        // Create a batch of signals to send.
        //
        while(count && ind < batch_size) {
            sigfs_payload_t* payload = (sigfs_payload_t*) (buf + tot_size);

            payload->payload_size = payload_size;
            *((int*) payload->payload) = publish_id;
            *((int*) (payload->payload + sizeof(int))) = sig_id;
            SIGFS_LOG_INDEX_DEBUG(log_ind, "Publishing signal [%.3d][%.8d]", publish_id, sig_id);
            tot_size += SIGFS_PAYLOAD_SIZE(payload);
            ++ind;
            ++sig_id;
            --count;
        }

#ifdef SIGFS_LOG
        if (sigfs_log_level_get() == SIGFS_LOG_LEVEL_DEBUG) {
            for(int ind1 = 0; ind1 < tot_size; ++ind1) {
                char* ptr = &buf[ind1];

                if ((ind1 % 12) == 0)
                    SIGFS_LOG_INDEX_DEBUG(log_ind, "write[%d]  payload_len [%lu]", ind1, *((uint32_t*) ptr));

                if ((ind1 % 12) == 4)
                    SIGFS_LOG_INDEX_DEBUG(log_ind, "write[%d]    pub_id [%lu]", ind1, *((uint32_t*) ptr));

                if ((ind1 % 12) == 8)
                    SIGFS_LOG_INDEX_DEBUG(log_ind ,"write[%d]    sig_id[%lu]", ind1, *((uint32_t*) ptr));

                if ((ind1 % 8) == 8 && *((uint64_t*) ptr) > 10000000 ) {
                    SIGFS_LOG_INDEX_FATAL(log_ind, "SUCK IT");
                    exit(1);
                }
            }
        }
#endif

        ssize_t res = write(fd, buf, tot_size);
        if (res != tot_size) {
            SIGFS_LOG_INDEX_FATAL(log_ind, "Could not write %lu bytes to file %s. Got %lu bytes written: %s",
                                  tot_size, filename, strerror(errno), res);
            exit(1);
        }

        SIGFS_LOG_INDEX_DEBUG(log_ind, "Published %d signals [%.3d][%.8d]-[%.3d][%.8d]", ind, publish_id, start_sig_id, publish_id, sig_id-1);

        // Are we done?
        if (!count)
            break;
    }
    close(fd);
    SIGFS_LOG_INDEX_DEBUG(log_ind, "Done. Published %d signals to %s", count, filename);
}


// Verify that we get a sequence of signals, possibly produced by more
// than one publisher.
//
// If two publishers with publish_id 1 and 2 send their own sequence
// of signals at the same time, they will be intermixed.
// This function checks that both signal streams arrive uninterrupted
// and in the right order.
//
// 'filename' is the file to read signals from.
// 'pub_count' is the number of publisjhers.
// 'signal_count' is the number of signals to expect (starting with
//                signal 0) from each publisher.
//
// Signals from each publisher are expected to have the format
// <publish_id (4 bytes)><signal_id (4 bytes)>

//
void check_signal_sequence(const char* filename, int pub_count, int signal_count, size_t payload_size)
{
    int expect_sigid[pub_count] = {};
    int pub_id = 0;
    int sig_id = 0;
    int total_count = signal_count * pub_count;
    char buf[100*(sizeof(sigfs_signal_t)+payload_size)];

#ifdef SIGFS_LOG
    int log_ind{0};
    {
        std::lock_guard<std::mutex> lock(mutex_);
        log_ind = next_log_ind++;
    }
#endif
    SIGFS_LOG_INDEX_DEBUG(log_ind, "Validating %d signals from %s",
                          total_count, filename);
    int fd = open(filename, O_RDONLY);

    if (fd == -1) {
        SIGFS_LOG_INDEX_FATAL(log_ind, "Could not open file %s: %s",
                              filename, strerror(errno));
        exit(1);
    }

    char dbg[1024];
    while(total_count) {
        SIGFS_LOG_INDEX_DEBUG(log_ind, "Reading signal %d.",  signal_count * pub_count - total_count)
        ssize_t rd_res = read(fd, buf, sizeof(buf));
        ssize_t bytes_left = rd_res;
        ssize_t offset = 0;

        if (rd_res == -1) {
            SIGFS_LOG_INDEX_FATAL(log_ind, "Could not read from file %s: %s",
                                  filename, strerror(errno));
            exit(1);
        }

#ifdef SIGFS_LOG
        if (sigfs_log_level_get() == SIGFS_LOG_LEVEL_DEBUG)  {
            dbg[0] = 0;
            int len = 0;
            for(int ind1 = 0; ind1 < rd_res; ++ind1) {
                char* ptr = &buf[ind1];

                if ((ind1 % 24) == 0) {
                    SIGFS_LOG_INDEX_DEBUG(log_ind, dbg);
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

        // Traverse buf until we have processed all data.
        int single_read_signals_processed = 0;
        while(true) {
            sigfs_signal_t *sig = ((sigfs_signal_t*) (buf + offset));

            SIGFS_LOG_INDEX_DEBUG(log_ind, "%ld bytes to process.", bytes_left);
            if (bytes_left < (ssize_t) sizeof(sigfs_signal_t)) {
                SIGFS_LOG_INDEX_FATAL(log_ind, "Need at least %lu bytes for signal header, got %lu",
                                      sizeof(sigfs_signal_t), bytes_left);
                exit(-1);
            }

            if (bytes_left < (ssize_t) SIGFS_SIGNAL_SIZE(sig)) {
                SIGFS_LOG_INDEX_FATAL(log_ind, "Signal header + payload size is %lu, got %d bytes",
                                      bytes_left, sig->payload.payload_size);
                exit(1);
            }


            if (sig->payload.payload_size != payload_size) {
                SIGFS_LOG_INDEX_FATAL(log_ind, "Wanted payload size of %lu, got %d",
                                      payload_size, sig->payload.payload_size);
                exit(1);
            }

            if (sig->lost_signals > 0) {
                SIGFS_LOG_INDEX_FATAL(log_ind, "Lost %d signals after processing %d signals",
                                      sig->lost_signals, signal_count * pub_count - total_count);
                exit(1);
            }

            //
            // Pub id and sig_id are stored in the first 8 bytes iof payload.
            //
            pub_id = *((int*) sig->payload.payload);
            sig_id = *((int*) (sig->payload.payload + sizeof(int)));
            if (pub_id < 0 || pub_id >= pub_count) {
                SIGFS_LOG_INDEX_FATAL(log_ind, "Publisher id %d is out of range [0-%d]", pub_id, pub_count - 1);
                exit(1);
            }

            SIGFS_LOG_INDEX_DEBUG(log_ind, "SigID[%lu] - pub_id[%.3d] Comparing expected signal ID [%.8d] with received [%.8d]",
                                  sig->signal_id, pub_id, expect_sigid[pub_id], sig_id);

            // Check that the rest of the signal payload after prefix matches expectations.
            if (sig_id != expect_sigid[pub_id]) {
                SIGFS_LOG_INDEX_FATAL(log_ind, "pub_id[%.3d] Expected signal ID [%.8d], received [%.8d]. Payoad size[%d] sigfs_signal_id[%lu]. rd_res[%lu]. bytes_left[%lu] offset[%lu]",
                                      pub_id, expect_sigid[pub_id], sig_id, sig->payload.payload_size, sig->signal_id, rd_res, bytes_left, offset);

                int len = 0;
                for(int ind1 = 0; ind1 < rd_res; ++ind1) {
                    char* ptr = &buf[ind1];

                    if ((ind1 % 24) == 0) {
                        SIGFS_LOG_INDEX_FATAL(log_ind, dbg);
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



                exit(1);
            }

            //
            // Payload good. Increase next expected signal ID for the
            // given prefix
            //
            expect_sigid[pub_id]++;
            total_count--;
            offset += SIGFS_SIGNAL_SIZE(sig);
            bytes_left -= SIGFS_SIGNAL_SIZE(sig);
            single_read_signals_processed++;

            // SIGFS_LOG_INDEX_DEBUG(log_ind, "Offset: %lu  rd_res: %lu. bytes_left: %lu", offset, rd_res, bytes_left);
            if (!bytes_left || !total_count) {
                SIGFS_LOG_INDEX_DEBUG(log_ind, "Processed %d signals from %lu bytes read.",
                                      single_read_signals_processed, offset);
                break;
            }
        }
    };

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
        {NULL, 0, NULL, 0}
    };
    int signal_count{1000000};
    int nr_publishers{1};
    int nr_subscribers{1};
    int batch_size{1};
    char filename[256] = {};
    size_t payload_size{8};

    // loop over all of the options
    while ((ch = getopt_long(argc, argv, "p:s:c:f:b:P:", long_options, NULL)) != -1) {
        // check to see if a single character or long option came through
        switch (ch)
        {
        case 'f':
            strcpy(filename, optarg);
            break;
        case 'p':
            nr_publishers = atoi(optarg);
            break;

        case 's':
            nr_subscribers = atoi(optarg);
            break;

        case 'c':
            signal_count = std::atoi(optarg);
            break;

        case 'P':
            payload_size = std::atoi(optarg);
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

    //
    // Launch a bunch of subscriber threads.
    //
    std::unique_ptr<std::thread> sub_thr[nr_subscribers];
    std::unique_ptr<std::thread> pub_thr[nr_publishers];

    for (int i=0; i < nr_subscribers; ++i) {
        sub_thr[i] = std::make_unique<std::thread>(check_signal_sequence, filename, nr_publishers, signal_count, payload_size);
    }

    usleep(100000);
    //
    // Launch a bunch of publisher threads
    //
    sigfs_log_set_start_time();
    for (int i=0; i < nr_publishers; ++i) {
        pub_thr[i] = std::make_unique<std::thread>(publish_signal_sequence, filename, i, signal_count, batch_size, payload_size);
    }


    for (int i=0; i < nr_publishers; ++i) {
        pub_thr[i]->join();
    }

    for (int i=0; i < nr_subscribers; ++i) {
        sub_thr[i]->join();
    }

    auto done = sigfs_usec_since_start();


    printf("nr-publishers: %d, nr-subscribers: %d, total-signal-count: %d\n",
           nr_publishers,
           nr_subscribers,
           signal_count * nr_publishers);

    printf("payload size   usec/signal   signals/sec   mbyte/sec/subscriber   signals received\n");
    printf("%12lu %13.2f %13.0f %11.3f %18d\n",
           payload_size,
           (float) done / (float) (signal_count * nr_publishers),
           (float) (signal_count*nr_publishers) / (float) (done / 1000000.0),
           (float) (payload_size * signal_count * nr_publishers) / ((float) done / 1000000.0) / (float) nr_subscribers / (float) (1024*1024),
           signal_count * nr_publishers);

    // printf("nr-publishers: %d, nr-subscribers: %d, signal-count: %d, payload_size: %lu, execution-time: %ld usec, %.0f signals/sec, %f nsec/signal\n",
    //        nr_publishers,
    //        nr_subscribers,
    //        signal_count * nr_publishers,
    //        payload_size,
    //        done,
    //        (float) (signal_count*nr_publishers) / (float) (done / 1000000.0),
    //        (float) done*1000 / (float) (signal_count*nr_publishers));

    exit(0);
}
