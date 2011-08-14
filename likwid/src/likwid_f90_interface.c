#include <stdlib.h>
#include <string.h>

#include <likwid.h>

void
likwid_markerinit_(void)
{
    likwid_markerInit();
}

void
likwid_markerclose_(void)
{
    likwid_markerClose();
}

void
likwid_markerstartregion_(char* regionTag, int* len)
{
    char* tmp = (char*) malloc(((*len)+1) * sizeof(char));
    strncpy(tmp, regionTag, (*len));
    tmp[(*len)] = 0;
    likwid_markerStartRegion( tmp );
	free(tmp);
}

void
likwid_markerstopregion_(char* regionTag, int* len)
{
    char* tmp = (char*) malloc(((*len)+1) * sizeof(char));
    strncpy(tmp, regionTag, (*len));
    tmp[(*len)] = 0;
    likwid_markerStopRegion( tmp );
	free(tmp);
}

