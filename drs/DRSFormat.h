#ifndef DRS_FORMAT_H
#define DRS_FORMAT_H

#include <stdio.h>

/*
https://gist.github.com/Phrohdoh/4186ec07191c2e3d3221

[HEADER]
[tableCount tableHdrs]
[fileCount  fileHdrs]
[file 1]
[file n]
*/

#define DRS_HDR_COPYRIGHT_LENGTH 40
#define DRS_HDR_VERSION_LENGTH    4
#define DRS_HDR_TYPE_LENGTH      12
#define DRS_TABLE_HDR_EXT_LENGTH  3

typedef struct s_drsHeader {
    char copyright[DRS_HDR_COPYRIGHT_LENGTH+1];  // Copyright information
    char version[DRS_HDR_VERSION_LENGTH+1];      // File Version
    char type[DRS_HDR_TYPE_LENGTH+1];            // Archive Type
    int  tableCount;                             // Num tables in file
    int  offset;                                 // Offset of 1st file
} drsHeader_t, *pDrsHeader_t;

typedef struct s_drsTableHeader {
    char fileType;                               // Related to file type
    char extension[DRS_TABLE_HDR_EXT_LENGTH+1];  // File Extension (reversed in file)
    int  offset;                                 // Table Offset
    int  fileCount;                              // Num files in table
} drsTableHeader_t, *pDrsTableHeader_t;

typedef struct s_drsFile {
    int            id;                           // Unique file ID
    int            offset;                       // File Offset
    int            size;                         // File Size
    unsigned char* data;                         // Raw file data
} drsFile_t, *pDrsFile_t;

typedef struct s_drsTable {
    drsTableHeader_t   header;
    pDrsFile_t   files;
} drsTable_t, *pDrsTable_t;

typedef struct s_drs {
    drsHeader_t  header;
    pDrsTable_t  tables;
    size_t       fileSize;
} drs_t, *pDrs_t;

int drs_load(const char* filePath, drs_t* drs);
void drs_free(drs_t* drs);

int drs_create_archive(drs_t* drs, const char* output);
int drs_extract_archive(drs_t* drs, const char* dir);

void drs_print_header(drs_t* drs, FILE* out);
void drs_print_table(drsTable_t* table, FILE* out);
void drs_print_file_headers(drsFile_t* file, FILE* out);

#endif
