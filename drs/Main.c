#include <stdio.h>

#include "DRSFormat.h"

//#define DSR_FILE "Interfac.drs"
//#define DSR_FILE "sounds.drs"
#define DSR_FILE "generated.dsr"

#if OS_IS_WINDOWS
#define FILE_PATH "C:\\Program Files (x86)\\Microsoft Games\\Age of Empires\\data\\" DSR_FILE
#else
#define FILE_PATH "../" DSR_FILE
#endif

int main(int argc, char* argv[]) {
    dsr_t dsr;
    int rc;

    //interfac.drs
    rc = dsr_load(FILE_PATH, &dsr);


    if (rc) {
        printf("RETURNED %d\n", rc);
    } else {
        dsr_print_header(&dsr, stdout);
        dsr_extract_archive(&dsr, "dsrFiles");
        //dsr_create_archive(&dsr, "../generated.drs");
        dsr_free(&dsr);
    }

    return rc;
}

