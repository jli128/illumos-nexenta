#!/bin/bash
#
# Copyright 2005 Nexenta Systems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# $Id$

. ./RELEASE

if ! test -e $GATEROOT/usr/src/pkgdefs; then
	echo "Cannot find $GATEROOT/usr/src/pkgdefs. GATEROOT is not defined?"
	exit 1
fi

#
# Until we find a cleaner solution, this script should be used
# everytime we synch up with a new OpenSolaris code release,
# so that the generated Debian packages are properly versioned.
#
NOTE="  * Synchronized with OpenSolaris code (${OS_REL} build ${BUILD})"

DATE=`/usr/bin/date --rfc-2822`
SIGNATURE=" -- ${DEBFULLNAME} <${DEBEMAIL}>  ${DATE}"

TMP_FILE=/tmp/tmp_file.$$

function update_control
{
	pkg_name=`basename $1`

	if [ -f $1/debian/control ]; then
		file=$1/debian/control
	else
		echo "Error: ${pkg_name} control file not found."
		return 1
	fi

	sed -e "s/^Maintainer: .*$/Maintainer: Nexenta Systems <support@nexenta.com>/g" ${file} > ${TMP_FILE}

	mv ${TMP_FILE} ${file}
	return 0
}

for f in `find . -maxdepth 1 -mindepth 1 -type d -name "sunw*" -o -type d -name "brcmbnx" -o -type d -name "libsunw-perl" -o -type d -name "nexenta-lu" -o -type d -name "nexenta-sunw" -o -type d -name "brcmbnxe" -o -type d -name "cpqary3" -o -type d -name "openipmi"`; do
	pkg_name=`basename $f`

	if update_control $f; then
		echo "updating: ${pkg_name}"
	fi
done
