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
    std::cout << "Usage: " << name << "-f <file> | --file=<file> " << std::endl;
    std::cout << "            [-c <signal-count> | --count=<signal-count>] " << std::endl;
    std::cout << "            [-h | --hex]" << std::endl << std::endl;
    std::cout << "-f <file>         The signal file to subscribe from." << std::endl;
    std::cout << "-c <signal-count> The number of signals to read before exiting. Default: 0=infinite." << std::endl;
    std::cout << "-h                Print data in hex. Default is to print escaped strings." << std::endl;
}



char* escape_string(char* buf, uint32_t buf_sz, const char* data, uint32_t data_len)
{
    uint32_t offset = 0;
    while(data_len) {
        if (offset >= buf_sz) {
            printf("Buffer overflow when escaping string. Size %u. offset %u\n", buf_sz, offset);
            exit(1);
        }
        switch (*data) {
         case '\"':
            strcat(buf + offset, "\\\"");
            offset += 2;
            break;

         case '\\':
            strcat(buf + offset, "\\\\");
            offset += 2;
            break;
         case '\n':
            strcat(buf + offset, "\\n");
            offset += 2;
            break;

         case '\r':
            strcat(buf + offset, "\\r");
            offset += 2;
            break;

         case '\t':
            strcat(buf + offset, "\\t");
            offset += 2;
            break;

         default:
             if (!isprint(*data)) {
                 sprintf(buf + offset, "\\%.3d", (int) *data);
                 offset += 4;
             } else  {
                 buf[offset++]=*data;
             }
        }
        ++data;
        --data_len;
    }
    buf[offset]=0;
    return buf;
}

char* hex_string(char* buf, uint32_t buf_sz, const char* data, uint32_t len)
{
    uint32_t offset = 0;
    while(len) {
        if (offset >= buf_sz) {
            printf("Buffer overflow when escaping string. Size %u. offset %u\n", buf_sz, offset);
            exit(1);
        }
        sprintf(buf + offset, "%.2X", *data);
        offset += 1;
        ++data;
    }
    return buf;
}

int main(int argc,  char *const* argv)
{
    int ch = 0;
    static struct option long_options[] =  {
        {"file", required_argument, NULL, 'f'},
        {"count", optional_argument, NULL, 'c'},
        {"hex", optional_argument, NULL, 'h'},
        {NULL, 0, NULL, 0}
    };
    std::string file{""};
    bool hex{false};
    int count = 0;
    int fd{-1};
    // loop over all of the options
    while ((ch = getopt_long(argc, argv, "c:f:h", long_options, NULL)) != -1) {
        // check to see if a single character or long option came through
        switch (ch)
        {
        case 'f':
            file = optarg;
            break;

        case 'c':
            count = std::atoi(optarg);
            break;

        case 'h':
            hex=true;
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

    char buf[65536];
    char esc_buf[sizeof(buf)*4];
    int ind = 0;
    if (count)
        std::cout << "Reading " << count << " signals. Ctrl-c to abort" << std::endl;
    else
        std::cout << "Reading signals. Ctrl-c to abort" << std::endl;

    puts("id, signals-lost, size, data");

    while(!count || ind < count) {
        ssize_t read_res = read(fd, buf, sizeof(buf));
        ssize_t offset=0;
        if (read_res == -1) {
            std::cout << "Failed to read " << sizeof(buf) << " bytes from file " << file << ": " << strerror(errno) << std::endl;
            exit(255);
        }

        while(read_res) {
            sigfs_signal_t *sig((sigfs_signal_t*) (buf + offset));

            if (hex)
                printf("%lu, %u, %u, \"%s\"\n",
                       sig->signal_id,
                       sig->lost_signals,
                       sig->payload.payload_size,
                       hex_string(esc_buf,
                                  sizeof(esc_buf),
                                  sig->payload.payload,
                                  sig->payload.payload_size));
            else
                printf("%lu, %u, %u, \"%s\"\n",
                       sig->signal_id,
                       sig->lost_signals,
                       sig->payload.payload_size,
                       escape_string(esc_buf,
                                     sizeof(esc_buf),
                                     sig->payload.payload,
                                     sig->payload.payload_size));

            read_res -= SIGFS_SIGNAL_SIZE(sig);
            offset +=SIGFS_SIGNAL_SIZE(sig);
        }
        ind++;
    }

    close(fd);
    exit(0);
}
