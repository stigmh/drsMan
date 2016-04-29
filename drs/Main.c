#include <stdio.h>

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

int main(int argc, char* argv[]) {
    drs_t drs;
    int rc;

    rc = drs_load(FILE_PATH, &drs);

    if (rc) {
        printf("RETURNED %d\n", rc);
    } else {
        drs_print_header(&drs, stdout);
        drs_extract_archive(&drs, "drsFiles");
        //drs_create_archive(&drs, "../generated.drs");
        drs_free(&drs);
    }

    return rc;
}

