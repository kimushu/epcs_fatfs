#ifndef __EPCS_FATFS_H__
#define __EPCS_FATFS_H__

#include <stddef.h>
#include "sys/alt_dev.h"
#include "sys/alt_llist.h"

typedef struct epcs_fatfs epcs_fatfs_dev;

extern int epcs_fatfs_init(epcs_fatfs_dev *dev);
extern int epcs_fatfs_format(void);

extern int epcs_fatfs_open(alt_fd *fd, const char *name, int flags, int mode);
extern int epcs_fatfs_close(alt_fd *fd);
extern int epcs_fatfs_read(alt_fd *fd, char *ptr, int len);
extern int epcs_fatfs_write(alt_fd *fd, const char *ptr, int len);
extern int epcs_fatfs_lseek(alt_fd *fd, int ptr, int dir);
extern int epcs_fatfs_fstat(alt_fd *fd, struct stat *buf);

struct epcs_fatfs
{
  alt_dev fs_dev;
  size_t prefix_len;
  void *fatfs;
};

#define EPCS_FATFS_INSTANCE(name, dev)  \
  static epcs_fatfs_dev dev =           \
  {                                     \
    {                                   \
      ALT_LLIST_ENTRY,                  \
      name##_MOUNT_POINT,               \
      epcs_fatfs_open,                  \
      epcs_fatfs_close,                 \
      epcs_fatfs_read,                  \
      epcs_fatfs_write,                 \
      epcs_fatfs_lseek,                 \
      epcs_fatfs_fstat,                 \
      NULL, /* ioctl */                 \
    },                                  \
  }

#define EPCS_FATFS_INIT(name, dev) \
  epcs_fatfs_init (&dev)

#endif /* __EPCS_FATFS_H__ */
