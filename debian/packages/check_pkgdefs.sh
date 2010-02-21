#!/bin/bash
#
# Copyright 2005 Nexenta Systems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# $Id: build_pkgs.sh 111928 2006-06-29 20:13:55Z mac $
#
# Check pkgdefs/SUNW* packages and debian/sunw* on whether some
# were added or removed

. ./RELEASE

if ! test -e $GATEROOT/usr/src/pkgdefs; then
	echo "Cannot find $GATEROOT/usr/src/pkgdefs. GATEROOT is not defined?"
	exit 1
fi

test "x$1" = xskip-broken && SKIP_BROKEN=1
test "x$1" = xshow-need-build && SHOW_NEED_BUILD=1

printf "=======================\n"
printf "OS Version:\t${OS_REL}.${BUILD}\n"
printf "=======================\n\n"

printf "Checking Packages... (this will take some time)\n\n"

cd $GATEROOT/usr/src/pkgdefs
dirlist=`find . -maxdepth 1 -mindepth 1 -type d ! -name "SUNW*.[cdmuv]"`
cd - > /dev/null 2>&1

dirlist=$(echo $dirlist|sed -e "s/BRCM/brcm/g" -e "s/SUNW/sunw/g" -e "s/\.i//g")
set -- $dirlist

i=1; n=$#
while [ $# -gt 0 ]; do
	pkg_name=`basename $1`

	if [ -f ${BROKEN_PKGS_LIST} ]; then
		egrep -v -e '^[ \t]*#|^[ \t]*$' ${BROKEN_PKGS_LIST} |
		grep -w "\<${pkg_name}\>" > /dev/null 2>&1

		if [ $? -eq 0 ]; then
			test "x$SKIP_BROKEN" != x1 && \
				printf "Skipping: %s [marked as broken]\n" \
				    ${pkg_name}

			i=`expr $i + 1`
			shift
			continue
		fi
	fi

	pkgdir=$GATEROOT/usr/src/pkgdefs/$(echo $pkg_name|sed -e "s/sunw/SUNW/")
	if test ! -d ./$pkg_name && \
	   test -f $pkgdir/prototype_i386 -o -f ${pkgdir}.i/prototype_i386; then
	   	test -f ${pkgdir}.i/prototype_i386 && pkgdir="$pkgdir.i"
		printf "New package: %s [%s]\n" ${pkg_name} \
			"`cat $pkgdir/pkginfo.tmpl|grep "^NAME"`"
	elif test -d ./$pkg_name && \
	     test -f $pkgdir/prototype_i386 -o -f ${pkgdir}.i/prototype_i386 && \
	     test "x$SHOW_NEED_BUILD" = x1 && \
	     test "x$(ls ${pkg_name}_*.deb 2>/dev/null|wc -l)" = x0; then
		test -f ${pkgdir}.i/prototype_i386 && pkgdir="$pkgdir.i"
		printf "Need build package: %s [%s]\n" ${pkg_name} \
			"`cat $pkgdir/pkginfo.tmpl|grep "^NAME"`"
	fi

	i=`expr $i + 1`
	shift
done

printf "\ndone.\n"
