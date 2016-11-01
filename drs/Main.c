#include <stdio.h>
#include <string.h>

#include "FileManager.h"
#include "DRSFormat.h"

//#define DRS_FILE "Interfac.drs"
#define DRS_FILE "sounds.drs"
//#define DRS_FILE "generated.drs"

#ifdef OS_IS_WINDOWS
#define FILE_PATH "C:\\Program Files (x86)\\Microsoft Games\\Age of Empires\\data\\" DRS_FILE
#else
#define FILE_PATH "../" DRS_FILE
#endif

typedef struct config_s {
    const char*  filePath;
    unsigned int create;
    unsigned int extract;
} config_t, *pConfig_t;

int parseParams(int argc, char* argv[], pConfig_t conf) {
    size_t idx;

    conf->filePath = FILE_PATH;
    conf->create   = 0;
    conf->extract  = 0;

    for (idx = 0; idx < (size_t)argc; ++idx) {
        if (!strcmp("-e", argv[idx]) || !strcmp("--extract", argv[idx])) {
            conf->extract = 1;
            continue;
        }

        if (!strcmp("-c", argv[idx]) || !strcmp("--create", argv[idx])) {
            conf->create = 1;
            continue;
        }

        if ((!strcmp("-f", argv[idx]) || !strcmp("--file", argv[idx])) && (idx+1 != argc)) {
            conf->filePath = argv[++idx];
            continue;
        }
    }

    if (conf->create == conf->extract) {
        fprintf(stderr, "Please specify either --create or --extract\n");
        return 1;
    }

    if (!conf->filePath || conf->filePath[0] == '\0') {
        fprintf(stderr, "Invalid file path specified\n");
        return 1;
    }

    return 0;
}

void usage(void) {
    printf("Not implemented\n");
}

int main(int argc, char* argv[]) {
    drs_t drs;
    config_t config;
    int rc;

    if (parseParams(argc, argv, &config)) {
        usage();
        return 1;
    }

    if (config.extract) {
        rc = drs_load(FILE_PATH, &drs);

        if (rc) {
            printf("RETURNED %d\n", rc);
        } else {
            drs_print_header(&drs, stdout);
            drs_extract_archive(&drs, "drsFiles");
            //drs_create_archive(&drs, "../generated.drs");
            drs_free(&drs);
        }
    } else {
        
    }

    return rc;
}

