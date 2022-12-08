#!/bin/bash

set -e

pwd=$(dirname $0)

mkdir -p $1/etc
mkdir -p $1/etc/init.d
mkdir -p $1/usr/bin
#if [ -f ${pwd}/profile ]; then
#    cp -f ${pwd}/profile $1/etc/profile
#else
#    echo "fatal error! ${pwd}/profile is not exist!"
#    exit 1
#fi

#$pwd/import_udhcpd.sh $1
#$pwd/import_timezone.sh $1
#$pwd/import_cron.sh $1
$pwd/mk_extra_dir.sh $1
#$pwd/export_extra_env.sh $1
#$pwd/gen_nfsmount.sh $1
#$pwd/install_init_script.sh $1

#install -m 777 $pwd/preinit $1/etc/preinit
#$pwd/gen_inittab.sh $1

exit 0
