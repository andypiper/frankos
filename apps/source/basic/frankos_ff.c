/*
 * frankos_ff.c — FatFS function implementations for the Frank OS port of MMBasic
 *
 * All functions are routed through the Frank OS sys_table (runtime function
 * pointer table at 0x20000000).  Functions not present in the table are
 * stubbed as no-ops / always-success.
 *
 * IMPORTANT: this file must NOT include m-os-api.h or m-os-api-ff.h.  Those
 * headers define macros like  #define fclose(f) f_close(f)  which conflict
 * with <stdio.h> / pico/stdlib.h already included by other translation units.
 *
 * Copyright (c) 2026 Mikhail Matveev <xtreme@rh1.tech>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <stdint.h>
#include <stddef.h>

/* "ff.h" resolves to frankos/ff.h which does #include_next "ff.h" →
 * basic/ff.h.  That gives us FRESULT, FIL, DIR, FILINFO, FATFS, TCHAR,
 * BYTE, UINT, DWORD, FSIZE_t, LBA_t, WORD, MKFS_PARM. */
#include "ff.h"

/* ── sys_table access ───────────────────────────────────────────────────────
 *
 * Frank OS places its system-call table at the very start of SRAM.
 * Each entry is a function pointer.  The indices below match the values
 * exported in api/m-os-api-ff.h (without including that file).
 */
static void * const * const _sys_tbl = (void * const *)0x20000000UL;

/* sys_table index constants (must match m-os-api-ff.h) */
#define SYS_F_OPEN      46
#define SYS_F_CLOSE     47
#define SYS_F_WRITE     48
#define SYS_F_READ      49
#define SYS_F_STAT      50
#define SYS_F_LSEEK     51
#define SYS_F_TRUNCATE  52
#define SYS_F_SYNC      53
#define SYS_F_OPENDIR   54
#define SYS_F_CLOSEDIR  55
#define SYS_F_READDIR   56
#define SYS_F_UNLINK    57
#define SYS_F_MKDIR     58
#define SYS_F_RENAME    59
#define SYS_F_GETFREE   61

/* ── Core file operations ───────────────────────────────────────────────── */

FRESULT f_open(FIL *fp, const TCHAR *path, BYTE mode)
{
    typedef FRESULT (*fn_t)(FIL *, const TCHAR *, BYTE);
    return ((fn_t)_sys_tbl[SYS_F_OPEN])(fp, path, mode);
}

FRESULT f_close(FIL *fp)
{
    typedef FRESULT (*fn_t)(FIL *);
    return ((fn_t)_sys_tbl[SYS_F_CLOSE])(fp);
}

FRESULT f_read(FIL *fp, void *buff, UINT btr, UINT *br)
{
    typedef FRESULT (*fn_t)(FIL *, void *, UINT, UINT *);
    return ((fn_t)_sys_tbl[SYS_F_READ])(fp, buff, btr, br);
}

FRESULT f_write(FIL *fp, const void *buff, UINT btw, UINT *bw)
{
    typedef FRESULT (*fn_t)(FIL *, const void *, UINT, UINT *);
    return ((fn_t)_sys_tbl[SYS_F_WRITE])(fp, buff, btw, bw);
}

FRESULT f_lseek(FIL *fp, FSIZE_t ofs)
{
    typedef FRESULT (*fn_t)(FIL *, FSIZE_t);
    return ((fn_t)_sys_tbl[SYS_F_LSEEK])(fp, ofs);
}

FRESULT f_truncate(FIL *fp)
{
    typedef FRESULT (*fn_t)(FIL *);
    return ((fn_t)_sys_tbl[SYS_F_TRUNCATE])(fp);
}

FRESULT f_sync(FIL *fp)
{
    typedef FRESULT (*fn_t)(FIL *);
    return ((fn_t)_sys_tbl[SYS_F_SYNC])(fp);
}

/* ── Directory operations ───────────────────────────────────────────────── */

FRESULT f_opendir(DIR *dp, const TCHAR *path)
{
    typedef FRESULT (*fn_t)(DIR *, const TCHAR *);
    return ((fn_t)_sys_tbl[SYS_F_OPENDIR])(dp, path);
}

FRESULT f_closedir(DIR *dp)
{
    typedef FRESULT (*fn_t)(DIR *);
    return ((fn_t)_sys_tbl[SYS_F_CLOSEDIR])(dp);
}

FRESULT f_readdir(DIR *dp, FILINFO *fno)
{
    typedef FRESULT (*fn_t)(DIR *, FILINFO *);
    return ((fn_t)_sys_tbl[SYS_F_READDIR])(dp, fno);
}

/* f_findfirst / f_findnext — implemented on top of opendir/readdir */
FRESULT f_findfirst(DIR *dp, FILINFO *fno, const TCHAR *path, const TCHAR *pattern)
{
    (void)pattern;   /* pattern filtering not supported; return all entries */
    FRESULT res = f_opendir(dp, path);
    if (res == FR_OK)
        res = f_readdir(dp, fno);
    return res;
}

FRESULT f_findnext(DIR *dp, FILINFO *fno)
{
    return f_readdir(dp, fno);
}

/* ── Filesystem-level operations ────────────────────────────────────────── */

FRESULT f_stat(const TCHAR *path, FILINFO *fno)
{
    typedef FRESULT (*fn_t)(const TCHAR *, FILINFO *);
    return ((fn_t)_sys_tbl[SYS_F_STAT])(path, fno);
}

FRESULT f_unlink(const TCHAR *path)
{
    typedef FRESULT (*fn_t)(const TCHAR *);
    return ((fn_t)_sys_tbl[SYS_F_UNLINK])(path);
}

FRESULT f_mkdir(const TCHAR *path)
{
    typedef FRESULT (*fn_t)(const TCHAR *);
    return ((fn_t)_sys_tbl[SYS_F_MKDIR])(path);
}

FRESULT f_rename(const TCHAR *path_old, const TCHAR *path_new)
{
    typedef FRESULT (*fn_t)(const TCHAR *, const TCHAR *);
    return ((fn_t)_sys_tbl[SYS_F_RENAME])(path_old, path_new);
}

FRESULT f_getfree(const TCHAR *path, DWORD *nclst, FATFS **fatfs)
{
    typedef FRESULT (*fn_t)(const TCHAR *, DWORD *, FATFS **);
    return ((fn_t)_sys_tbl[SYS_F_GETFREE])(path, nclst, fatfs);
}

/* ── Stubs for operations not available via sys_table ───────────────────── */

FRESULT f_mount(FATFS *fs, const TCHAR *path, BYTE opt)
{
    (void)fs; (void)path; (void)opt;
    return FR_OK;   /* Frank OS mounts volumes automatically */
}

FRESULT f_getcwd(TCHAR *buff, UINT len)
{
    if (buff && len > 1) { buff[0] = '/'; buff[1] = '\0'; }
    return FR_OK;
}

FRESULT f_chdir(const TCHAR *path)
{
    (void)path;
    return FR_OK;   /* no per-task CWD in Frank OS FatFS */
}

FRESULT f_chdrive(const TCHAR *path)
{
    (void)path;
    return FR_OK;
}

FRESULT f_chmod(const TCHAR *path, BYTE attr, BYTE mask)
{
    (void)path; (void)attr; (void)mask;
    return FR_OK;
}

FRESULT f_utime(const TCHAR *path, const FILINFO *fno)
{
    (void)path; (void)fno;
    return FR_OK;
}

FRESULT f_getlabel(const TCHAR *path, TCHAR *label, DWORD *vsn)
{
    (void)path;
    if (label) label[0] = '\0';
    if (vsn)   *vsn = 0;
    return FR_OK;
}

FRESULT f_setlabel(const TCHAR *label)
{
    (void)label;
    return FR_OK;
}

FRESULT f_forward(FIL *fp, UINT (*func)(const BYTE *, UINT), UINT btf, UINT *bf)
{
    (void)fp; (void)func; (void)btf;
    if (bf) *bf = 0;
    return FR_NOT_ENABLED;
}

FRESULT f_expand(FIL *fp, FSIZE_t fsz, BYTE opt)
{
    (void)fp; (void)fsz; (void)opt;
    return FR_NOT_ENABLED;
}

FRESULT f_mkfs(const TCHAR *path, const MKFS_PARM *opt, void *work, UINT len)
{
    (void)path; (void)opt; (void)work; (void)len;
    return FR_NOT_ENABLED;
}

FRESULT f_fdisk(BYTE pdrv, const LBA_t ptbl[], void *work)
{
    (void)pdrv; (void)ptbl; (void)work;
    return FR_NOT_ENABLED;
}

FRESULT f_setcp(WORD cp)
{
    (void)cp;
    return FR_OK;
}
