#
# This file and its contents are supplied under the terms of the
# Common Development and Distribution License ("CDDL"), version 1.0.
# You may only use this file in accordance with the terms of version
# 1.0 of the CDDL.
#
# A full copy of the text of the CDDL should have accompanied this
# source.  A copy of the CDDL is also available via the Internet at
# http://www.illumos.org/license/CDDL.
#
#
# Copyright (c) 2012 by Delphix. All rights reserved.
# Copyright 2014 Nexenta Systems, Inc.  All rights reserved.
#

include $(SRC)/Makefile.master

ROOTOPTPKG = $(ROOT)/opt/lofi-tests
BASELINEDIR = $(ROOTOPTPKG)/baseline

BFILES = add_dynamic.txt		\
	add_dynamic.1.txt		\
	add_dynamic.2.txt		\
	add_dynamic.3.txt		\
	add_dynamic.4.txt		\
	add_dynamic.5.txt		\
	add_dynamic.6.txt		\
	add_dynamic.7.txt		\
	add_dynamic.8.txt		\
	add_dynamic.9.txt		\
	add_dynamic.10.txt		\
	add_dynamic.11.txt		\
	add_dynamic.12.txt		\
	add_dynamic.13.txt		\
	add_dynamic.14.txt		\
	add_dynamic.15.txt		\
	add_dynamic.16.txt		\
	add_dynamic.17.txt		\
	add_dynamic.18.txt		\
	add_dynamic.19.txt		\
	add_dynamic.20.txt		\
	add_dynamic.21.txt		\
	add_dynamic.22.txt		\
	add_dynamic.23.txt		\
	add_dynamic.24.txt		\
	add_dynamic.25.txt		\
	add_dynamic.26.txt		\
	add_dynamic.27.txt		\
	add_dynamic.28.txt		\
	add_static.txt			\
	add_static.1.txt		\
	compression_dynamic.txt		\
	compression_dynamic.1.txt	\
	compression_static.txt		\
	compression_static.1.txt	\
	compression_static.2.txt	\
	compression_static.3.txt	\
	compression_static.4.txt	\
	compression_static.5.txt	\
	compression_static.6.txt	\
	compression_static.7.txt	\
	delete.txt			\
	delete.1.txt			\
	delete.2.txt			\
	fs_dynamic.txt			\
	fs_dynamic.1.txt		\
	fs_dynamic.2.txt		\
	fs_dynamic.3.txt		\
	fs_dynamic.4.txt		\
	fs_dynamic.5.txt		\
	fs_dynamic.6.txt		\
	fs_dynamic.7.txt		\
	fs_dynamic.8.txt		\
	fs_dynamic.9.txt		\
	getsysinfo.txt			\
	getsysinfo.1.txt		\
	summary.txt			\
	zones_named.txt			\
	zones_named.1.txt

BASEFILES = $(BFILES:%=$(BASELINEDIR)/%)
$(BASEFILES) := FILEMODE = 0444

all lint clean clobber:

install: $(BASEFILES)

$(BASEFILES): $(BASELINEDIR)

$(BASELINEDIR):
	$(INS.dir)

$(BASELINEDIR)/%.txt: %.txt
	$(INS.file)
