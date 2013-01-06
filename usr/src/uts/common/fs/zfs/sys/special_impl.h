/*
 * Copyright 2013 Nexenta Systems, Inc.  All rights reserved.
 */

/*
 * See special.c for details on the theory of operation
 */

#ifndef _SYS_SPECIAL_IMPL_H
#define	_SYS_SPECIAL_IMPL_H

#include <sys/special.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Special Class descriptor
 */
struct spa_specialclass {
	spa_specialclass_id_t	sc_id;
	uint64_t		sc_flags;
	uint64_t		sc_mask;
};

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_SPECIAL_IMPL_H */
