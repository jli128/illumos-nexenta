/*
 * Copyright 2008 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef _COMMON_TEST_H_
#define	_COMMON_TEST_H_

#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include <stdlib.h>
#include <sys/param.h>
#include <errno.h>
#include <stdarg.h>

#include <tet_api.h>

#ifdef __cplusplus
extern "C" {
#endif

#define	SUCCESS				0
#define	UNEXPECTED_FAIL			-1
#define	EXPECTED_FAIL			-2

#define	DEF_RSH				"rsh"
#define	DEF_RCP				"rcp"
#define	DEF_PING			"hatimerun -t 60 /usr/sbin/ping"

#define	E_GOSU				0x01
#define	E_NOTREPORT			0x02
#define	E_BACKGROUND			0x04
#define	E_HATIMERUN			0x08
#define	E_HIPRIORITY			0x10
#define	E_REPORT			0x20
#define	E_BGREPORT			0x40
#define	E_FILE				0x80
#define	E_CFGENV			0x800
#define	E_REBOOTPANIC			0x400
#define	E_TRACE				0x200

#define	MAXCMDLEN			1024
#define	FILE_NAME_LEN			256
#define	MSG_LEN				2048
#define	LINES_MAX			4096
#define	TOTAL_MAX			30000

#define	MAX_REPORT_BUF_SIZE		8192

#define	REMOTE_CFG_FILE_PREFIX		"/var/tmp/CFG"
#define	EXECUTE_FILE_NAME_PREFIX	"/tmp/EXECUTE"
#define	WAIT_RCP			30

extern char   *CTI_RSH;
extern char   *CTI_RCP;
extern char   *CTI_PING;

extern int    sysid;
extern int    sysno;

extern int    master;
extern int    first_system;
extern int    second_system;
extern int    third_system;
extern int    fourth_system;

extern int    master_sysno;
extern int    master_sysid;
extern char   *master_sysname;

extern int    num_of_sys;
extern int    num_of_rem_sys;
extern int    num_of_slaves;

extern int    *rem_sysids;
extern int    *all_sysids;
extern char   **all_sysnames;
extern char   *active_systems_mask;

extern int    Debug;
extern int    CmdTrace;
extern int    execute_command_line_no;

extern int    assert_no;
extern int    assert_req_node;
extern int    print_test_debug_flag;
extern int    check_print_size_flag;

extern int    cti_commonstartup(void);
extern int    cti_commoncleanup(void);

extern int    cti_checktestprintsize(int max_size, char *format, va_list ap);
extern char   *cti_strtestprint(char *head, char *format, va_list ap);

/* PRINTFLIKES */
extern void   cti_print(char *head, char *format, va_list ap);
extern void   cti_report(char *format, ...);
extern void   cti_warning(char *format, ...);
extern void   cti_debug(char *format, ...);
extern void   cti_assert(int no, char *text);
extern void   cti_requirement(int no, char *text);
extern void   cti_result(int result, char *format, ...);
extern void   cti_deleteall(char *format, ...);
extern void   cti_delete(int ic, char *format, ...);
extern void   cti_cancelall(int result, char *format, ...);
extern int   cti_cancel(int test_num, int result, char *format, ...);

extern int    cti_sync(int error);
extern int    cti_syncnoresult(int error);
extern int    cti_synctimeout(int timeout, int error);

extern int    cti_checkresult(int value);
extern int    cti_checkuniqueint(long long value);
extern int    cti_checkuniqueuint(unsigned long long value);
extern int    cti_checkunique(char *data, int size);
extern int    cti_synccheckdata(char *ptr, int len,
    int (*CheckFunc)(char **, int));
extern int    cti_sendrcvdata(char *data_ptr, int data_len, int snd_sysno);

extern void   cti_reportsetup(void);

extern int    cti_getsysid_sysno(int rem_sysno);
extern int    cti_getsysid_sysname(char *rem_sysname);
extern int    cti_getsysno_sysid(int rem_sysid);
extern int    cti_getsysno_sysname(char *rem_sysname);
extern char   *cti_getsysname_sysno(int rem_sysno);
extern char   *cti_getsysname_sysid(int rem_sysid);

extern int    cti_getexecuteoutname(char *outname, int no);
extern int    cti_getexecuteretname(char *outname, int no);

extern int    cti_execute(int flg, char *cmd);
extern int    cti_bgexecute(int flg, char *cmd, int *cmd_no);
extern int    cti_hiexecute(int flg, char *cmd);
extern int    cti_haexecute(int flg, int timeout, char *cmd);
extern int    cti_hihaexecute(int flg, int timeout, char *cmd);
extern int    cti_getstatusbg(int flg, int cmd_no);

extern int    cti_remoteexecute(int flg, char *node, char *cmd);
extern int    cti_bgremoteexecute(int flg, char *node, char *cmd,
    int *cmd_no);
extern int    cti_hiremoteexecute(int flg, char *node, char *cmd);
extern int    cti_haremoteexecute(int flg, char *node, int timeout,
    char *cmd);
extern int    cti_hiharemoteexecute(int flg, char *node, int timeout,
    char *cmd);
extern int    cti_getstatusbgremote(int flg, char *node_name,
    int cmd_no);
extern int    cti_getremoteenvfile(char *node_name, char *file_name);
extern int    cti_getremoteenvfilemaster(char *node_name, char *file_name);
extern int    cti_remotecopyenv(char *node);
extern int    cti_putremoteenvstr(char *node, char *name, char *val);
extern int    cti_putremoteenvint(char *node, char *name, int val);
extern int    cti_remotecleanupenv(char *node);
extern int    cti_remotecheckenv(char *node);

extern int    cti_outputjournal(char *outname, int report);
extern int    cti_outputjournalno(int cmd_no);
extern int    System(char *cmd);

extern void   cti_sigset(void);

extern int    cti_activehost(int rem_sysno, int active);
extern int    cti_getactivehost(int rem_sysno, int *active);

extern void   cti_sleep(int sleep_time);

extern int    cti_ismaster(int rem_sysno);

#ifdef __cplusplus
}
#endif

#endif /* _COMMON_TEST_H_ */
