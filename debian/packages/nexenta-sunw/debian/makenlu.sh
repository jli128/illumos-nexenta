#!/bin/bash

nlubin="/tmp/nlubin"
nlulib="/tmp/nlulib"
nlucmd="
	/usr/bin/locale
	/usr/bin/perl
	/usr/bin/dpkg-split
	/usr/bin/dpkg-deb
	/usr/bin/dpkg
	/usr/bin/apt-get
	/usr/bin/apt-extracttemplates
	/usr/bin/awk
	/usr/bin/gawk
	/usr/bin/cat
	/usr/bin/chgrp
	/usr/bin/chmod
	/usr/bin/chown
	/usr/bin/cmp
	/usr/bin/cp
	/usr/bin/cpio
	/usr/bin/csh
	/usr/bin/cut
	/usr/bin/date
	/usr/bin/dd
	/usr/bin/df
	/usr/bin/diff
	/usr/bin/du
	/usr/bin/echo
	/usr/sun/bin/ed
	/usr/bin/env
	/usr/bin/ex
	/usr/bin/expr
	/usr/bin/false
	/usr/bin/file
	/usr/bin/find
	/usr/bin/gettext
	/usr/bin/grep
	/usr/bin/egrep
	/usr/bin/fgrep
	/usr/bin/head
	/usr/bin/id
	/usr/bin/ksh
	/usr/bin/ksh93
	/usr/bin/line
	/usr/bin/ln
	/usr/bin/ls
	/usr/bin/mkdir
	/usr/bin/mktemp
	/usr/bin/more
	/usr/bin/mv
	/usr/bin/nawk
	/usr/bin/pgrep
	/usr/bin/pkill
	/usr/bin/printf
	/usr/bin/ps
	/usr/bin/ptree
	/usr/bin/rm
	/usr/bin/rmdir
	/usr/bin/sed
	/usr/bin/bash
	/usr/bin/sh
	/usr/bin/sleep
	/usr/bin/sort
	/usr/bin/strings
	/usr/bin/stty
	/usr/bin/su
	/usr/bin/sum
	/usr/bin/tail
	/usr/bin/tee
	/usr/bin/touch
	/usr/bin/tr
	/usr/bin/true
	/usr/bin/truss
	/usr/bin/tty
	/usr/bin/uname
	/usr/bin/uniq
	/usr/bin/uptime
	/usr/bin/vi
	/usr/bin/w
	/usr/bin/wc
	/usr/bin/xargs
	/usr/bin/zcat
	/usr/sbin/chroot
	/usr/sbin/halt
	/usr/lib/fs/ufs/lockfs
	/usr/sbin/lofiadm
	/usr/sbin/mkfile
	/usr/sbin/clri
	/bin/mknod
	/sbin/mount
	/usr/lib/fs/ufs/newfs
	/usr/sbin/prtconf
	/usr/sbin/reboot
	/bin/sync
	/bin/tar
	/sbin/uadmin
	/sbin/umount
	/usr/sbin/wall
	/usr/sbin/zonecfg
	/usr/bin/gzip
	/usr/bin/basename
	/usr/bin/dirname
	/sbin/biosdev
	/sbin/installgrub
	/sbin/fdisk
	/sbin/metastat
	/usr/bin/mkisofs
	/usr/sbin/svcadm
	/usr/sbin/svccfg
	/usr/bin/svcprop
"

# NOTE: we do not include /sbin/bootadm in the list on purpose!
#       we always want the latest bootadm to execute its upgrade features
#
#	/usr/sbin/add_drv not included because old version causing problems
#       during upgrade

prepare_nlu_env() {
	local libs

	rm -rf $nlulib $nlubin
	mkdir $nlulib $nlubin $nlubin/i86 $nlubin/amd64 $nlubin/sun $nlubin/sun/i86 $nlubin/sun/amd64

	isaexec_inode=$(ls -i /usr/lib/isaexec|awk '{print $1}')
	cp /usr/lib/isaexec $nlubin

	link=`ls -dl /usr/lib/64  | awk '{print $NF}'`

	set $nlucmd
	while [ $# -gt 0 ]
	do
		dir=${1%/*}
		cmd=${1##*/}
		if ! test -f $dir/$cmd; then
			shift
			continue
		fi
		cp $dir/amd64/$cmd $nlubin/amd64 2>/dev/null
		cp $dir/i86/$cmd $nlubin/i86 2>/dev/null
		if test $(ls -i $dir/$cmd|awk '{print $1}') == $isaexec_inode; then
			ln $nlubin/isaexec $nlubin/$cmd
		else
			cp $dir/$cmd $nlubin
		fi
		for subdir in bin sbin; do
			if test -f /usr/sun/$subdir/$cmd; then
				cp /usr/sun/$subdir/amd64/$cmd $nlubin/sun/amd64 2>/dev/null
				cp /usr/sun/$subdir/i86/$cmd $nlubin/sun/i86 2>/dev/null
				if test $(ls -i /usr/sun/$subdir/$cmd|awk '{print $1}') == $isaexec_inode; then
					ln $nlubin/isaexec $nlubin/sun/$cmd
				else
					cp /usr/sun/$subdir/$cmd $nlubin/sun 2>/dev/null
				fi
			fi
		done
		shift
	done

	libs_gnu="`ldd $nlubin/* 2>/dev/null | nawk '$3 ~ /lib/ { print $3 }' |sed -e 's/\/\//\//' | sort -u`"
	libs_gnu64="`ldd $nlubin/amd64/* 2>/dev/null | nawk '$3 ~ /lib/ { print $3 }' |sed -e 's/\/\//\//' | sort -u`"
	libs_sun="`ldd $nlubin/sun/* 2>/dev/null | nawk '$3 ~ /lib/ { print $3 }' |sed -e 's/\/\//\//' | sort -u`"
	libs_sun64="`ldd $nlubin/sun/amd64/* 2>/dev/null | nawk '$3 ~ /lib/ { print $3 }' |sed -e 's/\/\//\//' | sort -u`"
	libs="$libs_gnu $libs_gnu64 $libs_sun $libs_sun64 /usr/lib/64/nss_* /usr/lib/nss_*"

	release_date=$(uname -a|awk '{print $4}'|awk -F_ '{print $2}')

	ln -s $link $nlulib/64
	mkdir $nlulib/$link

	#
	# Copy libraries to proper directories
	#
	for lib in `echo $libs | sed -e 's/ /\n/g' | sort -u`
	do
		test -L $lib && continue
		if test $release_date -lt 20070402; then
			case $lib in
			*64/libc.so.1*)
				cp /var/lib/dpkg/alien/nexenta-sunw/b55.libc.64 $nlulib/amd64/libc.so.1
				continue;;
			*lib/libc.so.1*)
				cp /var/lib/dpkg/alien/nexenta-sunw/b55.libc.32 $nlulib/libc.so.1
				continue;;
			*) ;;
			esac
		fi
		case $lib in
		*/64/* | */$link/*)
			cp $lib $nlulib/64;;
		*)
			cp $lib $nlulib;;
		esac
	done

	mkdir -p /tmp/nl/amd64
	ln -s /tmp/nl/amd64 /tmp/nl/64
	cp /lib/ld.so.1 /tmp/nl/nl.1
	cp /lib/ld.so.1 $nlulib/nl.1
	cp /lib/amd64/ld.so.1 /tmp/nl/amd64/nl.1
	cp /lib/amd64/ld.so.1 $nlulib/amd64/nl.1

	for f in `ls $nlubin/sun/* $nlubin/sun/amd64/* $nlubin/* $nlubin/amd64/*`; do
		test ! -f "$f" && continue
		/var/lib/dpkg/alien/nexenta-sunw/nluld $f 2>/dev/null
		chmod 755 $f 2>/dev/null
	done

	echo \# Nexenta LU protected environment			>  $nlubin/env.sh
	echo "NLU_ENABLED=1; export NLU_ENABLED"			>> $nlubin/env.sh
	echo "LD_NOAUXFLTR=1; export LD_NOAUXFLTR"			>> $nlubin/env.sh
	echo "LD_LIBRARY_PATH=$nlulib; export LD_LIBRARY_PATH"		>> $nlubin/env.sh
	echo "LD_LIBRARY_PATH_64=$nlulib/64; export LD_LIBRARY_PATH_64"	>> $nlubin/env.sh
}

if test -d /debootstrap; then
	echo "Initial install: no need to initialize NLU protected environment ..."
elif ! test -d $nlubin; then
	echo "Initiating NLU protected environment ..."
	prepare_nlu_env
else
	echo "NLU protected environment already initiated..."
fi

exit 0
