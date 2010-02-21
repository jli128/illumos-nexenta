#!/bin/bash
#
# Copyright 2005 Nexenta Systems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# $Id$
#
# Clean up .deb packages
#

rm -f *.udeb *.deb *.dsc *.changes *.upload *.gz

dirlist=`find . -maxdepth 1 -mindepth 1 -type d -name "sunw*" -o -type d -name "brcmbnx" -o -type d -name "libsunw-perl" -o -type d -name "nexenta-lu" -o -type -d -name "nexenta-sunw"`
set -- $dirlist

i=1; n=$#
while [ $# -gt 0 ]; do
	pkg_name=`basename $1`
	printf "[%3s/%3s] Cleaning: %s\n" $i $n ${pkg_name}

	cd $1

	make -f debian/rules clean > /dev/null 2>&1
	rm -f debian/*.debhelper > /dev/null 2>&1
	rm -f debian/*.substvars > /dev/null 2>&1
	rm -f debian/files > /dev/null 2>&1
	rm -f debian/preinst > /dev/null 2>&1
	rm -f debian/postinst > /dev/null 2>&1
	rm -f debian/prerm > /dev/null 2>&1
	rm -f debian/postrm > /dev/null 2>&1
	cd - > /dev/null 2>&1

	i=`expr $i + 1`
	shift
done
