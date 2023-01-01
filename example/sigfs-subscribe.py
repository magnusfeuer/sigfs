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
    print(f"Usage: {name} -f <signal-file>")
    print("-f <signal-file>  The signal file to open and read from")

if __name__ == "__main__":
    try:
        options, remainder = getopt.getopt(
            sys.argv[1:],
            'f:',
            ['file='])

    except getopt.GetoptError as err:
        print(err)
        usage(sys.argv[0])
        sys.exit(1)

    fname = False
    for opt, arg in options:
        if opt in ('-f', '--file'):
            fname = arg
        else:
            print("Unknown option: {}".format(opt))
            usage(sys.argv[0])
            sys.exit(255)

    if not fname:
        print("Please provide -f <signal-file>");
        usage(sys.argv[0])
        sys.exit(255)

    # The way we read data here is simple but slow.
    # Reading and processing larger chunks of data may speed things up.
    #
    # The format of the read data will be:
    # uint32: Number of signals lost since last read
    # uint64: Unique signal ID of the signal read.
    # uint32: Length of signal payload
    # data:   Payload
    with open(fname, "rb") as f:
        print("Ctrl-c to exit")
        while True:
            # Read signal header data
            data = f.read(4+8+4)

            # Unpack header
            (lost_signals, signal_id, payload_size) = struct.unpack("=IQI", data)

            # Read remaining payload
            payload = f.read(payload_size)

            print(f"lost-signals: {lost_signals}  signal-id: {signal_id}  payload-size: {payload_size}  payload: {payload.decode('ascii')}")



