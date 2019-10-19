#!/bin/bash

MPOINT="./mount-point"

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

if [[ $automount = true ]]; then
    [[ -d mount-point ]] && rm -rf mount-point
    mkdir mount-point
    ./fs-fuse -t 2097152 -a virtual-disk -f '-d -s mount-point' >fuse.log 2>fuserr.log &
    fusepid=$$
    echo "Started fuse with pid $fusepid"
    sleep 1
fi

echo 'file 1' > ./test/file1.txt
echo "Copying file 1"
cp ./test/file1.txt $MPOINT/
# read -p "Press enter..."

echo "Creating file 2"
echo 'This is file 2' > $MPOINT/file2.txt
ls $MPOINT -la
# read -p "Press enter..."

if [[ $automount = true ]]; then
    kill -s TERM -$fusepid
fi
