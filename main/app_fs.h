#ifndef _APP_FS_H_
#define _APP_FS_H_

#include <ctype.h>

// fnmatch defines
#define	FNM_NOMATCH	1	// Match failed.
#define	FNM_NOESCAPE	0x01	// Disable backslash escaping.
#define	FNM_PATHNAME	0x02	// Slash must be matched by slash.
#define	FNM_PERIOD		0x04	// Period must be matched by period.
#define	FNM_LEADING_DIR	0x08	// Ignore /<tail> after Imatch.
#define	FNM_CASEFOLD	0x10	// Case insensitive search.
#define FNM_PREFIX_DIRS	0x20	// Directory prefixes of pattern match too.
#define	EOS	        '\0'


int app_fs_init();
void app_fs_writeTest(char *fname);
void app_fs_readTest(char *fname);
void app_fs_mkdirTest(char *dirname);
void app_fs_list(char *path, char *match);
int app_fs_writeFile(char *fname, char *mode, char *buf);
int app_fs_readFile(char *fname);
void app_fs_File_task_1(void* arg);
void app_fs_File_task_2(void* arg);
void app_fs_File_task_3(void* arg);
void app_fs_all_stuff_test();

#endif


