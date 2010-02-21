#!/sbin/sh
#
# This wrapper helps to avoid breakage of majority of GNU software
# which depends on real /bin/ldconfig (libc)

if [ -f /usr/bin/crle ]; then
	/usr/bin/crle -u
fi
