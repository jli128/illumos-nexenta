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
 * Copyright 2012 Nexenta, Systems Inc.  All rights reserved.
 */

#include <stdio.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/types.h>
#include <stddef.h>
#include <strings.h>
#include <sys/list.h>
#include <libdevinfo.h>
#include <sys/stat.h>
#include <sys/scsi/impl/uscsi.h>
#include <sys/scsi/generic/inquiry.h>
#include <fcntl.h>
#include <sys/scsi/scsi.h>
#include <assert.h>
#include <libdiskmgt.h>

#define	VERSION "1.0"

#define	DISK_DRIVER "sd"

/* will set timeout to 30 sec for all USCSI commands that we will issue */
#define	DISK_CMD_TIMEOUT 30

#define	IMPOSSIBLE_RQ_STATUS 0xff

/* length bigger then this can not be represented by 3 bytes */
#define MAX_FW_SIZE_IN_BYTES 16777215

/* 10 is strlen("/dev/rdsk/") */
#define	CTD_START_IN_PATH(path) (path + 10)

#define	MAX_DISK_LEN 9 /* enough space for a million disk names: sd0-sd999999 */
#define	INQ_VENDOR_LEN 9 /* struct scsi_inquiry defines these fields at: */
#define	INQ_MODEL_LEN 17 /* 8, 16 and 4 chars */
#define	INQ_REV_LEN 5 /* without string terminating zeros */

/* struct to hold relevant disk info */
typedef struct disk_info {
	list_node_t link;
	char device[MAX_DISK_LEN];
	char vendor[INQ_VENDOR_LEN];
	char model[INQ_MODEL_LEN];
	char rev[INQ_REV_LEN];
	char path[MAXPATHLEN];
	boolean_t inuse;
	int bad_info;
} disk_info_t;

/* prints sense data and status */
void print_status_and_sense(char *cmd, int rc, int status,
		struct scsi_extended_sense *sense) {
	fprintf(stdout, "%s ioctl() returned: %d ", cmd, rc);
	fprintf(stdout, "status: 0x%02x, ", status & STATUS_MASK);
	fprintf(stdout, "sense - skey: 0x%02x, ", sense->es_key);
	fprintf(stdout, "asc: 0x%02x, ", sense->es_add_code);
	fprintf(stdout, "ascq: 0x%02x\n", sense->es_qual_code);
}

/* determine if the uscsi cmd failed or not */
boolean_t uscsi_parse_status(struct uscsi_cmd *ucmd, int rc, boolean_t verbose) {
	struct scsi_extended_sense *sense;

	if (rc == -1 && errno == EAGAIN) {
		if (verbose)
			fprintf(stderr, "Disk is temporarily unavailable.\n");
		return (B_FALSE); /* unavailable */
	}

	if ((ucmd->uscsi_status & STATUS_MASK) == STATUS_RESERVATION_CONFLICT) {
		if (verbose)
			fprintf(stderr, "Disk is reserved.\n");
		return (B_FALSE); /* reserved by another system */
	}

	if (rc == -1 && ucmd->uscsi_status == 0 && errno == EIO) {
		if (verbose)
			fprintf(stderr, "Disk is unavailable.\n");
		return (B_FALSE); /* unavailable */
	}

	/* check if we have valid sense status */
	if (ucmd->uscsi_rqstatus == IMPOSSIBLE_RQ_STATUS) {
		if (verbose) {
			fprintf(stderr, "No sense data for command ");
			fprintf(stderr, "0x%02x.\n", ucmd->uscsi_cdb[0]);
		}
		return (B_FALSE);
	}

	if (ucmd->uscsi_rqstatus != STATUS_GOOD) {
		if (verbose) {
			fprintf(stderr, "Sense status for command ");
			fprintf(stderr, "0x%02x: ", ucmd->uscsi_cdb[0]);
			fprintf(stderr, "0x%02x.\n", ucmd->uscsi_rqstatus);
		}
		return (B_FALSE);
	}

	sense = (struct scsi_extended_sense *)ucmd->uscsi_rqbuf;
	if ((sense->es_key != KEY_RECOVERABLE_ERROR) &&
			(sense->es_key != KEY_NO_SENSE)) {
		if (verbose && sense->es_key == KEY_ILLEGAL_REQUEST &&
				sense->es_add_code == 0x2C && sense->es_qual_code == 0x0) {
			fprintf(stderr, " Illegal Request - Command ");
			fprintf(stderr, "0x%02x sequence error.\n", ucmd->uscsi_cdb[0]);
		} else if (verbose && sense->es_key == KEY_ILLEGAL_REQUEST &&
				sense->es_add_code == 0x24 && sense->es_qual_code == 0x0) {
			fprintf(stderr, " cmd 0x%02x: Illegal Request", ucmd->uscsi_cdb[0]);
			fprintf(stderr, " - Invalid field in CDB for  command.\n");
		} else if (verbose && sense->es_key == KEY_UNIT_ATTENTION &&
				sense->es_add_code == 0x3F && sense->es_qual_code == 0x01) {
			fprintf(stderr, " cmd 0x%02x: Unit Attention", ucmd->uscsi_cdb[0]);
			fprintf(stderr, " - Microcode changed.\n");
		} else if (verbose && sense->es_key == KEY_UNIT_ATTENTION &&
				sense->es_add_code == 0x29 && sense->es_qual_code == 0x0) {
			fprintf(stderr, " cmd 0x%02x: Unit Attention", ucmd->uscsi_cdb[0]);
			fprintf(stderr, " - Reset Occurred.\n");
		} else if (verbose) {
			fprintf(stderr, "Command 0x%02x produced", ucmd->uscsi_cdb[0]);
			fprintf(stderr, " sense data that indicated an error.\n");
		}
		return (B_FALSE);
	}

	if (rc == -1 && errno == EIO) {
		if (verbose) {
			fprintf(stderr, "Command 0x%02x", ucmd->uscsi_cdb[0]);
			fprintf(stderr, " resulted in I/O error.\n");
		}
		return (B_FALSE);
	}

	if (rc != 0) {
		if (verbose)
			fprintf(stderr, "cmd 0x%02x: Unknown error.\n", ucmd->uscsi_cdb[0]);
		return (B_FALSE);
	}

	if (verbose) {
		fprintf(stderr, "USCSI command 0x%02x", ucmd->uscsi_cdb[0]);
		fprintf(stderr, " completed successfully.\n");
	}
	return (B_TRUE);
}

/* dump raw hex cdb */
void print_cdb(union scsi_cdb *cdb, const char *cmd, int cdb_len) {
	int i;

	fprintf(stderr, "\n%s cdb (hex): ", cmd);
	for (i = 0; i < cdb_len; i++)
		fprintf(stderr, "%02x ", cdb->cdb_opaque[i]);
	fprintf(stderr, "\n");
}

/* will write microcode located in fw_img to the disk_fd */
boolean_t uscsi_write_buffer_dmc(int disk_fd, uint8_t mode, void * fw_img,
		size_t fw_len, struct scsi_extended_sense *sense, boolean_t verbose) {
	struct uscsi_cmd ucmd;
	union scsi_cdb cdb;
	int rc;

	(void) memset(&ucmd, 0, sizeof (struct uscsi_cmd));
	(void) memset(sense, 0, sizeof (struct scsi_extended_sense));
	(void) memset(&cdb, 0, sizeof (union scsi_cdb));

	cdb.scc_cmd = SCMD_WRITE_BUFFER;
	/* bits 0-4 (inclusive) of byte 1 contains mode field */
	cdb.cdb_opaque[1] = (mode & 0x1f);
	/* bytes 6-8 contain fw file length */
	cdb.cdb_opaque[6] = (uint8_t)((fw_len >> 16) & 0xff);
	cdb.cdb_opaque[7] = (uint8_t)((fw_len >> 8) & 0xff);
	cdb.cdb_opaque[8] = (uint8_t)(fw_len & 0xff);

	ucmd.uscsi_cdb = (caddr_t)&cdb;
	ucmd.uscsi_cdblen = CDB_GROUP1;
	ucmd.uscsi_flags = USCSI_SILENT | USCSI_WRITE | USCSI_ISOLATE;
	ucmd.uscsi_flags |= USCSI_RQENABLE | USCSI_DIAGNOSE;
	ucmd.uscsi_bufaddr = fw_img;
	ucmd.uscsi_buflen = fw_len;
	ucmd.uscsi_timeout = DISK_CMD_TIMEOUT;
	ucmd.uscsi_rqbuf = (caddr_t)sense;
	ucmd.uscsi_rqlen = sizeof (struct  scsi_extended_sense);
	ucmd.uscsi_rqstatus = IMPOSSIBLE_RQ_STATUS;

	if (verbose)
		print_cdb(&cdb, "write buffer", CDB_GROUP1);

	rc = ioctl(disk_fd, USCSICMD, &ucmd);
	if (verbose)
		print_status_and_sense("write buf", rc, ucmd.uscsi_status, sense);

	rc = uscsi_parse_status(&ucmd, rc, verbose);

	if (sense->es_key == KEY_ILLEGAL_REQUEST && sense->es_add_code == 0x2C &&
			sense->es_qual_code == 0x0) {
		fprintf(stderr, " Downloading of microcode has failed - ");
		fprintf(stderr, "Command sequence error.\n");
		rc = B_FALSE;
	} else if (sense->es_key == KEY_ILLEGAL_REQUEST &&
			sense->es_add_code == 0x24 && sense->es_qual_code == 0x0) {
		fprintf(stderr, " Downloading of microcode has failed - ");
		fprintf(stderr, "Invalid field in CDB.\n");
		rc = B_FALSE;
	} else if (sense->es_key == KEY_UNIT_ATTENTION &&
			sense->es_add_code == 0x3F && sense->es_qual_code == 0x01) {
		fprintf(stderr, " Microcode download successful\n");
		rc = B_TRUE;
	}

	return (rc);
}

/* Issue TUR command to device identified by file descriptor disk_fd */
boolean_t uscsi_test_unit_ready(int disk_fd, struct scsi_extended_sense *sense,
		boolean_t verbose) {
	struct uscsi_cmd ucmd;
	int rc;
	union scsi_cdb cdb;

	(void) memset(&ucmd, 0, sizeof (struct uscsi_cmd));
	(void) memset(sense, 0, sizeof (struct scsi_extended_sense));
	(void) memset(&cdb, 0, sizeof (union scsi_cdb));

	cdb.scc_cmd = SCMD_TEST_UNIT_READY;

	ucmd.uscsi_cdb = (caddr_t)&cdb;
	ucmd.uscsi_cdblen = CDB_GROUP0;
	ucmd.uscsi_flags = USCSI_SILENT | USCSI_DIAGNOSE | USCSI_RQENABLE;
	ucmd.uscsi_timeout = DISK_CMD_TIMEOUT;
	ucmd.uscsi_rqbuf = (caddr_t)sense;
	ucmd.uscsi_rqlen = sizeof (struct scsi_extended_sense);
	ucmd.uscsi_rqstatus = IMPOSSIBLE_RQ_STATUS;

	if (verbose)
	    print_cdb(&cdb, "test unit ready", CDB_GROUP0);

	rc = ioctl(disk_fd, USCSICMD, &ucmd);
	if (verbose)
		print_status_and_sense("test unit ready", rc, ucmd.uscsi_status, sense);

	return (uscsi_parse_status(&ucmd, rc, verbose));
}

/* Execute a uscsi inquiry command and put the resulting data into inqbuf. */
boolean_t uscsi_inquiry(int disk_fd, struct scsi_inquiry *inqbuf,
		struct scsi_extended_sense *sense, boolean_t verbose) {
	struct uscsi_cmd ucmd;
	union scsi_cdb cdb;
	int rc;

	(void) memset(inqbuf, 0, sizeof (struct scsi_inquiry));
	(void) memset(sense, 0, sizeof (struct scsi_extended_sense));
	(void) memset(&ucmd, 0, sizeof (ucmd));
	(void) memset(&cdb, 0, sizeof (union scsi_cdb));

	cdb.scc_cmd = SCMD_INQUIRY;
	/* bytes 3-4 contain data-in buf len */
	cdb.cdb_opaque[3] = (uint8_t)((sizeof (struct scsi_inquiry) >> 8) & 0xff);
	cdb.cdb_opaque[4] = (uint8_t)((sizeof (struct scsi_inquiry)) & 0xff);

	ucmd.uscsi_cdb = (caddr_t)&cdb;
	ucmd.uscsi_cdblen = CDB_GROUP0;
	ucmd.uscsi_flags = USCSI_READ | USCSI_SILENT | USCSI_ISOLATE;
	ucmd.uscsi_flags |= USCSI_RQENABLE | USCSI_DIAGNOSE;
	ucmd.uscsi_bufaddr = (caddr_t)inqbuf;
	ucmd.uscsi_buflen = sizeof (struct scsi_inquiry);
	ucmd.uscsi_timeout = DISK_CMD_TIMEOUT;
	ucmd.uscsi_rqbuf = (caddr_t)sense;
	ucmd.uscsi_rqlen = sizeof (struct scsi_extended_sense);
	ucmd.uscsi_rqstatus = IMPOSSIBLE_RQ_STATUS;

	if (verbose)
		print_cdb(&cdb, "inquiry", CDB_GROUP1);

	rc = ioctl(disk_fd, USCSICMD, &ucmd);
	if (verbose)
		print_status_and_sense("inquiry", rc, ucmd.uscsi_status, sense);

	return (uscsi_parse_status(&ucmd, rc, verbose));
}

/* Issue start command to device identified by file descriptor disk_fd */
boolean_t uscsi_start_unit(int disk_fd, struct scsi_extended_sense *sense,
		boolean_t verbose) {
	struct uscsi_cmd ucmd;
	union scsi_cdb cdb;
	int rc;

	(void) memset(&ucmd, 0, sizeof (struct uscsi_cmd));
	(void) memset(&cdb, 0, sizeof (union scsi_cdb));
	(void) memset(sense, 0, sizeof (struct scsi_extended_sense));

	cdb.scc_cmd = SCMD_START_STOP;
	/* bit 0 of byte 4 - start bit value. bits 4-7 - power condition field */
	cdb.cdb_opaque[4] = 0x01;

	ucmd.uscsi_cdb = (caddr_t)&cdb;
	ucmd.uscsi_cdblen = CDB_GROUP0;
	ucmd.uscsi_flags = USCSI_SILENT | USCSI_DIAGNOSE | USCSI_RQENABLE;
	ucmd.uscsi_flags |= USCSI_ISOLATE;
	ucmd.uscsi_timeout = DISK_CMD_TIMEOUT;
	ucmd.uscsi_rqbuf = (caddr_t)sense;
	ucmd.uscsi_rqlen = sizeof (struct scsi_extended_sense);
	ucmd.uscsi_rqstatus = IMPOSSIBLE_RQ_STATUS;

	if (verbose)
		print_cdb(&cdb, "start", CDB_GROUP0);

	rc = ioctl(disk_fd, USCSICMD, &ucmd);
	if (verbose)
		print_status_and_sense("start", rc, ucmd.uscsi_status, sense);

	return (uscsi_parse_status(&ucmd, rc, verbose));
}

/* copy chars to out without trailing and leading "space" chars */
void mem_trim_and_cpy(char *out, const char *buf, size_t buf_len) {
	while (buf_len != 0 && isspace(*buf) != 0) { /* ignore leading space(s) */
		buf++;
		buf_len--;
	}
	while (buf_len && isspace(buf[buf_len - 1]))
		buf_len--; /* ignore trailing space(s) */

	memcpy(out, buf, buf_len);
	out[buf_len] = 0; /* stringify */
}

/* given a disk - check if it's in use */
/* define NOINUSE_CHECK environment variable to turn of disk inuse check. */
void set_disk_inuse(disk_info_t *disk, boolean_t verbose) {
	char *msg, *slice, *character;
	char dev[MAXPATHLEN];
	int error = 0;
	int i;
	dm_descriptor_t	*slices = NULL;
	/*
	 * Let libdiskmgt think we are the format consumer, meaning slices will not
	 * be considered "inuse" if they have a filesystem or an exported zpool
	 */
	dm_who_type_t who = DM_WHO_FORMAT;

	/* need to give dm_get_slices() a "whole" disk name, ie - c#t#d# */
	strncpy(dev, CTD_START_IN_PATH(disk->path), MAXPATHLEN);
	if ((character = strrchr(dev, 'd')) != NULL) {
		character++;
		while (isdigit(*character))
			character++;
		*character = 0; /* terminate the string after d# */
	}

	disk->inuse = B_FALSE;
	dm_get_slices(dev, &slices, &error);
	if (error != 0) {
		if (verbose) {
			fprintf(stderr, "dm_get_slices() failed: %s.\n", strerror(error));
			fprintf(stderr, "Marking disk inuse.\n");
		}
		/*
		 * Marking disk as "in use" on errors since it's
		 * better to not touch a disk that MAY be in use by something.
		 */
		disk->inuse = B_TRUE;
		return;
	}

	/* loop through slices - set inuse */
	for (i = 0; slices[i]; i++) {
		slice = dm_get_name(slices[i], &error);
		if (dm_inuse(slice, &msg, who, &error) != 0 || error != 0) {
			/*
			 * Marking disk as "in use" even if we got here by way of
			 * (error != 0) since it's better to not touch a disk that MAY
			 * be in use by something. User can still use the disk by defining
			 * the NOINUSE_CHECK environment variable which will skip
			 * "in use" checking.
			 */
			disk->inuse = B_TRUE;
			if (error == 0) {
				if (verbose) {
					fprintf(stderr, "Disk '%s' is in use: ", disk->device);
					fprintf(stderr, "%s", msg);
				}
				free(msg);
			} else if (verbose) {
				fprintf(stderr, "dm_inuse() failed: %s.\n", strerror(error));
				fprintf(stderr, "Marking disk inuse.\n");
			}
			dm_free_name(slice);
			break;
		}
		dm_free_name(slice);
	}
	dm_free_descriptors(slices);
}

/* obtain devlink (c*d*t* path) from /device path */
int devlink_walk_cb(di_devlink_t devlink, void *arg) {
	const char *path;
	if ((path = di_devlink_path(devlink)) != NULL) {
		assert(strlen(path) < MAXPATHLEN);
		strncpy(arg, path, strlen(path)+1);
	} else
		strncpy(arg, "unknown_device_path", strlen("unknown_device_path")+1);

	return (DI_WALK_TERMINATE);
}

/*  get relevant disk info for char devices */
boolean_t set_disk_info(const di_node_t *node, disk_info_t *disk,
		boolean_t verbose) {
	int instance;
	char *m_path = NULL;
	di_minor_t di_minor;
	di_devlink_handle_t devlink_h;
	struct scsi_inquiry inq;
	int disk_fd;
	struct scsi_extended_sense sense;

	disk->bad_info = 1;

	/* populate device field with sd */
	assert(MAX_DISK_LEN > strlen(DISK_DRIVER) + sizeof (int) + 1);
	strncpy(disk->device, DISK_DRIVER, strlen(DISK_DRIVER)+1);

	instance = di_instance(*node);
	if (instance == -1) {
		fprintf(stderr, "Could not get the instance number of the device - ");
		fprintf(stderr, "di_instance() failed\n");
		strcpy(disk->device + strlen(DISK_DRIVER), "?");
		disk->bad_info = -1;
		return (B_FALSE);
	}

	/* append instance # to device name making sd# */
	snprintf(disk->device + strlen(DISK_DRIVER), MAX_DISK_LEN -
			strlen(DISK_DRIVER), "%d", instance);

	/* take a snapshot of devlinks bound to sd driver */
	if ((devlink_h = di_devlink_init(DISK_DRIVER, DI_MAKE_LINK)) == DI_LINK_NIL)
	{
		fprintf(stderr, "di_link_init() failed for disk %s.\n", disk->device);
		return (B_FALSE);
	}
	/* traverse devlink minor nodes */
	di_minor = di_minor_next(*node, DI_MINOR_NIL);
	for (; di_minor != DI_MINOR_NIL; di_minor = di_minor_next(*node, di_minor))
	{
		if (di_minor_spectype(di_minor) != S_IFCHR)
			continue; /* skip minor nodes that are not char devs */

		/* phys path to minor node (/devices/...) */
		if ((m_path = di_devfs_minor_path(di_minor)) == NULL) {
			fprintf(stderr, "couldn't get path for a minor node of disk ");
			fprintf(stderr, "'%s' - di_devfs_minor_path() ", disk->device);
			fprintf(stderr, "failed.\n");
			break;
		}
		if (strstr(m_path, ":a,raw") == NULL) {
			di_devfs_path_free(m_path);
			continue; /* skips minor nodes that are not slice 0 */
		}

		/*
		 * Lookup /dev/rdsk/ path for minor node defined by m_path
		 * The reason we're not just using the /devices m_path for WRITE_BUFFER
		 * is we need the c#t#d# lun to display to the user anyway
		 */
		if (di_devlink_walk(devlink_h, "^rdsk/", m_path, DI_PRIMARY_LINK,
				disk->path, devlink_walk_cb) == -1) {
			fprintf(stderr, "di_devlink_walk() failed for ");
			fprintf(stderr, "disk '%s'\n", disk->device);
			di_devfs_path_free(m_path);
			break;
		}
		di_devfs_path_free(m_path);

		/* check that the devlink (/dev/rdsk/) path was found by the above */
		if (strcmp(disk->path, "unknown_device_path") == 0) {
			fprintf(stderr, "di_devlink_path() for '%s' failed.", disk->device);
			break;
		}

		disk_fd = open(disk->path, O_RDONLY | O_NDELAY);
		if (disk_fd < 0) {
			fprintf(stderr, "open() on disk '%s', ", disk->device);
			fprintf(stderr, "dev path '%s' ", disk->path);
			fprintf(stderr, "failed: %s\n", strerror(errno));
			break;
		}

		/* found p0 for a char disk, now get vendor, model, and rev */
		if (uscsi_inquiry(disk_fd, &inq, &sense, verbose)) {
			disk->bad_info = 0;
			mem_trim_and_cpy(disk->vendor, inq.inq_vid, sizeof (inq.inq_vid));
			mem_trim_and_cpy(disk->model, inq.inq_pid, sizeof (inq.inq_pid));
			mem_trim_and_cpy(disk->rev, inq.inq_revision,
					sizeof (inq.inq_revision));
			set_disk_inuse(disk, verbose);
		} else {
			if (verbose) {
				fprintf(stderr, "uscsi_inquiry() failed, disk ");
				fprintf(stderr, "'%s' will be skipped\n", disk->device);
			}
		}
		close(disk_fd);
		break; /* have found slice 0 for a char device, can now exit */
	}
	di_devlink_fini(&devlink_h);

	return (disk->bad_info == 0 ? B_TRUE : B_FALSE);
}

/* kernel device tree walk */
int walk_nodes(di_node_t *root_node, list_t *disks, boolean_t verbose) {
	int *dtype;
	int disks_found = 0;
	disk_info_t *disk;
	di_node_t node;

	fprintf(stdout, "Searching for disks, found: ");
	/* start at root node and walk all sd nodes */
	node = di_drv_first_node(DISK_DRIVER, *root_node);
	for (; node != DI_NODE_NIL; node = di_drv_next_node(node)) {
		fflush(stdout);
		if (di_prop_lookup_ints(DDI_DEV_T_ANY, node, "inquiry-device-type",
				&dtype) >= 0) {
			if (((*dtype) & DTYPE_MASK) != DTYPE_DIRECT)
				continue; /* skip dev types that are not type 0 (not disks) */
		} else {
			if (verbose) {
				fprintf(stderr, "di_prop_lookup_ints(inquiry-device-type) ");
				fprintf(stderr, "failed, ignoring this node\n");
			}
			continue;
		}
		disks_found++;
		if ((disk = (disk_info_t *) malloc(sizeof (disk_info_t))) == NULL) {
			if (verbose)
				fprintf(stderr, "malloc(%d) failed\n", sizeof (disk_info_t));
			return (-disks_found);
		}
		list_insert_tail(disks, disk); /* preserves discovery order */
		if (set_disk_info(&node, disk, verbose) || verbose) {
			fprintf(stdout, "%s ", disk->device);
			if ((disks_found % 10) == 0)
				fprintf(stdout, "\n");
		} else
			disks_found--;
	}
	fprintf(stdout, "(%d total).\n", disks_found);

	return (disks_found);
}

/* allocate space and read into it the fw image */
char * get_fw_image(const char *fw_file, size_t *fw_len, boolean_t verbose) {
	struct stat stat_buf;
	char *fw_buf = NULL;
	FILE * fw_stream = fopen(fw_file, "r");

	*fw_len = 0;
	if (fw_stream != NULL) {
		if (fstat(fileno(fw_stream), &stat_buf) == -1) { /* fstat failed */
			if (verbose)
				fprintf(stderr, "fstat() on file '%s' failed\n", fw_file);
		} else {
			if ((fw_buf = malloc(stat_buf.st_size)) != NULL)
				*fw_len = stat_buf.st_size;
			else if (verbose)
				fprintf(stderr, "malloc() failed\n");
		}
	} else { /* fopen failed */
		if (verbose) {
			fprintf(stderr, "fopen() on fw image ");
			fprintf(stderr, "'%s' failed: %s\n", fw_file, strerror(errno));
		}
	}

	if (fw_buf != NULL && fread(fw_buf, 1, *fw_len, fw_stream) != *fw_len) {
		free(fw_buf);
		fw_buf = NULL;
		*fw_len = 0;
		if (verbose)
			fprintf(stderr, "fread() failed\n");
	}

	if (fw_stream != NULL)
		fclose(fw_stream);

	return (fw_buf);
}

/* print a dot for every second we wait as to not look like process is hanging */
void wait_print(int sec) {
	fprintf(stdout, " waiting for %d seconds:\n", sec);
	while (sec--) {
		fprintf(stdout, " .");
		fflush(stdout);
		if (! (sec % 30))
			fprintf(stdout, "\n");
		sleep(1);
	}
}

/* perform some housekeeping before write buffer */
boolean_t prep_disk_for_fw_dl(int disk_fd, int wait, boolean_t verbose) {
	struct scsi_extended_sense sense;
	boolean_t sts;

	if (verbose)
		fprintf(stderr, "  before dl prep\n");

	sync(); /* schedule in flight stuff to be put onto disk before dmc */

	if ((sts = uscsi_test_unit_ready(disk_fd, &sense, verbose)))
		return (B_TRUE);
	else {
		if (sense.es_key == KEY_NOT_READY && sense.es_add_code == 0x04 &&
				sense.es_qual_code == 0x02) {
			/* above sense data indicates disk needs a start command */
			sts = uscsi_start_unit(disk_fd, &sense, verbose);
			wait_print(wait);
		} else if (sense.es_key == KEY_UNIT_ATTENTION &&
				sense.es_add_code == 0x29 && sense.es_qual_code == 0x0)
			wait_print(wait); /* reset occurred */
		else if (sense.es_key == KEY_NOT_READY && sense.es_add_code == 0x04 &&
				sense.es_qual_code == 0x0)
			wait_print(wait); /* becoming ready */
	}

	return (uscsi_test_unit_ready(disk_fd, &sense, verbose)); /* trying again */
}

/* test disk after buffer write to make sure it's ready for operation */
boolean_t after_dl_prep(int disk_fd, int wait, boolean_t verbose) {
	struct scsi_extended_sense sense;
	boolean_t sts;

	if (verbose)
		fprintf(stderr, " after dl prep\n");
	wait_print(wait);	/* give disk time to become ready after dmc */

	if ((sts = uscsi_test_unit_ready(disk_fd, &sense, verbose)))
		return (B_TRUE);
	else {
		if (sense.es_key == KEY_NOT_READY && sense.es_add_code == 0x04 &&
				sense.es_qual_code == 0x02) {
			/* above sense data indicates disk needs a start command */
			sts = uscsi_start_unit(disk_fd, &sense, verbose);
			wait_print(wait);
		} else if (sense.es_key == KEY_UNIT_ATTENTION &&
				sense.es_add_code == 0x63 && sense.es_qual_code == 0x01)
			wait_print(wait); /* microcode changed */
		else if (sense.es_key == KEY_UNIT_ATTENTION &&
				sense.es_add_code == 0x29 && sense.es_qual_code == 0x0)
			wait_print(wait); /* reset occurred */
		else if (sense.es_key == KEY_NOT_READY && sense.es_add_code == 0x04 &&
				sense.es_qual_code == 0x0)
			wait_print(wait); /* becoming ready */
	}

	return (uscsi_test_unit_ready(disk_fd, &sense, verbose)); /* trying again */
}

/* read through fw image and look for model string */
boolean_t match_fw_image_to_model(const char *fw_image, size_t fw_len,
		const char *model) {
	size_t model_len = strlen(model);
	size_t limit = fw_len - model_len + 1;
	size_t i;

	assert(model_len > 0);
	for (i = 0; i < limit; i++)
		if (memcmp(fw_image + i, model, model_len) == 0)
			return (B_TRUE);

	return (B_FALSE);
}

/* check if the bad_info field is set, if so print the error */
boolean_t has_bad_info(disk_info_t *disk, boolean_t verbose) {
	if (disk->bad_info == -1) {
		if (verbose) {
			fprintf(stderr, "Encountered a device without an instance, ");
			fprintf(stderr, "it will be skipped.\n");
		}
		return (B_TRUE);
	} else if (disk->bad_info == 1) {
		if (verbose) {
			fprintf(stderr, "Was not able to get all of the needed info for ");
			fprintf(stderr, "disk '%s', it will be skipped.\n", disk->device);
		}
		return (B_TRUE);
	}

	return (B_FALSE);
}

/* print info for a single disk entry */
void print_disk(const disk_info_t *disk) {
	fprintf(stdout, "%-8s", disk->device);
	fprintf(stdout, " %-35s", CTD_START_IN_PATH(disk->path));
	fprintf(stdout, " %-9s", disk->vendor);
	fprintf(stdout, " %-17s", disk->model);
	fprintf(stdout, " %6s\n", disk->rev);
}

/* print header and info for all disks */
void print_disks(list_t *disks, boolean_t verbose) {
	disk_info_t *disk;
	fprintf(stdout, "%-8s", "DEVICE");
	fprintf(stdout, " %-35s", "LUN");
	fprintf(stdout, " %-9s", "VENDOR");
	fprintf(stdout, " %-17s", "MODEL");
	fprintf(stdout, " %6s\n", "FW REV");
	fprintf(stdout, "========================================");
	fprintf(stdout, "=======================================\n");
	for (disk = list_head(disks); disk; disk = list_next(disks, disk))
		if (!has_bad_info(disk, verbose))
			print_disk(disk);
}

/* perform fw update on one disk */
boolean_t process_disk(disk_info_t *disk, uint8_t mode, char * fw_buf,
		size_t fw_len, int wait, boolean_t ignore_mismatch, boolean_t verbose) {
	int disk_fd;
	struct scsi_extended_sense sense;
	boolean_t status;
	struct scsi_inquiry inq;

	fprintf(stdout, "Processing disk %s:\n\n", disk->device);
	status = match_fw_image_to_model(fw_buf, fw_len, disk->model);

	if (verbose) {
		fprintf(stderr, " fw img matched to model ");
		fprintf(stderr, "%s? %s\n", disk->model, status ? "yes" : "no");
	}
	if (!ignore_mismatch && !status) {
		fprintf(stdout, "Could not match fw image to disk ");
		fprintf(stdout, "(%s) model (%s), ",  disk->device, disk->model);
		fprintf(stdout, "will not proceed without ignore mismatch (-i) ");
		fprintf(stdout, "argument specified.\n");
		return (B_FALSE);
	}

	if (disk->inuse) {
		fprintf(stderr, "Disk '%s' is in use, ", disk->device);
		fprintf(stdout, "will not continue.\n");
		return (B_FALSE);
	}

	disk_fd = open(disk->path, O_RDWR | O_NONBLOCK);
	if (disk_fd < 0) {
		fprintf(stderr, "open() on disk ");
		fprintf(stderr, "'%s' failed: %s\n", disk->path, strerror(errno));
		return (B_FALSE);
	}

	status = prep_disk_for_fw_dl(disk_fd, wait, verbose);
	if (verbose) {
		fprintf(stderr, " Prep disk status: ");
		fprintf(stderr, "%s\n", status ? "success" : "failure");
	}

	if (!status) {
		close(disk_fd);
		return (B_FALSE);
	}

	/* write the fw onto the disk */
	status = uscsi_write_buffer_dmc(disk_fd, mode, fw_buf, fw_len, &sense,
			verbose);

	if (after_dl_prep(disk_fd, wait, verbose) && verbose)
		fprintf(stderr, " After fw download disk seem to be online.\n");
	else if (verbose)
		fprintf(stderr, " After fw download disk doesn't seem to be online.\n");

	fprintf(stdout, "\nFlashing %s firmware: ",  disk->device);
	fprintf(stdout, "%s\n", status ? "successful" : "failed");

	if (verbose) { /* get and display the "new" disk revision */
		if (uscsi_inquiry(disk_fd, &inq, &sense, verbose)) {
			mem_trim_and_cpy(disk->rev, inq.inq_revision,
					sizeof (inq.inq_revision));
			print_disk(disk);
		} else {
			fprintf(stderr, "uscsi_inquiry() failed on disk ");
			fprintf(stderr, "'%s', can't get revision.\n", disk->device);
		}
	}
	close(disk_fd);

	return (status);
}

void usage(const char * prog_name) {
	fprintf(stdout, "\nUsage: %s <-d (c#t#d# | sd#) | ", prog_name);
	fprintf(stdout, "-m model_string> <-p /path/to/fw/img> <-h> <-l> ");
	fprintf(stdout, "<-i> <-v> <-V> <-w #sec>\n");
	fprintf(stdout, "\t-h\tPrint this help message.\n");
	fprintf(stdout, "\t-l\tList discovered drives.\n");
	fprintf(stdout, "\t-d\tSpecify single disk to use for firmware download ");
	fprintf(stdout, "in c#t#d# or sd# format.\n");
	fprintf(stdout, "\t-m str\tSpecify model of drive(s) to download firmware");
	fprintf(stdout, " to. Disks whose model (obtained through SCSI INQUIRY ");
	fprintf(stdout, "command) exactly matches model str provided will be ");
	fprintf(stdout, "upgraded.\n");
	fprintf(stdout, "\t-p\tPath to the firmware file that will be downloaded ");
	fprintf(stdout, "onto the specified disk(s).\n");
	fprintf(stdout, "\t-i\tIgnore disk and fw model mismatch.\n");
	fprintf(stdout, "\t-v\tVerbose mode: turns on extra debug messages.\n");
	fprintf(stdout, "\t-V\tShow %s version.\n", prog_name);
	fprintf(stdout, "\t-w #sec\tNumber of seconds to delay checking for disk ");
	fprintf(stdout, "readiness after downloading the firmware. Also used for ");
	fprintf(stdout, "disk preparation timeouts. Default is 5.\n");
	exit(1);
}

int main(int argc, char *argv[]) {
	di_node_t root_node;
	int opt, i, wait = 5;
	uint8_t mode = 0x05; /* download, save and activate microcode */
	boolean_t ctd = B_FALSE, list = B_FALSE, model = B_FALSE, verbose = B_FALSE;
	boolean_t ignore_mismatch = B_FALSE, user_wait = B_FALSE;
	char *fw_img = NULL;
	size_t fw_len;
	char dl_fw_to_disk[MAXPATHLEN];
	char dl_fw_to_model[INQ_MODEL_LEN];
	list_t disks; /* list of disk_info_t structures */
	disk_info_t *disk = NULL;

	if (geteuid() > 1) {
		fprintf(stderr, "This utility requires root level permissions due to ");
		fprintf(stderr, "the use of USCSI(7i).\n");
		return (1);
	}

	/* if no command line args received do a disk list */
	if (argc == 1)
		list = B_TRUE;

	while ((opt = getopt(argc, argv, "d:p:hilm:vVw:")) != -1) {
		switch (opt) {
		case 'd': /* single disk mode */
			if (strlen(optarg) < MAXPATHLEN) {
				strncpy(dl_fw_to_disk, optarg, strlen(optarg)+1);
				ctd = B_TRUE;
			} else {
				fprintf(stderr, "disk name given to '-%c' ", optopt);
				fprintf(stderr, "is too long (%d)\n", MAXPATHLEN);
				usage(argv[0]);
			}
			break;
		case 'p': /* fw file */
			if (strlen(optarg) < MAXPATHLEN) {
				fw_img = get_fw_image(optarg, &fw_len, verbose);
				if (!fw_img) {
					fprintf(stderr, "could not use fw file '%s', ", optarg);
					fprintf(stderr, "check file permissions and existence.\n");
					usage(argv[0]);
				}
				if (fw_len > MAX_FW_SIZE_IN_BYTES) {
					fprintf(stderr, "%s does not support fw files ", argv[0]);
					fprintf(stderr, "bigger then ");
					fprintf(stderr, "%d bytes.\n", MAX_FW_SIZE_IN_BYTES);
					exit(1);
				}
			} else {
				fprintf(stderr, "fw path provided to ");
				fprintf(stderr, "'-%c' is too long (%d)\n", optopt, MAXPATHLEN);
				usage(argv[0]);
			}
			break;
		case 'h': /* print usage and exit */
			usage(argv[0]);
			break;
		case 'i': /* ignore fw and disk model mismatch */
			ignore_mismatch = B_TRUE;
			break;
		case 'l': /* list drives */
			list = B_TRUE;
			break;
		case 'm': /* only flash this model */
			if (strlen(optarg) < INQ_MODEL_LEN) {
				strncpy(dl_fw_to_model, optarg, strlen(optarg)+1);
				model = B_TRUE;
			} else {
				fprintf(stderr, "model provided to ");
				fprintf(stderr, "'-%c' is too long (%d)\n", optopt, MAXPATHLEN);
				usage(argv[0]);
			}
			break;
		case 'v': /* verbose mode */
			verbose = B_TRUE;
			break;
		case 'V': /* display program version */
			fprintf(stderr, "%s version " VERSION "\n", argv[0]);
			break;
		case 'w': /* wait time after dl */
			wait = atoi(optarg);
			if (wait < 0 || wait > 300) {
				fprintf(stderr, "'-%c %d' - time to wait after ", optopt, wait);
				fprintf(stderr, "microcode download is too long ( > 5min) or ");
				fprintf(stderr, "negative.\n");
				usage(argv[0]);
			}
			user_wait = B_TRUE;
			break;
		case ':':
		case '?':
			usage(argv[0]);
			break;
		}
	}

	for (i = optind; i < argc; i++) {
		printf("Unexpected argument '%s'\n", argv[i]);
		usage(argv[0]);
	}

	if (fw_img && (!ctd && !model)) {
		/* if fw file is given, need to associate it with a disk or model */
		fprintf(stderr, "Expected a disk specification ");
		fprintf(stderr, "('-m model' or '-d c#t#d#') to associate ");
		fprintf(stderr, "fw file with.\n");
		usage(argv[0]);
	}
	if (!fw_img && (ctd || model)) {
		/* if no fw file is given, having model or disk doesn't make sense */
		fprintf(stderr, "Expected a fw file ('-p /path/to/fw/file') to ");
		fprintf(stderr, "associate '-m model' or '-d c#t#d#' with.\n");
		usage(argv[0]);
	}

	if (model && ctd) {
		/* can't have both disk and model for fw download specified */
		fprintf(stderr, "-m and -d are mutually exclusive.\n");
		usage(argv[0]);
	}

	if ((ignore_mismatch || user_wait) && !fw_img) {
		/* need firmware to work with if above args are given */
		fprintf(stderr, "Expected '-p /path/to/fw.'\n");
		usage(argv[0]);
	}

	/*
	 * create a snapshot of kernel device tree include minor nodes,
	 * properties and subtree
	 */
	if ((root_node = di_init("/", DINFOCPYALL)) == DI_NODE_NIL) {
		fprintf(stderr, "di_init() failed\n");
		return (1);
	}

	list_create(&disks, sizeof (disk_info_t), offsetof(disk_info_t, link));
	i = walk_nodes(&root_node, &disks, verbose); /* walk device tree */
	di_fini(root_node);

	if (i == 0) {
		fprintf(stdout, "No disk(s) serviced by '" DISK_DRIVER "' ");
		fprintf(stderr, "driver were found.\n");
		return (1);
	}
	if (i < 0) {
		fprintf(stderr, "malloc() failed so not all disk(s) serviced by ");
		fprintf(stderr, "'" DISK_DRIVER "' driver were walked.\n");
		fprintf(stderr, "Encountered %d disk(s) before failure.\n", -i);
		return (1);
	}
	if (verbose) {
		fprintf(stderr, "Found %d disk(s) serviced by '" DISK_DRIVER "'", i);
		fprintf(stderr, " driver.\n");
	}

	if (list)
		print_disks(&disks, verbose);

	if (ctd) { /* if we were given a disk */
		for (disk = list_head(&disks); disk; disk = list_next(&disks, disk)) {
			if (has_bad_info(disk, verbose))
				continue;
			if (strcmp(CTD_START_IN_PATH(disk->path), dl_fw_to_disk) == 0 ||
					strcmp(disk->device, dl_fw_to_disk) == 0) {
				if (verbose) {
					fprintf(stderr, "matched provided disk ");
					fprintf(stderr, "'%s' to ", dl_fw_to_disk);
					fprintf(stderr, "'%s' or ", CTD_START_IN_PATH(disk->path));
					fprintf(stderr, "'%s'.\n", disk->device);
				}
				break;
			} else if (verbose) {
				fprintf(stderr, "did not match provided disk ");
				fprintf(stderr, "'%s' to ", dl_fw_to_disk);
				fprintf(stderr, "'%s' or ", CTD_START_IN_PATH(disk->path));
				fprintf(stderr, "'%s'.\n", disk->device);
			}
		}
		if (disk == NULL) {
			fprintf(stdout, "disk '%s' was not found.\n", dl_fw_to_disk);
		}
		else
			process_disk(disk, mode, fw_img, fw_len, wait, ignore_mismatch,
				verbose);
	}

	if (model) { /* if we were given a model */
		i = 0;
		for (disk = list_head(&disks); disk; disk = list_next(&disks, disk)) {
			if (has_bad_info(disk, verbose))
				continue;
			if (strcmp(dl_fw_to_model, disk->model) == 0) {
				i++;
				if (verbose) {
					fprintf(stderr, "matched provided model: ");
					fprintf(stderr, "'%s' to disk ", dl_fw_to_model);
					fprintf(stderr, "'%s'.\n", disk->device);
				}
				process_disk(disk, mode, fw_img, fw_len, wait, ignore_mismatch,
					verbose);
			} else if (verbose) {
				fprintf(stderr, "did not match provided model: ");
				fprintf(stderr, "'%s' to disk ", dl_fw_to_model);
				fprintf(stderr, "'%s'.\n", disk->device);
			}
		}
		if (i == 0)
			fprintf(stdout, "disk model '%s' was not found.\n", dl_fw_to_model);
	}

	/* cleanup start */
	if (fw_img)
		free(fw_img);
	while ((disk = (list_head(&disks))) != NULL) {
		list_remove(&disks, disk);
		free(disk);
	}
	list_destroy(&disks);

	return (0);
}
