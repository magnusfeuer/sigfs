#!/usr/bin/bash
#


launch_sigfs()
{
local CFG_BASE='
{
    "root": {
        "name": "/",
        "uid_access": [
            {
                "uid": UID,
                "access": [ ACCESS ]
            }
        ],
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
    echo "$CFG_BASE" | sed "s|UID|$(id -u)|g;s|GID|$(id -g)|g;s|ACCESS|$1|g" > $CFG_FILE
    fusermount -u $MOUNT_POINT > /dev/null 2>&1 
    SIGFS_LOG_LEVEL=6 ./sigfs -c $CFG_FILE $MOUNT_POINT > $SIGFS_LOG &
    SIGFS_PID=$!
    sleep 0.2
}


TEST_ROOT=/tmp/sigfs-test.${$}
MOUNT_POINT=${TEST_ROOT}/sigfs-root
CFG_FILE=${TEST_ROOT}/config.json
SIGFS_LOG=${TEST_ROOT}/sigfs.log
SIG_FILE=${MOUNT_POINT}/f1

rm -rf ${TEST_ROOT}

mkdir -p $MOUNT_POINT


#
# Check that we can read the file
#
launch_sigfs '"read"' 

# Stat the file
STAT=$(stat --printf='%A' ${SIG_FILE})

# Kill the server
kill -1 ${SIGFS_PID}
wait $SIGFS_PID

# Check the result
if [ "$STAT" != '-r--------' ]
then
    echo "${SIG_FILE} is not read-only: ${STAT}"
    exit 1
fi

#
# Check that we can write the file
#

launch_sigfs '"write"' 

STAT=$(stat --printf='%A' ${SIG_FILE}) 

# Kill the server
kill -1 ${SIGFS_PID}
wait $SIGFS_PID

if [ "$STAT" != '--w-------' ]
then
    echo "${SIG_FILE} is not write-only: ${STAT}"
    exit 1
fi

#
# Check that we can read and write the file
#

launch_sigfs '"read", "write"' 

STAT=$(stat --printf='%A' ${SIG_FILE}) 

# Kill the server
kill -1 ${SIGFS_PID}
wait $SIGFS_PID

if [ "$STAT" != '-rw-------' ]
then
    echo "${SIG_FILE} is not read and write: ${STAT}"
    exit 1
fi
echo "$0: passed"
exit 0;
