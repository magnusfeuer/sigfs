#
# To be included by other scripts
#

# $1       JSON Config string.
# $2..$#   Substitute patterns: 'label|replacement'
#
expand_labels()
{
    CFG="$1"
    # Initialize sed arguments with UID and GID expansion
    SED_ARG="s|UID|$(id -u)|g;s|GID|$(id -g)|g"

    # build up a sed script with the remaining labels to be expanded
    shift
    for sub in "$@"
    do
        SED_ARG="$SED_ARG;s|$sub|g"
    done

    echo "$CFG" | sed "$SED_ARG"
}


# $1       Temporary test directory
# $1       JSON config string
# $2..$#   Substitute patterns: 'label|replacement'
launch_sigfs()
{
    local TMP_DIR="$1"
    shift
    local CFG="$1"
    shift
    echo "$(expand_labels "$CFG" "$@")" > ${TMP_DIR}/sigfs.config

    fusermount -u ${TMP_DIR}/root > /dev/null 2>&1 
    SIGFS_LOG_LEVEL=5 ${SIGFS_EXE} -c ${TMP_DIR}/sigfs.config ${TMP_DIR}/root &
    SIGFS_PID=$!
    sleep 0.2
}

# Kill sigfs process previously launched with launch_sigfs
kill_sigfs()
{
    if [ -n "$SIGFS_PID" ]
    then
        # Kill the sigfs process
        kill -1 ${SIGFS_PID}
        wait $SIGFS_PID
    fi
}


SIGFS_PID=""
trap kill_sigfs SIGINT SIGTERM
