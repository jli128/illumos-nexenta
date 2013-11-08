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
 * Copyright 2012 Nexenta Systems, Inc.  All rights reserved.
 */

#include <sys/dmu_tx.h>
#include <sys/zap.h>
#include <sys/dsl_synctask.h>
#include <sys/spa_impl.h>
#include <sys/cos_impl.h>
#include <sys/dbuf.h>
#include <sys/debug.h>
#include <sys/zfeature.h>
#include "zfs_prop.h"

static int spa_alloc_cos_nosync(spa_t *spa, const char *cosname,
    uint64_t cosid);
static int cos_set_common(cos_t *cos, const char *strval, uint64_t ival,
    cos_prop_t prop);
static int cos_get_common(cos_t *cos, char **value, uint64_t *oval,
    cos_prop_t prop);

typedef boolean_t (*cos_check_func_t)(cos_t *, void *);

void
spa_cos_enter(spa_t *spa)
{
	mutex_enter(&spa->spa_cos_props_lock);
}

void
spa_cos_exit(spa_t *spa)
{
	mutex_exit(&spa->spa_cos_props_lock);
}

static boolean_t
cos_check_id(cos_t *cos, void *check_data)
{
	uint64_t id = (uint64_t)(unsigned long)check_data;
	return (cos->cos_id == id);
}

static boolean_t
cos_check_name(cos_t *cos, void *check_data)
{
	const char *name = (const char *)check_data;
	return (strncmp(cos->cos_name, name, MAXCOSNAMELEN-1) == 0);
}

static cos_t *
spa_lookup_cos_generic(spa_t *spa, cos_check_func_t cos_check_f,
    void *check_data)
{
	cos_t *cos;

	ASSERT(spa);
	ASSERT(cos_check_f);

	for (cos = list_head(&spa->spa_cos_list); cos != NULL;
	    cos = list_next(&spa->spa_cos_list, cos)) {
		if (cos_check_f(cos, check_data))
			break;
	}

	return (cos);
}

cos_t *
spa_lookup_cos_by_id(spa_t *spa, uint64_t id)
{
	ASSERT(spa);
	return (spa_lookup_cos_generic(spa, cos_check_id,
	    (void *)(unsigned long)id));
}

cos_t *
spa_lookup_cos_by_name(spa_t *spa, const char *name)
{
	ASSERT(spa);
	if (name == NULL)
		return (NULL);
	return (spa_lookup_cos_generic(spa, cos_check_name, (void *)name));
}

uint64_t
cos_refcount(cos_t *cos)
{
	ASSERT(cos);
	return (cos->cos_refcnt);
}

void
cos_hold(cos_t *cos)
{
	ASSERT(cos);

	mutex_enter(&cos->cos_lock);
	atomic_inc_64(&cos->cos_refcnt);
	mutex_exit(&cos->cos_lock);
}

void
cos_rele(cos_t *cos)
{
	ASSERT(cos);
	mutex_enter(&cos->cos_lock);
	if (cos->cos_refcnt > 0)
		atomic_dec_64(&cos->cos_refcnt);
	mutex_exit(&cos->cos_lock);
}

static int
cos_set_common(cos_t *cos, const char *strval, uint64_t ival, cos_prop_t prop)
{
	ASSERT(cos);

	switch (prop) {
	case COS_PROP_NAME:
		(void) snprintf(cos->cos_name, MAXCOSNAMELEN,
		    "%s", strval);
		break;

	case COS_PROP_UNMAP_FREED:
		cos->cos_unmap_freed = (boolean_t)ival;
		break;

	case COS_PROP_PREFERRED_READ:
		cos->cos_preferred_read = (boolean_t)ival;
		break;

	case COS_PROP_MINPENDING:
		cos->cos_min_pending = ival;
		break;

	case COS_PROP_MAXPENDING:
		cos->cos_max_pending = ival;
		break;

	default:
		return (EINVAL);
	}

	return (0);
}

/*ARGSUSED*/
static int
cos_get_common(cos_t *cos, char **value, uint64_t *oval, cos_prop_t prop)
{
	ASSERT(cos);

	switch (prop) {
	case COS_PROP_ID:
		*oval = cos->cos_id;
		break;

	case COS_PROP_NAME:
		if (cos->cos_name[0] != '\0')
			*value = spa_strdup(cos->cos_name);
		break;

	case COS_PROP_UNMAP_FREED:
		*oval = cos->cos_unmap_freed;
		break;

	case COS_PROP_PREFERRED_READ:
		*oval = cos->cos_preferred_read;
		break;

	case COS_PROP_MINPENDING:
		*oval = cos->cos_min_pending;
		break;

	case COS_PROP_MAXPENDING:
		*oval = cos->cos_max_pending;
		break;

	default:
		return (EINVAL);
	}

	return (0);
}

/* ARGSUSED */
static boolean_t
cos_count(cos_t *cos, void *cntp)
{
	spa_t *spa = cos->cos_spa;
	size_t *counterp = cntp;

	ASSERT(MUTEX_HELD(&spa->spa_cos_props_lock));

	(*counterp)++;

	/*
	 * The B_FALSE is an indication to spa_lookup_cos_generic()
	 * to continue iterating along the cos list
	 */
	return (B_FALSE);
}

static void
cos_sync_classes(spa_t *spa, uint64_t obj, dmu_tx_t *tx)
{
	dmu_buf_t *db;
	nvlist_t *nvl;
	nvlist_t **nvl_arr;
	cos_t *cos;
	char *buf;
	size_t bufsize = 0;
	size_t packedsize = 0;
	uint_t nvl_arr_sz;
	size_t num_classes = 0;
	int i;

	if (list_is_empty(&spa->spa_cos_list))
		goto empty;

	(void) spa_lookup_cos_generic(spa, cos_count, &num_classes);
	nvl_arr_sz = num_classes * sizeof (void *);

	if ((nvl_arr = kmem_alloc(nvl_arr_sz, KM_SLEEP)) == NULL)
		return;

	VERIFY(0 == nvlist_alloc(&nvl, NV_UNIQUE_NAME, KM_SLEEP));

	for (i = 0, cos = list_head(&spa->spa_cos_list); cos != NULL;
	    cos = list_next(&spa->spa_cos_list, cos), i++) {
		VERIFY(0 == nvlist_alloc(&nvl_arr[i], NV_UNIQUE_NAME,
		    KM_SLEEP));

		VERIFY(0 == nvlist_add_uint64(nvl_arr[i],
		    COS_ID, cos->cos_id));
		VERIFY(0 == nvlist_add_string(nvl_arr[i],
		    COS_NAME, cos->cos_name));

		if (cos->cos_preferred_read > 0)
			VERIFY(0 == nvlist_add_uint64(nvl_arr[i],
			    COS_PREFREAD, cos->cos_preferred_read));
		if (cos->cos_min_pending > 0)
			VERIFY(0 == nvlist_add_uint64(nvl_arr[i],
			    COS_MINPENDING, cos->cos_min_pending));
		if (cos->cos_max_pending > 0)
			VERIFY(0 == nvlist_add_uint64(nvl_arr[i],
			    COS_MAXPENDING, cos->cos_max_pending));
		if (cos->cos_unmap_freed)
			VERIFY(0 == nvlist_add_boolean_value(nvl_arr[i],
			    COS_UNMAPFREED, cos->cos_unmap_freed));
	}

	VERIFY(0 == nvlist_add_nvlist_array(nvl, "COS_ARRAY",
	    nvl_arr, num_classes));
	VERIFY(0 == nvlist_size(nvl, &packedsize, NV_ENCODE_XDR));

	bufsize = P2ROUNDUP((uint64_t)packedsize, SPA_MINBLOCKSIZE);
	buf = kmem_alloc(bufsize, KM_SLEEP);

	bzero(buf + packedsize, bufsize - packedsize);

	VERIFY(0 == nvlist_pack(nvl, &buf, &packedsize, NV_ENCODE_XDR,
	    KM_SLEEP));

	dmu_write(spa->spa_meta_objset, obj, 0, bufsize, buf, tx);

	kmem_free(buf, bufsize);
	for (i = 0; i < num_classes; i++)
		nvlist_free(nvl_arr[i]);
	kmem_free(nvl_arr, nvl_arr_sz);

empty:
	VERIFY(0 == dmu_bonus_hold(spa->spa_meta_objset, obj, FTAG, &db));
	dmu_buf_will_dirty(db, tx);
	*(uint64_t *)db->db_data = (uint64_t)packedsize;
	dmu_buf_rele(db, FTAG);
}

static void
cos_sync_props(void *arg1, dmu_tx_t *tx)
{
	spa_t *spa = arg1;
	objset_t *mos = spa->spa_meta_objset;

	spa_cos_enter(spa);

	if (spa->spa_cos_props_object == 0) {
		VERIFY((spa->spa_cos_props_object =
		    dmu_object_alloc(mos, DMU_OT_COS_PROPS, 0,
		    DMU_OT_COS_PROPS_SIZE, 8, tx)) > 0);

		VERIFY(zap_update(mos,
		    DMU_POOL_DIRECTORY_OBJECT,
		    DMU_POOL_COS_PROPS,
		    8, 1, &spa->spa_cos_props_object, tx) == 0);
	}
	cos_sync_classes(spa, spa->spa_cos_props_object, tx);

	spa_cos_exit(spa);
}

int
spa_load_cos_props(spa_t *spa)
{
	objset_t *mos = spa->spa_meta_objset;
	dmu_buf_t *db;
	nvlist_t *nvl = NULL;
	nvlist_t **nvl_arr;
	uint64_t packedsize;
	void *buf = NULL;
	uint_t n;
	int i;

	ASSERT(spa);

	if (spa->spa_cos_props_object == 0)
		return (ENOENT);

	spa_cos_enter(spa);

	VERIFY(0 == dmu_bonus_hold(mos, spa->spa_cos_props_object, FTAG, &db));
	packedsize = *(uint64_t *)db->db_data;
	dmu_buf_rele(db, FTAG);

	if (packedsize == 0) {
		spa_cos_exit(spa);
		return (0);
	}

	buf = kmem_alloc(packedsize, KM_SLEEP);

	VERIFY(0 == dmu_read(mos, spa->spa_cos_props_object,
	    0, packedsize, buf, DMU_READ_NO_PREFETCH));

	VERIFY(0 == nvlist_unpack(buf, packedsize, &nvl, 0));
	VERIFY(0 == nvlist_lookup_nvlist_array(nvl, "COS_ARRAY",
	    &nvl_arr, &n));

	for (i = 0; i < n; i++) {
		cos_t *cos = NULL;
		char *strval;
		uint64_t u64;
		boolean_t bv;

		if (nvlist_lookup_uint64(nvl_arr[i], COS_ID, &u64) == 0) {
			cos =  spa_lookup_cos_by_id(spa, u64);
			if (cos == NULL) {
				if (nvlist_lookup_string(nvl_arr[i], COS_NAME,
				    &strval) != 0)
					continue;

				if (spa_alloc_cos_nosync(spa, strval, u64) != 0)
					continue;
				cos =  spa_lookup_cos_by_name(spa, strval);
			}
		}

		if (cos == NULL)
			continue;

		if (nvlist_lookup_uint64(nvl_arr[i], COS_PREFREAD, &u64) == 0)
			cos->cos_preferred_read = u64;
		if (nvlist_lookup_uint64(nvl_arr[i], COS_MINPENDING, &u64) == 0)
			cos->cos_min_pending = u64;
		if (nvlist_lookup_uint64(nvl_arr[i], COS_MAXPENDING, &u64) == 0)
			cos->cos_max_pending = u64;
		if (nvlist_lookup_boolean_value(nvl_arr[i], COS_UNMAPFREED, &bv)
		    == 0)
			cos->cos_unmap_freed = bv;
	}

	spa_cos_exit(spa);

	nvlist_free(nvl);
	kmem_free(buf, packedsize);

	return (0);
}

int
cos_prop_validate(spa_t *spa, uint64_t id, nvlist_t *props)
{
	nvpair_t *elem;
	int error = 0;

	elem = NULL;
	while ((elem = nvlist_next_nvpair(props, elem)) != NULL) {
		cos_prop_t prop;
		char *propname;
		char *strval;
		uint64_t intval;
		cos_t *cos;

		propname = nvpair_name(elem);

		if ((prop = cos_name_to_prop(propname)) == ZPROP_INVAL)
			return (EINVAL);

		switch (prop) {
		case COS_PROP_NAME:
			error = nvpair_value_string(elem, &strval);
			if (error)
				break;

			/*
			 * Cannot exceed max length of cos name
			 * and cannot be same as existing one
			 */
			if ((strnlen(strval, MAXCOSNAMELEN) == MAXCOSNAMELEN) ||
			    (((cos = spa_lookup_cos_by_name(spa, strval)) !=
			    NULL) && cos->cos_id != id))
				error = EINVAL;
			break;
		case COS_PROP_UNMAP_FREED:
			break;
		case COS_PROP_PREFERRED_READ:
		case COS_PROP_MINPENDING:
			error = nvpair_value_uint64(elem, &intval);
			if (!error && intval > 100)
				error = EINVAL;
			break;

		case COS_PROP_MAXPENDING:
			error = nvpair_value_uint64(elem, &intval);
			if (!error && intval > 100)
				error = EINVAL;
			break;

		default:
			error = EINVAL;
		}

		if (error)
			break;
	}

	return (error);
}

/* ARGSUSED */
int
cos_sync_task_do(spa_t *spa)
{
	int err = 0;
	err = dsl_sync_task(spa->spa_name, NULL, cos_sync_props, spa, 3);
	return (err);
}

int
spa_cos_prop_set(spa_t *spa, const char *cosname, nvlist_t *nvp)
{
	cos_t *cos;
	nvpair_t *elem;
	boolean_t need_sync = B_FALSE;
	cos_prop_t prop;
	uint64_t intval;
	char *strval;
	zprop_type_t proptype;

	ASSERT(spa_writeable(spa));

	spa_cos_enter(spa);
	cos = spa_lookup_cos_by_name(spa, cosname);
	if (cos == NULL) {
		spa_cos_exit(spa);
		return (ENOENT);
	}

	elem = NULL;
	while ((elem = nvlist_next_nvpair(nvp, elem)) != NULL) {
		if ((prop = cos_name_to_prop(
		    nvpair_name(elem))) == ZPROP_INVAL)
			return (EINVAL);

		need_sync = B_TRUE;
		proptype = cos_prop_get_type(prop);

		if (nvpair_type(elem) == DATA_TYPE_STRING) {
			ASSERT(proptype == PROP_TYPE_STRING);
			VERIFY(nvpair_value_string(elem, &strval) == 0);

		} else if (nvpair_type(elem) == DATA_TYPE_UINT64) {
			VERIFY(nvpair_value_uint64(elem, &intval) == 0);

			if (proptype == PROP_TYPE_INDEX) {
				const char *unused;
				VERIFY(cos_prop_index_to_string(
				    prop, intval, &unused) == 0);
			}
		} else {
			ASSERT(0); /* not allowed */
		}

		(void) cos_set_common(cos, strval, intval, prop);
	}
	spa_cos_exit(spa);

	if (need_sync)
		return (cos_sync_task_do(spa));
	return (0);
}

int
spa_cos_prop_get(spa_t *spa, const char *cosname, nvlist_t **nvp)
{
	cos_t *cos;
	int err;
	cos_prop_t prop;

	VERIFY(nvlist_alloc(nvp, NV_UNIQUE_NAME, KM_SLEEP) == 0);

	spa_cos_enter(spa);
	cos = spa_lookup_cos_by_name(spa, cosname);
	if (cos == NULL) {
		spa_cos_exit(spa);
		return (ENOENT);
	}

	for (prop = COS_PROP_ID; prop < COS_NUM_PROPS; prop++) {
		uint64_t ival;
		char *strval = NULL;
		const char *propname = cos_prop_to_name(prop);

		if ((err = cos_get_common(cos, &strval, &ival, prop)) != 0)
			return (err);

		if (strval != NULL) {
			VERIFY(nvlist_add_string(*nvp, propname, strval) == 0);
			spa_strfree(strval);
		} else {
			VERIFY(nvlist_add_uint64(*nvp, propname, ival) == 0);
		}
	}
	spa_cos_exit(spa);

	return (0);
}

static uint64_t
generate_cos_id(spa_t *spa)
{
	uint64_t guid = 0;

	do {
		(void) random_get_pseudo_bytes((uint8_t *)&guid, sizeof (guid));
	} while (guid != 0 && spa_lookup_cos_by_id(spa, guid) != NULL);

	return (guid);
}

static int
spa_alloc_cos_nosync(spa_t *spa, const char *cosname, uint64_t cosid)
{
	cos_t *cos;

	ASSERT(spa);
	ASSERT(MUTEX_HELD(&spa->spa_cos_props_lock));

	cos = kmem_zalloc(sizeof (cos_t), KM_SLEEP);
	if (cos == NULL)
		return (ENOMEM);

	if (spa_lookup_cos_by_name(spa, cosname) != NULL)
		return (EINVAL);

	cos->cos_id = (cosid != 0 ? cosid : generate_cos_id(spa));
	if (cos->cos_id == 0)
		return (ENOSPC);

	cos->cos_spa = spa;
	(void) strlcpy(cos->cos_name, cosname, MAXCOSNAMELEN);
	mutex_init(&cos->cos_lock, NULL, MUTEX_DEFAULT, NULL);

	list_insert_tail(&spa->spa_cos_list, cos);

	return (0);
}

int
spa_alloc_cos(spa_t *spa, const char *cosname, uint64_t cosid)
{
	int err;
	dmu_tx_t *tx;

	if (!spa_feature_is_enabled(spa, SPA_FEATURE_COS_ATTR))
		return (ENOTSUP);

	spa_cos_enter(spa);
	err = spa_alloc_cos_nosync(spa, cosname, cosid);
	spa_cos_exit(spa);
	if (err)
		return (err);

	tx = dmu_tx_create_dd(spa->spa_dsl_pool->dp_mos_dir);
	VERIFY(0 == dmu_tx_assign(tx, TXG_WAIT));
	spa_feature_incr(spa, SPA_FEATURE_COS_ATTR, tx);
	dmu_tx_commit(tx);

	return (cos_sync_task_do(spa));
}

int
spa_free_cos(spa_t *spa, const char *cosname)
{
	int err = 0;
	cos_t *cos;

	ASSERT(spa);

	spa_cos_enter(spa);

	if ((cos = spa_lookup_cos_by_name(spa, cosname)) != NULL) {
		if (cos_refcount(cos) == 0)
			list_remove(&spa->spa_cos_list, cos);
		else
			err = EBUSY;
	} else {
		err = EINVAL;
	}

	spa_cos_exit(spa);

	if (err == 0) {
		dmu_tx_t *tx;

		kmem_free(cos, sizeof (cos_t));
		tx = dmu_tx_create_dd(spa->spa_dsl_pool->dp_mos_dir);
		VERIFY(0 == dmu_tx_assign(tx, TXG_WAIT));
		spa_feature_decr(spa, SPA_FEATURE_COS_ATTR, tx);
		dmu_tx_commit(tx);

		return (cos_sync_task_do(spa));
	}

	return (err);
}

int
spa_list_cos(spa_t *spa, nvlist_t *nvl)
{
	cos_t *cos;
	int err = 0;

	ASSERT(spa);

	spa_cos_enter(spa);

	for (cos = list_head(&spa->spa_cos_list); cos != NULL;
	    cos = list_next(&spa->spa_cos_list, cos)) {
		/* FIXME: modify to include all props */
		VERIFY(nvlist_add_uint64(nvl, cos->cos_name, cos->cos_id) == 0);
	}
	spa_cos_exit(spa);

	return (err);
}

void
spa_cos_init(spa_t *spa)
{
	ASSERT(spa);

	spa_cos_enter(spa);
	list_create(&spa->spa_cos_list, sizeof (cos_t), offsetof(cos_t,
	    cos_list_node));
	spa_cos_exit(spa);
}

/* ARGSUSED */
static boolean_t
cos_remove(cos_t *cos, void *check_data)
{
	spa_t *spa = cos->cos_spa;

	ASSERT(MUTEX_HELD(&spa->spa_cos_props_lock));

	list_remove(&spa->spa_cos_list, cos);
	kmem_free(cos, sizeof (cos_t));

	return (B_FALSE);
}

void
spa_cos_fini(spa_t *spa)
{
	ASSERT(spa);

	spa_cos_enter(spa);
	(void) spa_lookup_cos_generic(spa, cos_remove, NULL);
	list_destroy(&spa->spa_cos_list);
	spa_cos_exit(spa);
}
