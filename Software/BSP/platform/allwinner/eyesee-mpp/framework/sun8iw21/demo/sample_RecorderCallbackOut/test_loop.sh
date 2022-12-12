#!/bin/bash
count=0
while [ 1 ]
do
    /mnt/extsd/sample_RecorderCallbackOut -path /mnt/extsd/sample_RecorderCallbackOut.conf
    sleep 5;
    let count=$count+1;
    echo "test [$count] times done."
done
