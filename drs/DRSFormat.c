#include <stdlib.h>
#include <string.h>

#include "FileManager.h"
#include "DRSFormat.h"

int is_readable_ascii_char(char c) {
    return ('~' >= c) && (c >= ' ');
}

int drs_load(const char* filePath, drs_t* drs) {
    int idx;
    int iidx;
    size_t fileOffset = 0;
    size_t ffOffset = 0;
    unsigned char* fileBuffer = NULL;
    drsTable_t *drsTable = NULL;
    int rc = 0;

    if (!drs) {
        return 1;
    }

    /* Retrieve file contents */
    if ((rc = file_get_contents(filePath, &fileBuffer, &drs->fileSize))) {
        return rc + 1;
    }

    if (!fileBuffer || drs->fileSize <= sizeof(drsHeader_t)) {
        if (fileBuffer) {
            free(fileBuffer);
            fileBuffer = NULL;
        }
        return 7;
    }

    /***********************
     * Retrieve DRS header *
     ***********************/

    drs->tables = NULL;

    /**
     * Copy valid data from copyright string. First 40 bytes in file is reserved
     * for copyright data. It is not null terminated in file.
     **/
    while (is_readable_ascii_char(fileBuffer[fileOffset]) && fileOffset < DRS_HDR_COPYRIGHT_LENGTH) {
        drs->header.copyright[fileOffset] = fileBuffer[fileOffset];
        ++fileOffset;
    }

    /* zero terminate copyright string */
    drs->header.copyright[fileOffset] = '\0';
    fileOffset = DRS_HDR_COPYRIGHT_LENGTH;

    /* Retrieve DRS version, 4 bytes, not null terminated. int hack to save copy ops. */
    drs->header.version[0] = fileBuffer[fileOffset++];
    drs->header.version[1] = fileBuffer[fileOffset++];
    drs->header.version[2] = fileBuffer[fileOffset++];
    drs->header.version[3] = fileBuffer[fileOffset++];
    drs->header.version[4] = '\0';

    /* Retrieve DRS archive type, 12 bytes, not null terminated */
    idx = 0;
    while (is_readable_ascii_char(fileBuffer[idx+fileOffset]) && idx < DRS_HDR_TYPE_LENGTH) {
        drs->header.type[idx] = fileBuffer[idx+fileOffset];
        ++idx;
    }
    
    drs->header.type[idx] = '\0';
    fileOffset += DRS_HDR_TYPE_LENGTH;

    /* Retrieve table count and offset */
    drs->header.tableCount = *(int*)&fileBuffer[fileOffset];
    fileOffset += sizeof(int);
    drs->header.offset = *(int*)&fileBuffer[fileOffset];
    fileOffset += sizeof(int);

    /***********************
    *  Retrieve DRS tables *
    ************************/
  
    if (!(drs->tables = malloc(sizeof(drsTable_t) * drs->header.tableCount))) {
        free(fileBuffer);
        fileBuffer = NULL;
        return 8;
    }

    for (idx = 0; idx < drs->header.tableCount; ++idx) {
        drsTable = &drs->tables[idx];
        drsTable->files = NULL;

        /* Retrieve file type */
        drsTable->header.fileType = fileBuffer[fileOffset++];

        /* Retrieve file extension (reversed in file) */
        drsTable->header.extension[0] = fileBuffer[fileOffset + 2];
        drsTable->header.extension[1] = fileBuffer[fileOffset + 1];
        drsTable->header.extension[2] = fileBuffer[fileOffset + 0];
        drsTable->header.extension[3] = '\0';
        fileOffset += DRS_TABLE_HDR_EXT_LENGTH;

        /* Retrieve offset and file count*/
        drsTable->header.offset = *(int*)&fileBuffer[fileOffset];
        fileOffset += sizeof(int);
        drsTable->header.fileCount = *(int*)&fileBuffer[fileOffset];
        fileOffset += sizeof(int);

        /* Allocate file headers */
        drsTable->files = malloc(drsTable->header.fileCount * sizeof(drsFile_t));

        if (!drsTable->files) {
            for (iidx = 0; iidx < idx; ++iidx) {
                if (drs->tables[iidx].files) {
                    free(drs->tables[iidx].files);
                    drs->tables[iidx].files = NULL;
                }
            }

            free(drs->tables);
            drs->tables = NULL;
            free(fileBuffer);
            return 9;
        }

        /* Retrieve file header data */
        ffOffset = drsTable->header.offset;

        for (iidx = 0; iidx < drsTable->header.fileCount; ++iidx) {
            drsTable->files[iidx].id = *(int*)&fileBuffer[ffOffset];
            ffOffset += sizeof(int);
            drsTable->files[iidx].offset = *(int*)&fileBuffer[ffOffset];
            ffOffset += sizeof(int);
            drsTable->files[iidx].size = *(int*)&fileBuffer[ffOffset];
            ffOffset += sizeof(int);

            if ((drsTable->files[iidx].data = malloc(drsTable->files[iidx].size)) != NULL) {
                memcpy(drsTable->files[iidx].data, &fileBuffer[drsTable->files[iidx].offset], drsTable->files[iidx].size);
            } else {
                drsTable->files[iidx].size = 0;
            }
        }
    }

    free(fileBuffer);
    return rc;
}

void drs_free(drs_t* drs) {
    int i;
    int ii;

    if (drs) {
        if (drs->tables) {
            for (i = 0; i < drs->header.tableCount; ++i) {
                if (drs->tables->files) {
                    for (ii = 0; ii < drs->tables->header.fileCount; ++ii) {
                        if (drs->tables->files[ii].data) {
                            free(drs->tables->files[ii].data);
                            drs->tables->files[ii].data = NULL;
                            drs->tables->files[ii].size = 0;
                        }
                    }

                    free(drs->tables->files);
                    drs->tables->files = NULL;
                }
            }

            free(drs->tables);
            drs->tables = NULL;
        }
    }
}

int drs_create_archive(drs_t* drs, const char* output) {
    unsigned char* buffer;
    size_t bufferOffset;
    int totalSize;
    int fSz = 0;
    int i;
    int ii;

    if (!drs || !output || !drs->tables) {
        return 1;
    }

    /* Calculate size of header */
    totalSize = DRS_HDR_COPYRIGHT_LENGTH + DRS_HDR_VERSION_LENGTH +
        DRS_HDR_TYPE_LENGTH + 2*sizeof(int);

    /* Calculate size of table headers */
    totalSize += drs->header.tableCount *
        ((sizeof(char) * (DRS_TABLE_HDR_EXT_LENGTH+1)) + (2 * sizeof(int)));

    /* Calcluate size of file headers and files */
    for (i = 0; i < drs->header.tableCount; ++i) {
        for (ii = 0; ii < drs->tables[i].header.fileCount; ++ii) {
            totalSize += (3*sizeof(int));
            
            if (drs->tables[i].files) {
                totalSize += drs->tables[i].files[ii].size;
                fSz += drs->tables[i].files[ii].size;
            }
        }
    }

    if (totalSize != drs->fileSize) {
        fprintf(stderr, "DRS file size mismatched. Expected %d (0x%X), got %zu (0x%zX) - %d.\n",
                totalSize, totalSize, drs->fileSize, drs->fileSize, fSz);
        return 2;
    }

    if ((buffer = malloc(totalSize)) == NULL) {
        return 3;
    }

    /* Copy header */
    memcpy(buffer, &drs->header.copyright, DRS_HDR_COPYRIGHT_LENGTH);
    bufferOffset = DRS_HDR_COPYRIGHT_LENGTH;
    memcpy(&buffer[bufferOffset], &drs->header.version, DRS_HDR_VERSION_LENGTH);
    bufferOffset += DRS_HDR_VERSION_LENGTH;
    memcpy(&buffer[bufferOffset], &drs->header.type, DRS_HDR_TYPE_LENGTH);
    bufferOffset += DRS_HDR_TYPE_LENGTH;
    memcpy(&buffer[bufferOffset], &drs->header.tableCount, sizeof(int));
    bufferOffset += sizeof(int);
    memcpy(&buffer[bufferOffset], &drs->header.offset, sizeof(int));
    bufferOffset += sizeof(int);

    /* Copy out table headers */
    for (i = 0; i < drs->header.tableCount; ++i) {
        /* Type */
        buffer[bufferOffset++] = drs->tables[i].header.fileType;

        /* Extension (reversed)  */
        buffer[bufferOffset++] = drs->tables[i].header.extension[2]; 
        buffer[bufferOffset++] = drs->tables[i].header.extension[1];
        buffer[bufferOffset++] = drs->tables[i].header.extension[0];

        /* Offset and file count */
        memcpy(&buffer[bufferOffset], &drs->tables[i].header.offset, sizeof(int));
        bufferOffset += sizeof(int);
        memcpy(&buffer[bufferOffset], &drs->tables[i].header.fileCount, sizeof(int));
        bufferOffset += sizeof(int);
    }

    if (bufferOffset != drs->tables[0].header.offset) {
        fprintf(stderr, "DRS table offset mismatched. Expected 0x%08X, got 0x%08X\n.",
                (unsigned int)bufferOffset, drs->tables[0].header.offset); 
        free(buffer);
        buffer = NULL;
        return 4;
    }

    /* Copy out file headers */
    for (i = 0; i < drs->header.tableCount; ++i) {
        for (ii = 0; ii < drs->tables[i].header.fileCount; ++ii) {
            memcpy(&buffer[bufferOffset], &drs->tables[i].files[ii].id, sizeof(int));
            bufferOffset += sizeof(int);
            memcpy(&buffer[bufferOffset], &drs->tables[i].files[ii].offset, sizeof(int));
            bufferOffset += sizeof(int);
            memcpy(&buffer[bufferOffset], &drs->tables[i].files[ii].size, sizeof(int));
            bufferOffset += sizeof(int);
        }
    }

    /* Copy out raw file data */
    for (i = 0; i < drs->header.tableCount; ++i) {
        for (ii = 0; ii < drs->tables[i].header.fileCount; ++ii) {
            memcpy(&buffer[bufferOffset], drs->tables[i].files[ii].data,
                    drs->tables[i].files[ii].size);
            bufferOffset += drs->tables[i].files[ii].size;
        }
    }

    if (file_put_contents(output, buffer, bufferOffset)) {
        fprintf(stderr, "Failed to create DRS file %s\n", output);
        free(buffer);
        buffer = NULL;
        return 5;
    }

    free(buffer);
    buffer = NULL;

    return 0;
}

int drs_extract_archive(drs_t* drs, const char* dir) {
    int i;
    int ii;
    int attempts;
    size_t dirNameLen;
    char *fileName = NULL;
    char *dirName = NULL;

    if (!drs || !dir || !drs->tables || !drs->tables->files) {
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

    for (i = 0; i < drs->header.tableCount; ++i) {
        for (ii = 0; ii < drs->tables[i].header.fileCount; ++ii) {
            sprintf(fileName, "%s%c%d.%s%c", dirName, FS_DIR_CHAR, drs->tables[i].files[ii].id,
                drs->tables[i].header.extension, '\0');
            if (file_exists(fileName)) {
                attempts = 0;
                fprintf(stderr, "File %s is in use. ", fileName);
                do {
                    sprintf(fileName, "%s%c%d_%03d.%s%c", dirName, FS_DIR_CHAR, drs->tables[i].files[ii].id,
                        attempts, drs->tables[i].header.extension, '\0');
                } while (attempts++ < 1000 && file_exists(fileName));

                if (attempts == 1000) {
                    fprintf(stderr, "Failed to find an alternate name for file. Skipping.\n");
                    continue;
                } else {
                    fprintf(stderr, "Will use %s instead.\n", fileName);
                }
            }

            if (file_put_contents(fileName, drs->tables[i].files[ii].data, drs->tables[i].files[ii].size)) {
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

void drs_print_header(drs_t* drs, FILE* out) {
    int i;

    if (!drs || !out) {
        return;
    }

    fprintf(out, "%20s  %s\n%20s  %s\n%20s  %s\n%20s  %d\n%20s  0x%X\n%20s  %zu\n\n", "Copyright info:", drs->header.copyright,
        "File version:", drs->header.version, "Archive type:", drs->header.type, "Num. tables in file:", drs->header.tableCount,
        "1st file offset:", drs->header.offset, "File size:", drs->fileSize);
    
    for (i = 0; i < drs->header.tableCount; ++i) {
        printf("TABLE %d:\n\n", i);
        drs_print_table(&drs->tables[i], out);
    }
}

void drs_print_table(drsTable_t* table, FILE* out) {
#if 0
    int i;
#endif

    if (!table || !out) {
        return;
    }

    fprintf(out, "\t%20s  0x%02X (%c)\n\t%20s  %s\n\t%20s  0x%X\n\t%20s  %d\n\n\t%20s\n\n", "File type ID:", table->header.fileType, table->header.fileType,
        "Extension:", table->header.extension, "Table offset:", table->header.offset, "File count:", table->header.fileCount, "Files:");

#if 0
    for (i = 0; i < table->header.fileCount; ++i) {
        drs_print_file_headers(&table->files[i], out);
    }
#endif
}

void drs_print_file_headers(drsFile_t* file, FILE* out) {
    if (!file || !out) {
        return;
    }

    fprintf(out, "\t\t%20s  %04d\n\t\t%20s  0x%X\n\t\t%20s  %11d\n\n", "File ID:", file->id, "File Offset:", file->offset, "File Size:", file->size);
}
