
#include <sys/dirent.h>
#include "fs_types.h"

vfs_dir *fs_opendir(const char *path)
{
        vfs_dir *dir;

        dir = malloc(sizeof(vfs_dir));
	if (!dir)
	        return NULL;

	dir->type = get_fs_type(path);
	switch (dir->type) {
	case FS_PS3:
	        if (sysLv2FsOpenDir(path, &dir->dfd)) {
		        free(dir);
			return NULL;
		}
		break;
	case FS_NTFS:
	        dir->pdir = ps3ntfs_diropen(path);
		if (dir->pdir == NULL) {
		        free(dir);
			return NULL;
		}
		break;
	}

	return dir;
}

void fs_closedir(vfs_dir *d)
{
	switch (d->type) {
	case FS_PS3:
	        sysLv2FsCloseDir(d->dfd);
		break;
	case FS_NTFS:
	        ps3ntfs_dirclose(d->pdir);
		break;
	}
	free(d);
}

int fs_mkdir(const char *path, int mode)
{
        DIR *dir;

        switch (get_fs_type(path)) {
        case FS_NTFS:
                return ps3ntfs_mkdir(path, 0766);
        case FS_PS3:
                if (dir = opendir(path)) {
                        closedir(dir);
                        return FAILED;
                }
                return mkdir(path, mode);
        }
}

int fs_chmod(const char *path, int mode)
{
        switch (get_fs_type(path)) {
        case FS_NTFS:
                /* We never do chmod on ntfs from IRISMAN */
                return SUCCESS;
        case FS_PS3:
                return sysFsChmod(path, mode);
        }
}
