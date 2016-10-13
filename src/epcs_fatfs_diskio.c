/*-----------------------------------------------------------------------*/
/* Low level disk I/O module for EPCS                                    */
/*-----------------------------------------------------------------------*/

#include "diskio.h"		/* FatFs lower layer API */
#include "system.h"
#include "io.h"
#include <sched.h>
#include <stddef.h>

/* Definitions of physical drive number for each drive */
#define EPCS	0

#if (defined(EPCS_FATFS_IF_PERIDOT_SWI) && defined(EPCS_FATFS_IF_ALTERA_SPI)) || \
	(defined(EPCS_FATFS_IF_PERIDOT_SWI) && defined(EPCS_FATFS_IF_ALTERA_EPCS)) || \
	(defined(EPCS_FATFS_IF_ALTERA_SPI) && defined(EPCS_FATFS_IF_ALTERA_EPCS))
# error "Only one of interface.use_xxx options can be enabled"
#endif

#if defined(EPCS_FATFS_IF_PERIDOT_SWI)
# define USE_PERIDOT_SWI
# include "peridot_swi.h"
# define EPCS_COMMAND(wlen,wptr,rlen,rptr,f) \
	peridot_swi_flash_command((wlen), (wptr), (rlen), (rptr), (f))
# define EPCS_MERGE PERIDOT_SWI_FLASH_COMMAND_MERGE
#elif defined(EPCS_FATFS_IF_ALTERA_SPI)
# define USE_ALTERA_SPI
# define ALTERA_SPI_BASE \
	(EPCS_FATFS_IF_INST_NAME##_BASE)
# define ALTERA_SPI_SLAVE EPCS_FATFS_IF_SPI_SLAVE
#elif defined(EPCS_FATFS_IF_ALTERA_SPI)
# define USE_ALTERA_EPCS
/* EPCS controller uses altera_avalon_spi internally */
# define ALTERA_SPI_BASE \
	(EPCS_FATFS_IF_INST_NAME##_BASE+EPCS_FATFS_IF_INST_NAME##_REGISTER_OFFSET)
# define ALTERA_SPI_SLAVE 0
#else
# error "One of interface.use_xxx options should be enabled"
#endif
#ifdef ALTERA_SPI_BASE
# include "altera_avalon_spi.h"
# define EPCS_COMMAND(wlen,wptr,rlen,rptr,f) \
	alt_avalon_spi_command((ALTERA_SPI_BASE), \
			(ALTERA_SPI_SLAVE), \
			(wlen), (wptr), (rlen), (rptr), (f))
# define EPCS_MERGE ALT_AVALON_SPI_COMMAND_MERGE
#endif

#if (EPCS_FATFS_FLASH_SECTOR == 512)
# define SECT_SHIFT	9
#elif (EPCS_FATFS_FLASH_SECTOR == 1024)
# define SECT_SHIFT	10
#elif (EPCS_FATFS_FLASH_SECTOR == 2048)
# define SECT_SHIFT	11
#elif (EPCS_FATFS_FLASH_SECTOR == 4096)
# define SECT_SHIFT	12
#else
# error "Unsupported sector size"
#endif
#define SECT_SIZE	(1<<SECT_SHIFT)
#define PAGE_SIZE	256

#define EPCS_FATFS_FLASH_CMD_FREAD	0x0b
#define EPCS_FATFS_FLASH_CMD_WREN	0x06
#define EPCS_FATFS_FLASH_CMD_RDSR	0x05
//#define EPCS_FATFS_FLASH_CMD_ERASE	// defined in system.h
#define EPCS_FATFS_FLASH_CMD_PROG	0x02
#define EPCS_FATFS_FLASH_CMD_RDID	0x9f

static UINT epcs_total_sectors;	// Includes hidden sectors
static UINT epcs_start_sector;
static UINT epcs_end_sector;


/*-----------------------------------------------------------------------*/
/* Get Drive Status                                                      */
/*-----------------------------------------------------------------------*/
static DSTATUS EPCS_disk_status(void)
{
	/* Always ready */
	return 0;
}

DSTATUS disk_status (
	BYTE pdrv		/* Physical drive number to identify the drive */
)
{
	switch (pdrv)
	{
	case EPCS:
		return EPCS_disk_status();
	}
	return STA_NOINIT;
}



/*-----------------------------------------------------------------------*/
/* Initialize a Drive                                                    */
/*-----------------------------------------------------------------------*/
static DSTATUS EPCS_disk_initialize(void)
{
	static const BYTE rdid[1] = { EPCS_FATFS_FLASH_CMD_RDID };
	BYTE id[3];
	int result;

	// Detect capacity by READ IDENTIFICATION command
	result = EPCS_COMMAND(sizeof(rdid), rdid, sizeof(id), id, 0);
	if ((result < 0) || (id[0] == 0xff) || (id[1] == 0xff) || (id[2] == 0xff))
	{
		return STA_NOINIT;
	}
	epcs_total_sectors = (1 << (id[2] - SECT_SHIFT));

	// Calculate parameters
	epcs_start_sector = (EPCS_FATFS_FLASH_START + SECT_SIZE - 1) >> SECT_SHIFT;
#if (EPCS_FATFS_FLASH_END == 0)
	epcs_end_sector = epcs_total_sectors;
#else
	epcs_end_sector = EPCS_FATFS_FLASH_END >> SECT_SHIFT;
#endif

	if (epcs_start_sector >= epcs_end_sector)
	{
		return STA_NOINIT;
	}

	return 0;
}

DSTATUS disk_initialize (
	BYTE pdrv		/* Physical drive number to identify the drive */
)
{
	switch (pdrv)
	{
	case EPCS:
		return EPCS_disk_initialize();
	}
	return STA_NOINIT;
}



/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/
static DRESULT EPCS_disk_read(BYTE *buff, DWORD sector, UINT count)
{
	DWORD addr;
	BYTE cmd[5];
	int result;

	sector += epcs_start_sector;
	if ((sector + count) > epcs_end_sector)
	{
		return RES_PARERR;
	}
	addr = (sector << SECT_SHIFT);

	cmd[0] = EPCS_FATFS_FLASH_CMD_FREAD;
	cmd[1] = (BYTE)(addr >> 16);
	cmd[2] = (BYTE)(addr >> 8);
	cmd[3] = (BYTE)(addr >> 0);
	cmd[4] = 0xff;

	result = EPCS_COMMAND(sizeof(cmd), cmd, (count << SECT_SHIFT), buff, 0);
	if (result < 0)
	{
		return RES_ERROR;
	}

	return RES_OK;
}

DRESULT disk_read (
	BYTE pdrv,		/* Physical drive number to identify the drive */
	BYTE *buff,		/* Data buffer to store read data */
	DWORD sector,	/* Sector address in LBA */
	UINT count		/* Number of sectors to read */
)
{
	switch (pdrv)
	{
	case EPCS:
		return EPCS_disk_read(buff, sector, count);
	}

	return RES_PARERR;
}



/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/

#if _USE_WRITE
static DRESULT EPCS_disk_write(const BYTE *buff, DWORD sector, UINT count)
{
	static const BYTE encmd[1] = { EPCS_FATFS_FLASH_CMD_WREN };
	static const BYTE srcmd[1] = { EPCS_FATFS_FLASH_CMD_RDSR };
	BYTE wrcmd[4];
	int result;
	DWORD addr;
	INT page;
#ifdef EPCS_FATFS_FLASH_VERIFY
	const BYTE *v_buff = buff;
	DWORD v_addr;
	BYTE rdcmd[4];
	BYTE c_buff[PAGE_SIZE];
#endif

	sector += epcs_start_sector;
	if ((sector + count) > epcs_end_sector)
	{
		return RES_PARERR;
	}
	addr = (sector << SECT_SHIFT);
#ifdef EPCS_FATFS_FLASH_VERIFY
	v_addr = addr;
#endif

	for (; count > 0; --count)
	{
		wrcmd[0] = EPCS_FATFS_FLASH_CMD_ERASE;

		// Program pages
		for (page = -1; page < (SECT_SIZE / PAGE_SIZE); ++page)
		{
			// Write Enable
			result = EPCS_COMMAND(sizeof(encmd), encmd, 0, NULL, 0);
			if (result < 0)
			{
				return RES_ERROR;
			}

			wrcmd[1] = (BYTE)(addr >> 16);
			wrcmd[2] = (BYTE)(addr >> 8);
			wrcmd[3] = (BYTE)(addr >> 0);

			if (page < 0)
			{
				// Erase sector
				result = EPCS_COMMAND(sizeof(wrcmd), wrcmd, 0, NULL, 0);
				if (result < 0)
				{
					return RES_ERROR;
				}
				wrcmd[0] = EPCS_FATFS_FLASH_CMD_PROG;
			}
			else
			{
				// Page program
				result = EPCS_COMMAND(sizeof(wrcmd), wrcmd, 0, NULL, EPCS_MERGE);
				if (result >= 0)
				{
					result = EPCS_COMMAND(PAGE_SIZE, buff, 0, NULL, 0);
					buff += PAGE_SIZE;
					addr += PAGE_SIZE;
				}
			}

			if (result < 0)
			{
				return RES_ERROR;
			}

			// Wait until finish erase/program
			for (;;)
			{
				BYTE sr[2];
				result = EPCS_COMMAND(sizeof(srcmd), srcmd, sizeof(sr), sr, 0);
				if (result < 0)
				{
					return RES_ERROR;
				}
				if ((sr[1] & 1) == 0)
				{
					// WIP bit is off
					break;
				}
			}
		}

#ifdef EPCS_FATFS_FLASH_VERIFY
		// Verify
		rdcmd[0] = EPCS_FATFS_FLASH_CMD_FREAD;
		rdcmd[1] = (BYTE)(v_addr >> 16);
		rdcmd[2] = (BYTE)(v_addr >> 8);
		rdcmd[3] = (BYTE)(v_addr >> 0);
		rdcmd[4] = 0xff;
		result = EPCS_COMMAND(sizeof(rdcmd), rdcmd, 0, NULL, EPCS_MERGE);
		if (result < 0)
		{
			return RES_ERROR;
		}
		for (page = 0; page < (SECT_SIZE / PAGE_SIZE); ++page)
		{
			result = EPCS_COMMAND(0, NULL, PAGE_SIZE, c_buf, EPCS_MERGE);
			if (result < 0)
			{
				return RES_ERROR;
			}
			if (memcmp(c_buff, v_buff, PAGE_SIZE) != 0)
			{
				// Mismatch
				EPCS_COMMAND(0, NULL, 0, NULL, 0);
				return RES_ERROR;
			}
			v_buff += PAGE_SIZE;
		}
		EPCS_COMMAND(0, NULL, 0, NULL, 0);
		v_addr += SECT_SIZE;
#endif
	}

	return RES_OK;
}

DRESULT disk_write (
	BYTE pdrv,			/* Physical drive number to identify the drive */
	const BYTE *buff,	/* Data to be written */
	DWORD sector,		/* Sector address in LBA */
	UINT count			/* Number of sectors to write */
)
{
	switch (pdrv)
	{
	case EPCS:
		return EPCS_disk_write(buff, sector, count);
	}

	return RES_PARERR;
}
#endif


/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/

#if _USE_IOCTL
static DRESULT EPCS_disk_ioctl(BYTE cmd, void *buff)
{
	switch (cmd)
	{
	case CTRL_SYNC:
		return RES_OK;

	case GET_SECTOR_COUNT:
		*(DWORD *)buff = epcs_end_sector - epcs_start_sector;
		return RES_OK;

	case GET_SECTOR_SIZE:
		*(WORD *)buff = SECT_SIZE;
		return RES_OK;

	case GET_BLOCK_SIZE:
		*(DWORD *)buff = SECT_SIZE;
		return RES_OK;

	case CTRL_TRIM:
		return RES_OK;

	default:
		break;
	}

	return RES_PARERR;
}

DRESULT disk_ioctl (
	BYTE pdrv,		/* Physical drive number (0..) */
	BYTE cmd,		/* Control code */
	void *buff		/* Buffer to send/receive control data */
)
{
	switch (pdrv)
	{
	case EPCS:
		return EPCS_disk_ioctl(cmd, buff);
	}

	return RES_PARERR;
}
#endif
