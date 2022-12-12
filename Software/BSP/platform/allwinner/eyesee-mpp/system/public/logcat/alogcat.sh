#!/bin/sh

log_file_path=/tmp/log
cd $log_file_path
filenames=$(ls *.INFO | awk -F. '{print $1}')
cd - > /dev/null

if [ "$filenames" != "" ];then
    for f in $filenames
    do
        progname=$(ps | awk -v partten="$f" '{if ($3~partten) print $3}')
        if [ "$progname" != "" ];then
            basename=$(basename $progname)
            break
        fi
    done
fi

clean=0
if [ "$1" == "-c" ];then
    clean=1
    shift
fi

if [ "$1" == "" ] && [ "$basename" == "" ];then
    echo -e "\033[0;31mError, no log file found\033[0m";
    exit 1
fi

if [ "$1" != "" ] && [ "$basename" == "" ];then
    log_file=${log_file_path}/$1.INFO
else
    log_file=${log_file_path}/${basename}.INFO
fi

if [ "$clean" == "1" ];then
    echo > $log_file
    exit 0
fi

line_cnt=$(wc -l $log_file | awk '{print $1}')

tail -n $line_cnt -f $log_file | awk '{
    if ($1~/^E/)
        print "\033[0;31m" $0 "\033[0m"
    else if ($1~/^W/)
        print "\033[0;33m" $0 "\033[0m"
    else
        print $0
}'

exit 0
