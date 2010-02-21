#!/bin/bash

if ! test -f /tmp/nlubin/env.sh || test -f /.nexenta-lu.lock; then
	exit 1
fi

. /tmp/nlubin/env.sh

first_arg=$1

if test "x$first_arg" = xerror_unwind; then
	echo "***********************************************************************"
	echo "*                                                                     *"
	echo "*     Upgrade sequence returned an error. To enter NLU protected      *"
	echo "*          environment please type 'source /tmp/nlubin/env.sh'        *"
	echo "*                                                                     *"
	echo "***********************************************************************"
	exit 1
fi

echo
umount /lib/ld.so.1 2>/dev/null
umount /lib/amd64/ld.so.1 2>/dev/null
/tmp/nlubin/cp /usr/sbin/bootadm /tmp/nlubin
/var/lib/dpkg/alien/nexenta-sunw/nluld /tmp/nlubin/bootadm

# check for chrooted environment...
# NOTE: calling script must re-create boot archive on its own!
if test ! -r /etc/svc/volatile/repository_door; then
	mv /tmp/nlubin/env.sh /tmp/nlubin/env.sh.old
	/tmp/nlubin/cp /var/lib/dpkg/alien/nexenta-sunw/ld32 /lib/ld.so.1
	/tmp/nlubin/cp /var/lib/dpkg/alien/nexenta-sunw/ld64 /lib/amd64/ld.so.1
	exit 0
fi

/tmp/nlubin/bootadm update-archive
echo
if ! cat /boot/grub/menu.lst|grep ISADIR >/dev/null; then
	echo "***********************************************************************"
	echo "*                                                                     *"
	echo "*     GRUB's configuration file /boot/grub/menu.lst needs to be       *"
	echo "*     manually converted before reboot! To enter NLU protected        *"
	echo "*        environment please type 'source /tmp/nlubin/env.sh'          *"
	echo "*                                                                     *"
	echo "***********************************************************************"
	exit 1
fi

echo "Successful upgrade! To apply upgrade changes you need to reboot."

if cat /etc/release|grep NexentaOS >/dev/null; then
	echo -n "Would you like to reboot now? [Y/n] "
	read key
	if test "x$key" = xN -o "x$key" = xn; then
		echo "***********************************************************************"
		echo "*                                                                     *"
		echo "*        To enter NLU protected environment again please              *"
		echo "*               type 'source /tmp/nlubin/env.sh'                      *"
		echo "*                                                                     *"
		echo "***********************************************************************"
	else
		echo "Reboot requested..."
		mv /tmp/nlubin/env.sh /tmp/nlubin/env.sh.old
		reboot
	fi
else
	mv /tmp/nlubin/env.sh /tmp/nlubin/env.sh.old
fi
exit 0
