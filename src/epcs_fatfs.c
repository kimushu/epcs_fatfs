#include <malloc.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include "ff.h"
#include "epcs_fatfs.h"

#define SECT_SIZE	(EPCS_FATFS_FLASH_SECTOR)

static epcs_fatfs_dev *target_dev;

int epcs_fatfs_init(epcs_fatfs_dev *dev)
{
	dev->fatfs = (FATFS *)malloc(sizeof(FATFS));
	if (!dev->fatfs)
	{
		return -ENOMEM;
	}

	dev->prefix_len = strlen(dev->fs_dev.name) + 1;

	f_mount(
		dev->fatfs,
		"",	/* Default drive */
		0	/* Do not mount now */
	);

	alt_fs_reg(&dev->fs_dev);
	target_dev = dev;
	return 0;
}

int epcs_fatfs_format(void)
{
	FRESULT result;

	if (!target_dev)
	{
		return -ENODEV;
	}

	result = f_mkfs("", 1, SECT_SIZE);

	switch (result)
	{
	case FR_DISK_ERR:
		return -EIO;
	case FR_NOT_READY:
		return -EBUSY;
	case FR_MKFS_ABORTED:
		return -ENOSPC;
	case FR_INVALID_PARAMETER:
		return -EINVAL;
	default:
		break;
	}

	return 0;
}

int epcs_fatfs_open(alt_fd *fd, const char *name, int flags, int mode)
{
	FRESULT f_result;
	BYTE f_flags;
	FIL *fp;
	(void)mode;

	if (!target_dev)
	{
		return -ENODEV;
	}

	fp = (FIL *)malloc(sizeof(FIL));
	if (!fp)
	{
		return -ENOMEM;
	}

	f_flags = 0;
	if ((flags + 1) & FREAD) f_flags |= FA_READ;
	if ((flags + 1) & FWRITE) f_flags |= FA_WRITE;
	if (flags & O_CREAT)
	{
		if (flags & O_APPEND)
		{
			f_flags |= FA_OPEN_ALWAYS;
		}
		else if (flags & O_EXCL)
		{
			f_flags |= FA_CREATE_NEW;
		}
		else if (flags & O_TRUNC)
		{
			f_flags |= FA_CREATE_ALWAYS;
		}
		else
		{
			f_flags |= FA_OPEN_ALWAYS;
		}
	}
	else
	{
		f_flags |= FA_OPEN_EXISTING;
	}

	f_result = f_open(
		fp,
		name + target_dev->prefix_len,
		f_flags
	);

	if (f_result == FR_OK)
	{
		if ((flags & O_CREAT) && (flags & O_APPEND))
		{
			f_result = f_lseek(fp, f_size(fp));

			if (f_result != FR_OK)
			{
				goto on_error;
			}
		}
		fd->priv = (alt_u8 *)fp;
		return 0;
	}

on_error:
	free(fp);

	switch (f_result)
	{
	case FR_NO_FILE:
	case FR_NO_PATH:
		return -ENOENT;
	case FR_EXIST:
		return -EEXIST;
	case FR_DENIED:
		return -EPERM;
	case FR_INT_ERR:
		return -EFAULT;
	default:
		break;
	}

	return -EIO;
}

int epcs_fatfs_close(alt_fd *fd)
{
	FRESULT f_result;
	FIL *fp;

	if (!target_dev)
	{
		return -ENODEV;
	}

	fp = (FIL *)fd->priv;
	if (!fp)
	{
		return -EBADF;
	}

	f_result = f_close(fp);

	if (f_result == FR_OK)
	{
		free(fp);
		fd->priv = NULL;
		return 0;
	}

	// switch (f_result)
	// {
	// }

	return -EIO;
}

int epcs_fatfs_read(alt_fd *fd, char *ptr, int len)
{
	FRESULT f_result;
	FIL *fp;
	UINT read;

	if (!target_dev)
	{
		return -ENODEV;
	}

	fp = (FIL *)fd->priv;
	if (!fp)
	{
		return -EBADF;
	}

	f_result = f_read(fp, ptr, len, &read);

	if (f_result == FR_OK)
	{
		return read;
	}

	// switch (f_result)
	// {
	// }

	return -EIO;
}

int epcs_fatfs_write(alt_fd *fd, const char *ptr, int len)
{
	FRESULT f_result;
	FIL *fp;
	UINT written;

	if (!target_dev)
	{
		return -ENODEV;
	}

	fp = (FIL *)fd->priv;
	if (!fp)
	{
		return -EBADF;
	}

	f_result = f_write(fp, ptr, len, &written);

	if (f_result == FR_OK)
	{
		return written;
	}

	// switch (f_result)
	// {
	// }

	return -EIO;
}

int epcs_fatfs_lseek(alt_fd *fd, int ptr, int dir)
{
	FRESULT f_result;
	FIL *fp;
	LONG ofs;

	if (!target_dev)
	{
		return -ENODEV;
	}

	fp = (FIL *)fd->priv;
	if (!fp)
	{
		return -EBADF;
	}

	switch (dir)
	{
	case SEEK_CUR:
		ofs = f_tell(fp) + ptr;
		break;
	case SEEK_SET:
		ofs = ptr;
		break;
	case SEEK_END:
		ofs = f_size(fp) + ptr;
		break;
	default:
		return -EINVAL;
	}

	if ((ofs < 0) || (ofs > f_size(fp)))
	{
		return -EINVAL;
	}

	f_result = f_lseek(fp, ofs);

	if (f_result == FR_OK)
	{
		return f_tell(fp);
	}

	// switch (f_result)
	// {
	// }

	return -EIO;
}

int epcs_fatfs_fstat(alt_fd *fd, struct stat *buf)
{
	FIL *fp;

	if (!target_dev)
	{
		return -ENODEV;
	}

	fp = (FIL *)fd->priv;
	if (!fp)
	{
		return -EBADF;
	}

	memset(buf, 0, sizeof(*buf));
	buf->st_mode = S_IFREG;
	buf->st_size = f_size(fp);
	buf->st_blksize = SECT_SIZE;
	buf->st_blocks = (buf->st_size + SECT_SIZE + 1) / SECT_SIZE;

	return 0;
}

