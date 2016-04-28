#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "FileManager.h"

#ifdef OS_IS_WINDOWS
#include <io.h>
#include <direct.h>
#define FD_ACCESS(p, d) _access(p, d)
#define MK_DIR(d) _mkdir(d)
#else
#include <unistd.h>
#define FD_ACCESS(p, d) access(p, d)
#define MK_DIR(d) mkdir(d, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH)
#endif

FILE* file_open(const char* filePath, const char* flags) {
    FILE *fd = fopen(filePath, flags);
    return fd;
}

int file_close(FILE* fd) {
    if (fd) {
        fclose(fd);
        fd = NULL;
        return 0;
    }

    return 1;
}

int file_get_contents(const char* filePath, unsigned char** buffer, size_t* size) {
    FILE *fd;
    long fsize;

    *size = 0;

    if (!filePath || *buffer) {
        return 1;
    }

    if ((fd = file_open(filePath, "rb")) == NULL) {
        return 2;
    }

    fseek(fd, 0, SEEK_END);
    fsize = ftell(fd);
    fseek(fd, 0, SEEK_SET);

    if (fsize == -1) {
        fclose(fd);
        return 3;
    }

    *buffer = malloc(fsize + 1);

    if (!*buffer) {
        fclose(fd);
        return 4;
    }

    if (fread(*buffer, fsize, 1, fd) != 1) {
        free(*buffer);
        *buffer = NULL;
        fclose(fd);
        return 5;
    }

    if (file_close(fd)) {
        free(*buffer);
        *buffer = NULL;

        return 6;
    }

    *size = (size_t)fsize;
    return 0;
}

int file_put_contents(const char* filePath, unsigned char* buffer, size_t size) {
    FILE *fd;
    size_t rc = 0;

    if (!filePath || !buffer || !size) {
        return 1;
    }

    if ((fd = file_open(filePath, "wb")) == NULL) {
        return 2;
    }

    if ((rc = fwrite(buffer, 1, size, fd)) == size) {
        rc = 0;
    } else {
        rc = 3;
    }

    if (file_close(fd)) {
        return 4;
    }

    return (int)rc;
}

/* getline() is not an ANSI C function, hence unreferenced on ARM and some compilers. */
size_t
fm_getline(char** dst, size_t *bytes, FILE *fd) {
    char *pRealloc = NULL;
    char currentChar = '\0';
    size_t charsRead = 0;
    int memAlloced = 0;

    if (!fd) {
        return 0;
    }

    if (*dst && *bytes<2) {
        return 0;
    }
    else if (!*dst) {
        *bytes = 64;
        *dst = malloc(*bytes * sizeof(char));
        memAlloced = 1;

        if (!*dst) {
            *bytes = 0;
            return 0;
        }
    }

    while ((currentChar = getc(fd)) && !feof(fd) && (currentChar != '\n')) {
        (*dst)[charsRead++] = currentChar;

        if (charsRead == *bytes) {
            if (!memAlloced) {
                charsRead -= 1;
                break;
            }

            *bytes += 64;
            pRealloc = realloc(*dst, *bytes * sizeof(char));

            if (pRealloc) {
                *dst = pRealloc;
            }
            else {
                if (*dst) {
                    free(*dst);
                }

                *bytes = 0;
                return 0;
            }
        }
    }

    (*dst)[charsRead] = '\0';
    *bytes = charsRead;

    return (*bytes * sizeof(char));
}

int file_exists(const char* filePath) {
    if (FD_ACCESS(filePath, 0) != -1) {
        return 1;
    }

    return 0;
}

int directory_exists(const char* filePath) {
    struct stat status;

    if (FD_ACCESS(filePath, 0) == 0) {
        stat(filePath, &status);
        return (status.st_mode & S_IFDIR) != 0;
    }

    return 0;
}

int create_directory(const char* directoryName) {
    if (!directoryName) {
        return -1;
    }

    return MK_DIR(directoryName);
}
