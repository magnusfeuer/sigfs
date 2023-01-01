#!/usr/bin/env python3
#
# (C) 2023 Magnus Feuer
# Licensed under Mozille Public License v2
#
import getopt
import sys
import os
import struct

def usage(name):
    print(f"Usage: {name} -f <signal-file> -d <data>")
    print("-f <signal-file>  The signal file to open and read from")
    print("-d <data>  The data to publish as signal payload")

if __name__ == "__main__":
    try:
        options, remainder = getopt.getopt(
            sys.argv[1:],
            'f:d:',
            ['file=', 'data='])

    except getopt.GetoptError as err:
        print(err)
        usage(sys.argv[0])
        sys.exit(1)

    fname = False
    data = False
    for opt, arg in options:
        if opt in ('-f', '--file'):
            fname = arg
        elif opt in ('-d', '--data'):
            data = arg
        else:
            print("Unknown option: {}".format(opt))
            usage(sys.argv[0])
            sys.exit(255)

    if not fname:
        print("Please provide -f <signal-file>");
        usage(sys.argv[0])
        sys.exit(255)

    if not data:
        print("Please provide -d <data>");
        usage(sys.argv[0])
        sys.exit(255)

    # The format of the written data is:
    # uint32: Length of signal payload
    # data:   Payload
    with open(fname, "wb") as f:

        # The pack method takes an integer "I" and then {len(data)}
        # bytes of character to add the payload as a string ("s").
        #
        bin_data = struct.pack(f"=I{len(data)}s", len(data), bytes(data, "ascii"))
        f.write(bin_data)


