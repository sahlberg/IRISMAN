
#include <sys/dirent.h>
#include "utils.h"
#include "vfs_types.h"

fs_type get_fs_type(const char *path)
{
        if (!strncmp(path, "/ntfs", 5))
                return FS_NTFS;
        if (!strncmp(path, "/ext", 4))
                return FS_NTFS;

        return FS_PS3;
}
 
vfs_dir *vfs_opendir(const char *path)
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

void vfs_closedir(vfs_dir *d)
{
        if (d == NULL)
            return;

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

int vfs_mkdir(const char *path, int mode)
{
        DIR *dir;

        switch (get_fs_type(path)) {
        case FS_NTFS:
                return ps3ntfs_mkdir(path, 0766);
        case FS_PS3:
                if ((dir = opendir(path))) {
                        closedir(dir);
                        return FAILED;
                }
                return mkdir(path, mode);
        }
        return -1;
}

int vfs_chmod(const char *path, int mode)
{
        switch (get_fs_type(path)) {
        case FS_NTFS:
                /* We never do chmod on ntfs from IRISMAN */
                return SUCCESS;
        case FS_PS3:
                return sysFsChmod(path, mode);
        }
        return -1;
}

#define IS_DIRECTORY 1
int vfs_readdir(vfs_dir *d, vfs_dirent *de)
{
        u64 read;
        sysFSDirent dir;
        struct stat st;

        switch (d->type) {
        case FS_NTFS:
                if(ps3ntfs_dirnext(d->pdir, de->d_name, &st))
                        return 0;
                de->d_namlen = strlen(de->d_name);
                de->d_type = 0;
                if ((st.st_mode & FS_S_IFMT) == FS_S_IFDIR)
                        de->d_type |= IS_DIRECTORY;
                return 1;
        case FS_PS3:
                read = sizeof(sysFSDirent);
                if (sysLv2FsReadDir(d->dfd, &dir, &read))
                        return 0;
                if (!read)
                        return 0;
                de->d_type = dir.d_type;
                de->d_namlen = dir.d_namlen;
                strcpy(de->d_name, dir.d_name);
                return 1;
        }
        return 0;
}
  
int vfs_stat(const char *path, struct stat *st)
{
    switch (get_fs_type(path)) {
        case FS_NTFS:
            if (ps3ntfs_stat(path, st))
                return -1;
            return 0;
        case FS_PS3:
          if (stat(path, st))
              return -1;
          return 0;
    }
    return -1;
}

int vfs_unlink(const char *path)
{
    struct stat st;

    switch (get_fs_type(path)) {
    case FS_NTFS:
        return ps3ntfs_unlink(path);
    case FS_PS3:
        if (vfs_stat(path, &st)) {
            vfs_chmod(path, FS_S_IFMT | 0777);
            return sysLv2FsUnlink(path);
        }
        return SUCCESS;
    }
    return -1;
}
  
int vfs_rmdir(const char *path)
{
    vfs_dir *vdir;

    switch (get_fs_type(path)) {
    case FS_NTFS:
        return ps3ntfs_unlink(path);
    case FS_PS3:
        vdir = vfs_opendir(path);
        if(vdir) {
            vfs_closedir(vdir);

            vfs_chmod(path, FS_S_IFDIR | 0777);
            return sysLv2FsRmdir(path);
        }
    }
    return -1;
}

/*
 * SYS_O_RDONLY == O_RDONLY == 0x0000
 * SYS_O_WRONLY == O_WRONLY == 0x0001
 * SYS_O_RDWR   == O_RDWR   == 0x0002
 * SYS_O_TRUNC              == 0x0200
 * O_TRUNC                  == 0x0400
 * SYS_O_CREAT              == 0x0040
 * O_CREAT                  == 0x0200
 */
vfs_fh *vfs_open(const char *path, int flags, int mode)
{
        vfs_fh *fh;
        int f;

        fh = malloc(sizeof(vfs_fh));
        if (!fh)
                return NULL;

        fh->type = get_fs_type(path);
        switch (fh->type) {
        case FS_NTFS:
            fh->fd = ps3ntfs_open(path, O_RDONLY, mode);
            if (fh->fd < 0) {
                free(fh);
                return NULL;
            }
            return fh;
        case FS_PS3:
            f = flags & O_RDWR;
            if (flags & O_TRUNC)
                f |= SYS_O_TRUNC;
            if (flags & O_CREAT)
                f |= SYS_O_CREAT;
            if (sysLv2FsOpen(path, f, &fh->fd, mode, NULL, 0) < 0) {
                free(fh);
                return NULL;
            }
            return fh;
        }
        return NULL;
}

void vfs_close(vfs_fh *fh)
{
        if (fh == NULL)
            return;

        switch (fh->type) {
        case FS_PS3:
                sysLv2FsClose(fh->fd);
                break;
        case FS_NTFS:
                ps3ntfs_close(fh->fd);
                break;
        }
        free(fh);
}
            
ssize_t vfs_write(vfs_fh *fh, const void *buf, size_t count)
{
        u64 readed = count, writed = 0;

        switch (fh->type) {
        case FS_PS3:
            return ps3ntfs_write(fh->fd, buf, count);
        case FS_NTFS:
            if (sysLv2FsWrite(fh->fd, buf, readed, &writed) < 0)
                return -1;
            return writed;
        }
        return -1;
}

ssize_t vfs_read(vfs_fh *fh, void *buf, size_t count)
{
        u64 readed = count, writed = 0;

        switch (fh->type) {
        case FS_PS3:
            return ps3ntfs_read(fh->fd, buf, count);
        case FS_NTFS:
            if (sysLv2FsRead(fh->fd, buf, readed, &writed) < 0)
                return -1;
            return writed;
        }
        return -1;
}

_off64_t vfs_lseek64(vfs_fh *fh, _off64_t offset, int whence)
{
        u64 temp;

        /* Only support SEEK_SET for now */
        if (whence != SEEK_SET)
            return -1LL;

        switch (fh->type) {
        case FS_PS3:
            return ps3ntfs_seek64(fh->fd, offset, whence);
        case FS_NTFS:
          if (sysLv2FsLSeek64(fh->fd, offset, whence, &temp) < 0)
                return -1LL;
            return temp;
        }
        return -1LL;
}
