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

#include <sys/spa_impl.h>
#include <sys/vdev_impl.h>
#include <sys/cos_impl.h>
#include <sys/refcount.h>
#include <sys/dmu.h>
#include <sys/dsl_synctask.h>
#include <sys/zap.h>
#include <sys/zfeature.h>

#include <zfs_prop.h>

static void vdev_sync_props(void *arg1, dmu_tx_t *tx);

/*
 * The closed-source support for vdev-specific properties. Similar
 * properties can be set on per-pool and per-cos (class of storage)
 * basis; the functions below override the per-pool and per-cos
 * settings. In addition to the usual/standard path and fru,
 * the supported vdev properties include minpending/maxpending (request
 * queue length params), prefread (biases reads toward this device if
 * it is a part of a mirror, and cos (says which class vdev belongs).
 */

/*
 * Get specific property for a leaf-level vdev by property id or name.
 */
/*ARGSUSED*/
int
spa_vdev_get_common(spa_t *spa, uint64_t guid, char **value,
    uint64_t *oval, vdev_prop_t prop)
{
	vdev_t *vd;

	spa_vdev_state_enter(spa, SCL_ALL);

	if ((vd = spa_lookup_by_guid(spa, guid, B_TRUE)) == NULL)
		return (spa_vdev_state_exit(spa, NULL, ENOENT));

	if (!vd->vdev_ops->vdev_op_leaf)
		return (spa_vdev_state_exit(spa, NULL, ENOTSUP));

	switch (prop) {
	case VDEV_PROP_PATH:
		if (vd->vdev_path != NULL) {
			*value = spa_strdup(vd->vdev_path);
		}
		break;
	case VDEV_PROP_FRU:
		if (vd->vdev_fru != NULL) {
			*value = spa_strdup(vd->vdev_fru);
		}
		break;

	case VDEV_PROP_MINPENDING:
		*oval = vd->vdev_min_pending;
		break;

	case VDEV_PROP_MAXPENDING:
		*oval = vd->vdev_max_pending;
		break;

	case VDEV_PROP_PREFREAD:
		*oval = vd->vdev_preferred_read;
		break;

	case VDEV_PROP_COS:
		if (vd->vdev_cos != NULL)
			*value = spa_strdup(vd->vdev_cos->cos_name);
		else
			*value = NULL;
		break;

	case VDEV_PROP_SPAREGROUP:
		if (vd->vdev_spare_group != NULL)
			*value = spa_strdup(vd->vdev_spare_group);
		else
			*value = NULL;
		break;

	default:
		return (spa_vdev_state_exit(spa, NULL, ENOTSUP));
	}

	return (spa_vdev_state_exit(spa, NULL, 0));
}

/*
 * Update the stored property for this vdev.
 */
static int
spa_vdev_set_common(vdev_t *vd, const char *value,
    uint64_t ival, vdev_prop_t prop)
{
	spa_t *spa = vd->vdev_spa;
	cos_t *cos = NULL;
	boolean_t sync = B_FALSE;
	boolean_t reset_cos = B_FALSE;

	ASSERT(spa_writeable(spa));

	spa_vdev_state_enter(spa, SCL_ALL);

	if (!vd->vdev_ops->vdev_op_leaf)
		return (spa_vdev_state_exit(spa, NULL, ENOTSUP));

	switch (prop) {
	case VDEV_PROP_PATH:
		if (strcmp(value, vd->vdev_path) != 0) {
			spa_strfree(vd->vdev_path);
			vd->vdev_path = spa_strdup(value);
			sync = B_TRUE;
		}
		break;

	case VDEV_PROP_FRU:
		if (vd->vdev_fru == NULL) {
			vd->vdev_fru = spa_strdup(value);
			sync = B_TRUE;
		} else if (strcmp(value, vd->vdev_fru) != 0) {
			spa_strfree(vd->vdev_fru);
			vd->vdev_fru = spa_strdup(value);
			sync = B_TRUE;
		}
		break;

	case VDEV_PROP_MINPENDING:
		vd->vdev_min_pending = ival;
		break;

	case VDEV_PROP_MAXPENDING:
		vd->vdev_max_pending = ival;
		break;

	case VDEV_PROP_PREFREAD:
		vd->vdev_preferred_read = ival;
		break;

	case VDEV_PROP_COS:
		spa_cos_enter(spa);
		if ((value == NULL || value[0] == '\0') && ival == 0) {
			reset_cos = B_TRUE;
		} else {
			if (ival != 0)
				cos = spa_lookup_cos_by_id(spa, ival);
			else
				cos = spa_lookup_cos_by_name(spa, value);
		}


		if (!reset_cos && cos == NULL) {
			spa_cos_exit(spa);
			return (spa_vdev_state_exit(spa, NULL, ENOENT));
		}

		if (vd->vdev_cos != NULL)
			cos_rele(vd->vdev_cos);

		if (reset_cos) {
			vd->vdev_cos = NULL;
		} else {
			cos_hold(cos);
			vd->vdev_cos = cos;
		}
		spa_cos_exit(spa);
		break;

	case VDEV_PROP_SPAREGROUP:
		if (vd->vdev_spare_group == NULL) {
			vd->vdev_spare_group = spa_strdup(value);
			sync = B_TRUE;
		} else if (strcmp(value, vd->vdev_spare_group) != 0) {
			spa_strfree(vd->vdev_spare_group);
			vd->vdev_spare_group = spa_strdup(value);
			sync = B_TRUE;
		}
		break;

	default:
		return (spa_vdev_state_exit(spa, NULL, ENOTSUP));
	}

	return (spa_vdev_state_exit(spa, sync ? vd : NULL, 0));
}

/*
 * Closed source implementation of path/fru setting - called by open code
 */
int
spa_vdev_setpath(spa_t *spa, uint64_t guid, const char *newpath)
{
	vdev_t *vdev;

	if ((vdev = spa_lookup_by_guid(spa, guid, B_TRUE)) == NULL)
		return (ENOENT);

	return (spa_vdev_set_common(vdev, newpath, 0, VDEV_PROP_PATH));
}

int
spa_vdev_setfru(spa_t *spa, uint64_t guid, const char *newfru)
{
	vdev_t *vdev;

	if ((vdev = spa_lookup_by_guid(spa, guid, B_TRUE)) == NULL)
		return (ENOENT);

	return (spa_vdev_set_common(vdev, newfru, 0, VDEV_PROP_PATH));
}

/*
 * Check properties (names, values) in this nvlist
 */
/*ARGSUSED*/
int
spa_vdev_prop_validate(spa_t *spa, nvlist_t *props)
{
	nvpair_t *elem;
	int error = 0;

	if (!spa_feature_is_enabled(spa, SPA_FEATURE_VDEV_ATTR))
		return (ENOTSUP);

	elem = NULL;
	while ((elem = nvlist_next_nvpair(props, elem)) != NULL) {
		vdev_prop_t prop;
		char *propname;
		uint64_t ival;
		char *sval;

		propname = nvpair_name(elem);

		if ((prop = vdev_name_to_prop(propname)) == ZPROP_INVAL)
			return (EINVAL);

		switch (prop) {
		case VDEV_PROP_PATH:
		case VDEV_PROP_FRU:
			break;

		case VDEV_PROP_MINPENDING:
			error = nvpair_value_uint64(elem, &ival);
			if (!error && ival > 100)
				error = EINVAL;
			break;

		case VDEV_PROP_MAXPENDING:
			error = nvpair_value_uint64(elem, &ival);
			if (!error && ival > 100)
				error = EINVAL;
			break;

		case VDEV_PROP_PREFREAD:
			error = nvpair_value_uint64(elem, &ival);
			if (!error && ival > 10)
				error = EINVAL;
			break;

		case VDEV_PROP_COS:
			error = nvpair_value_string(elem, &sval);
			if (!error && sval[0] != '\0' &&
			    (strnlen(sval, MAXCOSNAMELEN) != MAXCOSNAMELEN)) {
				if ((spa_lookup_cos_by_name(spa, sval) == NULL))
					error = EINVAL;
			}
			break;

		case VDEV_PROP_SPAREGROUP:
			error = nvpair_value_string(elem, &sval);
			if (!error && sval[0] != '\0' &&
			    (strnlen(sval, MAXPATHLEN) == MAXPATHLEN))
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

/*
 * Set properties (names, values) in this nvlist, indicate if sync is needed
 */
static int
vdev_prop_set_nosync(vdev_t *vd, nvlist_t *nvp, boolean_t *needsyncp)
{
	int error = 0;
	nvpair_t *elem;
	vdev_prop_t prop;
	boolean_t need_sync = B_FALSE;

	elem = NULL;
	while ((elem = nvlist_next_nvpair(nvp, elem)) != NULL) {
		uint64_t ival = 0;
		char *strval = NULL;
		zprop_type_t proptype;
		if ((prop = vdev_name_to_prop(
		    nvpair_name(elem))) == ZPROP_INVAL)
			return (ENOTSUP);

		switch (prop) {
		case VDEV_PROP_MINPENDING:
		case VDEV_PROP_MAXPENDING:
		case VDEV_PROP_PREFREAD:
		case VDEV_PROP_COS:
		case VDEV_PROP_SPAREGROUP:
			need_sync = B_TRUE;
			break;
		default:
			need_sync = B_FALSE;
		}

		proptype = vdev_prop_get_type(prop);

		switch (proptype) {
		case PROP_TYPE_STRING:
			VERIFY(nvpair_value_string(elem, &strval) == 0);
			break;
		case PROP_TYPE_INDEX:
		case PROP_TYPE_NUMBER:
			VERIFY(nvpair_value_uint64(elem, &ival) == 0);
			if (proptype == PROP_TYPE_INDEX) {
				const char *unused;
				VERIFY(vdev_prop_index_to_string(
				    prop, ival, &unused) == 0);
			}
		}

		error = spa_vdev_set_common(vd, strval, ival, prop);
	}

	if (needsyncp != NULL)
		*needsyncp = need_sync;

	return (error);
}

/*
 * Set properties for the vdev and its children. Persist the properties
 * if VDEV_PROPS_PERSISTENT is defined.
 */
int
spa_vdev_prop_set(spa_t *spa, uint64_t vdev_guid, nvlist_t *nvp)
{
	int error;
	vdev_t	*vd;
	boolean_t need_sync = B_FALSE;

	if ((error = spa_vdev_prop_validate(spa, nvp)) != 0)
		return (error);

	if ((vd = spa_lookup_by_guid(spa, vdev_guid, B_TRUE)) == NULL)
		return (ENOENT);

	if (!vd->vdev_ops->vdev_op_leaf) {
		int i;
		for (i = 0; i < vd->vdev_children; i++) {
			error = spa_vdev_prop_set(spa,
			    vd->vdev_child[i]->vdev_guid, nvp);
			if (error != 0)
				break;
		}

		return (error);
	}

	if ((error = vdev_prop_set_nosync(vd, nvp, &need_sync)) != 0)
		return (error);

	if (need_sync)
		return (dsl_sync_task(spa->spa_name, NULL,
		    vdev_sync_props, spa, 3));

	return (error);
}

/*
 * Get properties for the vdev, put them on nvlist
 */
int
spa_vdev_prop_get(spa_t *spa, uint64_t vdev_guid, nvlist_t **nvp)
{
	int err;
	vdev_prop_t prop;

	VERIFY(nvlist_alloc(nvp, NV_UNIQUE_NAME, KM_SLEEP) == 0);

	for (prop = VDEV_PROP_PATH; prop < VDEV_NUM_PROPS; prop++) {
		uint64_t ival;
		char *strval = NULL;
		const char *propname = vdev_prop_to_name(prop);

		if ((err = spa_vdev_get_common(spa, vdev_guid, &strval,
		    &ival, prop)) != 0)
			return (err);

		if (strval != NULL) {
			VERIFY(nvlist_add_string(*nvp, propname, strval) == 0);
			spa_strfree(strval);
		} else {
			VERIFY(nvlist_add_uint64(*nvp, propname, ival) == 0);
		}
	}

	return (0);
}

/*
 * Get minpending  property for the vdev, respect the
 * hierarchy: vdev-level overrides cos-level
 */
uint64_t
vdev_get_minpending(vdev_t *vdev, uint64_t default_val)
{
	uint64_t val = default_val;

	ASSERT(vdev);

	mutex_enter(&vdev->vdev_cos_lock);

	if (vdev->vdev_cos != NULL &&
	    vdev->vdev_cos->cos_min_pending != 0) {
		val = vdev->vdev_cos->cos_min_pending;
		goto out;
	}

	if (vdev->vdev_min_pending != 0)
		val = vdev->vdev_min_pending;
out:
	mutex_exit(&vdev->vdev_cos_lock);
	return (val);
}

/*
 * Get maxpending property for the vdev, respect the
 * hierarchy: vdev-level overrides cos-level
 */
uint64_t
vdev_get_maxpending(vdev_t *vdev, uint64_t default_val)
{
	uint64_t val = default_val;

	ASSERT(vdev);

	mutex_enter(&vdev->vdev_cos_lock);

	if (vdev->vdev_cos != NULL &&
	    vdev->vdev_cos->cos_max_pending != 0) {
		val = vdev->vdev_cos->cos_max_pending;
		goto out;
	}

	if (vdev->vdev_max_pending != 0)
		val = vdev->vdev_max_pending;
out:
	mutex_exit(&vdev->vdev_cos_lock);
	return (val);
}

/*
 * Get prefread property for the vdev, respect the
 * hierarchy: vdev-level overrides cos-level
 */
uint64_t
vdev_get_prefread(vdev_t *vdev)
{
	uint64_t val = 0;

	ASSERT(vdev);

	mutex_enter(&vdev->vdev_cos_lock);

	if (vdev->vdev_cos != NULL &&
	    vdev->vdev_cos->cos_max_pending != 0) {
		val = vdev->vdev_cos->cos_max_pending;
		goto out;
	}

	if (vdev->vdev_preferred_read != 0)
		val = vdev->vdev_preferred_read;
out:
	mutex_exit(&vdev->vdev_cos_lock);
	return (val);
}


/*
 * Store vdev properties at offset 'offset' in object 'obj' in MOS
 */
uint64_t
vdev_store_props(vdev_t *vdev, objset_t *mos, uint64_t obj, uint64_t offset,
    dmu_tx_t *tx)
{
	vdev_props_phys_hdr_t *vpph;
	char *packed = NULL;
	char *buf = NULL;
	size_t size = 0;
	size_t nvsize = 0;
	size_t bufsize = 0;
	nvlist_t *nvl;
	const char *propname;
	boolean_t props_to_set = B_FALSE;

	if (!vdev->vdev_ops->vdev_op_leaf) {
		for (int c = 0; c < vdev->vdev_children; c++)
			size += vdev_store_props(vdev->vdev_child[c], mos, obj,
			    offset + size, tx);
		return (size);
	}

	VERIFY(nvlist_alloc(&nvl, NV_UNIQUE_NAME, KM_SLEEP) == 0);

	if (vdev->vdev_min_pending) {
		propname = vdev_prop_to_name(VDEV_PROP_MINPENDING);
		VERIFY(nvlist_add_uint64(nvl, propname,
		    vdev->vdev_min_pending) == 0);
		props_to_set = B_TRUE;
	}

	if (vdev->vdev_max_pending) {
		propname = vdev_prop_to_name(VDEV_PROP_MAXPENDING);
		VERIFY(nvlist_add_uint64(nvl, propname,
		    vdev->vdev_max_pending) == 0);
		props_to_set = B_TRUE;
	}

	if (vdev->vdev_preferred_read) {
		propname = vdev_prop_to_name(VDEV_PROP_PREFREAD);
		VERIFY(nvlist_add_uint64(nvl, propname,
		    vdev->vdev_preferred_read) == 0);
		props_to_set = B_TRUE;
	}

	if (vdev->vdev_cos != NULL) {
		propname = vdev_prop_to_name(VDEV_PROP_COS);
		VERIFY(nvlist_add_uint64(nvl, propname,
		    vdev->vdev_cos->cos_id) == 0);
		props_to_set = B_TRUE;
	}

	if (vdev->vdev_spare_group != NULL) {
		propname = vdev_prop_to_name(VDEV_PROP_SPAREGROUP);
		VERIFY(nvlist_add_string(nvl, propname,
		    vdev->vdev_spare_group) == 0);
		props_to_set = B_TRUE;
	}

	if (props_to_set) {
		/* there is something to set for this device */
		VERIFY(nvlist_size(nvl, &nvsize, NV_ENCODE_XDR) == 0);

		size = P2ROUNDUP(nvsize, 8);
		bufsize = sizeof (*vpph) + size;
		buf = kmem_alloc(bufsize, KM_SLEEP);
		vpph = (vdev_props_phys_hdr_t *)buf;
		packed = buf + sizeof (*vpph);
		if (size > nvsize)
			bzero(packed + nvsize, size - nvsize);
		VERIFY(nvlist_pack(nvl, &packed, &nvsize, NV_ENCODE_XDR,
			KM_SLEEP) == 0);
		
		vpph->vpph_guid = vdev->vdev_guid;
		vpph->vpph_nvsize = nvsize;
		vpph->vpph_size = bufsize;
		
		dmu_write(mos, obj, offset, bufsize, buf, tx);
	}

	nvlist_free(nvl);

	return (bufsize);
}

/*
 * Get the properties from nvlist and put then in vdev object
 */
static void
vdev_parse_props(vdev_t *vdev, char *packed, uint64_t nvsize)
{
	uint64_t ival;
	nvlist_t *nvl;
	const char *propname;
	char *sval;
	int err;

	ASSERT(vdev);

	if (!vdev->vdev_ops->vdev_op_leaf)
		return;

	if ((err = nvlist_unpack(packed, nvsize, &nvl, KM_SLEEP)) != 0) {
		cmn_err(CE_WARN, "Failed to unpack vdev props, err: %d\n", err);
		return;
	}

	propname = vdev_prop_to_name(VDEV_PROP_MINPENDING);
	if (nvlist_lookup_uint64(nvl, propname, &ival) == 0)
		vdev->vdev_min_pending = ival;

	propname = vdev_prop_to_name(VDEV_PROP_MAXPENDING);
	if (nvlist_lookup_uint64(nvl, propname, &ival) == 0)
		vdev->vdev_max_pending = ival;

	propname = vdev_prop_to_name(VDEV_PROP_PREFREAD);
	if (nvlist_lookup_uint64(nvl, propname, &ival) == 0)
		vdev->vdev_preferred_read = ival;

	propname = vdev_prop_to_name(VDEV_PROP_COS);
	if (nvlist_lookup_uint64(nvl, propname, &ival) == 0) {
		spa_t *spa = vdev->vdev_spa;
		cos_t *cos;
		spa_cos_enter(spa);
		if ((cos = spa_lookup_cos_by_id(spa, ival)) != NULL) {
			if (vdev->vdev_cos != NULL)
				cos_rele(vdev->vdev_cos);
			cos_hold(cos);
			vdev->vdev_cos = cos;
		}
		spa_cos_exit(spa);
	}

	propname = vdev_prop_to_name(VDEV_PROP_SPAREGROUP);
	if (nvlist_lookup_string(nvl, propname, &sval) == 0) {
		vdev->vdev_spare_group = spa_strdup(sval);
	}

	nvlist_free(nvl);
}

/*
 * Store properties of vdevs in the pool in the MOS of that pool
 */
static void
vdev_sync_props(void *arg1, dmu_tx_t *tx)
{
	spa_t *spa = (spa_t *)arg1;
	objset_t *mos = spa->spa_meta_objset;
	uint64_t size = 0;
	uint64_t *sizep;
	vdev_t *root_vdev;
	vdev_t *top_vdev;
	dmu_buf_t *db;

	mutex_enter(&spa->spa_vdev_props_lock);

	if (spa->spa_vdev_props_object == 0) {
		VERIFY((spa->spa_vdev_props_object =
		    dmu_object_alloc(mos, DMU_OT_VDEV_PROPS, 0,
		    DMU_OT_VDEV_PROPS_SIZE, 8, tx)) > 0);

		VERIFY(zap_update(mos,
		    DMU_POOL_DIRECTORY_OBJECT,
		    DMU_POOL_VDEV_PROPS,
		    8, 1, &spa->spa_vdev_props_object, tx) == 0);

		spa_feature_incr(spa, SPA_FEATURE_VDEV_ATTR, tx);
	}

	root_vdev = spa->spa_root_vdev;

	/* process regular vdevs */
	for (int c = 0; c < spa->spa_root_vdev->vdev_children; c++) {
		top_vdev = root_vdev->vdev_child[c];
		size += vdev_store_props(top_vdev, mos,
		    spa->spa_vdev_props_object, size, tx);
	}

	/* process aux vdevs */
	for (int i = 0; i < spa->spa_l2cache.sav_count; i++) {
		vdev_t *vd = spa->spa_l2cache.sav_vdevs[i];
		size += vdev_store_props(vd, mos,
		    spa->spa_vdev_props_object, size, tx);
	}
	
	for (int i = 0; i < spa->spa_spares.sav_count; i++) {
		vdev_t *vd = spa->spa_spares.sav_vdevs[i];
		size += vdev_store_props(vd, mos,
		    spa->spa_vdev_props_object, size, tx);
	}

	VERIFY(0 == dmu_bonus_hold(mos, spa->spa_vdev_props_object, FTAG, &db));
	dmu_buf_will_dirty(db, tx);

	sizep = db->db_data;
	*sizep = size;

	dmu_buf_rele(db, FTAG);

	mutex_exit(&spa->spa_vdev_props_lock);
}

/*
 * Load vdev properties from the vdev_props_object in the MOS
 */
int
vdev_load_props(spa_t *spa, boolean_t load_aux)
{
	objset_t *mos = spa->spa_meta_objset;
	vdev_t *vdev;
	dmu_buf_t *db;
	vdev_props_phys_hdr_t *vpph;
	size_t bufsize = 0;
	char *buf;
	char *pbuf;
	char *bufend;

	ASSERT(spa);

	if (spa->spa_vdev_props_object == 0)
		return (ENOENT);

	mutex_enter(&spa->spa_vdev_props_lock);

	VERIFY(0 == dmu_bonus_hold(mos, spa->spa_vdev_props_object, FTAG, &db));
	bufsize = *(uint64_t *)db->db_data;
	dmu_buf_rele(db, FTAG);

	if (bufsize == 0)
		goto out;

	buf = kmem_alloc(bufsize, KM_SLEEP);
	bufend = buf + bufsize;

	/* read and unpack array of nvlists */
	VERIFY(0 == dmu_read(mos, spa->spa_vdev_props_object,
	    0, bufsize, buf, DMU_READ_PREFETCH));

	for (pbuf = buf; pbuf < bufend; pbuf += vpph->vpph_size) {
		vpph = (vdev_props_phys_hdr_t *)pbuf;
		char *packed = pbuf + sizeof (*vpph);
		uint64_t nvsize = vpph->vpph_nvsize;

		vdev = spa_lookup_by_guid(spa, vpph->vpph_guid, load_aux);
		if (vdev == NULL)
			continue;
		vdev_parse_props(vdev, packed, nvsize);
	}

	kmem_free(buf, bufsize);
out:
	mutex_exit(&spa->spa_vdev_props_lock);

	return (0);
}
