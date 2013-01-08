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
 * ZFS Class of Storage is:
 *
 * 1. descriptor that combines user and system defined properties to control
 * ZFS IO pipeline stages and, effectively, propagate user data and/or
 * associated metadata to/from stable storage differently - on a per CoS basis
 *
 * 2. mechanism to match user data with a specific class (or classes) of disks
 * within the heterogeneous storage volume when placing the data
 *
 * The following COS attributes are supported:
 *  - min_pending/max_pending to control queue depths of the devices
 *  - preferred_read - weight for biasing reads (e.g. if vdev is a side
 *    of a mirror)
 *  - unmap_freed - whether to unmap unused space (e.g. TRIM)
 */


#ifndef _SYS_COS_IMPL_H
#define	_SYS_COS_IMPL_H

#include <sys/cos.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * in core cos structure
 */
struct cos {
	spa_t		*cos_spa;

	uint64_t	cos_id;
	uint64_t	cos_min_pending;
	uint64_t	cos_max_pending;
	uint64_t	cos_preferred_read;
	boolean_t	cos_unmap_freed;
	char		cos_name[MAXCOSNAMELEN];	/* user defined name */

	kmutex_t	cos_lock;
	uint64_t	cos_refcnt; /* simple ref count - needs atomic ops */
	list_node_t	cos_list_node;
};

#define	COS_ID		"cos_id"
#define	COS_MINPENDING	"cos_minpending"
#define	COS_MAXPENDING	"cos_maxpending"
#define	COS_PREFREAD	"cos_prefread"
#define	COS_UNMAPFREED	"cos_unmapfreed"
#define	COS_NAME	"cos_name"

/*
 * Persist cos properties if ALL_PROPS_PERSISTENT is defined.
 * At this time, we do not want to introduce incompatible persistent
 * data formats, so ALL_PROPS_PERSISTENT is not refined, and the
 * properties are not persisted.
 */
#ifdef	ALL_PROPS_PERSISTENT
#define	COS_PROPS_PERSISTENT
#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_COS_IMPL_H */
