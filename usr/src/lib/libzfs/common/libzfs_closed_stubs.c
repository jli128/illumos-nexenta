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
 * Copyright 2012 Nexenta Systems, Inc.  All rights reserved.
 */

/*
 * These are stubs for nza-closed functions to satisfy linker mapfile deps
 */

/*
 * Closed enums
 */
#include <sys/types.h>

typedef int vdev_prop_t;
typedef int cos_prop_t;

static char *undefined = "undefined";

/* ARGSUSED */
vdev_prop_t
vdev_name_to_prop(const char *propname)
{
	return (0);
}

/* ARGSUSED */
const char *
vdev_prop_to_name(vdev_prop_t prop)
{
	return (undefined);
}

/* ARGSUSED */
boolean_t
vdev_prop_readonly(vdev_prop_t prop)
{
	return (B_FALSE);
}

#ifndef _KERNEL
/* ARGSUSED */
const char *
vdev_prop_values(vdev_prop_t prop)
{
	return (undefined);
}
/* ARGSUSED */
const char *
vdev_prop_column_name(vdev_prop_t prop)
{
	return (undefined);
}

/* ARGSUSED */
boolean_t
vdev_prop_align_right(vdev_prop_t prop)
{
	return (B_FALSE);
}
#endif

/* ARGSUSED */
cos_prop_t
cos_name_to_prop(const char *propname)
{
	return (0);
}

/* ARGSUSED */
const char *
cos_prop_to_name(cos_prop_t prop)
{
	return (undefined);
}

/* ARGSUSED */
boolean_t
cos_prop_readonly(cos_prop_t prop)
{
	return (B_FALSE);
}

#ifndef _KERNEL
/* ARGSUSED */
const char *
cos_prop_values(cos_prop_t prop)
{
	return (undefined);
}

/* ARGSUSED */
const char *
cos_prop_column_name(cos_prop_t prop)
{
	return (undefined);
}

/* ARGSUSED */
boolean_t
cos_prop_align_right(cos_prop_t prop)
{
	return (B_FALSE);
}
#endif
