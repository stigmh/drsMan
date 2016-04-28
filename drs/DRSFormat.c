#include <stdlib.h>
#include <string.h>

#include "FileManager.h"
#include "DRSFormat.h"

int is_readable_ascii_char(char c) {
    return ('~' >= c) && (c >= ' ');
}

int dsr_load(const char* filePath, dsr_t* dsr) {
    int idx;
    int iidx;
    size_t fileOffset = 0;
    size_t ffOffset = 0;
    unsigned char* fileBuffer = NULL;
    dsrTable_t *dsrTable = NULL;
    int rc = 0;

    if (!dsr) {
        return 1;
    }

    /* Retrieve file contents */
    if ((rc = file_get_contents(filePath, &fileBuffer, &dsr->fileSize))) {
        return rc + 1;
    }

    if (!fileBuffer || dsr->fileSize <= sizeof(dsrHeader_t)) {
        if (fileBuffer) {
            free(fileBuffer);
            fileBuffer = NULL;
        }
        return 7;
    }

    /***********************
     * Retrieve DSR header *
     ***********************/

    dsr->tables = NULL;

    /**
     * Copy valid data from copyright string. First 40 bytes in file is reserved
     * for copyright data. It is not null terminated in file.
     **/
    while (is_readable_ascii_char(fileBuffer[fileOffset]) && fileOffset < DSR_HDR_COPYRIGHT_LENGTH) {
        dsr->header.copyright[fileOffset] = fileBuffer[fileOffset];
        ++fileOffset;
    }

    /* zero terminate copyright string */
    dsr->header.copyright[fileOffset] = '\0';
    fileOffset = DSR_HDR_COPYRIGHT_LENGTH;

    /* Retrieve DSR version, 4 bytes, not null terminated. int hack to save copy ops. */
    dsr->header.version[0] = fileBuffer[fileOffset++];
    dsr->header.version[1] = fileBuffer[fileOffset++];
    dsr->header.version[2] = fileBuffer[fileOffset++];
    dsr->header.version[3] = fileBuffer[fileOffset++];
    dsr->header.version[4] = '\0';

    /* Retrieve DSR archive type, 12 bytes, not null terminated */
    idx = 0;
    while (is_readable_ascii_char(fileBuffer[idx+fileOffset]) && idx < DSR_HDR_TYPE_LENGTH) {
        dsr->header.type[idx] = fileBuffer[idx+fileOffset];
        ++idx;
    }
    
    dsr->header.type[idx] = '\0';
    fileOffset += DSR_HDR_TYPE_LENGTH;

    /* Retrieve table count and offset */
    dsr->header.tableCount = *(int*)&fileBuffer[fileOffset];
    fileOffset += sizeof(int);
    dsr->header.offset = *(int*)&fileBuffer[fileOffset];
    fileOffset += sizeof(int);

    /***********************
    *  Retrieve DSR tables *
    ************************/
  
    if (!(dsr->tables = malloc(sizeof(dsrTable_t) * dsr->header.tableCount))) {
        free(fileBuffer);
        fileBuffer = NULL;
        return 8;
    }

    for (idx = 0; idx < dsr->header.tableCount; ++idx) {
        dsrTable = &dsr->tables[idx];
        dsrTable->files = NULL;

        /* Retrieve file type */
        dsrTable->header.fileType = fileBuffer[fileOffset++];

        /* Retrieve file extension (reversed in file) */
        dsrTable->header.extension[0] = fileBuffer[fileOffset + 2];
        dsrTable->header.extension[1] = fileBuffer[fileOffset + 1];
        dsrTable->header.extension[2] = fileBuffer[fileOffset + 0];
        dsrTable->header.extension[3] = '\0';
        fileOffset += DSR_TABLE_HDR_EXT_LENGTH;

        /* Retrieve offset and file count*/
        dsrTable->header.offset = *(int*)&fileBuffer[fileOffset];
        fileOffset += sizeof(int);
        dsrTable->header.fileCount = *(int*)&fileBuffer[fileOffset];
        fileOffset += sizeof(int);

        /* Allocate file headers */
        dsrTable->files = malloc(dsrTable->header.fileCount * sizeof(dsrFile_t));

        if (!dsrTable->files) {
            for (iidx = 0; iidx < idx; ++iidx) {
                if (dsr->tables[iidx].files) {
                    free(dsr->tables[iidx].files);
                    dsr->tables[iidx].files = NULL;
                }
            }

            free(dsr->tables);
            dsr->tables = NULL;
            free(fileBuffer);
            return 9;
        }

        /* Retrieve file header data */
        ffOffset = dsrTable->header.offset;

        for (iidx = 0; iidx < dsrTable->header.fileCount; ++iidx) {
            dsrTable->files[iidx].id = *(int*)&fileBuffer[ffOffset];
            ffOffset += sizeof(int);
            dsrTable->files[iidx].offset = *(int*)&fileBuffer[ffOffset];
            ffOffset += sizeof(int);
            dsrTable->files[iidx].size = *(int*)&fileBuffer[ffOffset];
            ffOffset += sizeof(int);

            if ((dsrTable->files[iidx].data = malloc(dsrTable->files[iidx].size)) != NULL) {
                memcpy(dsrTable->files[iidx].data, &fileBuffer[dsrTable->files[iidx].offset], dsrTable->files[iidx].size);
            } else {
                dsrTable->files[iidx].size = 0;
            }
        }
    }

    free(fileBuffer);
    return rc;
}

void dsr_free(dsr_t* dsr) {
    int i;
    int ii;

    if (dsr) {
        if (dsr->tables) {
            for (i = 0; i < dsr->header.tableCount; ++i) {
                if (dsr->tables->files) {
                    for (ii = 0; ii < dsr->tables->header.fileCount; ++ii) {
                        if (dsr->tables->files[ii].data) {
                            free(dsr->tables->files[ii].data);
                            dsr->tables->files[ii].data = NULL;
                            dsr->tables->files[ii].size = 0;
                        }
                    }

                    free(dsr->tables->files);
                    dsr->tables->files = NULL;
                }
            }

            free(dsr->tables);
            dsr->tables = NULL;
        }
    }
}

int dsr_create_archive(dsr_t* dsr, const char* output) {
    unsigned char* buffer;
    size_t bufferOffset;
    int totalSize;
    int fSz = 0;
    int i;
    int ii;

    if (!dsr || !output || !dsr->tables) {
        return 1;
    }

    /* Calculate size of header */
    totalSize = DSR_HDR_COPYRIGHT_LENGTH + DSR_HDR_VERSION_LENGTH +
        DSR_HDR_TYPE_LENGTH + 2*sizeof(int);

    /* Calculate size of table headers */
    totalSize += dsr->header.tableCount *
        ((sizeof(char) * (DSR_TABLE_HDR_EXT_LENGTH+1)) + (2 * sizeof(int)));

    /* Calcluate size of file headers and files */
    for (i = 0; i < dsr->header.tableCount; ++i) {
        for (ii = 0; ii < dsr->tables[i].header.fileCount; ++ii) {
            totalSize += (3*sizeof(int));
            
            if (dsr->tables[i].files) {
                totalSize += dsr->tables[i].files[ii].size;
                fSz += dsr->tables[i].files[ii].size;
            }
        }
    }

    if (totalSize != dsr->fileSize) {
        fprintf(stderr, "DSR file size mismatched. Expected %d (0x%X), got %zu (0x%zX) - %d.\n",
                totalSize, totalSize, dsr->fileSize, dsr->fileSize, fSz);
        return 2;
    }

    if ((buffer = malloc(totalSize)) == NULL) {
        return 3;
    }

    /* Copy header */
    memcpy(buffer, &dsr->header.copyright, DSR_HDR_COPYRIGHT_LENGTH);
    bufferOffset = DSR_HDR_COPYRIGHT_LENGTH;
    memcpy(&buffer[bufferOffset], &dsr->header.version, DSR_HDR_VERSION_LENGTH);
    bufferOffset += DSR_HDR_VERSION_LENGTH;
    memcpy(&buffer[bufferOffset], &dsr->header.type, DSR_HDR_TYPE_LENGTH);
    bufferOffset += DSR_HDR_TYPE_LENGTH;
    memcpy(&buffer[bufferOffset], &dsr->header.tableCount, sizeof(int));
    bufferOffset += sizeof(int);
    memcpy(&buffer[bufferOffset], &dsr->header.offset, sizeof(int));
    bufferOffset += sizeof(int);

    /* Copy out table headers */
    for (i = 0; i < dsr->header.tableCount; ++i) {
        /* Type */
        buffer[bufferOffset++] = dsr->tables[i].header.fileType;

        /* Extension (reversed)  */
        buffer[bufferOffset++] = dsr->tables[i].header.extension[2]; 
        buffer[bufferOffset++] = dsr->tables[i].header.extension[1];
        buffer[bufferOffset++] = dsr->tables[i].header.extension[0];

        /* Offset and file count */
        memcpy(&buffer[bufferOffset], &dsr->tables[i].header.offset, sizeof(int));
        bufferOffset += sizeof(int);
        memcpy(&buffer[bufferOffset], &dsr->tables[i].header.fileCount, sizeof(int));
        bufferOffset += sizeof(int);
    }

    if (bufferOffset != dsr->tables[0].header.offset) {
        fprintf(stderr, "DSR table offset mismatched. Expected 0x%08X, got 0x%08X\n.",
                (unsigned int)bufferOffset, dsr->tables[0].header.offset); 
        free(buffer);
        buffer = NULL;
        return 4;
    }

    /* Copy out file headers */
    for (i = 0; i < dsr->header.tableCount; ++i) {
        for (ii = 0; ii < dsr->tables[i].header.fileCount; ++ii) {
            memcpy(&buffer[bufferOffset], &dsr->tables[i].files[ii].id, sizeof(int));
            bufferOffset += sizeof(int);
            memcpy(&buffer[bufferOffset], &dsr->tables[i].files[ii].offset, sizeof(int));
            bufferOffset += sizeof(int);
            memcpy(&buffer[bufferOffset], &dsr->tables[i].files[ii].size, sizeof(int));
            bufferOffset += sizeof(int);
        }
    }

    /* Copy out raw file data */
    for (i = 0; i < dsr->header.tableCount; ++i) {
        for (ii = 0; ii < dsr->tables[i].header.fileCount; ++ii) {
            memcpy(&buffer[bufferOffset], dsr->tables[i].files[ii].data,
                    dsr->tables[i].files[ii].size);
            bufferOffset += dsr->tables[i].files[ii].size;
        }
    }

    if (file_put_contents(output, buffer, bufferOffset)) {
        fprintf(stderr, "Failed to create DSR file %s\n", output);
        free(buffer);
        buffer = NULL;
        return 5;
    }

    free(buffer);
    buffer = NULL;

    return 0;
}

int dsr_extract_archive(dsr_t* dsr, const char* dir) {
    int i;
    int ii;
    int attempts;
    size_t dirNameLen;
    char *fileName = NULL;
    char *dirName = NULL;

    if (!dsr || !dir || !dsr->tables || !dsr->tables->files) {
        return -1;
    }

    dirNameLen = strlen(dir);

    if (!dirNameLen) {
        return -1;
    }

    dirName = malloc(sizeof(char)*dirNameLen+1);
    if (!dirName) {
        return -1;
    }

    if (dir[dirNameLen - 1] == FS_DIR_CHAR) {
        dirNameLen -= 1;

        if (!dirNameLen) {
            free(dirName);
            return -1;
        }
    }

    memcpy(dirName, dir, sizeof(char)*dirNameLen);
    dirName[dirNameLen] = '\0';

    if (!directory_exists(dir)) {
        if (create_directory(dir)) {
            free(dirName);
            return -1;
        }
    }

    if (!(fileName = malloc(dirNameLen + 22))) {
        free(dirName);
        return -1;
    }

    for (i = 0; i < dsr->header.tableCount; ++i) {
        for (ii = 0; ii < dsr->tables[i].header.fileCount; ++ii) {
            sprintf(fileName, "%s%c%d.%s%c", dirName, FS_DIR_CHAR, dsr->tables[i].files[ii].id,
                dsr->tables[i].header.extension, '\0');
            if (file_exists(fileName)) {
                attempts = 0;
                fprintf(stderr, "File %s is in use. ", fileName);
                do {
                    sprintf(fileName, "%s%c%d_%03d.%s%c", dirName, FS_DIR_CHAR, dsr->tables[i].files[ii].id,
                        attempts, dsr->tables[i].header.extension, '\0');
                } while (attempts++ < 1000 && file_exists(fileName));

                if (attempts == 1000) {
                    fprintf(stderr, "Failed to find an alternate name for file. Skipping.\n");
                    continue;
                } else {
                    fprintf(stderr, "Will use %s instead.\n", fileName);
                }
            }

            if (file_put_contents(fileName, dsr->tables[i].files[ii].data, dsr->tables[i].files[ii].size)) {
                fprintf(stderr, "Failed to create file %s\n", fileName);
            }
        }
    }

    free(fileName);
    fileName = NULL;

    free(dirName);
    dirName = NULL;

    return 0;
}

void dsr_print_header(dsr_t* dsr, FILE* out) {
    int i;

    if (!dsr || !out) {
        return;
    }

    fprintf(out, "%20s  %s\n%20s  %s\n%20s  %s\n%20s  %d\n%20s  0x%X\n%20s  %zu\n\n", "Copyright info:", dsr->header.copyright,
        "File version:", dsr->header.version, "Archive type:", dsr->header.type, "Num. tables in file:", dsr->header.tableCount,
        "1st file offset:", dsr->header.offset, "File size:", dsr->fileSize);
    
    for (i = 0; i < dsr->header.tableCount; ++i) {
        printf("TABLE %d:\n\n", i);
        dsr_print_table(&dsr->tables[i], out);
    }
}

void dsr_print_table(dsrTable_t* table, FILE* out) {
    int i;

    if (!table || !out) {
        return;
    }

    fprintf(out, "\t%20s  0x%02X (%c)\n\t%20s  %s\n\t%20s  0x%X\n\t%20s  %d\n\n\t%20s\n\n", "File type ID:", table->header.fileType, table->header.fileType,
        "Extension:", table->header.extension, "Table offset:", table->header.offset, "File count:", table->header.fileCount, "Files:");

#if 0
    for (i = 0; i < table->header.fileCount; ++i) {
        dsr_print_file_headers(&table->files[i], out);
    }
#endif
}

void dsr_print_file_headers(dsrFile_t* file, FILE* out) {
    if (!file || !out) {
        return;
    }

    fprintf(out, "\t\t%20s  %04d\n\t\t%20s  0x%X\n\t\t%20s  %11d\n\n", "File ID:", file->id, "File Offset:", file->offset, "File Size:", file->size);
}
