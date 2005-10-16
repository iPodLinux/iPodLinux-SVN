#!/bin/bash

mountdev="/dev/sda3"
mntpoint="/mnt/ipod"

usage="Usage: $0 [-u] [[-m] [-l mount]] [-c file]"
copy=false; umount=false; mount=false;
args=`getopt muc:l: $*`
if [ $? != 0 ]
then
	echo "$usage"
	exit 2
fi
set -- $args

for i
do
	case "$i"
	in
		-l)
			mountdev="$2"; shift;
			shift;;
		-m)
			mount=true;
			shift;;
		-u)
			umount=true;
			shift;;
		-c)
			copy=true;
			cfile="$2"; shift;
			shift;;
		--)
			shift; break;;
	esac
done

if ! $mount && ! $copy && ! $umount
then
	echo $usage
	exit 2
fi

if $mount || $copy
then
	echo mounting.
	while ! mount $mountdev $mntpoint 2>/dev/null;
        do
		printf .
	done;
fi
if $copy
then
	echo copying.
	cp $cfile $mntpoint/bin/ || exit 1
fi
if $umount || $copy
then
	echo unmounting.
	umount $mntpoint || exit 1
fi

