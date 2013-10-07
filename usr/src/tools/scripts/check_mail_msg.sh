#!/bin/sh

# This is a POST_NIGHTLY hook script to check the mail_msg file.

#
# Helper function thanks to Chris Siden at Delphix
#
# Checks the given mail_msg file ($1) section starting $2 and ending $3
# If that section exists is non-empty, set the abort flag.
#
function check
{
	typeset mail_msg start end

	mail_msg="$1"
	start="==== $(echo "$2" | sed "s@/@\\\\/@") ===="
	end="==== $(echo "$3" | sed "s@/@\\\\/@") ===="

	if [[ ! -z "$(sed -n "/^$start\$/,/^$end\$/p" $mail_msg |\
		sed -e "/^$start\$/d" -e "/^$end\$/d" -e "/^$/d")" ]]; then
		echo "Noise in section: $2"
		touch $TMPDIR/abort
	fi
}

#
# Leave a copy of the mail_msg file in the top of the WS
# where it's easier for build automation to find it.
#
cd ${CODEMGR_WS}
cp ${LLOG}/mail_msg mail_msg.txt

# Check sections that should be empty:

check mail_msg.txt "Build errors" "Build warnings"
check mail_msg.txt "Build errors (DEBUG)" "Build warnings (DEBUG)"
check mail_msg.txt "Build errors (non-DEBUG)" "Build warnings (non-DEBUG)"

check mail_msg.txt "Build warnings" "Elapsed build time"
check mail_msg.txt "Build warnings (DEBUG)" "Elapsed build time (DEBUG)"
check mail_msg.txt "Build warnings (non-DEBUG)" "Elapsed build time (non-DEBUG)"

# ncp3 has warnings.  Fix those, then uncomment these.
#check mail_msg.txt "lint warnings src" "lint noise differences src"
#check mail_msg.txt "cstyle/hdrchk errors" "Find core files"

if [ -f $TMPDIR/abort ]; then
	echo "\nmail_msg has noise. FAIL" >> mail_msg.txt
	exit 1
fi

exit 0
