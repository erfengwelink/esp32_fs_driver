#include "app_fs.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

#include <errno.h>
#include <sys/fcntl.h>
#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include "esp_log.h"
#include "spiffs_vfs.h"

static const char tag[] = "[SPIFFS example]";

//-----------------------------------------------------------------------

int app_fs_init()
{
	vfs_spiffs_register();
	return 0;
}

static const char * rangematch(const char *pattern, char test, int flags)
{
  int negate, ok;
  char c, c2;

  /*
   * A bracket expression starting with an unquoted circumflex
   * character produces unspecified results (IEEE 1003.2-1992,
   * 3.13.2).  This implementation treats it like '!', for
   * consistency with the regular expression syntax.
   * J.T. Conklin (conklin@ngai.kaleida.com)
   */
  if ( (negate = (*pattern == '!' || *pattern == '^')) ) ++pattern;

  if (flags & FNM_CASEFOLD) test = tolower((unsigned char)test);

  for (ok = 0; (c = *pattern++) != ']';) {
    if (c == '\\' && !(flags & FNM_NOESCAPE)) c = *pattern++;
    if (c == EOS) return (NULL);

    if (flags & FNM_CASEFOLD) c = tolower((unsigned char)c);

    if (*pattern == '-' && (c2 = *(pattern+1)) != EOS && c2 != ']') {
      pattern += 2;
      if (c2 == '\\' && !(flags & FNM_NOESCAPE)) c2 = *pattern++;
      if (c2 == EOS) return (NULL);

      if (flags & FNM_CASEFOLD) c2 = tolower((unsigned char)c2);

      if ((unsigned char)c <= (unsigned char)test &&
          (unsigned char)test <= (unsigned char)c2) ok = 1;
    }
    else if (c == test) ok = 1;
  }
  return (ok == negate ? NULL : pattern);
}

//--------------------------------------------------------------------
static int fnmatch(const char *pattern, const char *string, int flags)
{
  const char *stringstart;
  char c, test;

  for (stringstart = string;;)
    switch (c = *pattern++) {
    case EOS:
      if ((flags & FNM_LEADING_DIR) && *string == '/') return (0);
      return (*string == EOS ? 0 : FNM_NOMATCH);
    case '?':
      if (*string == EOS) return (FNM_NOMATCH);
      if (*string == '/' && (flags & FNM_PATHNAME)) return (FNM_NOMATCH);
      if (*string == '.' && (flags & FNM_PERIOD) &&
          (string == stringstart ||
          ((flags & FNM_PATHNAME) && *(string - 1) == '/')))
              return (FNM_NOMATCH);
      ++string;
      break;
    case '*':
      c = *pattern;
      // Collapse multiple stars.
      while (c == '*') c = *++pattern;

      if (*string == '.' && (flags & FNM_PERIOD) &&
          (string == stringstart ||
          ((flags & FNM_PATHNAME) && *(string - 1) == '/')))
              return (FNM_NOMATCH);

      // Optimize for pattern with * at end or before /.
      if (c == EOS)
        if (flags & FNM_PATHNAME)
          return ((flags & FNM_LEADING_DIR) ||
                    strchr(string, '/') == NULL ?
                    0 : FNM_NOMATCH);
        else return (0);
      else if ((c == '/') && (flags & FNM_PATHNAME)) {
        if ((string = strchr(string, '/')) == NULL) return (FNM_NOMATCH);
        break;
      }

      // General case, use recursion.
      while ((test = *string) != EOS) {
        if (!fnmatch(pattern, string, flags & ~FNM_PERIOD)) return (0);
        if ((test == '/') && (flags & FNM_PATHNAME)) break;
        ++string;
      }
      return (FNM_NOMATCH);
    case '[':
      if (*string == EOS) return (FNM_NOMATCH);
      if ((*string == '/') && (flags & FNM_PATHNAME)) return (FNM_NOMATCH);
      if ((pattern = rangematch(pattern, *string, flags)) == NULL) return (FNM_NOMATCH);
      ++string;
      break;
    case '\\':
      if (!(flags & FNM_NOESCAPE)) {
        if ((c = *pattern++) == EOS) {
          c = '\\';
          --pattern;
        }
      }
      break;
      // FALLTHROUGH
    default:
      if (c == *string) {
      }
      else if ((flags & FNM_CASEFOLD) && (tolower((unsigned char)c) == tolower((unsigned char)*string))) {
      }
      else if ((flags & FNM_PREFIX_DIRS) && *string == EOS && ((c == '/' && string != stringstart) ||
    		  (string == stringstart+1 && *stringstart == '/')))
              return (0);
      else return (FNM_NOMATCH);
      string++;
      break;
    }
  // NOTREACHED
  return 0;
}

// ============================================================================

//-----------------------------------------
void app_fs_list(char *path, char *match) {

    DIR *dir = NULL;
    struct dirent *ent;
    char type;
    char size[9];
    char tpath[255];
    char tbuffer[80];
    struct stat sb;
    struct tm *tm_info;
    char *lpath = NULL;
    int statok;

    printf("LIST of DIR [%s]\r\n", path);
    // Open directory
    dir = opendir(path);
    if (!dir) {
        printf("Error opening directory\r\n");
        return;
    }

    // Read directory entries
    uint64_t total = 0;
    int nfiles = 0;
    printf("T  Size      Date/Time         Name\r\n");
    printf("-----------------------------------\r\n");
    while ((ent = readdir(dir)) != NULL) {
    	sprintf(tpath, path);
        if (path[strlen(path)-1] != '/') strcat(tpath,"/");
        strcat(tpath,ent->d_name);
        tbuffer[0] = '\0';

        if ((match == NULL) || (fnmatch(match, tpath, (FNM_PERIOD)) == 0)) {
			// Get file stat
			statok = stat(tpath, &sb);

			if (statok == 0) {
				tm_info = localtime(&sb.st_mtime);
				strftime(tbuffer, 80, "%d/%m/%Y %R", tm_info);
			}
			else sprintf(tbuffer, "                ");

			if (ent->d_type == DT_REG) {
				type = 'f';
				nfiles++;
				if (statok) strcpy(size, "       ?");
				else {
					total += sb.st_size;
					if (sb.st_size < (1024*1024)) sprintf(size,"%8d", (int)sb.st_size);
					else if ((sb.st_size/1024) < (1024*1024)) sprintf(size,"%6dKB", (int)(sb.st_size / 1024));
					else sprintf(size,"%6dMB", (int)(sb.st_size / (1024 * 1024)));
				}
			}
			else {
				type = 'd';
				strcpy(size, "       -");
			}

			printf("%c  %s  %s  %s\r\n",
				type,
				size,
				tbuffer,
				ent->d_name
			);
        }
    }
    if (total) {
        printf("-----------------------------------\r\n");
    	if (total < (1024*1024)) printf("   %8d", (int)total);
    	else if ((total/1024) < (1024*1024)) printf("   %6dKB", (int)(total / 1024));
    	else printf("   %6dMB", (int)(total / (1024 * 1024)));
    	printf(" in %d file(s)\r\n", nfiles);
    }
    printf("-----------------------------------\r\n");

    closedir(dir);

    free(lpath);

	uint32_t tot, used;
	spiffs_fs_stat(&tot, &used);
	printf("SPIFFS: free %d KB of %d KB\r\n", (tot-used) / 1024, tot / 1024);
}

//----------------------------------------------------
static int file_copy(const char *to, const char *from)
{
    FILE *fd_to;
    FILE *fd_from;
    char buf[1024];
    ssize_t nread;
    int saved_errno;

    fd_from = fopen(from, "rb");
    //fd_from = open(from, O_RDONLY);
    if (fd_from == NULL) return -1;

    fd_to = fopen(to, "wb");
    if (fd_to == NULL) goto out_error;

    while (nread = fread(buf, 1, sizeof(buf), fd_from), nread > 0) {
        char *out_ptr = buf;
        ssize_t nwritten;

        do {
            nwritten = fwrite(out_ptr, 1, nread, fd_to);

            if (nwritten >= 0) {
                nread -= nwritten;
                out_ptr += nwritten;
            }
            else if (errno != EINTR) goto out_error;
        } while (nread > 0);
    }

    if (nread == 0) {
        if (fclose(fd_to) < 0) {
            fd_to = NULL;
            goto out_error;
        }
        fclose(fd_from);

        // Success!
        return 0;
    }

  out_error:
    saved_errno = errno;

    fclose(fd_from);
    if (fd_to) fclose(fd_to);

    errno = saved_errno;
    return -1;
}

//--------------------------------
void app_fs_writeTest(char *fname)
{
	printf("==== Write to file \"%s\" ====\r\n", fname);

	int n, res, tot, len;
	char buf[40];

	FILE *fd = fopen(fname, "wb");
    if (fd == NULL) {
    	printf("     Error opening file\r\n");
    	return;
    }
    tot = 0;
    for (n = 1; n < 11; n++) {
    	sprintf(buf, "ESP32 spiffs write to file, line %d\r\n", n);
    	len = strlen(buf);
		res = fwrite(buf, 1, len, fd);
		if (res != len) {
	    	printf("     Error writing to file(%d <> %d\r\n", res, len);
	    	break;
		}
		tot += res;
    }
	printf("     %d bytes written\r\n", tot);
	res = fclose(fd);
	if (res) {
    	printf("     Error closing file\r\n");
	}
    printf("\r\n");
}

//-------------------------------
void app_fs_readTest(char *fname)
{
	printf("==== Reading from file \"%s\" ====\r\n", fname);

	int res;
	char *buf;
	buf = calloc(1024, 1);
	if (buf == NULL) {
    	printf("     Error allocating read buffer\"\r\n");
    	return;
	}

	FILE *fd = fopen(fname, "rb");
    if (fd == NULL) {
    	printf("     Error opening file\r\n");
    	free(buf);
    	return;
    }
    res = 999;
    res = fread(buf, 1, 1023, fd);
    if (res <= 0) {
    	printf("     Error reading from file\r\n");
    }
    else {
    	printf("     %d bytes read [\r\n", res);
        buf[res] = '\0';
        printf("%s\r\n]\r\n", buf);
    }
	free(buf);

	res = fclose(fd);
	if (res) {
    	printf("     Error closing file\r\n");
	}
    printf("\r\n");
}

//----------------------------------
void app_fs_mkdirTest(char *dirname)
{
	printf("==== Make new directory \"%s\" ====\r\n", dirname);

	int res;
	struct stat st = {0};
	char nname[80];

	if (stat(dirname, &st) == -1) {
	    res = mkdir(dirname, 0777);
	    if (res != 0) {
	    	printf("     Error creating directory (%d)\r\n", res);
	        printf("\r\n");
	        return;
	    }
    	printf("     Directory created\r\n\r\n");
		app_fs_list("/spiffs/", NULL);
		vTaskDelay(1000 / portTICK_RATE_MS);

    	printf("     Copy file from root to new directory...\r\n");
    	sprintf(nname, "%s/test.txt.copy", dirname);
    	res = file_copy(nname, "/spiffs/test.txt");
	    if (res != 0) {
	    	printf("     Error copying file (%d)\r\n", res);
	    }
    	printf("\r\n");
    	app_fs_list(dirname, NULL);
		vTaskDelay(1000 / portTICK_RATE_MS);

    	printf("     Removing file from new directory...\r\n");
	    res = remove(nname);
	    if (res != 0) {
	    	printf("     Error removing directory (%d)\r\n", res);
	    }
    	printf("\r\n");
    	app_fs_list(dirname, NULL);
		vTaskDelay(1000 / portTICK_RATE_MS);

    	printf("     Removing directory...\r\n");
	    res = remove(dirname);
	    if (res != 0) {
	    	printf("     Error removing directory (%d)\r\n", res);
	    }
    	printf("\r\n");
		app_fs_list("/spiffs/", NULL);
		vTaskDelay(1000 / portTICK_RATE_MS);
	}
	else {
		printf("     Directory already exists, removing\r\n");
	    res = remove(dirname);
	    if (res != 0) {
	    	printf("     Error removing directory (%d)\r\n", res);
	    }
	}

    printf("\r\n");
}
//
//------------------------------------------------------
int app_fs_writeFile(char *fname, char *mode, char *buf)
{
	FILE *fd = fopen(fname, mode);
    if (fd == NULL) {
        ESP_LOGE("[write]", "fopen failed");
    	return -1;
    }
    int len = strlen(buf);
	int res = fwrite(buf, 1, len, fd);
	if (res != len) {
        ESP_LOGE("[write]", "fwrite failed: %d <> %d ", res, len);
        res = fclose(fd);
        if (res) {
            ESP_LOGE("[write]", "fclose failed: %d", res);
            return -2;
        }
        return -3;
    }
	res = fclose(fd);
	if (res) {
        ESP_LOGE("[write]", "fclose failed: %d", res);
    	return -4;
	}
    return 0;
}

//------------------------------
int app_fs_readFile(char *fname)
{
    uint8_t buf[16];
	FILE *fd = fopen(fname, "rb");
    if (fd == NULL) {
        ESP_LOGE("[read]", "fopen failed");
        return -1;
    }
    int res = fread(buf, 1, 8, fd);
    if (res <= 0) {
        ESP_LOGE("[read]", "fread failed: %d", res);
        res = fclose(fd);
        if (res) {
            ESP_LOGE("[read]", "fclose failed: %d", res);
            return -2;
        }
        return -3;
    }
	res = fclose(fd);
	if (res) {
        ESP_LOGE("[read]", "fclose failed: %d", res);
    	return -4;
	}
    return 0;
}

//================================
void app_fs_File_task_1(void* arg)
{
    int res = 0;
    int n = 0;
    
    ESP_LOGI("[TASK_1]", "Started.");
    res = app_fs_writeFile("/spiffs/testfil1.txt", "wb", "1");
    if (res == 0) {
        while (n < 10000) {
            n++;
            res = app_fs_readFile("/spiffs/testfil1.txt");
            if (res != 0) {
                ESP_LOGE("[TASK_1]", "Error reading from file (%d), pass %d", res, n);
                break;
            }
            res = app_fs_writeFile("/spiffs/testfil1.txt", "a", "1");
            if (res != 0) {
                ESP_LOGE("[TASK_1]", "Error writing to file (%d), pass %d", res, n);
                break;
            }
            vTaskDelay(2);
            if ((n % 100) == 0) {
                ESP_LOGI("[TASK_1]", "%d reads/writes", n+1);
            }
        }
        if (n == 10000) ESP_LOGI("[TASK_1]", "Finished.");
    }
    else {
        ESP_LOGE("[TASK_1]", "Error creating file (%d)", res);
    }

    while (1) {
		vTaskDelay(1000 / portTICK_RATE_MS);
    }
}

//================================
void app_fs_File_task_2(void* arg)
{
    int res = 0;
    int n = 0;
    
    ESP_LOGI("[TASK_2]", "Started.");
    res = app_fs_writeFile("/spiffs/testfil2.txt", "wb", "2");
    if (res == 0) {
        while (n < 10000) {
            n++;
            res = app_fs_readFile("/spiffs/testfil2.txt");
            if (res != 0) {
                ESP_LOGE("[TASK_2]", "Error reading from file (%d), pass %d", res, n);
                break;
            }
            res = app_fs_writeFile("/spiffs/testfil2.txt", "a", "2");
            if (res != 0) {
                ESP_LOGE("[TASK_2]", "Error writing to file (%d), pass %d", res, n);
                break;
            }
            vTaskDelay(2);
            if ((n % 100) == 0) {
                ESP_LOGI("[TASK_2]", "%d reads/writes", n+1);
            }
        }
        if (n == 10000) ESP_LOGI("[TASK_2]", "Finished.");
    }
    else {
        ESP_LOGE("[TASK_2]", "Error creating file (%d)", res);
    }

    while (1) {
		vTaskDelay(1000 / portTICK_RATE_MS);
    }
}

//================================
void app_fs_File_task_3(void* arg)
{
    int res = 0;
    int n = 0;

    ESP_LOGI("[TASK_3]", "Started.");
    res = app_fs_writeFile("/spiffs/testfil3.txt", "wb", "3");
    if (res == 0) {
        while (n < 10000) {
            n++;
            res = app_fs_readFile("/spiffs/testfil3.txt");
            if (res != 0) {
                ESP_LOGE("[TASK_3]", "Error reading from file (%d), pass %d", res, n);
                break;
            }
            res = app_fs_writeFile("/spiffs/testfil3.txt", "a", "3");
            if (res != 0) {
                ESP_LOGE("[TASK_3]", "Error writing to file (%d), pass %d", res, n);
                break;
            }
            vTaskDelay(2);
            if ((n % 100) == 0) {
                ESP_LOGI("[TASK_3]", "%d reads/writes", n+1);
            }
        }
        if (n == 10000) ESP_LOGI("[TASK_3]", "Finished.");
    }
    else {
        ESP_LOGE("[TASK_3]", "Error creating file (%d)", res);
    }

    vTaskDelay(1000 / portTICK_RATE_MS);
    printf("\r\n");
	app_fs_list("/spiffs/", NULL);
    while (1) {
		vTaskDelay(1000 / portTICK_RATE_MS);
    }
}

void app_fs_all_stuff_test()
{
	app_fs_init();
	if (spiffs_is_mounted) {
		vTaskDelay(2000 / portTICK_RATE_MS);

		app_fs_writeTest("/spiffs/test.txt");
		app_fs_readTest("/spiffs/test.txt");
		app_fs_readTest("/spiffs/spiffs.info");

		app_fs_list("/spiffs/", NULL);
	    printf("\r\n");

		app_fs_mkdirTest("/spiffs/newdir");

		printf("==== List content of the directory \"images\" ====\r\n\r\n");
        app_fs_list("/spiffs/images", NULL);
	    printf("\r\n");
    }
	
	ESP_LOGI(tag, "=================================================");
    ESP_LOGI(tag, "STARTING MULTITASK TEST (3 tasks created)");
    ESP_LOGI(tag, "Each task will perform 1000 read&write operations");
    ESP_LOGI(tag, "Expected run time ~3 minutes");
    ESP_LOGI(tag, "=================================================\r\n");

    xTaskCreatePinnedToCore(app_fs_File_task_1, "app_fs_FileTask1", 10*1024, NULL, 5, NULL, 1);
    vTaskDelay(1000 / portTICK_RATE_MS);
    xTaskCreatePinnedToCore(app_fs_File_task_2, "app_fs_FileTask2", 10*1024, NULL, 5, NULL, 1);
    vTaskDelay(1000 / portTICK_RATE_MS);
    xTaskCreatePinnedToCore(app_fs_File_task_3, "app_fs_FileTask3", 10*1024, NULL, 5, NULL, 1);

    while (1) {
		vTaskDelay(1000 / portTICK_RATE_MS);
    }
}
//XFS -f app_fs_all_stuff_test -l 0

