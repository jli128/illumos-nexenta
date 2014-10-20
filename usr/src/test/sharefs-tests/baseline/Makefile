#
# This file and its contents are supplied under the terms of the
# Common Development and Distribution License ("CDDL"), version 1.0.
# You may only use this file in accordance with the terms of version
# 1.0 of the CDDL.
#
# A full copy of the text of the CDDL should have accompanied this
# source.  A copy of the CDDL is also available via the Internet at
# http.//www.illumos.org/license/CDDL.
#
#
# Copyright (c) 2012 by Delphix. All rights reserved.
# Copyright 2014 Nexenta Systems, Inc.  All rights reserved.
#

include $(SRC)/Makefile.master

ROOTOPTPKG = $(ROOT)/opt/sharefs-tests
BASELINEDIR = $(ROOTOPTPKG)/baseline

BFILES = sharectl.1.txt		\
	sharectl.10.txt		\
	sharectl.11.txt		\
	sharectl.12.txt		\
	sharectl.13.txt		\
	sharectl.14.txt		\
	sharectl.15.txt		\
	sharectl.16.txt		\
	sharectl.17.txt		\
	sharectl.18.txt		\
	sharectl.19.txt		\
	sharectl.2.txt		\
	sharectl.20.txt		\
	sharectl.21.txt		\
	sharectl.22.txt		\
	sharectl.23.txt		\
	sharectl.24.txt		\
	sharectl.25.txt		\
	sharectl.26.txt		\
	sharectl.27.txt		\
	sharectl.28.txt		\
	sharectl.29.txt		\
	sharectl.3.txt		\
	sharectl.30.txt		\
	sharectl.31.txt		\
	sharectl.32.txt		\
	sharectl.33.txt		\
	sharectl.34.txt		\
	sharectl.35.txt		\
	sharectl.36.txt		\
	sharectl.37.txt		\
	sharectl.38.txt		\
	sharectl.4.txt		\
	sharectl.5.txt		\
	sharectl.6.txt		\
	sharectl.7.txt		\
	sharectl.8.txt		\
	sharectl.9.txt		\
	sharectl.txt		\
	sharemgr.add.1.txt		\
	sharemgr.add.2.txt		\
	sharemgr.add.3.txt		\
	sharemgr.add.4.txt		\
	sharemgr.add.5.txt		\
	sharemgr.add.6.txt		\
	sharemgr.add.7.txt		\
	sharemgr.add.8.txt		\
	sharemgr.add.8.txt		\
	sharemgr.add.9.txt		\
	sharemgr.add.txt		\
	sharemgr.create.1.txt		\
	sharemgr.create.10.txt		\
	sharemgr.create.11.txt		\
	sharemgr.create.12.txt		\
	sharemgr.create.13.txt		\
	sharemgr.create.2.txt		\
	sharemgr.create.3.txt		\
	sharemgr.create.4.txt		\
	sharemgr.create.5.txt		\
	sharemgr.create.6.txt		\
	sharemgr.create.7.txt		\
	sharemgr.create.8.txt		\
	sharemgr.create.9.txt		\
	sharemgr.create.txt		\
	sharemgr.delete.1.txt		\
	sharemgr.delete.2.txt		\
	sharemgr.delete.3.txt		\
	sharemgr.delete.4.txt		\
	sharemgr.delete.5.txt		\
	sharemgr.delete.6.txt		\
	sharemgr.delete.7.txt		\
	sharemgr.delete.txt		\
	sharemgr.disable.1.txt		\
	sharemgr.disable.10.txt		\
	sharemgr.disable.2.txt		\
	sharemgr.disable.3.txt		\
	sharemgr.disable.4.txt		\
	sharemgr.disable.5.txt		\
	sharemgr.disable.6.txt		\
	sharemgr.disable.7.txt		\
	sharemgr.disable.8.txt		\
	sharemgr.disable.9.txt		\
	sharemgr.disable.txt		\
	sharemgr.move.1.txt		\
	sharemgr.move.2.txt		\
	sharemgr.move.3.txt		\
	sharemgr.move.4.txt		\
	sharemgr.move.5.txt		\
	sharemgr.move.6.txt		\
	sharemgr.move.7.txt		\
	sharemgr.move.8.txt		\
	sharemgr.move.txt		\
	sharemgr.remove.1.txt		\
	sharemgr.remove.10.txt		\
	sharemgr.remove.2.txt		\
	sharemgr.remove.3.txt		\
	sharemgr.remove.4.txt		\
	sharemgr.remove.5.txt		\
	sharemgr.remove.6.txt		\
	sharemgr.remove.7.txt		\
	sharemgr.remove.8.txt		\
	sharemgr.remove.9.txt		\
	sharemgr.remove.txt		\
	sharemgr.set_.1.txt		\
	sharemgr.set_.10.txt		\
	sharemgr.set_.11.txt		\
	sharemgr.set_.12.txt		\
	sharemgr.set_.2.txt		\
	sharemgr.set_.3.txt		\
	sharemgr.set_.4.txt		\
	sharemgr.set_.5.txt		\
	sharemgr.set_.6.txt		\
	sharemgr.set_.7.txt		\
	sharemgr.set_.8.txt		\
	sharemgr.set_.9.txt		\
	sharemgr.set_.txt		\
	sharemgr.set_security.1.txt		\
	sharemgr.set_security.10.txt		\
	sharemgr.set_security.11.txt		\
	sharemgr.set_security.12.txt		\
	sharemgr.set_security.13.txt		\
	sharemgr.set_security.14.txt		\
	sharemgr.set_security.15.txt		\
	sharemgr.set_security.2.txt		\
	sharemgr.set_security.3.txt		\
	sharemgr.set_security.4.txt		\
	sharemgr.set_security.5.txt		\
	sharemgr.set_security.6.txt		\
	sharemgr.set_security.7.txt		\
	sharemgr.set_security.8.txt		\
	sharemgr.set_security.9.txt		\
	sharemgr.set_security.txt		\
	sharemgr.set_share.1.txt		\
	sharemgr.set_share.2.txt		\
	sharemgr.set_share.3.txt		\
	sharemgr.set_share.4.txt		\
	sharemgr.set_share.5.txt		\
	sharemgr.set_share.6.txt		\
	sharemgr.set_share.7.txt		\
	sharemgr.set_share.8.txt		\
	sharemgr.set_share.txt		\
	sharemgr.show.1.txt		\
	sharemgr.show.2.txt		\
	sharemgr.show.txt		\
	sharemgr.txt		\
	sharemgr.unset_.1.txt		\
	sharemgr.unset_.10.txt		\
	sharemgr.unset_.11.txt		\
	sharemgr.unset_.12.txt		\
	sharemgr.unset_.2.txt		\
	sharemgr.unset_.3.txt		\
	sharemgr.unset_.4.txt		\
	sharemgr.unset_.5.txt		\
	sharemgr.unset_.6.txt		\
	sharemgr.unset_.7.txt		\
	sharemgr.unset_.8.txt		\
	sharemgr.unset_.9.txt		\
	sharemgr.unset_.txt		\
	sharemgr.usage.1.txt		\
	sharemgr.usage.2.txt		\
	sharemgr.usage.3.txt		\
	sharemgr.usage.txt		\
	sharemgr.zfs.1.txt		\
	sharemgr.zfs.2.txt		\
	sharemgr.zfs.3.txt		\
	sharemgr.zfs.4.txt		\
	sharemgr.zfs.txt		\
	sharemgr.zones.1.txt		\
	sharemgr.zones.2.txt		\
	sharemgr.zones.3.txt		\
	sharemgr.zones.txt		\
	summary.txt

BASEFILES = $(BFILES:%=$(BASELINEDIR)/%)
$(BASEFILES) := FILEMODE = 0444

all lint clean clobber:

install: $(BASEFILES)

$(BASEFILES): $(BASELINEDIR)

$(BASELINEDIR):
	$(INS.dir)

$(BASELINEDIR)/%.txt: %.txt
	$(INS.file)
