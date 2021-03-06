#include <stdio.h>
#include <string.h>
#include <lv2/sysfs.h>
#include <sys/file.h>
#include "ntfs.h"
#include "cobra.h"
#include "scsi.h"
#include <dirent.h>
#include <unistd.h>
#include <sys/process.h>

#include <io/pad.h>

#include "iso.h"
#include "file.h"
#include "firmware.h"
#include "fix_game.h"
#include "net.h"

//----------------
#define SUFIX(a)	((a==1)? "_1" :(a==2)? "_2" :(a==3)? "_3" :(a==4)? "_4" :(a==5)? " [auto]" :"")
#define SUFIX2(a)	((a==1)?" (1)":(a==2)?" (2)":(a==3)?" (3)":(a==4)?" (4)":(a==5)? " [auto]" :"")

#define USB_MASS_STORAGE_1(n)	(0x10300000000000AULL+(n)) /* For 0-5 */
#define USB_MASS_STORAGE_2(n)	(0x10300000000001FULL+((n)-6)) /* For 6-127 */
#define USB_MASS_STORAGE(n)	(((n) < 6) ? USB_MASS_STORAGE_1(n) : USB_MASS_STORAGE_2(n))

enum emu_modes
{
	PS3ISO = 0,
	BDISO  = 1,
	DVDISO = 2,
	PSXISO = 3,
};

typedef struct
{
	uint64_t device;
	uint32_t emu_mode;
	uint32_t num_sections;
	uint32_t num_tracks;
} __attribute__((packed)) rawseciso_args;

#define cue_buf  plugin_args
#define PLUGIN_ARGS_SIZE	0x10000
#define MAX_SECTIONS		((PLUGIN_ARGS_SIZE-sizeof(rawseciso_args))/8)

uint8_t plugin_args[PLUGIN_ARGS_SIZE];

#define MAX_PATH_LEN  0x420

char path[MAX_PATH_LEN];
char wm_path[MAX_PATH_LEN];
char image_file[MAX_PATH_LEN];

uint32_t sections[MAX_SECTIONS], sections_size[MAX_SECTIONS];
ntfs_md *mounts;
int mountCount;

#include "fake_iso.h"

//----------------

int main(int argc, const char* argv[])
{
	detect_firmware();

	int i, parts;
	unsigned int num_tracks;
	u8 cue=0; int ext_len = 4;

	int emu_mode;
	TrackDef tracks[100];
	ScsiTrackDescriptor *scsi_tracks;

	rawseciso_args *p_args;

	sysFSDirent dir;
	DIR_ITER *pdir = NULL, *psubdir= NULL;
	struct stat st;
	char c_path[10][16]={"PS3ISO", "BDISO", "DVDISO", "PSXISO", "VIDEO", "MOVIES", "PKG", "Packages", "packages", "BDFILE"};
	char cover_ext[4][8]={".jpg", ".png", ".PNG", ".JPG"};

	sysLv2FsUnlink((char*)"/dev_hdd0/tmp/wmtmp/games.html");

	int fd=-1;
	u64 read = 0;
	char path0[MAX_PATH_LEN], subpath[MAX_PATH_LEN];
	char direntry[MAX_PATH_LEN];
	char filename[MAX_PATH_LEN];
	bool is_iso = false;

	bool mmCM_found = false; char mmCM_cache[64], mmCM_path[64], titleID[16];

	// detect if multiMAN is installed
	sprintf(mmCM_cache, "%s", "/dev_hdd0/game/BLES80608/USRDIR/cache");
	if(file_exists((char*)"/dev_hdd0/game/BLES80608/USRDIR/EBOOT.BIN")) {mmCM_found=true; mkdir(mmCM_cache, S_IRWXO | S_IRWXU | S_IRWXG | S_IFDIR);}
	else
	{sprintf(mmCM_cache, "%s", "/dev_hdd0/game/NPEA00374/USRDIR/cache");
	if(file_exists((char*)"/dev_hdd0/game/NPEA00374/USRDIR/EBOOT.BIN")) {mmCM_found=true; mkdir(mmCM_cache, S_IRWXO | S_IRWXU | S_IRWXG | S_IFDIR);}
	else
	{sprintf(mmCM_cache, "%s", "/dev_hdd0/tmp/game_repo/main/cache");
	if(file_exists((char*)"/dev_hdd0/tmp/game_repo/main/EBOOT.BIN"))    {mmCM_found=true; mkdir(mmCM_cache, S_IRWXO | S_IRWXU | S_IRWXG | S_IFDIR);}
	}
	}

	// create cache folder
	snprintf(path, sizeof(path), "/dev_hdd0/tmp/wmtmp");
	mkdir(path, S_IRWXO | S_IRWXU | S_IRWXG | S_IFDIR);
	sysFsChmod(path, S_IFDIR | 0777);

//--- hold CROSS to keep previous cached files
	unsigned button = 0;

	padInfo padinfo;
	padData paddata;

	ioPadInit(7);

	int n, r;
	for(r = 0; r < 10; r++)
	{
		ioPadGetInfo(&padinfo);
		for(n = 0; n < 7; n++)
		{
			if(padinfo.status[n])
			{
				ioPadGetData(n, &paddata);
				button = (paddata.button[2] << 8) | (paddata.button[3] & 0xff);
				if (paddata.len > 0) break;
			}
		}
		if(button) break; else usleep(20000);
	}
	ioPadEnd();

	if(button) ; // skip removal of .ntfs[ files
	else
//---

	{
		sysLv2FsOpenDir(path, &fd);
		if(fd>=0)
		{
			while(!sysLv2FsReadDir(fd, &dir, &read) && read)
				if(strstr(dir.d_name, ".ntfs[") || (strlen(dir.d_name)>4 && strstr(dir.d_name + strlen(dir.d_name) - 4, ".iso"))) {sprintf(path0, "%s/%s", path, dir.d_name); sysLv2FsUnlink(path0);}
			sysLv2FsCloseDir(fd);
		}
    }

	mountCount = ntfsMountAll(&mounts, NTFS_DEFAULT | NTFS_RECOVER /* | NTFS_READ_ONLY */ );
	if (mountCount <= 0) goto exit;

	for (u8 profile = 0; profile < 6; profile++)
	{
		for (i = 0; i < mountCount; i++)
		{
			if (strncmp(mounts[i].name, "ntfs", 4) == 0 || strncmp(mounts[i].name, "ext", 3) == 0)
			{
				for(u8 m = 0; m < 10; m++) //0="PS3ISO", 1="BDISO", 2="DVDISO", 3="PSXISO", 4="VIDEO", 5="MOVIES", 6="PKG", 7="Packages", 8="packages", 9="BDFILE"
				{
					bool has_dirs; u16 flen; has_dirs = false;

					snprintf(path, sizeof(path), "%s:/%s%s", mounts[i].name, c_path[m], SUFIX(profile));

					pdir = ps3ntfs_diropen(path);
					if(pdir!=NULL)
					{
						while(ps3ntfs_dirnext(pdir, dir.d_name, &st) == 0)
						{
							sprintf(filename, "%s", dir.d_name);

							ext_len = 4;
							flen = strlen(filename); if(flen < ext_len) continue;

							//--- create .ntfs[BDFILES]
							if(m >= 4)
							{
								if((m==4 || m==5) && !strcasestr(".mp4|.mkv|.avi|.wmv|.flv|.mpg|mpeg|.mov|m2ts|.vob|.asf|divx|xvid|.pam|.bik|bink|.vp6|.mth|.3gp|rmvb|.ogm|.ogv|.m2t|.mts|.tsv|.tsa|.tts|.vp3|.vp5|.vp8|.264|.m1v|.m2v|.m4b|.m4p|.m4r|.m4v|mp4v|.mpe|bdmv|.dvb|webm|.nsv", filename + flen - ext_len)) continue; else
								if((m>=6 && m<=8) && !strstr(".pkg", filename + flen - ext_len)) continue;
								if((m==9)         && (dir.d_name[0]=='.' || strstr(dir.d_name, ".")==NULL)) continue;

								sprintf(filename, "/dev_hdd0/tmp/wmtmp/[%s] %s.iso", c_path[m], dir.d_name);
								if(file_exists(filename)) continue;

								snprintf(path, sizeof(path), "%s:/%s%s/%s", mounts[i].name, c_path[m], SUFIX(profile), dir.d_name);

								build_fake_iso(filename, path, (mounts[i].interface->ioType & 0xff) - '0');
								continue;
							}
							//---------------

							//--- is ISO?
							is_iso =	( (strcasestr(filename + flen - ext_len, ".iso")) ) ||
							  (m>0 && ( ( (strcasestr(filename + flen - ext_len, ".bin")) ) ||
										( (strcasestr(filename + flen - ext_len, ".img")) ) ||
										( (strcasestr(filename + flen - ext_len, ".mdf")) ) ));

							if(!is_iso) {ext_len = 6; is_iso = (flen >= ext_len && strcasestr(filename + flen - ext_len, ".iso.0"));}

////////////////////////////////////////////////////////
							sprintf(direntry, "%s", dir.d_name);

							//--- is SUBFOLDER?
							if(!is_iso)
							{
								sprintf(subpath, "%s:/%s%s/%s", mounts[i].name, c_path[m], SUFIX(profile), dir.d_name);
								psubdir = ps3ntfs_diropen(subpath);
								if(psubdir==NULL) continue;
								sprintf(subpath, "%s", filename); has_dirs=true;
next_ntfs_entry:
								if(ps3ntfs_dirnext(psubdir, dir.d_name, &st) < 0) {has_dirs=false; continue;}
								if(dir.d_name[0]=='.') goto next_ntfs_entry;

								sprintf(direntry, "%s/%s", subpath, dir.d_name);
								sprintf(filename, "[%s] %s", subpath, dir.d_name);

								ext_len = 4;
								flen = strlen(filename); if(flen < ext_len) goto next_ntfs_entry;

								is_iso =	( (strcasestr(filename + flen - ext_len, ".iso")) ) ||
								  (m>0 && ( ( (strcasestr(filename + flen - ext_len, ".bin")) ) ||
											( (strcasestr(filename + flen - ext_len, ".img")) ) ||
											( (strcasestr(filename + flen - ext_len, ".mdf")) ) ));

								if(!is_iso) {ext_len = 6; is_iso = (flen >= ext_len && strcasestr(filename + flen - ext_len, ".iso.0"));}
							}
////////////////////////////////////////////////////////

							//--- cache ISO
							if( is_iso )
							{
								filename[flen - ext_len]=0;
								snprintf(path, sizeof(path), "%s:/%s%s/%s", mounts[i].name, c_path[m], SUFIX(profile), direntry);

								//--- PS3ISO: fix game, cache SFO, ICON0 and PIC1 (if mmCM is installed)
								if(m == PS3ISO)
								{
									titleID[0]=0;
									sprintf(wm_path, "/dev_hdd0/tmp/wmtmp/%s.SFO", filename);
									if(file_exists(wm_path)==false)
										ExtractFileFromISO(path, "/PS3_GAME/PARAM.SFO;1", wm_path);

									if(c_firmware < 4.78f && need_fix(wm_path))
									{
										fix_iso(path, 0x100000UL);

										// refresh PARAM.SFO
										sysLv2FsUnlink(wm_path);
										ExtractFileFromISO(path, "/PS3_GAME/PARAM.SFO;1", wm_path);
									}

									if(mmCM_found && file_exists(wm_path))
									{
										get_titleid(wm_path, titleID);
										sprintf(mmCM_path, "%s/%s.SFO", mmCM_cache, titleID);
										if(file_exists(mmCM_path)==false)
											sysLv2FsLink(wm_path, mmCM_path);
									}

									sprintf(wm_path, "/dev_hdd0/tmp/wmtmp/%s.PNG", filename);
									if(file_exists(wm_path)==false)
										ExtractFileFromISO(path, "/PS3_GAME/ICON0.PNG;1", wm_path);

									if(mmCM_found && titleID[0]>' ' && file_exists(wm_path))
									{
										sprintf(mmCM_path, "%s/%s_320.PNG", mmCM_cache, titleID);
										if(file_exists(mmCM_path)==false)
											sysLv2FsLink(wm_path, mmCM_path);

										sprintf(mmCM_path, "%s/%s_1920.PNG", mmCM_cache, titleID);
										if(file_exists(mmCM_path)==false)
											ExtractFileFromISO(path, "/PS3_GAME/PIC1.PNG;1", mmCM_path);
									}
								}
								else
								{
									// cache cover image for BDISO, DVDISO, PSXISO
									sprintf(image_file, "%s", path);
									int e, plen = strlen(image_file) - ext_len;

									for(e = 0; e < 4; e++)
									{
										image_file[plen]=0; strcat(image_file, cover_ext[e]);
										if(file_exists(image_file)) break;
									}

									if(e < 4)
									{
										sprintf(wm_path, "/dev_hdd0/tmp/wmtmp/%s%s", filename, cover_ext[e]);
										if(file_exists(wm_path) == false)
											copy_file(image_file, wm_path);
									}
								}

								// get file sectors
								parts = ps3ntfs_file_to_sectors(path, sections, sections_size, MAX_SECTIONS, 1);

								// get multi-part file sectors
								if(ext_len == 6)
								{
									char iso_name[MAX_PATH_LEN], iso_path[MAX_PATH_LEN];

									sprintf(iso_name, "%s", path);
									iso_name[strlen(iso_name) - 1] = 0;

									for (u8 o = 1; o < 64; o++)
									{
										if(parts >= MAX_SECTIONS) break;

										sprintf(iso_path, "%s%i", iso_name, o);
										if(file_exists(iso_path) == false) break;

										parts += ps3ntfs_file_to_sectors(iso_path, sections + parts, sections_size + parts, MAX_SECTIONS - parts, 1);
									}
								}

								if (parts >= MAX_SECTIONS) continue;

								if (parts > 0)
								{
									int cd_sector_size; cd_sector_size = 2352;

									num_tracks = 1;
									if(m==PS3ISO) emu_mode = EMU_PS3; else
									if(m==BDISO ) emu_mode = EMU_BD;  else
									if(m==DVDISO) emu_mode = EMU_DVD; else
									if(m==PSXISO)
									{
										emu_mode = EMU_PSX;
										cue=0;
										int fd;

										// detect CD sector size
										fd = ps3ntfs_open(path, O_RDONLY, 0);
										if(fd >= 0)
										{
											char buffer[0x10]; buffer[0xD] = 0;
											ps3ntfs_seek64(fd, 0x9320LL, SEEK_SET); ps3ntfs_read(fd, (void *)buffer, 0xC); if(memcmp(buffer, "PLAYSTATION ", 0xC)==0) cd_sector_size = 2352; else {
											ps3ntfs_seek64(fd, 0x8020LL, SEEK_SET); ps3ntfs_read(fd, (void *)buffer, 0xC); if(memcmp(buffer, "PLAYSTATION ", 0xC)==0) cd_sector_size = 2048; else {
											ps3ntfs_seek64(fd, 0x9220LL, SEEK_SET); ps3ntfs_read(fd, (void *)buffer, 0xC); if(memcmp(buffer, "PLAYSTATION ", 0xC)==0) cd_sector_size = 2336; else {
											ps3ntfs_seek64(fd, 0x9920LL, SEEK_SET); ps3ntfs_read(fd, (void *)buffer, 0xC); if(memcmp(buffer, "PLAYSTATION ", 0xC)==0) cd_sector_size = 2448; }}}
											ps3ntfs_close(fd);
										}

										// parse CUE file
										path[strlen(path)-3]='C'; path[strlen(path)-2]='U'; path[strlen(path)-1]='E';
										fd = ps3ntfs_open(path, O_RDONLY, 0);
										if(fd<0)
										{
											path[strlen(path)-3]='c'; path[strlen(path)-2]='u'; path[strlen(path)-1]='e';
											fd = ps3ntfs_open(path, O_RDONLY, 0);
										}

										if (fd >= 0)
										{
											int r = ps3ntfs_read(fd, (char *)cue_buf, sizeof(cue_buf));
											ps3ntfs_close(fd);

											if (r > 0)
											{
												char dummy[64];

												if (cobra_parse_cue(cue_buf, r, tracks, 100, &num_tracks, dummy, sizeof(dummy)-1) != 0)
												{
													num_tracks=1;
													cue=0;
												}
												else
													cue=1;
											}
										}
									}

									//--- build .ntfs[ file
									p_args = (rawseciso_args *)plugin_args; memset(p_args, 0, PLUGIN_ARGS_SIZE);
									p_args->device = USB_MASS_STORAGE((mounts[i].interface->ioType & 0xff) - '0');
									p_args->emu_mode = emu_mode;
									p_args->num_sections = parts;

									memcpy(plugin_args+sizeof(rawseciso_args), sections, parts*sizeof(uint32_t));
									memcpy(plugin_args+sizeof(rawseciso_args)+(parts*sizeof(uint32_t)), sections_size, parts*sizeof(uint32_t));

									if (emu_mode == EMU_PSX)
									{
										int max = MAX_SECTIONS - ((num_tracks * sizeof(ScsiTrackDescriptor)) / 8);

										if (parts == max) continue;

										if(cd_sector_size == 2352)
											p_args->num_tracks = num_tracks;
										else
											p_args->num_tracks = num_tracks | (cd_sector_size<<4);

										scsi_tracks = (ScsiTrackDescriptor *)(plugin_args+sizeof(rawseciso_args)+(2*parts*sizeof(uint32_t)));

										if (!cue)
										{
											scsi_tracks[0].adr_control = 0x14;
											scsi_tracks[0].track_number = 1;
											scsi_tracks[0].track_start_addr = 0;
										}
										else
										{
											for (u8 j = 0; j < num_tracks; j++)
											{
												scsi_tracks[j].adr_control = (tracks[j].is_audio) ? 0x10 : 0x14;
												scsi_tracks[j].track_number = j + 1;
												scsi_tracks[j].track_start_addr = tracks[j].lba;
											}
										}
									}

									//--- write .ntfs[ file
									FILE *flistW;
									snprintf(path, sizeof(path), "/dev_hdd0/tmp/wmtmp/%s%s.ntfs[%s]", filename, SUFIX2(profile), c_path[m]);
									flistW = fopen(path, "wb");
									if(flistW!=NULL)
									{
										fwrite(plugin_args, sizeof(plugin_args), 1, flistW);
										fclose(flistW);
										sysFsChmod(path, 0666);
									}
								}
							}
//////////////////////////////////////////////////////////////
							if(has_dirs) goto next_ntfs_entry;
//////////////////////////////////////////////////////////////
						}
						ps3ntfs_dirclose(pdir);
					}
				}
			}
		}
	}

exit:
	//--- Unmount ntfs devices
	for (u8 u = 0; u < mountCount; u++) ntfsUnmount(mounts[u].name, 1);

	//--- Force refresh xml (webMAN)
	int refresh_xml=-1;
	refresh_xml=connect_to_webman();
	if(refresh_xml>=0) ssend(refresh_xml, "GET /refresh.ps3 HTTP/1.0\r\n");

	//--- Launch RELOAD.SELF
	char self_path[1024] = "";
	if(argc > 0 && argv)
	{
		if(!strncmp(argv[0], "/dev_hdd0/game/", 15))
		{
			int n;

			strcpy(self_path, argv[0]);

			n = 15; while(self_path[n] != '/' && self_path[n] != 0) n++;

			if(self_path[n] == '/') self_path[n] = 0;

			strcat(self_path, "/USRDIR/RELOAD.SELF");
		}
	}

	if(file_exists(self_path))
		sysProcessExitSpawn2(self_path, NULL, NULL, NULL, 0, 1001, SYS_PROCESS_SPAWN_STACK_SIZE_1M);

	return 0;
}
