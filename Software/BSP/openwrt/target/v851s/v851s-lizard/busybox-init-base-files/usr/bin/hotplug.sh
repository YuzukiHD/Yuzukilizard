#!/bin/sh
: <<'COMMENTBLOCK'
# remove
if [ "${ACTION}" == "remove" ]; then
    MOUNTPOINT="$(grep -w "^/dev/${MDEV}" /proc/mounts | awk '{print $2}')"
    [ -n "${MOUNTPOINT}" ] \
        && /bin/umount -l ${MOUNTPOINT} 2>/dev/null 1>/dev/null
    exit 0
fi

# add
if [ "${ACTION}" == "add" ]; then
    case ${MDEV} in
        mmcblk[0-9])
            [ -d "/sys/block/${MDEV}/${MDEV}p1" ] && exit 0
            MOUNTPOINT=/mnt/SDCARD
            ;;
        mmcblk[0-9]p[0-9])
            MOUNTPOINT=/mnt/SDCARD
            ;;
        sd[a-z])
            [ -d "/sys/block/${MDEV}/${MDEV}1" ] && exit 0
            MOUNTPOINT=/mnt/exUDISK
            ;;
        sd[a-z][0-9])
            MOUNTPOINT=/mnt/exUDISK
            ;;
        *)
            exit 0
            ;;
    esac
    for fstype in vfat ext4
    do
        [ "${fstype}" = "ext4" -a -x "/usr/sbin/e2fsck" ] \
            && e2fsck -p /dev/${MDEV} >/dev/null
        /bin/mount -t ${fstype} -o utf8 /dev/${MDEV} ${MOUNTPOINT} && exit 0
    done
    [ ! -b "/dev/${MDEV}" -a -f "/sys/block/${MDEV%p*}/${MDEV}/uevent" ] \
        && echo add > /sys/block/${MDEV%p*}/${MDEV}/uevent
fi
COMMENTBLOCK
exit 0
