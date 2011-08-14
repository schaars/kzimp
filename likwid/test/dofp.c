#include <stdio.h>

#ifdef PERFMON
#include <likwid.h>
#endif

#define SIZE 1000000
#define N1   100
#define N2   300
#define N3   500

double sum = 0, a[SIZE], b[SIZE], c[SIZE];

main()
{
    int i, j;
    double alpha = 3.14;

    /* Initialize */
    for (i=0; i<SIZE; i++)
    {
        a[i] = 1.0/(double) i;
        b[i] = 1.0;
        c[i] = (double) i;
    }

#ifdef PERFMON
    printf("Using likwid\n");
    likwid_markerInit();
#endif

/****************************************************/
#ifdef PERFMON
    likwid_markerStartRegion("plain");
#endif
    for (j = 0; j < N1; j++)
    {
        for (i = 0; i < SIZE; i++) 
        {
            a[i] = b[i] + alpha * c[i];
            sum += a[i];
        }
    }
#ifdef PERFMON
    likwid_markerStopRegion("plain");
#endif
    printf("Flops performed plain: %g\n",(double)N1*SIZE*3);
/****************************************************/

    for (j = 0; j < N1; j++)
    {
/****************************************************/
#ifdef PERFMON
        likwid_markerStartRegion("accumulate");
#endif
        for (i = 0; i < SIZE; i++) 
        {
            a[i] = b[i] + alpha * c[i];
            sum += a[i];
        }
#ifdef PERFMON
        likwid_markerStopRegion("accumulate");
#endif
/****************************************************/
    }
    printf( "OK, dofp result = %e\n", sum);
    printf("Flops performed accumulate: %g\n",(double)N1*SIZE*3);

/****************************************************/
#ifdef PERFMON
    likwid_markerStartRegion("out");
#endif
#pragma omp parallel for
    for (j = 0; j < N3; j++)
    {
        for (i=0; i<SIZE; i++)
        {
            a[i] = 1.0/(double) i;
            b[i] = 1.0;
            c[i] = (double) i;
        }
    }

/****************************************************/
/****************************************************/
#ifdef PERFMON
    likwid_markerStartRegion("inklusive");
#endif
#pragma omp parallel for
    for (j = 0; j < N2; j++)
    {
        for (i = 0; i < SIZE; i++) 
        {
            a[i] = b[i] + alpha * c[i];
            sum += a[i];
        }
    }
#ifdef PERFMON
    likwid_markerStopRegion("inklusive");
#endif
/****************************************************/
/****************************************************/

/****************************************************/
/****************************************************/
#ifdef PERFMON
    likwid_markerStartRegion("overlap");
#endif
    for (j = 0; j < N1; j++)
    {
        for (i = 0; i < SIZE; i++) 
        {
            a[i] = b[i] + alpha * c[i];
            sum += a[i];
        }
    }
#ifdef PERFMON
    likwid_markerStopRegion("out");
#endif
/****************************************************/

    for (j = 0; j < N3; j++)
    {
        for (i = 0; i < SIZE; i++) 
        {
            a[i] = b[i] + alpha * c[i];
            sum += a[i];
        }
    }
#ifdef PERFMON
    likwid_markerStopRegion("overlap");
#endif
/****************************************************/
/****************************************************/

    printf("Flops performed out: %g\n",(double)(N1+N2+N3)*SIZE*3);
    printf("Flops performed inclusive: %g\n",(double)N2*SIZE*3);
    printf("Flops performed overlap: %g\n",(double) (N1+N3)*SIZE*3);

#ifdef PERFMON
    likwid_markerClose();
#endif
    printf( "OK, dofp result = %e\n", sum);
}
