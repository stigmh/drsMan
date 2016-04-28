#ifndef DSR_FORMAT_H
#define DSR_FORMAT_H

#include <stdio.h>

/*
https://gist.github.com/Phrohdoh/4186ec07191c2e3d3221

[HEADER]
[tableCount tableHdrs]
[fileCount  fileHdrs]
[file 1]
[file n]
*/

#define DSR_HDR_COPYRIGHT_LENGTH 40
#define DSR_HDR_VERSION_LENGTH    4
#define DSR_HDR_TYPE_LENGTH      12
#define DSR_TABLE_HDR_EXT_LENGTH  3

typedef struct s_dsrHeader {
    char copyright[DSR_HDR_COPYRIGHT_LENGTH+1];  // Copyright information
    char version[DSR_HDR_VERSION_LENGTH+1];      // File Version
    char type[DSR_HDR_TYPE_LENGTH+1];            // Archive Type
    int  tableCount;                             // Num tables in file
    int  offset;                                 // Offset of 1st file
} dsrHeader_t, *pDsrHeader_t;

typedef struct s_dsrTableHeader {
    char fileType;                               // Related to file type
    char extension[DSR_TABLE_HDR_EXT_LENGTH+1];  // File Extension (reversed)
    int  offset;                                 // Table Offset
    int  fileCount;                              // Num files in table
} dsrTableHeader_t, *pDsrTableHeader_t;

typedef struct s_dsrFile {
    int            id;                           // Unique file ID
    int            offset;                       // File Offset
    int            size;                         // File Size
    unsigned char* data;                         // Raw file data
} dsrFile_t, *pDsrFile_t;

typedef struct s_dsrTable {
    dsrTableHeader_t   header;
    pDsrFile_t   files;
} dsrTable_t, *pDsrTable_t;

typedef struct s_dsr {
    dsrHeader_t  header;
    pDsrTable_t  tables;
    size_t       fileSize;
} dsr_t, *pDsr_t;

int dsr_load(const char* filePath, dsr_t* dsr);
void dsr_free(dsr_t* dsr);

int dsr_create_archive(dsr_t* dsr, const char* output);
int dsr_extract_archive(dsr_t* dsr, const char* dir);

void dsr_print_header(dsr_t* dsr, FILE* out);
void dsr_print_table(dsrTable_t* table, FILE* out);
void dsr_print_file_headers(dsrFile_t* file, FILE* out);

#endif
