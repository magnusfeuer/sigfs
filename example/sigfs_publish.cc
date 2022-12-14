// Copyright (C) 2022, Magnus Feuer
// This program is licensed under the terms and conditions of the
// Mozilla Public License, version 2.0.  The full text of the
// Mozilla Public License is at https://www.mozilla.org/MPL/2.0/
//
// Author: Magnus Feuer (magnus@feuerworks.com)
//

//
// Simple publisher that writes signal(s) to a sigfs file.
//
#include <getopt.h>
#include "sigfs_common.h"
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

void usage(const char* name)
{
    std::cout << "Usage: " << name << " -d <data> | --data=<data>" << std::endl;
    std::cout << "        -f <file> | --file=<file>" << std::endl;
    std::cout << "        -c <signal-count> | --count=<signal-count>" << std::endl;
    std::cout << "        -s <usec> | --sleep=<usec>" << std::endl;
    std::cout << "-f <file>         The signal file to publish to." << std::endl;
    std::cout << "-c <signal-count> How many signals to send." << std::endl;
    std::cout << "-s <usec>         How many microseconds to sleep between each send." << std::endl;
    std::cout << "-d <data>         Data to publish. \"%d\" will be replaced with counter." << std::endl;
    std::cout << "-h                Print data in hex. Default is to print escaped strings." << std::endl;
}

int main(int argc,  char *const* argv)
{
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

    //
    // loop over all of the options
    //
    fmt_string[0] = 0;
    while ((ch = getopt_long(argc, argv, "d:f:c:s:h", long_options, NULL)) != -1) {
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

    if (!fmt_string[0]) {
        std::cout << std::endl << "Missing argument: -d <data>" << std::endl << std::endl;
        usage(argv[0]);
        exit(255);
    }

    if (file.empty()) {
        std::cout << std::endl << "Missing argument: -f <file>" << std::endl << std::endl;
        usage(argv[0]);
        exit(255);
    }

    fd = open(file.c_str(), O_WRONLY);
    if (fd == -1) {
        std::cout << "Could not open " << file << " for writing: " << strerror(errno) << std::endl;
        exit(255);
    }

    // Create a buffer that can host our data string plus whatever number we put in
    char buf[sizeof(sigfs_payload_t) + strlen(fmt_string) + 100];
    sigfs_payload_t* payload = (sigfs_payload_t*) buf;

    int ind = 0;
    while(ind < count) {
        payload->payload_size = sprintf(payload->payload, fmt_string, ind + 1);

        ssize_t write_res = write(fd, payload, SIGFS_PAYLOAD_SIZE(payload));
        if (write_res == -1) {
            std::cout << "Failed to write " << strlen(buf) << " bytes to file " << file << ": " << strerror(errno) << std::endl;
            exit(255);
        }

        if (usec_sleep)
            usleep(usec_sleep);

        ++ind;
    }
    close(fd);
    exit(0);
}
