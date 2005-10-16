#!/bin/bash

dev="courtc"

module="tools/podzilla"
usage="Usage: $0 [-m module] folder"

args=`getopt m: $*`
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
		-m)
			module="$2"; shift;
			shift;;
		--)
			shift; break;;
	esac
done

if [ -z "$1" ]
then
	echo "$usage"
	exit 2
fi
echo Downloading $module to $1...

if [ -z "$dev" ]
then
	proto="pserver"
	dev="anonymous"
	cvs -d:$proto:$dev@cvs.sf.net:/cvsroot/ipodlinux login || exit 1
else
	proto="ext"
	export CVS_RSH=ssh
fi

cvs -z3 -d:$proto:$dev@cvs.sf.net:/cvsroot/ipodlinux co -d $1 $module || exit 1
