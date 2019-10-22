# David Cantador Piedras ***REMOVED***W
# David Davó Laviña ***REMOVED***
#!/bin/bash

MPOINT="./mount-point"
TIMEOUT=20

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

echo 'file 1' > './test/file1.txt'
echo "Copying file 1"
cp './test/file1.txt' $MPOINT/
# read -p "Press enter..."

echo "Creating file 2"
echo 'This is file 2' > $MPOINT/file2.txt
ls $MPOINT -lai
# read -p "Press enter..."

echo "Creating file 3"
head -c 8000 /dev/urandom > './test/random.bin'
cp './test/random.bin' $MPOINT/

echo "Truncating file 3"
truncate -s 4096 $MPOINT/random.bin
truncate -s 4096 './test/random.bin' || exit 1
diff -q $MPOINT/random.bin './test/random.bin' || exit 1

echo "Removing file 2"
rm $MPOINT/file2.txt
ls $MPOINT -lai

echo "Testing reading"
cat $MPOINT/file1.txt > './test/file1.txt.mpoint'
diff -q 'test/file1.txt' './test/file1.txt.mpoint' || exit 1;

if [[ $automount = true ]]; then
    fusermount -u ./mount-point || { echo "Couldn't umount"; exit 1; }
    # kill -s TERM -$fusepid || { echo "Couldn't kill fuse"; exit 1; }

    echo "Testing mounting again"
    ./fs-fuse2 -t 2097152 -m -a virtual-disk -f '-d -s mount-point' >>fuse.log 2>>fuserr.log || exit 1 &
    fusepid=$!
    echo "Started fuse with pid $fusepid"
    wait_fuse $fusepid || exit 1

    echo "Testing reading random.bin"
    diff -q $MPOINT/random.bin './test/random.bin' || exit 1

    echo "Testing creating new file"
    echo "Hello World!" > $MPOINT/newfile.txt
    ls $MPOINT -lai
    
    fusermount -u ./mount-point || { echo "Couldn't umount"; exit 1; }
fi
