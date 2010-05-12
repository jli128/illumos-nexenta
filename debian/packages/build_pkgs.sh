#!/bin/bash
#
# Copyright 2005 Nexenta Systems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# $Id$
#
# Generate .deb packages out of OpenSolaris proto_$ARCH
# based on pkgdefs/<pkg>/prototype_$ARCH

. ./RELEASE

if ! test -e $GATEROOT/usr/src/pkgdefs; then
	echo "Cannot find $GATEROOT/usr/src/pkgdefs. GATEROOT is not defined?"
	exit 1
fi

cont_pkg=$1
numofjobs=4

function build_one() {
	local p=$1
	chmod 755 debian/rules
	make -f debian/rules clean > /dev/null 2>&1
	dpkg-buildpackage -k$GPGKEY -p$GPGSCRIPT -sgpg -b > /dev/null 2>&1
	if [ $? != 0 ]; then
		printf "\n=== FAILED in $p ===\n"
		printf "To debug execute the following command:\n"
		printf "GATEROOT=$GATEROOT cd $p;  dpkg-buildpackage -k$GPGKEY -p$GPGSCRIPT -sgpg -b\n";
		exit 1
	fi
}

printf "=======================\n"
printf "OS Version:\t${OS_REL}.${BUILD}\n"
printf "=======================\n\n"

printf "Generating Packages... (this will take some time)\n\n"

test "x$cont_pkg" = x && rm -f *.udeb *.deb *.dsc *.gz *.upload *.changes */debian/files

dirlist=`find . -maxdepth 1 -mindepth 1 -type d -name "sunw*" -o -type d -name "brcmbnx" -o -type d -name "libsunw-perl" -o -type d -name "nexenta-lu" -o -type d -name "nexenta-sunw" -o -type d -name "cpqary3" -o -type d -name "brcmbnxe"`
set -- $dirlist

jn=0; i=1; n=$#
while [ $# -gt 0 ]; do
	pkg_name=`basename $1`

	if test "x$cont_pkg" != x; then
		if test "x$pkg_name" = "x$cont_pkg"; then
			echo "Found $cont_pkg. Continue ..."
			cont_pkg=
		else
			echo "Skipping $pkg_name ..."
			i=`expr $i + 1`
			shift
			continue
		fi
	fi

	if [ -f ${BROKEN_PKGS_LIST} ]; then
		egrep -v -e '^[ \t]*#|^[ \t]*$' ${BROKEN_PKGS_LIST} |
		grep -w "\<${pkg_name}\>" > /dev/null 2>&1

		if [ $? -eq 0 ]; then
			printf "[%3s/%3s] Skipping: %s [marked as broken or disabled]\n" \
			    $i $n ${pkg_name}

			i=`expr $i + 1`
			shift
			continue
		fi
	fi

	printf "[%3s/%3s] Building: %s\n" $i $n ${pkg_name}
	cd $1
	build_one $pkg_name &
	cd - > /dev/null 2>&1

	jn=`expr $jn + 1`
	if test $jn -ge $numofjobs; then
		wait
		jn=0
	fi

	i=`expr $i + 1`
	shift
done
