/*
 * Definitions to handle different types of filesystem types/APIs
 */
#ifndef _FS_TYPES_H_
#define _FS_TYPES_H_

#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sysutil/osk.h>
#include <sys/file.h>
#include <lv2/sysfs.h>
#include "main.h"
#include "ntfs.h"

typedef enum {
              FS_PS3 = 0,
              FS_NTFS
} fs_type;

fs_type get_fs_type(const char *path);

typedef struct {
        u8 d_type;
        u8 d_namlen;
        char d_name[MAXPATHLEN + 1];
} vfs_dirent;

typedef struct {
        fs_type type;
        union {
                int dfd;        /* PS3 filesystem */
                DIR_ITER *pdir; /* NTFS */
        };
        vfs_dirent dirent;
} vfs_dir;

typedef struct {
        fs_type type;
        union {
            int fd;        /* PS3/NTFS */
        };
} vfs_fh;

/*
 * Accepts flags:
 * O_RDONLY, O_WRONLY, O_RDWR, O_CREAT, O_TRUNC
 */
vfs_fh *vfs_open(const char *path, int flags, int mode);
void vfs_close(vfs_fh *fh);
ssize_t vfs_write(vfs_fh *fh, const void *buf, size_t count);
ssize_t vfs_read(vfs_fh *fh, void *buf, size_t count);
_off64_t vfs_lseek64(vfs_fh *fh, _off64_t offset, int whence);

vfs_dir *vfs_opendir(const char *path);
void vfs_closedir(vfs_dir *d);
int vfs_mkdir(const char *path, int mode);
int vfs_chmod(const char *path, int mode);
int vfs_readdir(vfs_dir *d, vfs_dirent *de);
int vfs_stat(const char *path, struct stat *st);
int vfs_unlink(const char *path);
int vfs_rmdir(const char *path);

#endif /* _FS_TYPES_H_ */
