#!/bin/bash
#
# Copyright 2011 Nexenta Systems, Inc.  All rights reserved.
# Use is subject to license terms.
#

. ./RELEASE

#
# Until we find a cleaner solution, this script should be used
# everytime we synch up with a new OpenSolaris code release,
# so that the generated Debian packages are properly versioned.
#
NOTE="  * Synchronized with OpenSolaris code (${OS_REL} build ${BUILD})"

DATE=`/usr/bin/date --rfc-2822`
SIGNATURE=" -- ${DEBFULLNAME} <${DEBEMAIL}>  ${DATE}"

TMP_FILE=/tmp/tmp_file.$$

function update_changelog
{
	pkg_name=`basename $1`
	changelog="$pkg_name (${CORE_REVISION}) ${DISTRO}; urgency=${URG}"
	comments="${changelog}\n\n${NOTE}\n\n${SIGNATURE}\n\n"

	if [ -f $1/debian/changelog ]; then
		file=$1/debian/changelog
	else
		echo "Error: ${pkg_name} changelog file not found."
		return 1
	fi

	grep "$pkg_name (${CORE_REVISION})" ${file} >/dev/null 2>&1
	if [ $? -eq 0 ]; then
		return 1
	fi

	printf "${comments}" > ${TMP_FILE}
	cat ${file} >> ${TMP_FILE}

	mv ${TMP_FILE} ${file}
	return 0
}

function update_control
{
	pkg_name=`basename $1`

	if [ -f $1/debian/control ]; then
		file=$1/debian/control
	else
		echo "Error: ${pkg_name} control file not found."
		return 1
	fi

	sed -e "s/(>=${OS_REL}.${OLD_BUILD}-[1-9]\+)/(>=${CORE_REVISION})/g" \
	    ${file} > ${TMP_FILE}

	mv ${TMP_FILE} ${file}
	return 0
}

for f in `find . -maxdepth 1 -mindepth 1 -type d -name "sunw*" -o -type d -name "brcmbnx" -o -type d -name "libsunw-perl" -o -type d -name "nexenta-lu" -o -type d -name "nexenta-sunw" -o -type d -name "brcmbnxe" -o -type d -name cpqary3`; do
	pkg_name=`basename $f`

	if update_changelog $f && update_control $f; then
		echo "updating: ${pkg_name}"
	fi
done
