#!/bin/bash

pwd=$(dirname $0)

echo $pwd $1

mkdir -vp $1/squashfs
mkdir -vp $1/home
mkdir -vp $1/data
mkdir -vp $1/mnt/extsd
mkdir -vp $1/mnt/sdcard

exit 0
