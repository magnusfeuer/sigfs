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


