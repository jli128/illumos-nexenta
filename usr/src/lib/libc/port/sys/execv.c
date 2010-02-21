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
 * Copyright 2008 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

#pragma ident	"%Z%%M%	%I%	%E% SMI"

/*
 *	execv(file, argv)
 *
 *	where argv is a vector argv[0] ... argv[x], NULL
 *	last vector element must be NULL
 *	environment passed automatically
 */

#pragma weak _execv = execv
#define execve _execve

#include "lint.h"
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

extern int __execve(const char *file, char *const *argv, char *const *envp);

#pragma weak execve = _execve
int execve(const char *file, char *const *argv, char *const *envp)
{
	char path[256];

	if (getenv("SUN_PERSONALITY")) {
		if (strncmp(file, "/usr/bin", strlen("/usr/bin")) == 0 ||
		    strncmp(file, "/bin", strlen("/bin")) == 0) {
			*path = 0;
			if (getenv("NLU_ENABLED"))
				strcpy(path, "/tmp/nlubin/sun");
			else
				strcpy(path, "/usr/sun/bin");
			strcat(path, strrchr(file, '/'));
			if (access(path, F_OK) == 0)
				file = path;
		} else if (strncmp(file, "/usr/sbin", strlen("/usr/sbin")) == 0 ||
			   strncmp(file, "/sbin", strlen("/sbin")) == 0) {
			*path = 0;
			if (getenv("NLU_ENABLED"))
				strcpy(path, "/tmp/nlubin/sun");
			else
				strcpy(path, "/usr/sun/sbin");
			strcat(path, strrchr(file, '/'));
			if (access(path, F_OK) == 0)
				file = path;
		}
	}
	if (getenv("NLU_ENABLED")) {
		/* if SUN_PERSONALITY succeeded above we simply fall
		 * through... */
		if (strncmp(file, "/usr/bin", strlen("/usr/bin")) == 0 ||
		    strncmp(file, "/bin", strlen("/bin")) == 0 ||
		    strncmp(file, "/usr/sbin", strlen("/usr/sbin")) == 0 ||
		    strncmp(file, "/sbin", strlen("/sbin")) == 0) {
			*path = 0;
			strcpy(path, "/tmp/nlubin");
			strcat(path, strrchr(file, '/'));
			if (access(path, F_OK) == 0)
				file = path;
		}
	}

	return (__execve(file, argv, envp));
}

int
execv(const char *file, char *const argv[])
{
	extern  char    **_environ;
	return (execve(file, argv, _environ));
}
