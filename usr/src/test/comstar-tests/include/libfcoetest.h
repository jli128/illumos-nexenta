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

#ifndef _LIBFCOETEST_H
#define	_LIBFCOETEST_H

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
#include <libfcoe.h>
#include <libstmf.h>
#include <commontest.h>

/* DEFINES */
#define	WWN "wwn."
#define	FCOE_ATOB(x)	(((x) >= '0' && (x) <= '9') ? ((x) - '0') :\
			((x) >= 'a' && (x) <= 'f') ?\
			((x) - 'a' + 10) : ((x) - 'A' + 10))


/* globals */
typedef struct fcoe_status_list {
	int	errno;
	char	*msg;
} fcoe_status_list_t;

int fcoeCommonTargetCleanUp();
int fcoeCommonGetInterface(char **, int);
char *getFCOEStatus(int errno);
int WWN2MacName(FCOE_PORT_WWN *pwwn, FCOE_UINT8 portType,
    FCOE_UINT8 *macLinkName);
int stmfDevid2WWN(FCOE_PORT_WWN *pwwn, stmfDevid *devid);

#ifdef	__cplusplus
}
#endif

#endif /* _LIBFCOETEST_H */
