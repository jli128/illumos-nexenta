/*
 * This file and its contents are supplied under the terms of the
 * Common Development and Distribution License ("CDDL"), version 1.0.
 * You may only use this file in accordance with the terms of version
 * 1.0 of the CDDL.
 *
 * A full copy of the text of the CDDL should have accompanied this
 * source.  A copy of the CDDL is also available via the Internet at
 * http://www.illumos.org/license/CDDL.
 */

/*
 * Copyright 2013 Nexenta Systems, Inc.  All rights reserved.
 */

/*
 * These are stubs for nza-closed functions to satisfy linker mapfile deps
 */

/*
 * Closed enums
 */
#include <sys/types.h>
#include <zfs_prop.h>
#include <sys/spa.h>
#include <sys/spa_impl.h>
#include <sys/special.h>

static const char *undefined = "undefined";

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

zprop_desc_t *
vdev_prop_get_table(void)
{
	return (zfs_prop_get_table());
}

zprop_desc_t *
cos_prop_get_table(void)
{
	return (zfs_prop_get_table());
}

/* ARGSUSED */
zprop_type_t
vdev_prop_get_type(vdev_prop_t prop)
{
	return (PROP_TYPE_STRING);
}

/* ARGSUSED */
zprop_type_t
cos_prop_get_type(cos_prop_t prop)
{
	return (PROP_TYPE_STRING);
}

void
vdev_prop_init(void)
{
}

void
cos_prop_init(void)
{
}

/* ARGSUSED */
metaslab_class_t *
spa_select_class(spa_t *spa, zio_prop_t *io_prop)
{
	return (spa->spa_normal_class);
}

/* ARGSUSED */
boolean_t
stop_wrc_thread(spa_t *dummy)
{
	return (B_TRUE);
}

void
start_wrc_thread(spa_t *dummy)
{
}

/* ARGSUSED */
void
spa_cos_enter(spa_t *dummy)
{
}

/* ARGSUSED */
void
spa_cos_exit(spa_t *dummy)
{
}

/* ARGSUSED */
void
spa_cos_init(spa_t *dummy)
{
}

/* ARGSUSED */
void
spa_cos_fini(spa_t *dummy)
{
}

/* ARGSUSED */
spa_specialclass_id_t
spa_specialclass_id(spa_t *dummy)
{
	return (SPA_SPECIALCLASS_ZIL);
}

/* ARGSUSED */
void
spa_check_special(spa_t *dummy)
{
}

/* ARGSUSED */
void
spa_set_specialclass(spa_t *dummy, spa_specialclass_id_t dummy2)
{
}

/* ARGSUSED */
int
vdev_load_props(spa_t *dummy)
{
	return (0);
}

/* ARGSUSED */
int
spa_load_cos_props(spa_t *dummy)
{
	return (0);
}

/* ARGSUSED */
boolean_t
spa_watermark_none(spa_t *dummy)
{
	return (B_TRUE);
}

/* ARGSUSED */
boolean_t
spa_special_enabled(spa_t *dummy)
{
	return (B_FALSE);
}

void
zio_parallel_checksum_init()
{
}

void
zio_parallel_checksum_fini()
{
}
