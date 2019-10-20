#!/bin/bash
MPOINT='./mount-point/'
TIMEOUT=10

automount=false
while [ "$1" != "" ]; do
    case $1 in
        -m | --automount)
            shift
            automount=true
            ;;
    esac
done

rm -R -f test
mkdir test

function wait_fuse {
    endseconds=$(($SECONDS+$TIMEOUT))
    while [[ $SECONDS -lt $endseconds ]] ; do
        grep -qs "$PWD/mount-point " /proc/mounts && return 0
        kill -0 $1 >/dev/null 2>&1 || { echo "fuse with pid $1 dieded"; return 1; }
        sleep 1
    done

    echo "Mounting timed out"
    return -1
}

if [[ $automount = true ]]; then
    grep -qs "$PWD/mount-point " /proc/mounts && { echo "Can't mount: Alredy mounted"; exit 1; } 
    [[ -d mount-point ]] && rm -rf mount-point
    mkdir mount-point
    ./fs-fuse2 -t 2097152 -a virtual-disk -f '-d -s mount-point' >fuse.log 2>fuserr.log &
    fusepid=$!
    echo "Started fuse with pid $fusepid"
    wait_fuse $fusepid || exit 1 
fi

if [[ $automount = true ]]; then
    fusermount -u ./mount-point || { echo "Couldn't umount"; exit 1; }
fi
