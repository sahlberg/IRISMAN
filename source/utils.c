/*
    (c) 2011 Hermes/Estwald <www.elotrolado.net>
    IrisManager (HMANAGER port) (c) 2011 D_Skywalk <http://david.dantoine.org>

    HMANAGER4 is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    HMANAGER4 is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with HMANAGER4.  If not, see <http://www.gnu.org/licenses/>.

*/

/* NOTE: Added patch_error_09() with some modifications

Credits:
- Rancid-o
- Zz_SACRO_zZ

*/

#include "utils.h"
#include "language.h"
#include <sys/file.h>
#include "ntfs.h"
#include "iso.h"
#include "osk_input.h"
#include "main.h"
#include "file_manager.h"
#include "ftp/functions.h"
#include "favourites.h"
#include "ps3mapi.h"
#include "dialog.h"
#include "vfs_types.h"

//---
#define USB_MASS_STORAGE_1(n)	(0x10300000000000AULL+(n)) /* For 0-5 */
#define USB_MASS_STORAGE_2(n)	(0x10300000000001FULL+((n)-6)) /* For 6-127 */
#define USB_MASS_STORAGE(n)	(((n) < 6) ? USB_MASS_STORAGE_1(n) : USB_MASS_STORAGE_2(n))

#define MAX_SECTIONS	((0x10000-sizeof(rawseciso_args))/8)

#define cue_buf  plugin_args

#define MAX_PATH_LEN 0x420

enum DiscEmu
{
	EMU_OFF = 0,
	EMU_PS3,
	EMU_PS2_DVD,
	EMU_PS2_CD,
	EMU_PSX,
	EMU_BD,
	EMU_DVD,
	EMU_MAX,
};

typedef struct
{
	uint64_t device;
	uint32_t emu_mode;
	uint32_t num_sections;
	uint32_t num_tracks;
} __attribute__((packed)) rawseciso_args;

typedef struct
{
	uint32_t lba;
	int is_audio;
} TrackDef;

typedef struct _ScsiTrackDescriptor
{
	uint8_t reserved;
	uint8_t adr_control;
	uint8_t track_number;
	uint8_t reserved2;
	uint32_t track_start_addr;
} __attribute__((packed)) ScsiTrackDescriptor;

int emu_mode;
u8 cue=0;
TrackDef tracks[100];
int i, parts;
unsigned int num_tracks;

ScsiTrackDescriptor *scsi_tracks;
uint32_t sections[MAX_SECTIONS], sections_size[MAX_SECTIONS];

rawseciso_args *p_args;

char ntfs_path[MAX_PATH_LEN];

int cobra_parse_cue(void *cue, uint32_t size, TrackDef *tracks, unsigned int max_tracks, unsigned int *num_tracks, char *filename, unsigned int fn_size);
//--


typedef s32 Lv2FsMode;
typedef s32 Lv2FsFile;

extern bool is_mamba_v2;

extern int filter_by_letter;

extern int show_custom_icons;
extern char * language[];
extern char self_path[MAXPATHLEN];
extern char video_path[MAXPATHLEN];
extern int game_list_category;
extern int mode_homebrew;

extern char ps2classic_path[MAXPATHLEN];

char extension[10];

extern char audio_extensions[300];
extern char video_extensions[300];
extern char browser_extensions[100];
extern char custom_homebrews[400];

extern int retro_mode;

extern int roms_count;
extern int max_roms;

// retroArch cores 1.0
extern char retro_root_path[ROMS_MAXPATHLEN];
extern char retro_snes_path[ROMS_MAXPATHLEN];
extern char retro_gba_path[ROMS_MAXPATHLEN];
extern char retro_gen_path[ROMS_MAXPATHLEN];
extern char retro_nes_path[ROMS_MAXPATHLEN];
extern char retro_mame_path[ROMS_MAXPATHLEN];
extern char retro_fba_path[ROMS_MAXPATHLEN];
extern char retro_doom_path[ROMS_MAXPATHLEN];
extern char retro_quake_path[ROMS_MAXPATHLEN];
extern char retro_pce_path[ROMS_MAXPATHLEN];
extern char retro_gb_path[ROMS_MAXPATHLEN];
extern char retro_gbc_path[ROMS_MAXPATHLEN];
extern char retro_atari_path[ROMS_MAXPATHLEN];
extern char retro_vb_path[ROMS_MAXPATHLEN];
extern char retro_nxe_path[ROMS_MAXPATHLEN];
extern char retro_wswan_path[ROMS_MAXPATHLEN];

// retroArch cores 1.2
extern char retro_a7800_path[ROMS_MAXPATHLEN];
extern char retro_lynx_path[ROMS_MAXPATHLEN];
extern char retro_gw_path[ROMS_MAXPATHLEN];
extern char retro_vectrex_path[ROMS_MAXPATHLEN];
extern char retro_2048_path[ROMS_MAXPATHLEN];

//void UTF8_to_Ansi(char *utf8, char *ansi, int len); // from osk_input
void UTF32_to_UTF8(u32 *stw, u8 *stb);

int copy_async(char *path1, char *path2, u64 size, char *progress_string1, char *progress_string2);     // pkg_install.c
int copy_async_gbl(char *path1, char *path2, u64 size, char *progress_string1, char *progress_string2); // updates.c (it can copy to ntfs devices)

char * LoadFile(char *path, int *file_size)
{
    *file_size = (int)get_filesize(path);

    if(!is_ntfs_path(path)) sysLv2FsChmod(path, FS_S_IFMT | 0777);

    if(*file_size==0) return NULL;

    char *mem = NULL;
    mem = malloc(*file_size); if(!mem) return NULL;

    int fd = ps3ntfs_open(path, O_RDONLY, 0777);

    if(fd >= 0)
    {
        ps3ntfs_read(fd, mem, *file_size);
        ps3ntfs_close(fd);
    }

    return mem;
}

int SaveFile(char *path, char *mem, int file_size)
{
    unlink_secure(path);

    int fd = ps3ntfs_open(path, O_WRONLY | O_CREAT | O_TRUNC, 0777);

    if(fd >= 0)
    {
        if(ps3ntfs_write(fd, (void *) mem, file_size)!=file_size)
        {
            ps3ntfs_close(fd);
            return FAILED;
        }
        ps3ntfs_close(fd);
    }

    if(!is_ntfs_path(path)) sysLv2FsChmod(path, FS_S_IFMT | 0777);

    return SUCCESS;
}

int ExtractFileFromISO(char *iso_file, char *file, char *outfile)
{
    if(iso_file[0]=='.' || file_exists(iso_file)==false) return FAILED;

    int fd = ps3ntfs_open(iso_file, O_RDONLY, 0);
    if(fd >= 0)
    {
        u32 flba;
        u64 size;
        char *mem = NULL;

        int re = get_iso_file_pos(fd, file, &flba, &size);

        if(!re && (mem = malloc(size)) != NULL)
        {
            re = ps3ntfs_read(fd, (void *) mem, size);
            ps3ntfs_close(fd);
            if(re == size)
              SaveFile(outfile, mem, size);

            free(mem);

            return (re == size) ? SUCCESS : FAILED;
        }
        else
            ps3ntfs_close(fd);
    }

    return FAILED;
}

u8 game_category[3] = "??";

bool is_ntfs_path(char *path)
{
    return get_fs_type(path) == FS_NTFS;
}


bool is_video(char *ext)
{
    sprintf(extension, "%s ", ext);

    return (strcasestr(video_extensions, extension) != NULL);
}

bool is_audio(char *ext)
{
    sprintf(extension, "%s ", ext);

    return (strcasestr(audio_extensions, extension) != NULL);
}

bool is_audiovideo(char *ext)
{
    return (is_video(ext) || is_audio(ext));
}

bool is_browser_file(char *ext)
{
    sprintf(extension, "%s ", ext);

    return (strcasestr(browser_extensions, extension) != NULL);
}

u64 get_filesize(char *path)
{
    bool is_ntfs = is_ntfs_path(path);

    if(is_ntfs)
    {
        struct stat st;
        if (ps3ntfs_stat(path, &st) < 0) return 0ULL;
        return st.st_size;
    }
    else
    {
        sysFSStat stat;
        if (sysLv2FsStat(path, &stat) < 0) return 0;
        return stat.st_size;
    }
}

bool isDir(char* path )
{
    if(is_ntfs_path(path))
    {
        struct stat st;
        return ps3ntfs_stat(path, &st) >= SUCCESS && (st.st_mode & FS_S_IFDIR);
    }

    sysFSStat stat;
    return sysLv2FsStat(path, &stat) == SUCCESS && (stat.st_mode & FS_S_IFDIR);;
}

bool file_exists( char* path )
{
    if(is_ntfs_path(path))
    {
        struct stat st;
        return ps3ntfs_stat(path, &st) >= SUCCESS;
    }

    sysFSStat stat;
    return sysLv2FsStat(path, &stat) == SUCCESS;
}

char * get_extension(char *path)
{
    int n = strlen(path);
    int m = n;

    while(m > 1 && path[m] != '.' && path[m] != '/') m--;

    if(!strcmp(&path[m], ".0"))
        while(m > 1 && path[m] != '.' && path[m] != '/') m--;

    if(path[m] == '.') return &path[m];

    return &path[n];
}

char *str_replace(char *orig, char *rep, char *with)
{
    char *result; // the return string
    char *ins;    // the next insert point
    char *tmp;    // varies
    int len_rep;  // length of rep
    int len_with; // length of with
    int len_front; // distance between rep and end of last rep
    int count;    // number of replacements

    if (!orig) return NULL;

    if (!rep)
    {
        result = malloc(strlen(orig) + 1);
        strcpy(result, orig);
        return result;
    }

    len_rep = strlen(rep);

    if (!with) with = "";

    len_with = strlen(with);

    ins = orig;
    for (count = 0; (tmp = strstr(ins, rep)); ++count)
    {
        ins = tmp + len_rep;
    }

    // first time through the loop, all the variable are set correctly
    // from here on,
    //    tmp points to the end of the result string
    //    ins points to the next occurrence of rep in orig
    //    orig points to the remainder of orig after "end of rep"
    tmp = result = malloc(strlen(orig) + (len_with - len_rep) * count + 1);

    if (!result) return NULL;

    while (count--)
    {
        ins = strstr(orig, rep);
        len_front = ins - orig;
        tmp = strncpy(tmp, orig, len_front) + len_front;
        tmp = strcpy(tmp, with) + len_with;
        orig += len_front + len_rep; // move to next "end of rep"
    }
    strcpy(tmp, orig);
    return result;
}

char * get_filename(char *path)
{
    int n = strlen(path);
    int m = n;

    while(m > 0 && path[m] != '/') m--;

    if(path[m] == '/') m++;

    return &path[m];
}

int strcmpext(char *path, char *ext)
{
    int path_len = strlen(path);
    int ext_len = strlen(ext);

    if(ext_len >= path_len) return FAILED;

    return strncasecmp(path + path_len - ext_len, ext, ext_len);
}

int fix_PS3_EXTRA_attribute(char *path)
{
    if(is_ntfs_path(path) || strlen(path) <= 4) return FAILED;

    char filepath[MAXPATHLEN];

    sprintf(filepath, "%s/PS3_EXTRA", path);
    if(!file_exists(filepath))  return SUCCESS;

    Lv2FsFile fd;
    u64 bytes;
    u64 position = 0LL;

    unsigned pos, str, len = 0;
    unsigned char *mem = NULL;

    sprintf(filepath, "%s/PS3_GAME/PARAM.SFO", path);
    if(!is_ntfs_path(filepath)) sysLv2FsChmod(filepath, FS_S_IFMT | 0777);

    if(!sysLv2FsOpen(filepath, 0, &fd, S_IREAD | S_IRGRP | S_IROTH, NULL, 0))
    {
        sysLv2FsLSeek64(fd, 0, 2, &position);
        len = (u32) position;

        if(len > 0x4000) {sysLv2FsClose(fd); return -2;}

        mem = (unsigned char *) malloc(len + 16);
        if(!mem) {sysLv2FsClose(fd); return -2;}

        memset(mem, 0, len + 16);

        sysLv2FsLSeek64(fd, 0, 0, &position);

        if(sysLv2FsRead(fd, mem, len, &bytes) != 0) bytes = 0LL;

        len = (u32) bytes;

        sysLv2FsClose(fd);

        str = (mem[8] + (mem[9]<<8));
        pos = (mem[0xc] + (mem[0xd]<<8));

        int indx = 0;

        while(str < len)
        {
            if(mem[str] == 0) break;

            if(!strncmp((char *) &mem[str], "ATTRIBUTE", 10))
            {
                if(!(mem[pos + 2] & 0x2))
                {
                    mem[pos + 2] |= 0x2; //Turn on PS3_EXTRA Flag

                    SaveFile(filepath, (char *) mem, len);
                }

                free(mem);
                return SUCCESS;
            }
            while(mem[str] && str < len) str++; str++;
            pos  += (mem[0x1c + indx] + (mem[0x1d + indx]<<8));
            indx += 16;
        }

        if(mem) free(mem);
    }
    return FAILED;
}

int parse_ps3_disc(char *path, char * id)
{
    if(is_ntfs_path(path) || strlen(path) <= 4) return FAILED;
    if(strncmp(path + strlen(path) - 4, ".SFB", 4)) return FAILED;
    if(!file_exists(path)) return FAILED;

    int n;

    Lv2FsFile fd;
    u64 bytes;
    u64 position = 0LL;

    strncpy(id, "UNKNOWN", 63);

    if(!sysLv2FsOpen(path, 0, &fd, S_IREAD | S_IRGRP | S_IROTH, NULL, 0))
    {
        unsigned len;
        unsigned char *mem = NULL;

        sysLv2FsLSeek64(fd, 0, 2, &position);
        len = (u32) position;

        if(len > 0x4000) {sysLv2FsClose(fd); return -2;}

        mem = (unsigned char *) malloc(len + 16);
        if(!mem) {sysLv2FsClose(fd); return -2;}

        memset(mem, 0, len + 16);

        sysLv2FsLSeek64(fd, 0, 0, &position);

        if(sysLv2FsRead(fd, mem, len, &bytes) != 0) bytes = 0LL;

        len = (u32) bytes;

        sysLv2FsClose(fd);

        for(n = 0x20; n < 0x200; n += 0x20)
        {
            if(!strcmp((char *) &mem[n], "TITLE_ID"))
            {
                n = (mem[n + 0x12]<<8) | mem[n + 0x13];
                memcpy(id, &mem[n], 16);

                return SUCCESS;
            }
        }
    }

    return FAILED;
}

extern int firmware;


int patch_exe_error_09(char *path_exe)
{
    if(is_ntfs_path(path_exe)) return 0;

    sysLv2FsChmod(path_exe, FS_S_IFMT | 0777);

    if(firmware >= 0x486C) return SUCCESS;

    u16 fw_421 = 42100;
    u16 fw_486 = 48600;
    int offset_fw;
    s32 ret;
    u64 bytesread = 0;
    u64 written = 0;
    u64 pos = 0;
    u16 ver = 0;
    int file = -1;
    int flag = 0; //not patched

    // open self/sprx and changes the fw version
    ret = sysLv2FsOpen( path_exe, SYS_O_RDWR, &file, 0, NULL, 0 );
    if(ret == SUCCESS)
    {
        // set to offset position
        ret = sysLv2FsLSeek64( file, 0xC, 0, &pos );
        if(ret == SUCCESS && pos == 0xCULL)
        {
            // read offset in file
            ret = sysLv2FsRead( file, &offset_fw, 0x4, &bytesread );

            if(ret == SUCCESS && bytesread == 4ULL && offset_fw > 0xF)
            {
                u8 retried; retried = 0; offset_fw -= 0x78;

                retry_offset_exe:

                if(offset_fw < 0x90 || offset_fw > 0x800) offset_fw = strstr(path_exe, ".sprx") ? 0x258 : 0x428;
                offset_fw += 6;

                ret = sysLv2FsLSeek64( file, (u64) offset_fw, 0, &pos );

                if(ret == SUCCESS && pos == (u64) offset_fw)
                {
                    ret = sysLv2FsRead( file, &ver, 0x2, &bytesread ); //self/sprx min fw version

                    if(retried == 0 && (ver % 100) > 0) {offset_fw = (offset_fw==0x258) ? 0x278 : 0; retried = 1; goto retry_offset_exe;}

                    if(ret == SUCCESS && bytesread == 0x2ULL && (ver >= 34000 && ver <= fw_486))
                    {
                        ret = sysLv2FsLSeek64( file, (u64) offset_fw, 0, &pos );
                        u16 cur_firm = ((firmware>>12) & 0xF) * 10000 + ((firmware>>8) & 0xF) * 1000 + ((firmware>>4) & 0xF) * 100;

                        if(ret == SUCCESS && ver > cur_firm)
                        {
                            if(ver > fw_421 && (firmware >= 0x421C && firmware < 0x485C))
                            {
                                sysLv2FsWrite( file, &cur_firm, 0x2, &written );
                                flag = 1; //patch applied
                            }
                            else
                                flag = -1; //requires a higher firmware
                        }
                    }
                }
            }
        }

        sysLv2FsClose( file );
   }

   return flag;
}


// exported from PS3 Ita Manager method with some modifications (support caps types, error control
// automatic file offset and "intelligent" patch method

static int self_alarm_version = 0;

#include "param_sfo.h"

void patch_error_09( const char *path, int quick_ver_check )
{

    if(quick_ver_check)
    {
        char ps3_sys_ver[8] = "00.0000";
        char sfo[MAX_PATH_LEN];
        sprintf(sfo, "%s/PS3_GAME/PARAM.SFO", path);
        get_field_param_sfo(sfo, "PS3_SYSTEM_VER", ps3_sys_ver, 7);

        int cur_firm = ((firmware>>12) & 0xF) * 10000 + ( (firmware>>8) & 0xF) * 1000 + ( (firmware>>4) & 0xF) * 100;
        int gam_firm = (ps3_sys_ver[1] - '0') * 10000 + (ps3_sys_ver[3] - '0') * 1000 + (ps3_sys_ver[4] - '0') * 100;
        if(cur_firm >= gam_firm) return;
    }


    int d = -1;
    s32 ret = 1;

    /* Open the directory specified by "path". */
    ret = sysLv2FsOpenDir( path, &d );

    /* Check it was opened. */
    if( d == -1 ) return;

    int ext;
    char f[MAX_PATH_LEN];

    while(true)
    {
        sysFSDirent entry;
        u64 read = 0;

        /* "Readdir" gets subsequent entries from "d". */
        ret = sysLv2FsReadDir( d, &entry, &read );
        if ( read == 0 || ret != SUCCESS )
        {
            /* There are no more entries in this directory, so break
               out of the while loop. */
            break;
        }

        //DIRECTORY
        if(entry.d_type & DT_DIR)
        {
            if(strncmp( entry.d_name, ".", 1) != SUCCESS && strncmp( entry.d_name, "..", 2) != SUCCESS &&
               strstr("GAMES|GAMEZ", entry.d_name) == NULL)
            {
                sprintf( f, "%s/%s", path, entry.d_name);

             /* Recursively call "list_dir" with the new path. */
                patch_error_09(f, 0);
            }
        }
        else
        {
            ext = entry.d_namlen - 4;

            if(ext > 1 )
            {
                // SELF/SPRX/EBOOT.BIN
                if( strcasestr("sprx|self", entry.d_name + ext) != NULL ||
                   (strcmp(entry.d_name, "EBOOT.BIN" ) == SUCCESS))
                {
                    sprintf( f, "%s/%s", path, entry.d_name);

                    int r = patch_exe_error_09(f);
                    if(r == -1)
                    {
                        self_alarm_version = 1;
                        DPrintf(">> %s requires a higher CFW!!!\n", entry.d_name);
                    }
                    else if(r == 1)
                        DPrintf(">> %s [fixed]\n", entry.d_name);
                    else
                        DPrintf(">> %s\n", entry.d_name);
                }
            }
        }
    }

    /* After going through all the entries, close the directory. */
    sysLv2FsCloseDir( d );
}

int sys_shutdown()
{
    unlink_secure("/dev_hdd0/tmp/turnoff");

    lv2syscall4(SC_SYS_POWER,SYS_SHUTDOWN,0,0,0);
    return_to_user_prog(int);
}

int sys_reboot()
{
    unlink_secure("/dev_hdd0/tmp/turnoff");

    //lv2syscall4(SC_SYS_POWER,SYS_HARD_REBOOT,0,0,0);
    lv2syscall3(SC_SYS_POWER,SYS_REBOOT,0,0);
    return_to_user_prog(int);
}

int sys_soft_reboot()
{
    unlink_secure("/dev_hdd0/tmp/turnoff");

    lv2syscall4(SC_SYS_POWER,SYS_SOFT_REBOOT,0,0,0);
    return_to_user_prog(int);
}

int unlink_secure(void *path)
{
    bool is_ntfs = is_ntfs_path(path);

    if(is_ntfs)
    {
        return ps3ntfs_unlink(path);
    }

    sysFSStat s;

    if(sysLv2FsStat(path, &s) >= 0)
    {
        sysLv2FsChmod(path, FS_S_IFMT | 0777);
        return sysLv2FsUnlink(path);
    }

    return FAILED;
}

int rename_secure(void *path1, void *path2)
{
    bool is_ntfs = is_ntfs_path(path1);

    if(is_ntfs)
    {
        return ps3ntfs_rename(path1, path2);
    }

    sysFSStat s;

    if(sysLv2FsStat(path1, &s) >= 0)
    {
        return sysLv2FsRename(path1, path2);
    }

    return FAILED;
}

int mkdir_secure(void *path)
{
    int ret = FAILED;

    bool is_ntfs = is_ntfs_path(path);

    if(is_ntfs)
    {
        ret = ps3ntfs_mkdir(path, 0777);
    }
    else
    {
        DIR  *dir = opendir(path);
        if(!dir)
        {
            ret = mkdir(path, S_IRWXO | S_IRWXU | S_IRWXG | S_IFDIR);
        }
        else
            closedir(dir);
    }

    return ret;
}

int rmdir_secure(void *path)
{
    int ret = FAILED;

    bool is_ntfs = is_ntfs_path(path);

    if(is_ntfs)
    {
        return ps3ntfs_unlink((char*)path);
    }

    DIR  *dir = opendir(path);
    if(dir)
    {
        closedir(dir);

        sysFsChmod(path, FS_S_IFDIR | 0777);
        ret = sysLv2FsRmdir(path);
    }

    return ret;
}

void utf8_truncate(char *utf8, char *utf8_trunc, int len)
{
u8 *ch= (u8 *) utf8;

    *utf8_trunc = 0;

    while(*ch != 0 && len > 0)
    {
        // 3, 4 bytes utf-8 code
        if(((*ch & 0xF1) == 0xF0 || (*ch & 0xF0) == 0xe0) && (*(ch+1) & 0xc0) == 0x80)
        {
            //*utf8_trunc++=' '; // ignore
            memcpy(utf8_trunc, &ch, 3+1*((*ch & 0xF1) == 0xF0));
            utf8_trunc+= 3+1*((*ch & 0xF1) == 0xF0);
            len--;
            ch += 2 + 1 * ((*ch & 0xF1) == 0xF0);
        }
        else if((*ch & 0xE0) == 0xc0 && (*(ch+1) & 0xc0) == 0x80) // 2 bytes utf-8 code
        {
            memcpy(utf8_trunc, &ch, 2);
            utf8_trunc+=2;
            len--;
            ch++;
        }
        else
        {
            if(*ch<32) *ch=32;
            *utf8_trunc++=*ch;

            len--;
        }

        ch++;
    }

    while(len > 0)
    {
        *utf8_trunc++=0;
        len--;
    }
}
void utf8_to_ansi(char *utf8, char *ansi, int len)
{
    u8 *ch= (u8 *) utf8;
    u8 c;

    *ansi = 0;

    while(*ch != 0 && len > 0)
    {
        // 3, 4 bytes utf-8 code
        if(((*ch & 0xF1) == 0xF0 || (*ch & 0xF0) == 0xe0) && (*(ch+1) & 0xc0) == 0x80)
        {
            *ansi++=' '; // ignore
            len--;
            ch+=2+1*((*ch & 0xF1) == 0xF0);
        }
        else
        // 2 bytes utf-8 code
        if((*ch & 0xE0) == 0xc0 && (*(ch+1) & 0xc0) == 0x80)
        {
            c = (((*ch & 3)<<6) | (*(ch+1) & 63));

            if(c >= 0xC0 && c <= 0xC5) c='A';
            else if(c == 0xc7) c='C';
            else if(c >= 0xc8 && c <= 0xcb) c='E';
            else if(c >= 0xcc && c <= 0xcf) c='I';
            else if(c == 0xd1) c='N';
            else if(c >= 0xd2 && c <= 0xd6) c='O';
            else if(c >= 0xd9 && c <= 0xdc) c='U';
            else if(c == 0xdd) c='Y';
            else if(c >= 0xe0 && c <= 0xe5) c='a';
            else if(c == 0xe7) c='c';
            else if(c >= 0xe8 && c <= 0xeb) c='e';
            else if(c >= 0xec && c <= 0xef) c='i';
            else if(c == 0xf1) c='n';
            else if(c >= 0xf2 && c <= 0xf6) c='o';
            else if(c >= 0xf9 && c <= 0xfc) c='u';
            else if(c == 0xfd || c == 0xff) c='y';
            else if(c>127) c=*(++ch+1); //' ';

            *ansi++=c;
            len--;
            ch++;
        }
        else
        {
            if(*ch<32) *ch=32;
            *ansi++=*ch;

            len--;
        }

        ch++;
    }

    while(len > 0)
    {
        *ansi++=0;
        len--;
    }
}

#include "utils_gamelist.h"
#include "console_debug.h"
#include "fast_copy.h"
#include "sys8_path_table.h"

static s64 count_cache_bytes(char *path)
{
    int n = 0;
    s64 bytes = 0;

    while(n < 100)
    {
        struct stat s;
        sprintf(buff, "%s.666%2.2i", path, n);

        if(stat(buff, &s) < 0) break; //end loop

        bytes += s.st_size;
        n++;
    }

    return bytes; //return total bytes counted

}

static int num_directories = 0, num_files_big = 0, num_files_split = 0, fixed_self_sprx = 0;

static int my_game_test(char *path)
{
    DIR  *dir;
    int seconds = 0, seconds2 = 0;

    if(strstr(path, "/PS3_GAME"))
        DPrintf("%s\n", strstr(path, "/PS3_GAME"));
    else
        DPrintf("%s\n", path);

    char f[MAX_PATH_LEN];
    struct stat s;

    dir = opendir(path);
    if(!dir) return FAILED;

    sysFsChmod(path, FS_S_IFDIR | 0777);

    while(true)
    {
        struct dirent *entry = readdir(dir);

        if(!entry) break;

        if(entry->d_name[0] == '.' && (entry->d_name[1] == 0 || entry->d_name[1] == '.')) continue;

        if((entry->d_type & DT_DIR))
        {
            num_directories++;

            sprintf(f, "%s/%s", path, entry->d_name);
            my_game_test(f);

            if(abort_copy) break;
        }
        else
        {
            s64 size = 0LL;

            sprintf(f, "%s/%s", path, entry->d_name);

            if(stat(f, &s) != SUCCESS) {DPrintf("File error!!!\n -> %s\n\n", f); continue;}
            size = s.st_size;

            int ext = entry->d_namlen - 4;
            if(ext > 1 && ( strcasestr("sprx|self", entry->d_name + ext) != NULL ||
                          ( strcmp(entry->d_name, "EBOOT.BIN" ) == SUCCESS )))
            {
                int r = patch_exe_error_09(f);

                if(r == -1)
                {
                    self_alarm_version = 1;
                    DPrintf(">> %s requires a higher CFW!!!\n", entry->d_name);
                }
                else if(r == 1)
                {
                    fixed_self_sprx++;
                    DPrintf(">> %s [fixed]\n", entry->d_name);
                }
                else
                    DPrintf(">> %s\n", entry->d_name);

            }

            if(entry->d_namlen > 6)
            {
                char *p = f;
                p += strlen(f) - 6; // adjust for .666xx
                if(p[0] ==  '.' && p[1] ==  '6' && p[2] ==  '6' && p[3] ==  '6')
                {
                    DPrintf("%s %lli MB %s\n\n", language[GLUTIL_SPLITFILE], size/0x100000LL, f);
                    num_files_split++;

                    if(copy_split_to_cache && p[4] == '0' && p[5] == '0')
                    {
                        if(nfilecached < MAX_FILECACHED)
                        {
                            sprintf(buff, language[GAMETESTS_FOUNDINSTALL], entry->d_name);
                            if(DrawDialogYesNo(buff) == YES)
                            {
                                sprintf(&filecached[nfilecached][0][0], "%s/%s", path, entry->d_name);
                                sprintf(&filecached[nfilecached][1][0], "%s", entry->d_name);

                                char * a = strstr((char *) &filecached[nfilecached][0][0], ".66600");
                                if(a) a[0] = 0;
                                a = strstr((char *) &filecached[nfilecached][1][0], ".66600");
                                if(a) a[0] = 0;

                                filecached_bytes[nfilecached] = count_cache_bytes(&filecached[nfilecached][0][0]);

                                //prepare next
                                nfilecached++;
                            }
                        }
                    }
                }
                else
                if(entry->d_namlen > 7)
                {
                    char *p = f;
                    p += strlen(f) - 7; // adjust for .666xx
                    if(p[2] ==  '.' && p[3] ==  'p' &&  p[4] ==  'a' && p[5] ==  'r' && p[6] ==  't')
                    {
                        DPrintf("%s %lli MB %s\n\n", language[GLUTIL_SPLITFILE], size / 0x100000LL, f);
                        num_files_split++;

                        if(copy_split_to_cache && p[0] == '.' && p[1] == '1')
                        {   num_files_split++;
                            if(nfilecached < MAX_FILECACHED)
                            {
                                sprintf(buff, language[GAMETESTS_FOUNDINSTALL], entry->d_name);
                                if(DrawDialogYesNo(buff) == YES)
                                {
                                    sprintf(&filecached[nfilecached][0][0], "%s/%s", path, entry->d_name);
                                    sprintf(&filecached[nfilecached][1][0], "%s", entry->d_name);

                                    char * a = strstr((char *) &filecached[nfilecached][0][0], ".1.part");
                                    if(a) a[0] = 0;
                                    a = strstr((char *) &filecached[nfilecached][1][0], ".1.part");
                                    if(a) a[0] = 0;

                                    filecached_bytes[nfilecached]=count_cache_bytes(&filecached[nfilecached][0][0]);

                                    //prepare next
                                    nfilecached++;
                                }
                            }
                        }
                    }
                }
            }

            if(size >= 0x100000000LL)
            {
                DPrintf("%s %lli MB %s\n\n", language[GAMETESTS_BIGFILE], size/0x100000LL, f); num_files_big++;
            }

            //prepare info for user
            seconds = (int) (time(NULL)-time_start);

            file_counter++;

            global_device_bytes += size;

            if(seconds != seconds2)
            {
                sprintf(string1,"%s: %i %s: %2.2i:%2.2i:%2.2i Vol: %1.2f GB\n", language[GAMETESTS_TESTFILE], file_counter,
                        language[GLUTIL_TIME], seconds/3600, (seconds / 60) % 60, seconds % 60,
                        ((double) global_device_bytes) / GIGABYTES);

                cls2();

                DbgHeader( string1);
                DbgMess(language[GLUTIL_HOLDTRIANGLEAB]);

                DbgDraw();

                tiny3d_Flip();

                ps3pad_poll();

                seconds2 = seconds;
            }

            if(abort_copy) break;
            if(!copy_split_to_cache && (new_pad & BUTTON_CIRCLE_)) abort_copy = 1;

            if(abort_copy) break;
        }

    }

    sprintf(string1,"%s: %i %s: %2.2i:%2.2i:%2.2i Vol: %1.2f GB\n", language[GAMETESTS_TESTFILE], file_counter,
                    language[GLUTIL_TIME], seconds / 3600, (seconds / 60) % 60, seconds % 60,
                    ((double) global_device_bytes) / GIGABYTES);

    cls2();

    DbgHeader( string1);
    DbgMess(language[GLUTIL_HOLDTRIANGLEAB]);

    DbgDraw();

    tiny3d_Flip();


    closedir(dir);

    return SUCCESS;
}

static int my_game_countsize(char *path)
{
    DIR  *dir;
    dir = opendir(path);
    if(!dir) return FAILED;

    DPrintf("count in %s\n", path);

    char f[MAX_PATH_LEN];
    struct stat s;

    while(true)
    {
        struct dirent *entry = readdir(dir);

        if(!entry) break;

        if(entry->d_name[0] == '.' && (entry->d_name[1] == 0 || entry->d_name[1] == '.')) continue;

        if((entry->d_type & DT_DIR))
        {
            sprintf(f, "%s/%s", path, entry->d_name);

            if(strcmp(entry->d_name, "PS3_UPDATE") != 0) my_game_countsize(f);

            if(abort_copy) break;
        }
        else
        {
            s64 size = 0LL;

            sprintf(f, "%s/%s", path, entry->d_name);

            if(stat(f, &s) != SUCCESS) {DPrintf("File error!!!\n -> %s\n\n", f); continue;}
            size = s.st_size;

            file_counter++;

            copy_total_size += size;

            if((file_counter  & 15) ==  1)
            {
                sprintf(string1,"%s: %i Vol: %1.2f GB\n", language[GAMETESTS_CHECKSIZE], file_counter, ((double) copy_total_size) / GIGABYTES);
                cls2();

                DbgHeader( string1);
                DbgMess(language[GLUTIL_HOLDTRIANGLESK]);

                DbgDraw();

                tiny3d_Flip();
            }

            if(ps3pad_poll())
            {
                abort_copy = 1;
            }

            if(abort_copy) break;
        }
    }

    closedir(dir);

    {
        sprintf(string1,"%s: %i Vol: %1.2f GB\n", language[GAMETESTS_CHECKSIZE], file_counter, ((double) copy_total_size) / GIGABYTES);
        cls2();

        DbgHeader( string1);
        DbgMess(language[GLUTIL_HOLDTRIANGLESK]);

        DbgDraw();

        tiny3d_Flip();
    }

    return SUCCESS;
}

static int my_game_delete(char *path)
{
    DIR  *dir;
    char f[MAX_PATH_LEN];
    int seconds;

    dir = opendir(path);
    if(!dir) return FAILED;
    sysFsChmod(path, FS_S_IFDIR | 0777);

    copy_split_to_cache = 0;

    while(true)
    {
        struct dirent *entry = readdir(dir);

        if(!entry) break;

        if(entry->d_name[0] == '.' && (entry->d_name[1] == 0 || entry->d_name[1] == '.')) continue;

        if((entry->d_type & DT_DIR))
        {
            sprintf(f, "%s/%s", path, entry->d_name);
            my_game_delete(f);

            DPrintf("Deleting <%s>\n\n", path);
            if(sysLv2FsRmdir(f)) {abort_copy = 3; DPrintf("Deleting Error!!!\n -> <%s>\n\n", entry->d_name); break;}

            if(abort_copy) break;

            file_counter--;

            goto display_mess;
        }
        else
        {
            sprintf(f, "%s/%s", path, entry->d_name);

            sysFsChmod(f, FS_S_IFMT | 0777);
            if(sysLv2FsUnlink(f)){abort_copy = 3;DPrintf("Deleting Error!!!\n -> %s\n\n", f); break;}

            DPrintf("%s %s\n\n", language[GAMEDELFL_DELETED], f);

            display_mess:

            seconds = (int) (time(NULL) - time_start);

            sprintf(string1,"%s: %i %s: %2.2i:%2.2i:%2.2i\n", language[GAMEDELFL_DELETING], file_counter,
                    language[GLUTIL_TIME], seconds / 3600, (seconds / 60) % 60, seconds % 60);

            file_counter++;

            cls2();

            DbgHeader( string1);
            DbgMess(language[GLUTIL_HOLDTRIANGLEAB]);
            DbgDraw();

            tiny3d_Flip();

            if(abort_copy) break;

            if(ps3pad_poll())  abort_copy = 1;

            if(abort_copy) break;
        }
    }

    closedir(dir);

    return SUCCESS;
}

static int _my_game_copy(char *path, char *path2)
{
    Lv2FsFile  dir;

    filepath_check(path2);

    if (sysFsOpendir(path, &dir)) {DPrintf("Error in sysFsOpendir()\n"); abort_copy = 7; return FAILED;}

    if(!copy_split_to_cache) DPrintf("\n%s: %s\n%s: %s\n\n", language[FASTCPADD_COPYING], path, language[GLUTIL_WTO], path2);

    while(true)
    {
        sysFSDirent entry;
        u64 read;
        read = sizeof(sysFSDirent);

        if(sysFsReaddir(dir, &entry, &read) || !read || abort_copy) break;

        if(entry.d_name[0] == '.' && entry.d_name[1] == 0) continue;
        if(entry.d_name[0] == '.' && entry.d_name[1] == '.' && entry.d_name[2] == 0) continue;

        if((entry.d_type & DT_DIR))
        {
            char *d1 = (char *) malloc(MAX_PATH_LEN);
            char *d2 = (char *) malloc(MAX_PATH_LEN);

            if(!d1 || !d2) {if(d1) free(d1); if(d2) free(d2); sysFsClosedir(dir); DPrintf("malloc() Error!!!\n\n"); abort_copy = 2; return FAILED;}

            sprintf(d1,"%s/%s", path, entry.d_name);
            sprintf(d2,"%s/%s", path2, entry.d_name);

            if(!copy_split_to_cache) mkdir_secure(d2);

            if(strcmp(entry.d_name, "PS3_UPDATE") != 0) _my_game_copy(d1, d2);

            free(d1); free(d2);
        }
        else if(strcmp(entry.d_name, "PS3UPDAT.PUP") == 0) ;
        else
        {
           //if(!copy_split_to_cache) DPrintf("> %s\n", entry.d_name);

           if(fast_copy_add(path, path2, entry.d_name) < 0)
           {
                abort_copy = 666;
                DPrintf("Failed in fast_copy_add()\n");
                sysFsClosedir(dir);
                return FAILED;
           }
        }

        if(abort_copy) break;
    }

/*
    if(fast_num_files >= 1) {
        int ret = fast_copy_process();

        if(ret < 0 || abort_copy) {
            UTF8_to_Ansi(language[FASTCPADD_FAILED], ansi, 1024);
            DPrintf("%s%i\n", ansi, ret);
            abort_copy = 666;
            DPrintf("Failed in fast_copy_add()\n");
            sysFsClosedir(dir);
            return FAILED;
        }
    }
*/
    sysFsClosedir(dir);
    if(abort_copy) return FAILED;

    return SUCCESS;
}

static int my_game_copy(char *path, char *path2)
{

    progress_action2 = 0;

    bar1_countparts = 0.0f;
    bar2_countparts = 0.0f;

    msgDialogOpen2(mdialogprogress2, progress_bar_title, progress_callback, (void *) 0xadef0045, NULL);

    msgDialogProgressBarSetMsg(MSG_PROGRESSBAR_INDEX0, " ");
    msgDialogProgressBarSetMsg(MSG_PROGRESSBAR_INDEX1, " ");
    msgDialogProgressBarReset(MSG_PROGRESSBAR_INDEX0);
    msgDialogProgressBarReset(MSG_PROGRESSBAR_INDEX1);

    sysUtilCheckCallback(); tiny3d_Flip();

    global_device_bytes = 0;

    if(fast_copy_async(path, path2, 1) < 0) {abort_copy = 665;  msgDialogAbort(); return FAILED;}

    int ret = _my_game_copy(path, path2);

    int ret2 = fast_copy_process();

    fast_copy_async(path, path2, 0);

    msgDialogAbort();

    if(ret < 0 || ret2 < 0) return FAILED;

    return SUCCESS;
}


static char filename[MAX_PATH_LEN];

void copy_from_selection(int game_sel)
{
    copy_split_to_cache = 0;
    copy_total_size = 0LL;
    file_counter = 0;
    abort_copy = 0;

    DCls();
    my_game_countsize(directories[game_sel].path_name);
    total_fast_files = file_counter;

    sleep(2); DCls();

    if(!(directories[game_sel].flags & D_FLAG_BDVD))
    {
        if(!(directories[game_sel].flags & D_FLAG_HDD0))
        {
            // test for HDD0
            u32 blockSize;
            u64 freeSize;
            u64 free_hdd0;
            sysFsGetFreeSize("/dev_hdd0/", &blockSize, &freeSize);
            free_hdd0 = ( ((u64)blockSize * freeSize));

            if((copy_total_size + 0x40000000LL) >= (s64) free_hdd0)
            {
                sprintf(filename, language[GAMECHCPY_NOSPACE], free_hdd0, ((double) (copy_total_size + 0x40000000LL - free_hdd0)) / GIGABYTES);
                DrawDialogOK(filename);

                return;
            }
        }
    }

    if(abort_copy) //abort by user or got an error
    {
        if(DrawDialogYesNo(language[GAMECPYSL_GSIZEABCNTASK]) != YES)
        {
            forcedevices = D_FLAG_USB;
            return;
        }
    }

    if(directories[game_sel].flags & D_FLAG_BDVD)  {copy_from_bluray(); return;}

#ifdef PSDEBUG
    int ret;
#endif
    int n;
    int curr_device = 0;
    char name[MAX_PATH_LEN];
    int dest = 0;

    dialog_action = 0;
    abort_copy = 0;

    char * hdd_folder2 = hdd_folder;

    char * path_install = __MKDEF_GAMES_DIR;

    if(directories[game_sel].flags & (PS1_FLAG)) {path_install = "PSXGAMES"; hdd_folder2 = "dev_hdd0";}

    if(directories[game_sel].flags & D_FLAG_HDD0)
    {
        // is hdd0
        for(n = 1; n < 11; n++)
        {
            dialog_action = 0;

            if((fdevices >> n) & 1)
            {
                if(copy_total_size)
                {
                    sprintf(filename, "%s\n\n%s HDD0 %s USB00%c?\nVol: %1.2f GB", directories[game_sel].title,
                            language[GLUTIL_WANTCPYFROM], language[GLUTIL_WTO], 47 + n,
                            ((double) copy_total_size) / GIGABYTES);
                }
                else
                {
                    sprintf(filename, "%s\n\n%s HDD0 %s USB00%c?", directories[game_sel].title, language[GLUTIL_WANTCPYFROM], language[GLUTIL_WTO], 47 + n);
                }

                msgDialogOpen2( mdialogyesno, filename, my_dialog, (void*) 0x0000aaaa, NULL );

                wait_dialog();

                if(dialog_action == 1)  {curr_device = n; break;} // exit
            }
        }


       dest = n;

       if(dialog_action == 1)
       {
            u32 blockSize;
            u64 freeSize;
            u64 free_usb;

            sprintf(progress_bar_title, "HDD0 -> USB00%c", 47 + n);

            sprintf(name, "/dev_usb00%c/", 47 + curr_device);
            sysFsGetFreeSize(name, &blockSize, &freeSize);

            free_usb = ( ((u64)blockSize * freeSize));

            if((copy_total_size + 0x100000LL) >= (s64) free_usb)
            {
                sprintf(filename, "%s\n\n%s%1.2f GB\n\n%s", "Warning: There is not enough space in USB to copy it", "You need ",
                    ((double) (copy_total_size + 0x100000LL - free_usb)) / GIGABYTES,  "Do you want to abort?");

                dialog_action = 0;

                msgDialogOpen2( mdialogyesno2, filename, my_dialog, (void*) 0x0000aaaa, NULL);

                wait_dialog();

                if(dialog_action == 1)  {return;} else dialog_action = 1; // exit
            }

            if(directories[game_sel].flags & (PS1_FLAG))
            {
                sprintf(name, "/dev_usb00%c/PSXGAMES", 47 + curr_device);
                mkdir_secure(name);
                sprintf(name, "/dev_usb00%c/PSXGAMES", 47 + curr_device);
                mkdir_secure(name);

                char * p = strstr(directories[game_sel].path_name, "/PSXGAMES");

                if(!p) p = "NULL"; else p += 10;

                sprintf(name, "/dev_usb00%c/%s/%s", 47 + curr_device, path_install, p);
                mkdir_secure(name);

            }
            else
            {
                sprintf(name, "/dev_usb00%c/" __MKDEF_GAMES_DIR, 47 + curr_device);
                mkdir_secure(name);
                sprintf(name, "/dev_usb00%c/" __MKDEF_GAMES_DIR, 47 + curr_device);
                mkdir_secure(name);

                char * p = strstr(directories[game_sel].path_name, "/" __MKDEF_GAMES_DIR);
                if(!p) p = strstr(directories[game_sel].path_name, "/GAMES");

                if(!p) p = "NULL"; else p += 7;

                sprintf(name, "/dev_usb00%c/" __MKDEF_GAMES_DIR "/%s", 47 + curr_device, p);
                mkdir_secure(name);
            }
        }

    }
    else if(fdevices & 1)
    {
        //is usb
        for(n = 1; n < 11; n++)
        {
            if((directories[game_sel].flags >> n) & 1) break;
        }

        if(n == 11) return;

        curr_device = 0;

        dest = 0;

        sprintf(filename, "%s\n\n%s USB00%c %s HDD0?\nVol: %1.2f GB", directories[game_sel].title,
                        language[GLUTIL_WANTCPYFROM], 47 + n, language[GLUTIL_WTO],
                        ((double) copy_total_size) / GIGABYTES);

        dialog_action = 0;
        msgDialogOpen2( mdialogyesno, filename, my_dialog, (void*)0x0000aaaa, NULL );

        wait_dialog();

        if(dialog_action == 1)
        {
            sprintf(progress_bar_title, "USB00%c -> HDD0", 47 + n);

            char *p;
            if((directories[game_sel].flags & (PS1_FLAG)) == (PS1_FLAG))
            {
                p = strstr(directories[game_sel].path_name, "/PSXGAMES");

                if(!p) {p = strstr(directories[game_sel].path_name, "/PSXISO"); if(p) p += 8;} else p += 10;

                if(!p) p = "NULL";
            }
            else
            if((directories[game_sel].flags & (PS2_FLAG)) == (PS2_FLAG))
            {
                p = strstr(directories[game_sel].path_name, "/PS2ISO");

                if(!p) p = "NULL"; else p += 8;
            }
            else
            if((directories[game_sel].flags & (PSP_FLAG)) == (PSP_FLAG))
            {
                p = strstr(directories[game_sel].path_name, "/PSPISO");

                if(!p) p = "NULL"; else p += 8;
            }
            else
            {
                p = strstr(directories[game_sel].path_name, "/" __MKDEF_GAMES_DIR); if(p) p += 7;

                if(!p) {p = strstr(directories[game_sel].path_name, "/GAMES"); if(p) p += 7;}

                if(!p) {p = strstr(directories[game_sel].path_name, "/GAMEZ"); if(p) p += 7;}

                if(!p) {p = strstr(directories[game_sel].path_name, "/PS3ISO"); if(p) p += 8;}

                if(!p) p = "NULL";
            }


            while(p[0] == '_') p++; // skip special char

            if(!memcmp(hdd_folder,"dev_hdd0", 9) || (directories[game_sel].flags & (PS1_FLAG)))
            {

                sprintf(name, "/%s/%s", hdd_folder2, path_install);
                mkdir_secure(name);
                sprintf(name, "/%s/%s/%s", hdd_folder2, path_install, p);
                mkdir_secure(name);
            }
            else if (!memcmp(hdd_folder, "GAMES", 6) || !memcmp(hdd_folder, "dev_hdd0_2", 11))
            {
                sprintf(name, "/%s/GAMES", "dev_hdd0");
                mkdir_secure(name);
                sprintf(name, "/%s/GAMES/%s", "dev_hdd0", p);
                mkdir_secure(name);

            }
            else
            {
                sprintf(name, "/dev_hdd0/game/%s", hdd_folder);
                mkdir_secure(name);
                sprintf(name, "/dev_hdd0/game/%s/" __MKDEF_GAMES_DIR, hdd_folder);
                mkdir_secure(name);
                sprintf(name, "/dev_hdd0/game/%s/" __MKDEF_GAMES_DIR "/%s", hdd_folder, p);
                mkdir_secure(name);
            }
        }
    }

    // reset to update datas
    forcedevices = (1 << curr_device);

    if(dialog_action == 1)
    {
        time_start = time(NULL);

        abort_copy = 0;
        DCls();
        file_counter = 0;
        new_pad = 0;

        DPrintf("%s %s\n %s %s\n\n", language[GAMECPYSL_STARTED], directories[game_sel].path_name, language[GLUTIL_WTO], name);

        if(curr_device != 0) copy_mode = 1; // break files >= 4GB
        else copy_mode = 0;

        copy_is_split=0;

        my_game_copy((char *) directories[game_sel].path_name, (char *) name);

        cls2();

        int seconds = (int) (time(NULL) - time_start);
        int vflip = 0;

        if(!abort_copy)
        {
            char *p;
            if(directories[game_sel].flags & (PS1_FLAG)) p = strstr(directories[game_sel].path_name, "/PSXGAMES");
            else
            {
                p = strstr(directories[game_sel].path_name, "/" __MKDEF_GAMES_DIR);

                if(!p) p = strstr(directories[game_sel].path_name, "/GAMES");
            }

            if(!p) p = "NULL"; else p+= 7;

            if(p[0] == '_') copy_is_split=555; // only rename the game
            while(p[0] == '_') p++; // skip special char

            if(dest == 0)
            {
                // change to no split!!!
                if(!memcmp(hdd_folder,"dev_hdd0", 9) || (directories[game_sel].flags & (PS1_FLAG)))
                    sprintf(filename, "/%s/%s/%s", hdd_folder2, path_install, p);
                else if (!memcmp(hdd_folder, "GAMES", 6) || !memcmp(hdd_folder, "dev_hdd0_2", 11))
                    sprintf(filename, "/%s/GAMES/%s", "dev_hdd0", p);
                else
                    sprintf(filename, "/dev_hdd0/game/%s/" __MKDEF_GAMES_DIR "/%s", hdd_folder, p);
            }
            else
            {
                if(copy_is_split)
                    sprintf(filename, "/dev_usb00%c/%s/_%s", 47+dest, path_install, p);
                else
                    sprintf(filename, "/dev_usb00%c/%s/%s", 47+dest, path_install, p);
            }
            //DrawDialogOK(name);
            //DrawDialogOK(filename);

            filepath_check(filename);

            // try rename
            if(copy_is_split)
            {
            #ifdef PSDEBUG
                ret =
            #endif
                sysLv2FsRename(name, filename);
            }


            if(dest != 0 && copy_is_split)
            {
                sprintf(filename, language[GAMECPYSL_SPLITEDUSBNFO], directories[game_sel].title, 47 + curr_device);

                dialog_action = 0;

                msgDialogOpen2( mdialogok, filename, my_dialog2, (void*) 0x0000aaab, NULL );

                wait_dialog();
            }

        }

        while(true)
        {
            if(abort_copy)
            {
                sprintf(string1,"%s  %s: %2.2i:%2.2i:%2.2i\n", language[GLUTIL_ABORTED], language[GLUTIL_TIME],
                    seconds / 3600, (seconds/60) % 60, seconds % 60);
            }
            else
            {
                sprintf(string1,"%s  %s: %2.2i:%2.2i:%2.2i Vol: %1.2f GB\n", language[GAMECPYSL_DONE], language[GLUTIL_TIME],
                    seconds / 3600, (seconds / 60) % 60, seconds % 60, ((double) global_device_bytes) / GIGABYTES);
            }

            cls2();

            DbgHeader( string1);

            if(vflip & 32)
                DbgMess(language[GLUTIL_XEXIT]);
            else
                DbgMess("");

            vflip++;

            DbgDraw();

            tiny3d_Flip();

            if(ps3pad_poll())
            {
               new_pad = 0;
               break;
            }

        }

        if(abort_copy)
        {
            if(dest == 0)
                sprintf(filename, "%s\n\n%s HDD0?", directories[game_sel].title, language[GAMECPYSL_FAILDELDUMP]);
            else
                sprintf(filename, "%s\n\n%s USB00%c?", directories[game_sel].title, language[GAMECPYSL_FAILDELDUMP], 47 + dest);

            dialog_action = 0;

            msgDialogOpen2( mdialogyesno, filename, my_dialog, (void*) 0x0000aaaa, NULL );

            wait_dialog();

            if(dialog_action == 1)
            {
                abort_copy = 0;
                time_start = time(NULL);
                file_counter = 0;

                my_game_delete((char *) name);

                rmdir_secure((char *) name); // delete this folder

                game_sel = 0;
            }
            else
            {
                char *p;

                if(directories[game_sel].flags & (PS1_FLAG))
                    p = strstr(directories[game_sel].path_name, "/PSXGAMES");
                else
                {
                    p = strstr(directories[game_sel].path_name, "/" __MKDEF_GAMES_DIR);
                    if(!p) p = strstr(directories[game_sel].path_name, "/GAMES");
                }

                if(!p) p = "NULL"; else p+= 7;

                if(p[0] == '_') p++; // skip special char

                if(dest == 0)
                {
                    if(!memcmp(hdd_folder,"dev_hdd0", 9) || (directories[game_sel].flags & (PS1_FLAG)))
                        sprintf(filename, "/%s/%s/_%s", hdd_folder2, path_install, p);
                    else if (!memcmp(hdd_folder, "GAMES", 6) || !memcmp(hdd_folder, "dev_hdd0_2", 11))
                        sprintf(filename, "/%s/GAMES/_%s", "dev_hdd0", p);
                    else
                        sprintf(filename, "/dev_hdd0/game/%s/" __MKDEF_GAMES_DIR "/_%s", hdd_folder, p);
                }
                else
                    sprintf(filename, "/dev_usb00%c/" __MKDEF_GAMES_DIR "/_%s", 47 + dest, p);


                #ifdef PSDEBUG
                ret =
                #endif
                sysLv2FsRename(name, filename);
            }
        }

        game_sel = 0;

    }

}

void copy_from_bluray()
{
    char name[MAX_PATH_LEN];

    int curr_device = 0;
    sysFSStat status;

    char id[16];

    int n;
#ifdef PSDEBUG
    int ret;
#endif

    dialog_action = 0;
    abort_copy = 0;

    for(n = 0; n < 11; n++)
    {
        dialog_action = 0;

        if((fdevices >> n) & 1)
        {
            if(n == HDD0_DEVICE)
                sprintf(filename, "%s\n\n%s BDVD %s HDD0?\nVol: %1.2f GB", bluray_game,
                        language[GLUTIL_WANTCPYFROM], language[GLUTIL_WTO], ((double) copy_total_size) / GIGABYTES);
            else
                sprintf(filename, "%s\n\n%s BDVD %s USB00%c?\nVol: %1.2f GB",  bluray_game,
                        language[GLUTIL_WANTCPYFROM], language[GLUTIL_WTO], 47 + n, ((double) copy_total_size) / GIGABYTES);

            msgDialogOpen2( mdialogyesno, filename, my_dialog, (void*) 0x0000aaaa, NULL);

            wait_dialog();

            if(dialog_action == 1)
            {
                if(n != 0) sprintf(progress_bar_title, "BDVD -> USB00%c", 47 + n);
                else       sprintf(progress_bar_title, "BDVD -> HDD0");
                           curr_device = n; break;
            }              // exit
        }
    }


    // reset to update datas
    forcedevices = (1 << curr_device);

    if(dialog_action == 1)
    {
        if(curr_device == 0) sprintf(name, "/dev_hdd0");
        else sprintf(name, "/dev_usb00%c", 47 + curr_device);

        if(curr_device == 0)
        {
            u32 blockSize;
            u64 freeSize;
            u64 free_hdd0;
            sysFsGetFreeSize("/dev_hdd0/", &blockSize, &freeSize);
            free_hdd0 = ( ((u64)blockSize * freeSize));

            if((copy_total_size + 0x40000000LL) >= (s64) free_hdd0)
            {
                sprintf(filename, language[GAMECHCPY_NOSPACE], free_hdd0, ((double) (copy_total_size + 0x40000000LL - free_hdd0)) / GIGABYTES);
                DrawDialogOK(filename);
                return;
            }
        }
        else
        {
            u32 blockSize;
            u64 freeSize;
            u64 free_usb;

            sprintf(filename,"%s/", name);
            sysFsGetFreeSize(filename, &blockSize, &freeSize);
            free_usb = ( ((u64)blockSize * freeSize) );

            if((copy_total_size + 0x100000LL) >= (s64) free_usb)
            {
                sprintf(filename, "%s\n\n%s%1.2f GB\n\n%s", "Warning: There is not enough space in USB to copy it", "You need ",
                    ((double) (copy_total_size + 0x100000LL - free_usb)) / GIGABYTES, "Do you want to abort?");

                dialog_action = 0;

                msgDialogOpen2( mdialogyesno2, filename, my_dialog, (void*) 0x0000aaaa, NULL);

                wait_dialog();

                if(dialog_action == 1)  {return;}  // exit
                else dialog_action = 1;
            }
        }

        if (sysFsStat(name, &status) == 0 && !parse_ps3_disc((char *) "/dev_bdvd/PS3_DISC.SFB", id))
        {
            if(curr_device == 0)
            {
                if(!memcmp(hdd_folder,"dev_hdd0", 9))
                {
                    sprintf(name, "/%s/" __MKDEF_GAMES_DIR, hdd_folder);
                    mkdir_secure(name);
                    sprintf(name, "/%s/" __MKDEF_GAMES_DIR "/%s", hdd_folder, id);
                    mkdir_secure(name);
                }
                else if (!memcmp(hdd_folder, "GAMES", 6) || !memcmp(hdd_folder, "dev_hdd0_2", 11))
                {
                    sprintf(name, "/%s/GAMES", "dev_hdd0");
                    mkdir_secure(name);
                    sprintf(name, "/%s/GAMES/%s", "dev_hdd0", id);
                    mkdir_secure(name);
                }
                else
                {
                    sprintf(name, "/dev_hdd0/game/%s", hdd_folder);
                    mkdir_secure(name);
                    sprintf(name, "/dev_hdd0/game/%s/" __MKDEF_GAMES_DIR, hdd_folder);
                    mkdir_secure(name);
                    sprintf(name, "/dev_hdd0/game/%s/" __MKDEF_GAMES_DIR "/%s", hdd_folder, id);
                    mkdir_secure(name);
                }
            }
            else
            {
                sprintf(name, "/dev_usb00%c/" __MKDEF_GAMES_DIR, 47 + curr_device);
                mkdir_secure(name);
                sprintf(name, "/dev_usb00%c/" __MKDEF_GAMES_DIR, 47 + curr_device);
                mkdir_secure(name);
                sprintf(name, "/dev_usb00%c/" __MKDEF_GAMES_DIR "/%s", 47 + curr_device, id);
                mkdir_secure(name);
            }

            time_start = time(NULL);
            abort_copy = 0;
            DCls();
            file_counter = 0;
            new_pad = 0;

            if(curr_device != 0) copy_mode = 1; // break files >= 4GB
            else copy_mode = 0;

            copy_is_split = 0;

            my_game_copy((char *) "/dev_bdvd", (char *) name);

            int seconds = (int) (time(NULL) - time_start);
            int vflip = 0;

            if(copy_is_split && !abort_copy)
            {
                if(curr_device == 0)
                {
                    if (!memcmp(hdd_folder,"dev_hdd0", 9))
                        sprintf(filename, "/%s/" __MKDEF_GAMES_DIR "/_%s", hdd_folder, id);
                    else if (!memcmp(hdd_folder, "GAMES", 6) || !memcmp(hdd_folder, "dev_hdd0_2", 11))
                        sprintf(filename, "/%s/GAMES/_%s", "dev_hdd0", id);
                    else
                        sprintf(filename, "/dev_hdd0/game/%s/" __MKDEF_GAMES_DIR "/_%s", hdd_folder, id);
                }
                else
                        sprintf(filename, "/dev_usb00%c/" __MKDEF_GAMES_DIR "/_%s", 47 + curr_device, id);

                #ifdef PSDEBUG
                ret =
                #endif
                sysLv2FsRename(name, filename);

                if(curr_device == 0)
                    sprintf(filename, language[GAMECPYSL_SPLITEDHDDNFO], id);
                else
                    sprintf(filename, language[GAMECPYSL_SPLITEDUSBNFO], id, 47 + curr_device);

                dialog_action = 0;
                msgDialogOpen2( mdialogok, filename, my_dialog2, (void*) 0x0000aaab, NULL );
                wait_dialog();

            }

            while(true)
            {
                if(abort_copy)
                    sprintf(string1,"%s  %s: %2.2i:%2.2i:%2.2i\n", language[GLUTIL_ABORTED], language[GLUTIL_TIME],
                            seconds / 3600, (seconds/60) % 60, seconds % 60);
                else
                    sprintf(string1,"%s  %s: %2.2i:%2.2i:%2.2i Vol: %1.2f GB\n", language[GAMECPYSL_DONE], language[GLUTIL_TIME],
                        seconds / 3600, (seconds/60) % 60, seconds % 60, ((double) global_device_bytes) / GIGABYTES);

                cls2();

                DbgHeader( string1);

                if(vflip & 32)
                    DbgMess(language[GLUTIL_XEXIT]);
                else
                    DbgMess("");

                vflip++;

                DbgDraw();

                tiny3d_Flip();

                if(ps3pad_poll())
                {
                   new_pad = 0;
                   break;
                }

            }

            if(abort_copy)
            {
                if(curr_device == 0)
                    sprintf(filename, "%s\n\n%s HDD0?", id, language[GAMECPYSL_FAILDELDUMP]);
                else
                    sprintf(filename, "%s\n\n%s USB00%c?", id, language[GAMECPYSL_FAILDELDUMP], 47 + curr_device);

                dialog_action = 0;
                msgDialogOpen2(mdialogyesno, filename, my_dialog, (void*) 0x0000aaaa, NULL );

                wait_dialog();

                if(dialog_action == 1)
                {
                    time_start = time(NULL);
                    file_counter = 0;
                    abort_copy = 0;
                    my_game_delete((char *) name);

                    rmdir_secure((char *) name); // delete this folder

                }
                else
                {
                    if(curr_device == 0)
                    {
                        if(!memcmp(hdd_folder,"dev_hdd0", 9))
                            sprintf(filename, "/%s/" __MKDEF_GAMES_DIR "/_%s", hdd_folder, id);
                        else if (!memcmp(hdd_folder, "GAMES", 6) || !memcmp(hdd_folder, "dev_hdd0_2", 11))
                            sprintf(filename, "/%s/GAMES/_%s", "dev_hdd0", id);
                        else
                            sprintf(filename, "/dev_hdd0/game/%s/" __MKDEF_GAMES_DIR "/_%s", hdd_folder, id);
                    }
                    else
                        sprintf(filename, "/dev_usb00%c/" __MKDEF_GAMES_DIR "/_%s", 47 + curr_device, id);

                    #ifdef PSDEBUG
                    ret =
                    #endif
                    sysLv2FsRename(name, filename);

                }
            }
        }
    }
}

/////////////////

float cache_need_free = 0.0f;

void copy_to_cache(int game_sel, char * hmanager_path)
{

    if(directories[game_sel].flags & D_FLAG_BDVD)  {return;}
    if(directories[game_sel].flags & D_FLAG_HDD0)  {return;}

    int n;

    char name[MAX_PATH_LEN];
    char name2[MAX_PATH_LEN];
    int dest = 0;

    dialog_action = 0;
    abort_copy = 0;


    for(n = 1; n < 11; n++)
    {
       if((directories[game_sel].flags >> n) & 1) break;
    }

    if(n == 11) return;

    dest = n;

    sprintf(filename, "%s\n\n%s USB00%c %s HDD0 CACHE?", directories[game_sel].title, language[GLUTIL_WANTCPYFROM], 47 + dest, language[GLUTIL_WTO]);

    dialog_action = 0;
    msgDialogOpen2( mdialogyesno, filename, my_dialog, (void*)0x0000aaaa, NULL );

    wait_dialog();

    if(dialog_action == 1)
    {
        sprintf(name2, "%s/PS3_GAME", directories[game_sel].path_name);

        path_cache = name;

        nfilecached = 0;
        filecached[nfilecached][0][0] = 0;
        filecached[nfilecached][1][0] = 0;

        // reset to update data

        time_start = time(NULL);

        abort_copy = 0;
        DCls();
        file_counter = 0;
        new_pad = 0;

        //////////////

        global_device_bytes = 0;
        cache_need_free = 0;
        num_directories = file_counter = num_files_big = num_files_split = fixed_self_sprx = 0;

        copy_split_to_cache = 1;
        my_game_test((char *) name2);
        copy_split_to_cache = 0;

        if(!nfilecached)
        {
            sprintf(string1, language[GAMECHCPY_ISNEEDONEFILE]);

            DrawDialogOK(string1);

            cache_need_free = 0.0f;

            global_device_bytes = 0;

            abort_copy = 0;
            file_counter = 0;
            copy_mode = 0;

            new_pad = 0;

            return;
        }

        u32 blockSize;
        u64 freeSize;
        float freeSpace;
        float tmp_total_bytes = (global_device_bytes / GIGABYTES); //save total bytes counted

        sysFsGetFreeSize("/dev_hdd0/", &blockSize, &freeSize);
        freeSpace = ( ((u64)blockSize * freeSize) );
        freeSpace = freeSpace / GIGABYTES;

        for(n = 0; n < nfilecached; n++)
            cache_need_free += filecached_bytes[n];

        global_device_bytes = cache_need_free; // update with correct value

        cache_need_free = (cache_need_free / GIGABYTES) + 2.0f; // +2 for system

        if(freeSpace < cache_need_free) {
            sprintf(string1, language[GAMECHCPY_NEEDMORESPACE], freeSpace, cache_need_free);

            DrawDialogOK(string1);

            draw_cache_external();

            new_pad = 0;
        }

        sysFsGetFreeSize("/dev_hdd0/", &blockSize, &freeSize);
        freeSpace = ( ((u64)blockSize * freeSize));
        freeSpace = freeSpace / GIGABYTES;

        if(freeSpace < cache_need_free)
        {
            sprintf(string1, language[GAMECHCPY_NOSPACE], freeSpace, cache_need_free);

            DrawDialogOK(string1);

            cache_need_free = 0.0f;

            global_device_bytes = 0;

            abort_copy = 0;
            file_counter = 0;
            copy_mode = 0;

            new_pad = 0;

            return;
        }

        sprintf(string1, language[GAMECHCPY_CACHENFOSTART], (cache_need_free - 2.0f), tmp_total_bytes, (tmp_total_bytes - (cache_need_free - 2.0f)) , freeSpace);
        DrawDialogOK(string1);

        sprintf(name, "%s/cache", hmanager_path);
        mkdir_secure(name);
        sprintf(name, "%s/cache/%s", hmanager_path, directories[game_sel].title_id);
        mkdir_secure(name);

        copy_total_size = global_device_bytes;

        cache_need_free = 0.0f;
        global_device_bytes = 0;

        ////////////////

        DPrintf("%s %s\n %s %s\n\n", language[GAMECPYSL_STARTED], directories[game_sel].path_name, language[GLUTIL_WTO], name);

        abort_copy = 0;
        file_counter = 0;
        copy_mode = 0;

        copy_is_split = 0;

        copy_split_to_cache = 1;
        my_game_copy((char *) name2, (char *) name);
        copy_split_to_cache = 0;

        int seconds = (int) (time(NULL) - time_start);
        int vflip = 0;

        while(true)
        {
            if(abort_copy)
                sprintf(string1,"%s  %s: %2.2i:%2.2i:%2.2i\n", language[GLUTIL_ABORTED], language[GLUTIL_TIME],
                        seconds / 3600, (seconds/60) % 60, seconds % 60);
            else
                sprintf(string1,"%s  %s: %2.2i:%2.2i:%2.2i Vol: %1.2f GB\n", language[GAMECPYSL_DONE], language[GLUTIL_TIME],
                        seconds / 3600, (seconds/60) % 60, seconds % 60, ((double) global_device_bytes) / GIGABYTES);

            cls2();

            DbgHeader( string1);

            if(vflip & 32)
                DbgMess(language[GLUTIL_XEXIT]);
            else
                DbgMess("");

            vflip++;

            DbgDraw();

            tiny3d_Flip();

            if(ps3pad_poll())
            {
               new_pad = 0;
               break;
            }

        }

        if(abort_copy || nfilecached  == 0)
        {
            sprintf(filename, "%s\n\n%s USB00%c?", directories[game_sel].title, language[GAMECHCPY_FAILDELFROM], 47 + dest);

            dialog_action = 0;

            msgDialogOpen2( mdialogyesno, filename, my_dialog, (void*) 0x0000aaaa, NULL );

            wait_dialog();

            if(dialog_action == 1)
            {
                abort_copy = 0;
                time_start = time(NULL);
                file_counter = 0;

                my_game_delete((char *) name);

                rmdir_secure((char *) name); // delete this folder

                game_sel = 0;
            }
        }
        else
        {
           sprintf(name, "%s/cache/%s/paths.dir", hmanager_path, directories[game_sel].title_id);
           SaveFile(name, (char *) filecached, 2048 * nfilecached);
           sprintf(name, "%s/cache/%s/name_entry", hmanager_path, directories[game_sel].title_id);
           SaveFile(name, (char *) directories[game_sel].title, 64);
        }

    }

}
////////////////////
void delete_game(int game_sel)
{

    int n;

    copy_split_to_cache = 0;

    if(directories[game_sel].flags & D_FLAG_BDVD) return;

    for(n = 0; n < 11; n++)
    {
        if((directories[game_sel].flags >> n) & 1) break;
    }

    if(n == HDD0_DEVICE)
        sprintf(filename, "%s\n\n%s HDD0?", directories[game_sel].title, language[GAMEDELSL_WANTDELETE]);
    else
        sprintf(filename, "%s\n\n%s USB00%c?", directories[game_sel].title, language[GAMEDELSL_WANTDELETE], 47 + n);

    dialog_action = 0;

    msgDialogOpen2( mdialogyesno, filename, my_dialog, (void*) 0x0000aaaa, NULL );

    wait_dialog();

    // reset to update datas
    forcedevices = (1 << n);

    if(dialog_action == 1) {

        time_start = time(NULL);

        abort_copy = 0;
        DCls();
        file_counter = 0;
        new_pad = 0;

        DPrintf("%s %s\n\n", language[GAMEDELSL_STARTED], directories[game_sel].path_name);

        my_game_delete((char *) directories[game_sel].path_name);

        rmdir_secure((char *) directories[game_sel].path_name); // delete this folder

        game_sel = 0;

        int seconds = (int) (time(NULL) - time_start);
        int vflip = 0;

        while(true)
        {
            if(abort_copy)
                sprintf(string1,"%s  %s: %2.2i:%2.2i:%2.2i\n", language[GLUTIL_ABORTED], language[GLUTIL_TIME],
                        seconds / 3600, (seconds/60) % 60, seconds % 60);
            else
                sprintf(string1,"%s  %s: %2.2i:%2.2i:%2.2i\n", language[GAMEDELSL_DONE], language[GLUTIL_TIME],
                        seconds / 3600, (seconds/60) % 60, seconds % 60);

            cls2();

            DbgHeader( string1);

            if(vflip & 32)
                DbgMess(language[GLUTIL_XEXIT]);
            else
                DbgMess("");

            vflip++;

            DbgDraw();

            tiny3d_Flip();

            if(ps3pad_poll())
            {
               new_pad = 0;
               break;
            }
        }
    }
}


void test_game(int game_sel)
{

    int r = 0;

    time_start = time(NULL);

    abort_copy = 0;

    copy_split_to_cache = 0;

    DCls();

    file_counter = 0;
    new_pad = 0;

    global_device_bytes = 0;

    num_directories = file_counter = num_files_big = num_files_split = fixed_self_sprx = 0;


    self_alarm_version = 0;

    if(!(directories[game_sel].flags & D_FLAG_NTFS))
    {
        my_game_test(directories[game_sel].path_name);

        r = self_alarm_version;
    }

    char game_update_path[MAX_PATH_LEN];

    sprintf(game_update_path, "/dev_hdd0/game/%c%c%c%c%s", directories[game_sel].title_id[0], directories[game_sel].title_id[1],
            directories[game_sel].title_id[2], directories[game_sel].title_id[3], &directories[game_sel].title_id[5]);
    sysFSStat s;

    self_alarm_version = 0;

    if(!sysLv2FsStat(game_update_path, &s))
        patch_error_09(game_update_path, !(old_pad & BUTTON_SELECT));
    //else if((directories[game_sel].flags & (BDVD_FLAG | PS3_FLAG)) == (PS3_FLAG))
    //    patch_error_09(directories[game_sel].path_name, 1);

    DPrintf(language[GAMETSTSL_FINALNFO2], num_directories, file_counter, num_files_big, num_files_split);
    DPrintf("\n\n");
    DPrintf("SPRX/SELF fixed (error 0x80010009) %i", fixed_self_sprx);
    DPrintf("\n\n");

    if(r)
        DPrintf("This game requires a higher CFW or rebuild the SELFs/SPRX\n\nEste juego requiere un CFW superior o reconstruir los SELFs/SPRX\n\n");

    if(self_alarm_version)
        DPrintf("The update of this game requires a higher CFW or rebuild the SELFs/SPRX\n\nLa actualizacion de este juego requiere un CFW superior o reconstruir los SELFs/SPRX\n\n");

    int seconds = (int) (time(NULL) - time_start);
    int vflip = 0;

    while(true)
    {
        if(abort_copy)
            sprintf(string1,"%s  %s: %2.2i:%2.2i:%2.2i\n", language[GLUTIL_ABORTED],
                    language[GLUTIL_TIME], seconds / 3600, (seconds/60) % 60, seconds % 60);
        else
            sprintf(string1,"%s: %i %s: %2.2i:%2.2i:%2.2i Vol: %1.2f GB\n", language[GAMETSTSL_TESTED], file_counter,
                    language[GLUTIL_TIME], seconds / 3600, (seconds / 60) % 60, seconds % 60,
                    ((double) global_device_bytes) / GIGABYTES);

        cls2();

        DbgHeader( string1);

        if(vflip & 32)
            DbgMess(language[GLUTIL_XEXIT]);
        else
            DbgMess("");

        vflip++;

        DbgDraw();

        tiny3d_Flip();

        ps3pad_poll();
        if(new_pad & (BUTTON_CIRCLE_ | BUTTON_TRIANGLE | BUTTON_CROSS_))
        {
           new_pad = 0;
           break;
        }
    }


    // rename in test for non executable games
    if(num_files_split && (directories[game_sel].flags & D_FLAG_USB))
    {
        char *str = strstr(directories[game_sel].path_name, __MKDEF_GAMES_DIR);

        if(str && str[7] != '_')
        {
            int n = (str - directories[game_sel].path_name);
            memcpy(filename, directories[game_sel].path_name, n + 7);filename[n+7] = '_'; filename[n+8] =0;
            strcat(filename, str + 7);
            sysLv2FsRename(directories[game_sel].path_name, filename);
            //DrawDialogOK(filename);
            forcedevices = D_FLAG_USB;
        }
    }
}

void DeleteDirectory(const char* path)
{
    char newpath[0x440];
    sysFSDirent dir; u64 read = sizeof(sysFSDirent);

    bool is_ntfs = is_ntfs_path((char *) path);

    if (!is_ntfs)
    {
        int dfd;

        if (sysLv2FsOpenDir(path, &dfd)) return;

        sysFsChmod(path, FS_S_IFDIR | 0777);

        read = sizeof(sysFSDirent);
        while (!sysLv2FsReadDir(dfd, &dir, &read))
        {
            if (!read) break;

            if(dir.d_name[0]=='.' && (dir.d_name[1]==0 || dir.d_name[1]=='.')) continue;

            sprintf(newpath, "%s/%s", path, dir.d_name);

            if (dir.d_type & DT_DIR)
            {
                DeleteDirectory(newpath);
                sysLv2FsChmod(newpath, FS_S_IFDIR | 0777);
                sysLv2FsRmdir(newpath);
            }
            else
            {
                sysLv2FsChmod(path, FS_S_IFMT | 0777);
                sysLv2FsUnlink(newpath);
            }
        }

        sysLv2FsCloseDir(dfd);
    }
    else
    {
        struct stat st;
        DIR_ITER *pdir = NULL;

        if ((pdir = ps3ntfs_diropen(path)) == NULL) return;

        while (ps3ntfs_dirnext(pdir, dir.d_name, &st) == 0)
        {
            if(dir.d_name[0]=='.' && (dir.d_name[1]==0 || dir.d_name[1]=='.')) continue;

            sprintf(newpath, "%s/%s", path, dir.d_name);

            if (S_ISDIR(st.st_mode))
            {
                DeleteDirectory(newpath);
            }

            ps3ntfs_unlink(newpath);
        }

        ps3ntfs_dirclose(pdir);
    }
}

int FixDirectory(const char* path, int fcount)
{
    int dfd;
    u64 read;
    sysFSDirent dir;

    if (sysLv2FsOpenDir(path, &dfd)) return FAILED;

    sysFsChmod(path, FS_S_IFDIR | 0777);

    if (fcount == 0)
    {
        msgDialogOpen2(mdialogprogress, progress_bar_title, progress_callback, (void *) 0xadef0045, NULL);
        msgDialogProgressBarReset(MSG_PROGRESSBAR_INDEX0);
    }

    read = sizeof(sysFSDirent);
    while (!sysLv2FsReadDir(dfd, &dir, &read) && (fcount >=0))
    {
        if (!read)
            break;
        if (!strcmp(dir.d_name, ".") || !strcmp(dir.d_name, ".."))
            continue;

        char newpath[0x440];
        strcpy(newpath, path);
        strcat(newpath, "/");
        strcat(newpath, dir.d_name);

        fcount++;

        if ((fcount & 4) == 4)
        {
            static char string1[256];
            msgDialogProgressBarReset(MSG_PROGRESSBAR_INDEX0);

            sprintf(string1, "Fixing permissions: %s", dir.d_name);
            msgDialogProgressBarSetMsg(MSG_PROGRESSBAR_INDEX0, string1);

            //estimated progress bar (just to show some progress)
            bar1_countparts = 10 * fcount / (5 * fcount + 100);
            msgDialogProgressBarInc(MSG_PROGRESSBAR_INDEX0, (u32) bar1_countparts);

            DbgDraw();
            tiny3d_Flip();
        }

        if(ps3pad_poll()) return FAILED;

        if (dir.d_type & DT_DIR)
        {
            if(FixDirectory(newpath, fcount) == FAILED) {sysLv2FsCloseDir(dfd); return FAILED;}
        }
        else
        {
            sysLv2FsChmod(newpath, FS_S_IFMT | 0777);
        }
    }

    sysLv2FsCloseDir(dfd);
    return SUCCESS;
}

/*******************************************************************************************************************************************************/
/* Configfiles                                                                                                                                         */
/*                                                                                                                                                     */
/* Caprice32 - Amstrad CPC Emulator                                                                                                                    */
/*   (c) Copyright 1997-2004 Ulrich Doewich                                                                                                            */
/*   (c) Copyright 2011 D_Skywalk - ported and adapted for Irismanager                                                                                 */
/*******************************************************************************************************************************************************/

/*
   Ex:
   snd_pp_device = getConfigValueInt("/home/user/config.ini", "sound", "pp_device", 0);
   max_tracksize = getConfigValueInt(chFileName, "file", "max_track_size", 6144-154);
*/

int getConfigMemValueInt (char* mem, int size, char* pchSection, char* pchKey, int iDefaultValue)
{

   char chLine[MAX_CFGLINE_LEN + 1];
   char* pchToken;
   int n = 0;

      while(n < size)
      {
        // grab one line
        int flag = 0;
        int m = 0;
        while(mem && n < size && m<MAX_CFGLINE_LEN-1)
        {
            chLine[m] = mem[n]; n++;
            if(chLine[m] == '\n' || chLine[m] == '\r') {m++; flag = 1; continue;}
            if(flag) {n--;break;}
            m++;
        }

        chLine[m]= 0;  // grab one line
         pchToken = strtok(chLine, "[]"); // check if there's a section key
         if((pchToken != NULL) && (pchToken[0] != '#') && (strcmp(pchToken, pchSection) == 0)) {
            while(n < size) { // get the next line
               m = 0; flag = 0;
               while(mem && n < size && m < MAX_CFGLINE_LEN - 1)
               {
                    chLine[m] = mem[n]; n++;
                    if(chLine[m] == '\n' || chLine[m] == '\r') {m++; flag = 1; continue;}
                    if(flag) {n--;break;}
                    m++;
               }
               chLine[m]= 0;
               pchToken = strtok(chLine, "\t =\n\r"); // check if it has a key=value pair
               if((pchToken != NULL) && (pchToken[0] != '#') && (strcmp(pchToken, pchKey) == 0)) {
                  char* pchPtr = strtok(NULL, "\t =#\n\r"); // get the value if it matches our key
                  if (pchPtr != NULL) {
                     return (strtol(pchPtr, NULL, 0)); // return as integer
                  } else {
                     return iDefaultValue; // no value found
                  }
               }
            }
         }
      }

   return iDefaultValue; // no value found
}

/*
   Ex:
   getConfigValueString(chFileName, "file", "snap_path", snap_path, sizeof(snap_path)-1, defaultPath);
   getConfigValueString(chFileName, "file", "push_file", snap_file, sizeof(push_file)-1, "push0.sna");

*/

void reverse_strings(u8 *str)
{
    int n, m, l;
    int len1, len2;
    u32 string_buffer[1024];

    while(true)
    {
        if(*str == 0) break;
        while(*str < 32) str++;
        if(*str == 0) break;

        len1 = 0; len2 =0;
        // process one phrase
        while(str[len1]>=32)
        {
            if((str[len1] & 0xE0) == 0xC0 && (str[len1 + 1] & 0xc0) == 0x80)
            {
                string_buffer[len2] = ((u32) str[len1 + 1] << 8) | (u32) str[len1 + 0];
                len1+=2; len2++;
            }
            else if((str[len1] & 0xF0) == 0xE0  && (str[len1 + 1] & 0xc0) == 0x80 && (str[len1 + 2] & 0xc0) == 0x80)
            {
                string_buffer[len2] = ((u32) str[len1 + 2] << 16) | ((u32) str[len1 + 1] << 8) | (u32) str[len1 + 0];
                len1+=3; len2++;
            }
            else if((str[len1] & 0xF0) == 0xF0  && (str[len1 + 1] & 0xc0) == 0x80  && (str[len1 + 2] & 0xc0) == 0x80  && (str[len1 + 3] & 0xc0) == 0x80)
            {
                string_buffer[len2] = ((u32) str[len1 + 3] << 24) | ((u32) str[len1 + 2] << 16) | ((u32) str[len1 + 1] << 8) | (u32) str[len1 + 0];
                len1+=4; len2++;
            }
            else
            {
                if(str[len1 + 0] & 0x80)  string_buffer[len2] = '?'; else string_buffer[len2] = (u32) str[len1 + 0];
                len1++; len2++;
            }
        }

        l = 0;

        len2--;
        for(n = 0; n < len2 + 1; n++)
        {
            // reverse UTF8 symbols
            if(string_buffer[len2 - n] == 32   || (string_buffer[len2 - n] & 0xffffff80))
            {
                str[l] = string_buffer[len2 - n] & 0xff; string_buffer[len2 - n]>>= 8;l++;
                if(string_buffer[len2 - n] & 0xff)
                {
                    str[l] = string_buffer[len2 - n] & 0xff; string_buffer[len2 - n]>>= 8;l++;
                    if(string_buffer[len2 - n] & 0xff)
                    {
                        str[l] = string_buffer[len2 - n] & 0xff; string_buffer[len2 - n]>>= 8;l++;
                        if(string_buffer[len2 - n] & 0xff)
                        {
                            str[l] = string_buffer[len2 - n] & 0xff;
                            l++;
                        }
                    }
                }
            }
            else
            {
                // dont reverse words with symbols from 32 to 127 UTF8
                int s;
                m= 0;
                while((m+n) < (len2 + 1) && (string_buffer[len2 - n - m] & 0xff) > 32 &&
                      (string_buffer[len2 - n - m] & 0xffffff80) == 0) m++;

                for(s = 0; s < m; s++)
                {
                    str[l + m - s - 1] = string_buffer[len2 - n - s] & 0xff;
                }
                l += m;
                n += m - 1;
            }
        }

        str += len1;
    }
}

void convertStringEndl(char* string, int iSize);

extern int reverse_language;

int getConfigMemValueString(char* mem, int size, char* pchSection, char* pchKey, char* pchValue, int iSize, char* pchDefaultValue)
{

   char chLine[MAX_CFGLINE_LEN + 1];
   int n = 0;
   int find_section = 0;

   // open the config file

   while(n < size)
   {
      if(mem[n] == '[') break;
      n++;
   }

   while(n<size)
   {
       // grab one line
       int flag = 0;
       int m = 0, l;
       while(mem && n < size && m < MAX_CFGLINE_LEN - 1)
       {
           chLine[m] = mem[n]; n++;
           if(chLine[m] == '\n' || chLine[m] == '\r') {m++; flag = 1; continue;}
           if(flag) {n--; break;}
           m++;
       }
       chLine[m]= 0;

       // note from Estwald
       // routines to avoid the stupid strok() "break-lines" function. XD
       // standard function is not the better way to get custom strings

       m = 0; while (chLine[m] == ' ' || chLine[m] == '\t') m++; // skip spaces and tabs
       if(chLine[m] == '#') continue; // skip comment. Next line

       if(chLine[m] == '[')
       {
           // section token finded
           if(find_section) break; // this is another different section, sure

           m++;
           l = m;

           while (chLine[l]!=']' && chLine[l]!=0) l++;

           if(chLine[l]!=']') break;  // section error!!!

           // test if section do not match
           if(strlen(pchSection) != (l - m) || strncmp(pchSection, &chLine[m], l - m)) continue;
           find_section = 1;
       }

       if(find_section)
       {
           // get strings

           // find string to compare
           l = m; while (chLine[l] != '=' && chLine[l] != ' ' && chLine[l] != '\t' && chLine[l] != 0) l++;

           if(chLine[l] == 0) continue; // no match, error in line?. Next line

           if((l-m) < 4) continue; // invalid, sure (string too short). Next line

           if(strlen(pchKey) != (l - m) || strncmp(pchKey, &chLine[m], l - m)) continue;// no match. Next line

           if(chLine[l]!='=') while (chLine[l] == ' ' || chLine[l] == '\t') l++; // skip spaces

           if(chLine[l]!='=') continue;// no match. Next line (string can be valid, but you need find '=')

           m = l + 1; // skip '='
           while (chLine[m] == ' ' || chLine[m] == '\t') m++; // skip spaces and tabs

           l=m; while(chLine[l]!=0 && chLine[l]!='\n' && chLine[l]!='\r') l++; // find the end of line

           chLine[l] = 0; // break line to avoid \n \r

           // &chLine[m] countain the replacement string without string splitted ;)

           convertStringEndl(&chLine[m], iSize);
           strncpy(pchValue, &chLine[m], iSize); // copy to destination

           pchValue[iSize -1] = 0;

           if(reverse_language)
           {
               if(pchValue[0] == '-')
               {
                   // don't reverse this string
                   strncpy(pchValue, &chLine[m + 1], iSize);
                   pchValue[iSize - 1] = 0;
               }
               else
                   reverse_strings((u8*) pchValue);
           }

           return SUCCESS;
       }
   }

   strncpy(pchValue, pchDefaultValue, iSize); // no value found, return the default
   pchValue[iSize -1] = 0;
   return FAILED;
}

#define MAX_CONVERSIONS 1

char conversionTable[MAX_CONVERSIONS][2] =
{
    { '@', '\n' },
   // { '_', ' ' },
};

void convertStringEndl(char* string, int iSize)
{
    int n;
    int m = 0;
    int flag = 0;

    do
    {
        if(string[m] == 9) string[m] = ' ';
        if(string[m] < 32) {string[m] = 0; return;} // break the string
        // special symbol '_' used as space when it is concatenated
        if(string[m] == '_') {
            if(flag || string[m + 1] == '_') {flag = 1; string[m]=' ';}
        }
        else flag = 0;

        for(n = 0; n < MAX_CONVERSIONS; n++)
            if(string[m] == conversionTable[n][0])
                string[m] = conversionTable[n][1];
        m++;
    }
    while(iSize--);
}

void copy_usb_to_iris(char * path)
{
    int n;
    char name[MAX_PATH_LEN];

    copy_split_to_cache = 0;
    copy_total_size = 0LL;
    file_counter = 0;
    abort_copy = 0;

    for(n = 1; n < 11; n++)
    {
         if((fdevices >> n) & 1) break;
    }

    if(n == 11) return;

    n+= 47;
    sprintf(name, "/dev_usb00%c/iris", n);

    DCls();
    my_game_countsize(name);

    u32 blockSize;
    u64 freeSize;
    u64 free_hdd0;
    sysFsGetFreeSize("/dev_hdd0/", &blockSize, &freeSize);
    free_hdd0 = ( ((u64)blockSize * freeSize));

    if((copy_total_size + 0x40000000LL) >= (s64) free_hdd0)
    {
        sprintf(filename, language[GAMECHCPY_NOSPACE], free_hdd0, ((double) (copy_total_size + 0x40000000LL - free_hdd0)) / GIGABYTES);
        DrawDialogOK(filename);
        return;
    }

    if(abort_copy) //abort by user or got an error
    {
        if(DrawDialogYesNo(language[GAMECPYSL_GSIZEABCNTASK]) != 1)
        {
            return;
        }
        else
        {
            //old mode
            copy_total_size = 0;
        }
    }

    dialog_action = 0;
    abort_copy = 0;

    sprintf(filename, "/dev_usb/iris %s Iris Manager?\nVol: %1.2f MB",
                        language[GLUTIL_WTO],
                        ((double) copy_total_size)/(1024.0*1024));

    dialog_action = 0;

    msgDialogOpen2( mdialogyesno, filename, my_dialog, (void*)0x0000aaaa, NULL );

    wait_dialog();

    if(dialog_action == 1)
    {
        time_start = time(NULL);

        abort_copy = 0;
        DCls();
        file_counter = 0;
        new_pad = 0;

        DPrintf("%s %s\n %s %s\n\n", language[GAMECPYSL_STARTED], "USB", language[GLUTIL_WTO], "Iris Manager");

        copy_mode = 0;

        copy_is_split = 0;

        // sys8_perm_mode(1);
        my_game_copy((char *) name, (char *) path);
        // sys8_perm_mode(0);

        cls2();

        int seconds = (int) (time(NULL) - time_start);
        int vflip = 0;

        while(true)
        {
            if(abort_copy)
                sprintf(string1,"%s  %s: %2.2i:%2.2i:%2.2i\n", language[GLUTIL_ABORTED], language[GLUTIL_TIME],
                        seconds / 3600, (seconds/60) % 60, seconds % 60);
            else
                sprintf(string1,"%s  %s: %2.2i:%2.2i:%2.2i Vol: %1.2f GB\n", language[GAMECPYSL_DONE], language[GLUTIL_TIME],
                        seconds / 3600, (seconds/60) % 60, seconds % 60, ((double) global_device_bytes) / GIGABYTES);


            cls2();

            DbgHeader( string1);

            if(vflip & 32)
                DbgMess(language[GLUTIL_XEXIT]);
            else
                DbgMess("");

            vflip++;

            DbgDraw();

            tiny3d_Flip();

            if(ps3pad_poll())
            {
               new_pad = 0;
               break;
            }
        }
    }
}

u64 string_to_ull( char *string )
{
    u64 ull;
    ull = strtoull( (const char *)string, NULL, 16 );
    return ull;
}

void urldec(char *url)
{
	if(strchr(url, '%'))
	{
		u16 pos = 0; char c;
		for(u16 i = 0; url[i] >= ' '; i++, pos++)
		{
			if(url[i] == '+')
				url[pos] = ' ';
			else if(url[i] != '%')
				url[pos] = url[i];
			else
			{
				url[pos] = 0; u8 n = 2;
				while(n--)
				{
					url[pos] <<= 4, i++, c = (url[i] | 0x20);
					if(c >= '0' && c <= '9') url[pos] += c -'0';      else
					if(c >= 'a' && c <= 'f') url[pos] += c -'a' + 10;
				}
			}
		}
		url[pos] = 0;
	}
}
