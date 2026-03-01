/*
 * frankos/diskio.h - Disk I/O stub for Frank OS
 *
 * FileIO.c calls disk_ioctl(0, MMC_GET_OCR, buff) inside CheckSDCard().
 * On Frank OS there is no raw SD-card driver; we stub the types and
 * declare no-op functions so the code compiles and the SD-present check
 * always fails gracefully.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef FRANKOS_DISKIO_H
#define FRANKOS_DISKIO_H

#include <stdint.h>

#ifndef BYTE
typedef uint8_t  BYTE;
#endif
#ifndef DWORD
typedef uint32_t DWORD;
#endif

/* Status codes */
typedef BYTE  DSTATUS;
#define STA_NOINIT   0x01
#define STA_NODISK   0x02
#define STA_PROTECT  0x04

/* Result codes */
typedef enum {
    RES_OK  = 0,
    RES_ERROR,
    RES_WRPRT,
    RES_NOTRDY,
    RES_PARERR
} DRESULT;

/* Generic ioctls */
#define CTRL_SYNC          0
#define GET_SECTOR_COUNT   1
#define GET_SECTOR_SIZE    2
#define GET_BLOCK_SIZE     3
#define CTRL_TRIM          4

/* MMC/SD-specific ioctls */
#define MMC_GET_TYPE       10
#define MMC_GET_CSD        11
#define MMC_GET_CID        12
#define MMC_GET_OCR        13
#define MMC_GET_SDSTAT     14

/* Function stubs — no SD hardware on Frank OS. */
static inline DSTATUS disk_initialize(BYTE drv)
{ (void)drv; return STA_NOINIT | STA_NODISK; }

static inline DSTATUS disk_status(BYTE drv)
{ (void)drv; return STA_NOINIT | STA_NODISK; }

static inline DRESULT disk_read(BYTE drv, BYTE *buff, DWORD sector, unsigned count)
{ (void)drv; (void)buff; (void)sector; (void)count; return RES_ERROR; }

static inline DRESULT disk_write(BYTE drv, const BYTE *buff, DWORD sector, unsigned count)
{ (void)drv; (void)buff; (void)sector; (void)count; return RES_ERROR; }

static inline DRESULT disk_ioctl(BYTE drv, BYTE cmd, void *buff)
{ (void)drv; (void)cmd; (void)buff; return RES_ERROR; }

#endif /* FRANKOS_DISKIO_H */
