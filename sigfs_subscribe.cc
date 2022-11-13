// Copyright (C) 2022, Magnus Feuer
// This program is licensed under the terms and conditions of the
// Mozilla Public License, version 2.0.  The full text of the
// Mozilla Public License is at https://www.mozilla.org/MPL/2.0/
//
// Author: Magnus Feuer (magnus@feuerworks.com)
//

//
// Simple publisher that reads signal(s) from a sigfs file.
//

#include <getopt.h>
#include "sigfs.h"
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
    std::cout << "Usage: " << name << "-f <file> | --file=<file> " << std::endl;
    std::cout << "            [-c <signal-count> | --count=<signal-count>] " << std::endl;
    std::cout << "            [-c <signal-count> | --count=<signal-count>] " << std::endl;
}


int main(int argc,  char *const* argv)
{
    int ch = 0;
    static struct option long_options[] =  {
        {"file", required_argument, NULL, 'f'},
        {"count", optional_argument, NULL, 'c'},
        {NULL, 0, NULL, 0}
    };
    std::string file{""};
    int count = 0;
    int fd{-1};
    // loop over all of the options
    while ((ch = getopt_long(argc, argv, "c:f:", long_options, NULL)) != -1) {
        // check to see if a single character or long option came through
        switch (ch)
        {
        case 'f':
            file = optarg;
            break;

        case 'c':
            count = std::atoi(optarg);
            break;

        default:
            usage(argv[0]);
            exit(255);
        }
    }


    if (file.empty()) {
        std::cout << std::endl << "Missing argument: -f <file>" << std::endl << std::endl;
        usage(argv[0]);
        exit(255);
    }

    fd = open(file.c_str(), O_RDONLY);
    if (fd == -1) {
        std::cout << "Could not open " << file << " for reading: " << strerror(errno) << std::endl;
        exit(255);
    }

    char buf[sizeof(sigfs_signal_t) + 10240];
    sigfs_signal_t *sig((sigfs_signal_t*) buf);
    int ind = 0;
    if (count)
        std::cout << "Reading " << count << " signals. Ctrl-c to abort" << std::endl;
    else
        std::cout << "Reading signals. Ctrl-c to abort" << std::endl;

    while(!count || ind < count) {
        ssize_t read_res = read(fd, buf, sizeof(buf));

        if (read_res == -1) {
            std::cout << "Failed to read " << sizeof(buf) << " bytes from file " << file << ": " << strerror(errno) << std::endl;
            exit(255);
        }
        printf("Signal %d\n", ind);
        printf("  size:              %lu\n", read_res );
        printf("  sig->lost_signals: %u\n",  sig->lost_signals);
        printf("  sig->data_size:    %u\n",  sig->data_size);
        printf("  sig->buffer:       [%-*s]\n\n",  (int) sig->data_size, sig->data );
        ind++;
    }

    close(fd);
    exit(0);
}
