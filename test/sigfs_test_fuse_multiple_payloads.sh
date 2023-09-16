#!/usr/bin/bash
#
# Do side-by-side speed comparison with https://github.com/magnusfeuer/dbus-signals
#

./sigfs_test_fuse -f ../x/f1 -b30 -p 1 -s 1 -c 10000 -P32
for payload_sz in 64 128 256 512 1024 2048 4096 8192 16384 32768
do
    ./sigfs_test_fuse -f ../x/f1 -b30 -p 1 -s 1 -c 10000 -P${payload_sz} | tail -1
done
