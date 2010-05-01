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
 * Copyright 2007 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#pragma ident	"%Z%%M%	%I%	%E% SMI"

#include <sys/debug.h>
#include <sys/types.h>
#include <sys/zmod.h>

/*
 * use_gzip_hardware is a tunable parameter which can be used to
 * ON/OFF the hardware compression.
 */
int use_gzip_hardware = 0;

#ifdef _KERNEL
#include <sys/systm.h>

#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/ksynch.h>
#include <sys/cmn_err.h>
#include <sys/modctl.h>                         /* ddi_modopen() */
#include <sys/zio_compress.h>
#include <sys/zio_checksum.h> /* for the default checksum value */
#include <sys/time.h>

#define HRD_MOD_NAME "drv/wen"			/* wen driver module name */
#define HRD_MAGIC_KEY_LEN 2			/* length of magic key */


#define INDRA_VERIFY_COUNT_DEFAULT 10
#define INDRA_LOG_PATH "/var/log/indra_log" 
#define INDRA_VERFAIL_MESG "XXXX HARDWARE COMPRESSION VERIFICATION FAILED XXXX"
#define INDRA_MAGIC_DECOMP_NOT_SUPPORTED 2

static long indra_verify_failed = 0;
long indra_verify_count = INDRA_VERIFY_COUNT_DEFAULT;

static void write_log(char *filepath);
static void read_log(char *filepath);

typedef void 	*(hrd_open_func_t)(void);
typedef void 	(hrd_release_func_t)(void *cookie);
typedef int 	(hrd_magic_func_t)(void *cookie, int *offset, char **magic);
typedef int 	(compress_func_t)(void *cookie, void *input_buffer, void
			*output_buffer, size_t input_size, size_t
			*output_size);
typedef compress_func_t decompress_func_t;


typedef struct compression_hardware_operations {
	hrd_open_func_t 	*open_hrd;
	hrd_release_func_t 	*release_hrd;
	compress_func_t		*compress_func;
	decompress_func_t 	*decompress_func;
	hrd_magic_func_t 	*get_hrd_magic;
} chops;

enum hardware_states {
	HRD_CHECK 		= 1,
	HRD_PRESENT 		= 2,
	HRD_NOT_PRESENT 	= 3,
};

struct compression_hardware {
	kmutex_t 		hrd_data_lock;
	enum hardware_states	hrd_state;
	ddi_modhandle_t 	module;
	chops 			hrd_ops;
	void 			*cookie;
	char 			*magic;
	int		 	magic_offset;
};

/* Error Codes returned by the IOCTLs */
typedef enum error_code {
    NOT_COMPRESSIBLE = 8
} error_code_t;

static void deinit_hardware_device(struct compression_hardware *h);
static struct compression_hardware chrd;

int chk_magic;

/*
 * void init_gzip_hardware_compress(void)
 *	1. Initilizes the mutex
 *	2. changes hardware state to indicate hardware needs to be discovered
 */
void 
init_gzip_hardware_compress(void)
{
	mutex_init(&(chrd.hrd_data_lock), NULL, MUTEX_DEFAULT, NULL);
	chrd.hrd_state 		= HRD_CHECK;
	use_gzip_hardware	= 1;
	chk_magic		= 1;		/*Always check the magic key*/

         
}

/*
 * void fini_gzip_hardware_compress(void)
 *	Unintilizes the hardware compression
 */
void 
fini_gzip_hardware_compress(void)
{
	mutex_enter(&(chrd.hrd_data_lock));
	deinit_hardware_device(&chrd);
	mutex_exit(&(chrd.hrd_data_lock));
	mutex_destroy(&(chrd.hrd_data_lock));
	use_gzip_hardware	= 0;
	chk_magic		= 1;
}

/*
 * static void deinit_hardware_device(struct compression_hardware *)
 * 	1. frees compression_hardware structure
 *	2. Must be called with compression_hardware mutex held
 *	3. function will not release the mutex
 */
static void 
deinit_hardware_device(struct compression_hardware *h)
{
	chops *ops = &(h->hrd_ops);

	ops->decompress_func 	= NULL;
	ops->compress_func 	= NULL;
	ops->get_hrd_magic 	= NULL;

	if ((ops->release_hrd != NULL) && (h->cookie != NULL)) {
		ops->release_hrd(h->cookie);
	}	

	ops->release_hrd 	= NULL;
	h->cookie		= NULL;
	h->magic 		= NULL;
	h->magic_offset 	= 0;
	chk_magic		= 1;
	ops->open_hrd 		= NULL;

	if (h->module != NULL) {
		ddi_modclose(h->module);
		h->module 	= NULL;
	}

	/* hardware card not present or not properly intialized */
	h->hrd_state 		= HRD_NOT_PRESENT;
}

/*
 * static int initialize_hardware_device(struct compression_hardware *)
 * 	1. Initilizes the hardware structure
 *	2. Finds the symbols of hardware device driver
 * Return Value:
 *	0			success
 *	-1			failure
 */
static int 
initialize_hardware_device(struct compression_hardware *h)
{
	kmutex_t *hrd_data_lock 	= &(h->hrd_data_lock);
	enum hardware_states *hrd_state = &(h->hrd_state);
	chops *ops			= &(h->hrd_ops);
	int error;

	mutex_enter(hrd_data_lock);

	/* check if multiple threads called initialize_hardware_device */
	if (*hrd_state == HRD_NOT_PRESENT)
		goto failed;
	else if (*hrd_state == HRD_PRESENT)
		goto success;
        

	ASSERT(*hrd_state == HRD_CHECK);
	ASSERT(h->module == NULL);

	h->module = ddi_modopen(HRD_MOD_NAME, KRTLD_MODE_FIRST, &error);
	if (h->module == NULL)
		goto failed;

	ASSERT(ops->open_hrd == NULL);
	if ((ops->open_hrd = (hrd_open_func_t *) ddi_modsym(h->module,
			"wen_kern_open", &error)) == NULL)
		goto failed;

	ASSERT(ops->release_hrd == NULL);
	if ((ops->release_hrd = (hrd_release_func_t *) ddi_modsym(h->module,
			"wen_kern_release", &error)) == NULL)
		goto failed;

	ASSERT(h->cookie == NULL);
	if ((h->cookie = ops->open_hrd()) == NULL)
		goto failed;

	ASSERT(ops->get_hrd_magic == NULL);
	if ((ops->get_hrd_magic = (hrd_magic_func_t *) ddi_modsym(h->module,
			"wen_kern_hdr_magic", &error)) == NULL)
		goto failed;

	ASSERT(h->magic == NULL);
	chk_magic = ops->get_hrd_magic(h->cookie, &h->magic_offset, &h->magic);
	if (h->magic == NULL)
		goto failed;
        if (chk_magic == INDRA_MAGIC_DECOMP_NOT_SUPPORTED) {
            cmn_err(CE_WARN, "*** Decompression not supported by hardware ***");
        }

	ASSERT(ops->compress_func == NULL);
	if ((ops->compress_func = (compress_func_t *) ddi_modsym(h->module,
			"wen_kern_compress", &error)) == NULL)
		goto failed;

	ASSERT(ops->decompress_func == NULL);
	if ((ops->decompress_func = (decompress_func_t *) ddi_modsym(h->module,
			"wen_kern_decompress", &error)) == NULL)
		goto failed;
        
success:
	/* harware card present and properly initialized */
	*hrd_state = HRD_PRESENT;

        /* Check if in /var/log/indra_log         *
         * verification failed message is present */

        read_log(INDRA_LOG_PATH);

	mutex_exit(hrd_data_lock);
	return 0;

failed:
	deinit_hardware_device(h);
	mutex_exit(hrd_data_lock);
	return -1;
}

/*
 * size_t gzip_compress(void *, void *, size_t, size_t)
 *	Tries to compress data in hardware if it failes calls software decompression
 * Return Value:
 * 	success			length of the compressed data
 *	failure			length of the source data
 */
size_t
gzip_compress(void *s_start, void *d_start, size_t s_len, size_t d_len, int n)
{
	size_t dstlen = d_len;
        

	ASSERT(d_len <= s_len);


	if (use_gzip_hardware) {

	        struct compression_hardware *h = &chrd;
	        chops *ops = &h->hrd_ops;
	        enum hardware_states *hrd_state = &h->hrd_state;
                int ret_val = 0;
                size_t d_verifylen = 0;
                void *d_verifybuf = NULL;
                static long verify_count = 0;

		if (*hrd_state == HRD_CHECK) {
			if (initialize_hardware_device(h) < 0) {
				cmn_err(CE_NOTE, "WEN device initialization "
					"failed.");
			}
		}

		if (*hrd_state == HRD_PRESENT) {
			/* hardware compression */
			ASSERT(h->module		!= NULL);
			ASSERT(ops->open_hrd 		!= NULL);
			ASSERT(ops->release_hrd		!= NULL);
			ASSERT(h->cookie 		!= NULL);
			ASSERT(ops->get_hrd_magic	!= NULL);
			ASSERT(h->magic 		!= NULL);
			ASSERT(ops->compress_func 	!= NULL);
			ASSERT(ops->decompress_func 	!= NULL);

                        if (indra_verify_failed) {
                            goto failed;
                        }

			ret_val = ops->compress_func(h->cookie, s_start, d_start,
					s_len, &dstlen);
                        if (0 == ret_val) {

                            if (indra_verify_count > 0) {

                                /* Compression Verification */ 

                                mutex_enter(&(chrd.hrd_data_lock));
                                verify_count++;
                                if (verify_count % (indra_verify_count+1) == 0) {
                                    verify_count = 0;
                                    mutex_exit(&(chrd.hrd_data_lock));

                                    d_verifylen = s_len;
                                    d_verifybuf = kmem_zalloc(s_len, KM_SLEEP);

                                    if (chk_magic == INDRA_MAGIC_DECOMP_NOT_SUPPORTED) {
                                        /* verify through software decompression */
	                                if (z_uncompress(d_verifybuf, &d_verifylen, d_start, dstlen) != Z_OK) {
                                            indra_verify_failed = 1;
                                        }
                                        /* verify through hardware decompression */
                                    } else if (ops->decompress_func(h->cookie, d_start,
	                                               d_verifybuf, dstlen, &d_verifylen) != 0) {
                                        indra_verify_failed = 1;
                                    } 

                                    /* compare original and decompressed data */
                                    if (!indra_verify_failed) {
                                        if (memcmp(s_start, d_verifybuf, s_len)!=0) {
                                            indra_verify_failed = 1;
                                        }
                                    }

                                    kmem_free(d_verifybuf, s_len);

                                    if (indra_verify_failed) {
                                        cmn_err(CE_WARN, INDRA_VERFAIL_MESG); 
                                        write_log(INDRA_LOG_PATH);
		                        if (d_len == s_len) {
		                            bcopy(s_start, d_start, s_len);
                                        }
                                        goto failed;
                                    }
                                } else {
                                    mutex_exit(&(chrd.hrd_data_lock));
                                }
                          }
       
		 	  goto success;
                     } else if (NOT_COMPRESSIBLE == ret_val) {
                            goto failed; 
                        }
		}
	}

	dstlen = d_len;
	/* software compression */
	if (z_compress_level(d_start, &dstlen, s_start, s_len, n) != Z_OK) {
		if (d_len != s_len)
			goto failed;

		bcopy(s_start, d_start, s_len);
		goto failed;
	}
success:	
	return(dstlen);
failed:
	return (s_len);
}

/*
 * int match_magic(char *, char *, int, int)
 *	matches the hardware header magic key.
 * Return Value:
 * 	1		Magic key matched
 * 	0		Magic key not matched
 */
inline int 
match_magic(char *s, char *magic, int offset, int length)
{
	int rc;
	rc = bcmp((s + offset), magic, length);
	return (rc ? 0 : 1);
}

/*
 * int gzip_decompress(void *, void *, size_t, size_t, int)
 *	checks if data can be decompressed in hardware if yes passed it also
 *	calls software decompression if required.
 * Return Value:
 *	0		success
 *	-1		failure
 */
 /*ARGSUSED*/
int
gzip_decompress(void *s_start, void *d_start, size_t s_len, size_t d_len, int n)
{
	size_t dstlen = d_len;

	ASSERT(d_len >= s_len);

	if (use_gzip_hardware) {
	        struct compression_hardware *h = &chrd;
	        chops *ops = &h->hrd_ops;
	        enum hardware_states *hrd_state = &h->hrd_state;
		if (*hrd_state == HRD_CHECK) {
			if (initialize_hardware_device(h) < 0) {
				cmn_err(CE_NOTE, "WEN device initialization"
					"failed.");
			}
		}

		if (*hrd_state == HRD_PRESENT) {
			/* hardware decompression */
			ASSERT(h->module 		!= NULL);
			ASSERT(ops->open_hrd		!= NULL);
			ASSERT(ops->release_hrd		!= NULL);
			ASSERT(h->cookie 		!= NULL);
			ASSERT(ops->get_hrd_magic	!= NULL);
			ASSERT(h->magic 		!= NULL);
			ASSERT(ops->compress_func 	!= NULL);
			ASSERT(ops->decompress_func	!= NULL);
                        if (chk_magic != INDRA_MAGIC_DECOMP_NOT_SUPPORTED) {
			    if (!chk_magic || match_magic(s_start, h->magic,
			        h->magic_offset, HRD_MAGIC_KEY_LEN)) {
			    	/* harware can decompress */
				    if (ops->decompress_func(h->cookie, s_start,
				        d_start, s_len, &dstlen) == 0) 
					goto success;
			    }		
                        }
	        }
	}

	dstlen = d_len;
	if (z_uncompress(d_start, &dstlen, s_start, s_len) != Z_OK)
		goto failed;
success:
	return (0);
failed:
	return (-1);
}

/*
 *  Writes verify fail message to a log file
 *  on compression verification failure 
 *
 *  Return Value:  NULL
 */

 static void write_log(char *filepath)
 {
     size_t bufsize = 4096;
     size_t buflen;
     char *buf;
     char *temp;
     vnode_t *vp;
     int oflags = FWRITE | FTRUNC | FCREAT | FOFFMAX;

     buf = kmem_zalloc(bufsize, KM_SLEEP);
     temp = kmem_zalloc(MAXPATHLEN, KM_SLEEP);

     snprintf(temp, MAXPATHLEN, "%s.tmp", filepath);
     snprintf(buf, bufsize, 
             "Indra Networks, Inc. Copyright 2009\n"
             "--------------------------------------\n"
             INDRA_VERFAIL_MESG 
             "\n"
     	     );

     buflen = strlen(buf);
     if (vn_open(temp, UIO_SYSSPACE, oflags, 0644, &vp, CRCREAT, 0) == 0) {
         if (vn_rdwr(UIO_WRITE, vp, buf, buflen, 0, UIO_SYSSPACE,
              0, RLIM64_INFINITY, kcred, NULL) == 0 &&
              VOP_FSYNC(vp, FSYNC, kcred, NULL) == 0) {
              (void) vn_rename(temp, filepath, UIO_SYSSPACE);
         } else {
             cmn_err(CE_WARN, "*** Failed to write %s ***", temp);
         }
         (void) VOP_CLOSE(vp, oflags, 1, 0, kcred, NULL);
                        VN_RELE(vp);
     } else {
         cmn_err(CE_WARN, "*** Failed to open %s ***", temp);
     }

     (void) vn_remove(temp, UIO_SYSSPACE, RMFILE);

     kmem_free(buf, bufsize);
     kmem_free(temp, MAXPATHLEN);

     return;
 }

/*
 *  Reads error information from the log file
 *  If verify failure message found , it sets a flag
 *  indicating no further h/w or s/w compression
 *
 *  Return Value:  NULL
 */

 static void read_log(char *filepath)
 {
     char *buf;
     vnode_t *vp;
     int bufsize = 4096;
     int oflags = FREAD;
     ssize_t resid = 0;

     buf = kmem_zalloc(bufsize, KM_SLEEP);

     if (vn_open(filepath, UIO_SYSSPACE, oflags, 0, &vp, 0, 0) == 0) {
         if (vn_rdwr(UIO_READ, vp, buf, bufsize, (offset_t)0, UIO_SYSSPACE,
              0, (rlim64_t)0, kcred, &resid) == 0) {
             if (strstr(buf, INDRA_VERFAIL_MESG)) {
                 cmn_err(CE_WARN, INDRA_VERFAIL_MESG);
                 indra_verify_failed = 1;
             }
         }
         (void) VOP_CLOSE(vp, oflags, 1, (offset_t)0, kcred, NULL);
         VN_RELE(vp);
     }

     kmem_free(buf, bufsize);

     return;
 }

#else /* !_KERNEL */

#include <strings.h>
size_t
gzip_compress(void *s_start, void *d_start, size_t s_len, size_t d_len, int n)
{
	size_t dstlen = d_len;

	ASSERT(d_len <= s_len);

	if (z_compress_level(d_start, &dstlen, s_start, s_len, n) != Z_OK) {
		if (d_len != s_len)
			return (s_len);

		bcopy(s_start, d_start, s_len);
		return (s_len);
	}

	return (dstlen);
}

/*ARGSUSED*/
int
gzip_decompress(void *s_start, void *d_start, size_t s_len, size_t d_len, int n)
{
	size_t dstlen = d_len;

	ASSERT(d_len >= s_len);

	if (z_uncompress(d_start, &dstlen, s_start, s_len) != Z_OK)
		return (-1);

	return (0);
}

void
init_gzip_hardware_compress(void)
{
}

void
fini_gzip_hardware_compress(void)
{
}

#endif
