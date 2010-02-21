#!/bin/ksh -p
#
# Copyright 2005 Nexenta Systems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# $Id$
#

. ./RELEASE

TMP_FILE=/tmp/$0.$$
bad=0

trap 'cleanup' EXIT HUP QUIT TERM

function cleanup
{
	rm -f ${TMP_FILE} 2> /dev/null
}

function check_empty_version
{
	pkg_name=$1
	pkg_dep=$2

	egrep -v -e '^[ \t]*#|^[ \t]*$' ${NON_ON_PKGS_LIST} |
	grep -w "\<${pkg_dep}\>" > /dev/null 2>&1

	if [ $? -ne 0 ]; then
		printf "ERROR (${pkg_name}): ${pkg_dep} is missing version ${ver}\n"
		bad=1
	fi
}

function is_broken_pkg
{
	pkg_name=$1
	pkg_def=$2

	if [ ! -f ${BROKEN_PKGS_LIST} ]; then
		printf "WARNING (${pkg_name}): ${pkg_dep} is missing\n"
		return
	fi

	# If the package is marked as broken, then ignore
	egrep -v -e '^[ \t]*#|^[ \t]*$' ${BROKEN_PKGS_LIST} |
	grep -w "\<${pkg_name}\>" | read broken_pkg

	if [ "${pkg_name}" != "${broken_pkg}" ]; then
		printf "WARNING (${pkg_name}): ${pkg_dep} is missing\n"
	fi
}

function check_version
{
	pkg_name=$1
	pkg_dep=$2
	ver=$3

	egrep -v -e '^[ \t]*#|^[ \t]*$' ${NON_ON_PKGS_LIST} |
	grep -w "\<${pkg_dep}\>" > /dev/null 2>&1

	if [ $? -eq 0 ]; then
		printf "WARNING (${pkg_name}): version dependency on ${pkg_dep} ${ver} [non-ON pkg]\n"
	else
		grep -w "\<${pkg_dep}\>" ${TMP_FILE} |
		nawk '{ print $2 }' | read ver_actual

		if [ -z ${ver_actual:-} ]; then
			is_broken_pkg ${pkg_name} ${pkg_dep}
		elif [ "${ver}" != "(>=${ver_actual})" ]; then
			printf "WARNING (${pkg_name}): ${pkg_dep} has version: ${ver}, expected (>=${ver_actual})\n"
		fi
	fi	
}

function scan_depend
{
	pkg_name=`basename $1`

	egrep -e "^Depends:" $1/debian/control | \
	sed -e 's/^Depends:[ \t]*//g' | \
	sed -e 's/[,|]/\n/g' | nawk '{ print $1,$2 }' | \
	while read pkg ver; do
		echo "${pkg}" | egrep -e "sunw*" > /dev/null 2>&1
		if [ $? -eq 0 ]; then
			if [ -z "${ver:-}" ]; then
				check_empty_version ${pkg_name} ${pkg}
			else
				check_version ${pkg_name} ${pkg} ${ver}
			fi
		fi
	done
}

function scan_core
{
	pkg_name=`basename $1`

	read pkg < $1/debian/changelog
	echo "${pkg}" | nawk '{ print $2 }' | read ver

	if [ "${ver}" != "(${CORE_REVISION})" ]; then
		printf "ERROR (${pkg_name}): ${ver}, expected (${CORE_REVISION}) [core out-of-sync]\n"
		bad=1
	fi

	egrep -e "^Depends:" $1/debian/control | \
	egrep -e "${NEXENTALU}" > /dev/null 2>&1

	if [ "${pkg_name}" != "${NEXENTALU}" -a $? -eq 0 ]; then
		printf "ERROR (${pkg_name}): invalid dependency on ${NEXENTALU}\n"
		bad=1
	fi
}

function scan_ver
{
	pkg_name=`basename $1`

	read pkg < $1/debian/changelog
	echo "${pkg}" | nawk '{ print $2 }' | read ver

	if [ "${ver}" != "(${CORE_REVISION})" ]; then
		printf "WARNING (${pkg_name}): ${ver}, while core is (${CORE_REVISION}) [bug fix?]\n"
	fi

	echo "${pkg_name} ${ver}" | sed -e 's/(//g' | sed -e 's/)//g' >> \
	    ${TMP_FILE}
}

printf "==================================\n"
printf "Current Core Revision:\t${CORE_REVISION}\n"
printf "==================================\n\n"
printf "Cross checking ... (this may take a while)\n\n"

# Get versions
rm -f ${TMP_FILE} 2> /dev/null
touch ${TMP_FILE}
for f in `find . -maxdepth 1 -mindepth 1 -type d -name "sunw*" -o -type d -name "brcmbnx" -o -type d -name "libsunw-perl" -o -type d -name "nexenta-lu" -o -type d -name "nexenta-sunw"`; do
	scan_ver $f
done

for f in `find . -maxdepth 1 -mindepth 1 -type d -name "sunw*" -o -type d -name "brcmbnx" -o -type d -name "libsunw-perl" -o -type d -name "nexenta-lu" -o -type d -name "nexenta-sunw"`; do
	scan_depend $f
done

egrep -v -e '^[ \t]*#|^[ \t]*$' ${CORE_PKGS_LIST} |
while read pkg; do
	scan_core ${pkg}
done

if [ "$bad" -eq 0 ]; then
	printf "\n=== SUCCESS ===\n"
else
	printf "\n=== FAILED ===\n"
fi
