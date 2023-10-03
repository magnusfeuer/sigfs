#!/usr/bin/env bash
#

TEST_TMP=/tmp/sigfs-test.${$}

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
SIGFS_EXE=${SCRIPT_DIR}/../sigfs

. ${SCRIPT_DIR}/sigfs_util.sh

# $1      Test name
# $2      Test file path
# $3      Expected test file permission string '-rw-------'
# $4      JSON config string
# $5..$# - Substitute patterns: 'label|replacement'
#
test_file_access()
{
    local TEST_NAME="$1"
    shift
    local TEST_FILE="$1"
    shift
    local EXPECTED_PERMISSIONS="$1"
    shift
    local CFG="$1"
    shift

    launch_sigfs ${TEST_TMP} "$CFG" "$@"
    local STAT=$(stat --printf='%A' ${TEST_FILE}) 
    
    # Kill the sigfs process
    kill -1 ${SIGFS_PID}
    wait $SIGFS_PID

    if [ "$STAT" != "$EXPECTED_PERMISSIONS" ]
    then
        echo "${TEST_FILE} permissions ${STAT} did not match expected ${EXPECTED_PERMISSIONS}"
        echo "$0: $TEST_NAME - failed"
        exit 1
    fi
    echo "$0: $TEST_NAME - passed"
}

rm -rf ${TEST_TMP}
mkdir -p $TEST_TMP/root

CONFIG='{
    "root": {
        "name": "/",
        "entries": [
            { 
                "name": "f1",
                "uid_access": [  
                   { "uid": UID,  "access": [ ACCESS ]  }
                ]
            }
        ]
    }
}'


#
# Check that we can read the file
#
test_file_access "Read access" "${TEST_TMP}/root/f1" "-r--------" "$CONFIG" 'ACCESS|"read"' 
test_file_access "Write access" "${TEST_TMP}/root/f1" "--w-------" "$CONFIG" 'ACCESS|"write"' 
test_file_access "Read / write access" "${TEST_TMP}/root/f1" "-rw-------" "$CONFIG" 'ACCESS|"read", "write"'

exit 0

