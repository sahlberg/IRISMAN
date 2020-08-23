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
        fs_type type;
        union {
                int dfd;        /* PS3 filesystem */
                DIR_ITER *pdir; /* NTFS */
	};
} vfs_dir;

vfs_dir *fs_opendir(const char *path);
void fs_closedir(vfs_dir *d);
int fs_mkdir(const char *path, int mode);

#endif /* _FS_TYPES_H_ */
