#ifndef FILE_MANAGER_H
#define FILE_MANAGER_H

#if defined(_WIN32) || defined(_WIN64)
#define FS_DIR_CHAR '\\'
#ifndef OS_IS_WINDOWS
#define OS_IS_WINDOWS
#endif
#else
#define FS_DIR_CHAR '/'
#ifndef OS_IS_LINUX
#define OS_IS_LINUX
#endif
#endif

#include <stdio.h>

FILE* file_open(const char* filePath, const char* flags);
size_t fm_getline(char** dst, size_t *bytes, FILE *fd);
int file_close(FILE* fd);
int file_get_contents(const char* filePath, unsigned char** buffer, size_t* size);
int file_put_contents(const char* filePath, unsigned char* buffer, size_t size);

int file_exists(const char* filePath);
int directory_exists(const char* filePath);
int directory_scan(const char* dirName, const char* contents, size_t* size);
int create_directory(const char* directoryName);

#endif
