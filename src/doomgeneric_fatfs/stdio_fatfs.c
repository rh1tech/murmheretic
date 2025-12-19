#include "ff.h"
#include <stdio.h>
#include <errno.h>
#include <string.h>

// Map FILE* to FIL* for FatFS
// We'll use a simple array of file handles
#define MAX_OPEN_FILES 8

typedef struct {
    FIL fil;
    int in_use;
} file_handle_t;

static file_handle_t file_handles[MAX_OPEN_FILES];

// Convert FIL* back to FILE* (just cast the address)
static FILE* fil_to_file(FIL *fil) {
    return (FILE*)fil;
}

// Convert FILE* to FIL*
static FIL* file_to_fil(FILE *fp) {
    return (FIL*)fp;
}

FILE *__wrap_fopen(const char *filename, const char *mode) {
    BYTE fatfs_mode = 0;
    FRESULT fr;
    int i;
    
    // Find free handle
    for (i = 0; i < MAX_OPEN_FILES; i++) {
        if (!file_handles[i].in_use) break;
    }
    
    if (i >= MAX_OPEN_FILES) {
        errno = ENOMEM;
        return NULL;
    }
    
    // Parse mode
    if (strchr(mode, 'r')) {
        fatfs_mode = FA_READ;
        if (strchr(mode, '+')) fatfs_mode |= FA_WRITE;
    } else if (strchr(mode, 'w')) {
        fatfs_mode = FA_WRITE | FA_CREATE_ALWAYS;
        if (strchr(mode, '+')) fatfs_mode |= FA_READ;
    } else if (strchr(mode, 'a')) {
        fatfs_mode = FA_WRITE | FA_OPEN_APPEND;
        if (strchr(mode, '+')) fatfs_mode |= FA_READ;
    }
    
    fr = f_open(&file_handles[i].fil, filename, fatfs_mode);
    
    if (fr != FR_OK) {
        errno = EIO;
        return NULL;
    }
    
    file_handles[i].in_use = 1;
    return fil_to_file(&file_handles[i].fil);
}

int __wrap_fclose(FILE *fp) {
    FIL *fil = file_to_fil(fp);
    int i;
    
    // Find the handle
    for (i = 0; i < MAX_OPEN_FILES; i++) {
        if (file_handles[i].in_use && &file_handles[i].fil == fil) {
            f_close(fil);
            file_handles[i].in_use = 0;
            return 0;
        }
    }
    
    return EOF;
}

size_t __wrap_fread(const void *ptr, size_t size, size_t nmemb, FILE *fp) {
    FIL *fil = file_to_fil(fp);
    UINT br;
    FRESULT fr;
    
    fr = f_read(fil, (void*)ptr, size * nmemb, &br);
    if (fr != FR_OK) return 0;
    
    return br / size;
}

int __wrap_fgetc(FILE *fp) {
    FIL *fil = file_to_fil(fp);
    UINT br;
    FRESULT fr;
    unsigned char c;
    
    fr = f_read(fil, &c, 1, &br);
    if (fr != FR_OK || br == 0) return EOF;
    
    return (int)c;
}

size_t __wrap_fwrite(const void *ptr, size_t size, size_t nmemb, FILE *fp) {
    FIL *fil = file_to_fil(fp);
    UINT bw;
    FRESULT fr;
    
    fr = f_write(fil, ptr, size * nmemb, &bw);
    if (fr != FR_OK) return 0;
    
    return bw / size;
}

int __wrap_fseek(FILE *fp, long offset, int whence) {
    FIL *fil = file_to_fil(fp);
    FSIZE_t pos;
    
    switch (whence) {
        case SEEK_SET:
            pos = offset;
            break;
        case SEEK_CUR:
            pos = f_tell(fil) + offset;
            break;
        case SEEK_END:
            pos = f_size(fil) + offset;
            break;
        default:
            return -1;
    }
    
    return (f_lseek(fil, pos) == FR_OK) ? 0 : -1;
}

long __wrap_ftell(FILE *fp) {
    FIL *fil = file_to_fil(fp);
    return (long)f_tell(fil);
}

int __wrap_remove(const char *filename) {
    FRESULT fr = f_unlink(filename);
    return (fr == FR_OK) ? 0 : -1;
}

int __wrap_rename(const char *oldname, const char *newname) {
    FRESULT fr = f_rename(oldname, newname);
    return (fr == FR_OK) ? 0 : -1;
}

// Initialize file handles
void stdio_fatfs_init(void) {
    int i;
    for (i = 0; i < MAX_OPEN_FILES; i++) {
        file_handles[i].in_use = 0;
    }
}
