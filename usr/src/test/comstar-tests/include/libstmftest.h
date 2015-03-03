/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */

/*
 * Copyright 2009 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef _LIBSTMFTEST_H
#define	_LIBSTMFTEST_H

#ifdef	__cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <libintl.h>
#include <unistd.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <inttypes.h>
#include <libstmf.h>


/* DEFINES */

#define	VERSION_STRING_MAX_LEN  10

/*
 *  MAJOR - This should only change when there is an incompatible change made
 *  to the interfaces or the output.
 *
 *  MINOR - This should change whenever there is a new command or new feature
 *  with no incompatible change.
 */
#define	VERSION_STRING_MAJOR	"1"
#define	VERSION_STRING_MINOR	"0"
#define	MAX_DEVID_INPUT		256
#define	GUID_INPUT		32
#define	MAX_LU_NBR		16383
#define	ONLINE_LU		0
#define	OFFLINE_LU		1
#define	ONLINE_TARGET		2
#define	OFFLINE_TARGET		3

#define	MAX_WAIT_RETRIES	6000

/* maximum length of an option argument */
#define	MAXOPTARGLEN   256

/* globals */
char *cmdName;

/*
 * This structure is passed into the caller's callback function and
 * will contain a list of all options entered and their associated
 * option arguments if applicable
 */
typedef struct _cmdOptions {
	int optval;
	char optarg[MAXOPTARGLEN + 1];
} cmdOptions_t;

int waitForOffline();
int waitForOnline();
void guidToAscii(stmfGuid *guid, char *guidAsciiBuf);


#ifdef	__cplusplus
}
#endif

#endif /* _LIBSTMFTEST_H */
