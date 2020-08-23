/*
 * Definitions to handle different types of filesystem types/APIs
 */


typedef enum {
	      FS_PS3 = 0,
	      FS_NTFS
} fs_type;

fs_type get_fs_type(char *path);
