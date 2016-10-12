/*-----------------------------------------------------------------------*/
/* Low level disk I/O module for EPCS                                    */
/*-----------------------------------------------------------------------*/

#include "diskio.h"		/* FatFs lower layer API */
#include "system.h"
#include "io.h"
#include <sched.h>

/* Definitions of physical drive number for each drive */
#define EPCS	0

#ifdef EPCS_FATFS_EPCS_USE_SWI
# ifdef EPCS_FATFS_EPCS_USE_SPI
#  error "epcs.use_swi and epcs.use_spi cannot be used at the same time"
# endif
# define USE_SWI

# define REG_WRITE(d)		IOWR((EPCS_FATFS_EPCS_BASE), 5, (d))
# define REG_READ()			IORD((EPCS_FATFS_EPCS_BASE), 5)

# define BIT_IRQENA			(1 << 15)
# define BIT_RDY			(1 << 9)
# define BIT_STA			(1 << 9)
# define BIT_SS				(1 << 8)
#else
# ifdef EPCS_FATFS_EPCS_USE_SPI
#  define USE_SPI
#  define SPI_OFFSET 0
# else
#  define SPI_OFFSET (0x400/4)
# endif

# define REG_TXD_WRITE(d)	IOWR((EPCS_FATFS_EPCS_BASE), SPI_OFFSET+0, (d))
# define REG_RXD_READ()		IORD((EPCS_FATFS_EPCS_BASE), SPI_OFFSET+1)
# define REG_STA_WRITE(d)	IOWR((EPCS_FATFS_EPCS_BASE), SPI_OFFSET+2, (d))
# define REG_STA_READ()		IORD((EPCS_FATFS_EPCS_BASE), SPI_OFFSET+2)
# define REG_CTL_WRITE(d)	IOWR((EPCS_FATFS_EPCS_BASE), SPI_OFFSET+3, (d))
# define REG_CTL_READ()		IORD((EPCS_FATFS_EPCS_BASE), SPI_OFFSET+3)

# define BIT_RRDY			(1 << 7)
# define BIT_TRDY			(1 << 6)

# ifdef USE_SPI
#  define REG_SS_WRITE(d)	IOWR((EPCS_FATFS_EPCS_BASE), SPI_OFFSET+5, (d))
#  define REG_SS_READ()		IORD((EPCS_FATFS_EPCS_BASE), SPI_OFFSET+5)
#  define BIT_SS			(1 << (EPCS_FATFS_EPCS_SPI_SLAVE))
# endif
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
/* Start transaction                                                     */
/*-----------------------------------------------------------------------*/
static void EPCS_start(void)
{
#ifdef USE_SWI
	// Wait for device ready
	while (!(REG_READ() & BIT_RDY)) sched_yield();
#endif

	// Slave select
#ifdef USE_SPI
	REG_SS_WRITE(BIT_SS);
#endif
}



/*-----------------------------------------------------------------------*/
/* Data transfer                                                         */
/*-----------------------------------------------------------------------*/
static BYTE EPCS_transfer(
	BYTE tx_data
)
{
#ifdef USE_SWI
	DWORD rx;
	REG_WRITE(BIT_STA | BIT_SS | tx_data);
	while (!((rx = REG_READ()) & BIT_RDY)) sched_yield();
	return (BYTE)rx;
#endif
}



/*-----------------------------------------------------------------------*/
/* End transaction                                                       */
/*-----------------------------------------------------------------------*/
static void EPCS_end(void)
{
	// End transaction
#ifdef USE_SWI
	REG_WRITE(0);
#endif
}



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
	BYTE jedec_cap;

	// Detect capacity by READ IDENTIFICATION command
	EPCS_start();
	EPCS_transfer(EPCS_FATFS_FLASH_CMD_RDID);
	EPCS_transfer(0xff);	/* mfg */
	EPCS_transfer(0xff);	/* type */
	jedec_cap = EPCS_transfer(0xff);
	EPCS_end();
	epcs_total_sectors = (1 << (jedec_cap - SECT_SHIFT));

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
	DWORD bytes;

	sector += epcs_start_sector;
	if ((sector + count) > epcs_end_sector)
	{
		return RES_PARERR;
	}
	addr = (sector << SECT_SHIFT);

	EPCS_start();
	EPCS_transfer(EPCS_FATFS_FLASH_CMD_FREAD);
	EPCS_transfer((BYTE)(addr >> 16));
	EPCS_transfer((BYTE)(addr >> 8));
	EPCS_transfer((BYTE)(addr >> 0));
	EPCS_transfer(0xff);
	for (bytes = (count << SECT_SHIFT); bytes > 0; --bytes)
	{
		*buff++ = EPCS_transfer(0xff);
	}
	EPCS_end();
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
	BYTE cmd;
	DWORD addr;
	INT page;
	DWORD bytes;
#ifdef EPCS_FATFS_FLASH_VERIFY
	const BYTE *v_buff = buff;
	DWORD v_addr;
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
		cmd = EPCS_FATFS_FLASH_CMD_ERASE;

		// Program pages
		for (page = -1; page < (SECT_SIZE / PAGE_SIZE); ++page)
		{
			// Write Enable
			EPCS_start();
			EPCS_transfer(EPCS_FATFS_FLASH_CMD_WREN);
			EPCS_end();

			EPCS_start();
			EPCS_transfer(cmd);
			EPCS_transfer((BYTE)(addr >> 16));
			EPCS_transfer((BYTE)(addr >> 8));
			EPCS_transfer((BYTE)(addr >> 0));

			if (page < 0)
			{
				// Erase sector
				EPCS_end();
				cmd = EPCS_FATFS_FLASH_CMD_PROG;
			}
			else
			{
				// Page program
				for (bytes = PAGE_SIZE; bytes > 0; --bytes)
				{
					EPCS_transfer(*buff++);
				}
				EPCS_end();
				addr += PAGE_SIZE;
			}

			// Wait until finish erase/program
			for (;;)
			{
				EPCS_start();
				EPCS_transfer(EPCS_FATFS_FLASH_CMD_RDSR);
				EPCS_transfer(0xff);
				cmd = EPCS_transfer(0xff);
				EPCS_end();
				if ((cmd & 1) == 0)
				{
					// WIP bit is off
					break;
				}
			}
		}

#ifdef EPCS_FATFS_FLASH_VERIFY
		// Verify
		EPCS_start();
		EPCS_transfer(EPCS_FATFS_FLASH_CMD_FREAD);
		EPCS_transfer((BYTE)(addr >> 16));
		EPCS_transfer((BYTE)(addr >> 8));
		EPCS_transfer((BYTE)(addr >> 0));
		EPCS_transfer(0xff);
		for (bytes = SECT_SIZE; bytes > 0; --bytes)
		{
			BYTE written = EPCS_transfer(0xff);
			if (written != *v_buff++)
			{
				// Mismatch
				EPCS_end();
				return RES_ERROR;
			}
		}
		EPCS_end();
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
