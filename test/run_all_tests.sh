#!/usr/bin/env bash
#
# Run all test scripts.
#


# Thank you:
# https://stackoverflow.com/questions/59895/how-do-i-get-the-directory-where-a-bash-script-is-located-from-within-the-script
SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
SIGFS_EXE=${SCRIPT_DIR}/../sigfs

# Load commonly used functions
. ${SCRIPT_DIR}/sigfs_util.sh

# Test 1
# Check queue integrity
#
${SCRIPT_DIR}/sigfs_test_queue_integrity || exit 1


# Test 2
# Check permmssion system
#
${SCRIPT_DIR}/sigfs_test_access.sh || exit 1


# Test 3
# Check sigfs filesystem integrity with single 
#
#
TEST_TMP=/tmp/sigfs-test.${$}
rm -rf ${TEST_TMP}
mkdir -p ${TEST_TMP}/root

TEST_FILE=${TEST_TMP}/root/f1

CONFIG='{
    "root": {
        "name": "/",
        "entries": [
            { 
                "name": "f1",
                "uid_access": [  
                   { "uid": UID,  "access": [ "read", "write" ]  }
                ]
            }
        ]
    }
}'


# Launch sigfs with the given config file
# File system mount point is ${TEST_TMP}/root
#
launch_sigfs ${TEST_TMP} "$CONFIG"

SIGFS_LOG_LEVEL=6 ${SCRIPT_DIR}/sigfs_test_fuse -f ${TEST_FILE} -p1 -s1 -p8 -c1 -b1 -t "Single signal test" || exit 1
