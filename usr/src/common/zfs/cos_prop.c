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
 * Copyright 2012 Nexenta Systems, Inc. All rights reserved.
 */

#include <sys/zio.h>
#include <sys/spa.h>
#include <sys/zfs_ioctl.h>
#include <sys/fs/zfs.h>

#include "zfs_prop.h"

#if defined(_KERNEL)
#include <sys/systm.h>
#else
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#endif

static zprop_desc_t cos_prop_table[COS_NUM_PROPS];

zprop_desc_t *
cos_prop_get_table(void)
{
	return (cos_prop_table);
}

void
cos_prop_init(void)
{
	static zprop_index_t boolean_table[] = {
		{ "off",	0},
		{ "on",		1},
		{ NULL }
	};

	zprop_register_number(COS_PROP_ID, "cosid", 0, PROP_READONLY,
	    ZFS_TYPE_COS, "<cos system id>", "COSID");

	/* string properties */
	zprop_register_string(COS_PROP_NAME, "cosname", NULL, PROP_DEFAULT,
	    ZFS_TYPE_COS, "<cos user defined name>", "COSNAME");

	zprop_register_index(COS_PROP_UNMAP_FREED, "unmap", 0,
	    PROP_DEFAULT, ZFS_TYPE_COS, "on | off", "UNMAP", boolean_table);

	/* default number properties */
	zprop_register_number(COS_PROP_PREFERRED_READ, "prefread", 0,
	    PROP_DEFAULT, ZFS_TYPE_COS, "<preferred read (0..10)>", "PREFREAD");
	zprop_register_number(COS_PROP_MINPENDING, "minpending", 0,
	    PROP_DEFAULT, ZFS_TYPE_COS, "<min pending (0..100)>", "MINPENDING");
	zprop_register_number(COS_PROP_MAXPENDING, "maxpending", 0,
	    PROP_DEFAULT, ZFS_TYPE_COS, "<max pending (0..100)>",
	    "MAXPENDING");
}

/*
 * Given a property name and its type, returns the corresponding property ID.
 */
cos_prop_t
cos_name_to_prop(const char *propname)
{
	return (zprop_name_to_prop(propname, ZFS_TYPE_COS));
}

/*
 * Given a cos property ID, returns the corresponding name.
 * Assuming the cos propety ID is valid.
 */
const char *
cos_prop_to_name(cos_prop_t prop)
{
	return (cos_prop_table[prop].pd_name);
}

zprop_type_t
cos_prop_get_type(cos_prop_t prop)
{
	return (cos_prop_table[prop].pd_proptype);
}

boolean_t
cos_prop_readonly(cos_prop_t prop)
{
	return (cos_prop_table[prop].pd_attr == PROP_READONLY);
}

const char *
cos_prop_default_string(cos_prop_t prop)
{
	return (cos_prop_table[prop].pd_strdefault);
}

uint64_t
cos_prop_default_numeric(cos_prop_t prop)
{
	return (cos_prop_table[prop].pd_numdefault);
}

int
cos_prop_string_to_index(cos_prop_t prop, const char *string,
    uint64_t *index)
{
	return (zprop_string_to_index(prop, string, index, ZFS_TYPE_COS));
}

int
cos_prop_index_to_string(cos_prop_t prop, uint64_t index,
    const char **string)
{
	return (zprop_index_to_string(prop, index, string, ZFS_TYPE_COS));
}

uint64_t
cos_prop_random_value(cos_prop_t prop, uint64_t seed)
{
	return (zprop_random_value(prop, seed, ZFS_TYPE_COS));
}

#ifndef _KERNEL

const char *
cos_prop_values(cos_prop_t prop)
{
	return (cos_prop_table[prop].pd_values);
}

const char *
cos_prop_column_name(cos_prop_t prop)
{
	return (cos_prop_table[prop].pd_colname);
}

boolean_t
cos_prop_align_right(cos_prop_t prop)
{
	return (cos_prop_table[prop].pd_rightalign);
}
#endif
