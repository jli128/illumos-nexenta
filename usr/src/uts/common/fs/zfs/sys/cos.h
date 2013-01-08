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

#ifndef _SYS_COS_H
#define	_SYS_COS_H

#include <sys/avl.h>
#include <sys/zfs_context.h>
#include <sys/nvpair.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#include <sys/fs/zfs.h>

#ifdef	__cplusplus
extern "C" {
#endif

#define	MAXCOSNAMELEN	(30)


/*
 * This is the interface to SPA Class Of Storage related functionality.
 * For more details, see comments in cos_impl.h
 */

/*
 * Forward declaration
 */
typedef struct cos cos_t;

void spa_cos_enter(spa_t *);
void spa_cos_exit(spa_t *);
void spa_cos_init(spa_t *);
void spa_cos_fini(spa_t *);

int spa_alloc_cos(spa_t *, const char *, uint64_t);
int spa_free_cos(spa_t *, const char *);
int spa_list_cos(spa_t *, nvlist_t *);
int spa_cos_prop_set(spa_t *, const char *, nvlist_t *);
int spa_cos_prop_get(spa_t *, const char *, nvlist_t **);

cos_t *spa_lookup_cos_by_id(spa_t *, uint64_t);
cos_t *spa_lookup_cos_by_name(spa_t *, const char *);

int spa_load_cos_props(spa_t *);

void cos_hold(cos_t *cos);
void cos_rele(cos_t *cos);

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_COS_H */
