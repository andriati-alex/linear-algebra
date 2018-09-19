#include "../include/array_memory.h"



/* ======================================================================== */
/* ======================== ALLOCATE MEMORY SECTION ======================= */
/* ======================================================================== */



Rarray rarrDef(int n)
{ return (double * restrict) malloc(n * sizeof(double)); }

Carray carrDef(int n)
{ return (double complex * restrict) malloc(n * sizeof(double complex)); }

CMKLarray CMKLdef(int n)
{ return (MKL_Complex16 *) malloc(n * sizeof(MKL_Complex16)); }

Rmatrix rmatDef(int m, int n)
{   /* Allocate real matrix of m rows by n columns */
    int i;
    Rmatrix M;
    M = (double ** restrict) malloc(m * sizeof(double *));
    for (i = 0; i < m; i++) { M[i] = rarrDef(n); }
    return M;
}

Cmatrix cmatDef(int m, int n)
{   /* Allocate complex matrix of m rows by n columns */
    int i;
    Cmatrix M;
    M = (double complex ** restrict) malloc(m * sizeof(double complex *));
    for (i = 0; i < m; i++) { M[i] = carrDef(n); }
    return M;
}



/* ======================================================================== */
/* ======================== RELEASE MEMORY SECTION ======================== */
/* ======================================================================== */



void rarrFree(Rarray v) { free(v); }

void carrFree(Carray v) { free(v); }

void rmatFree(int m, Rmatrix M)
{   /* Release allocated Real Matrix of m rows */
    int i;
    for (i = 0; i < m; i++) { free(M[i]); }
    free(M);
}

void cmatFree(int m, Cmatrix M)
{   /* Release allocated Complex Matrix of m rows */
    int i;
    for (i = 0; i < m; i++) { free(M[i]); }
    free(M);
}

void CCSFree(CCSmat M)
{   /* Release CCS matrix */
    free(M->col);
    free(M->vec);
    free(M);
}

void RCCSFree(RCCSmat M)
{   /* Release CCS matrix */
    free(M->col);
    free(M->vec);
    free(M);
}



/* ======================================================================== */
/* ============================ PRINT SECTION ============================= */
/* ======================================================================== */



void cPrint(double complex z)
{ printf("(%9.2E,%9.2E )", creal(z), cimag(z)); }

void carrPrint(int n, Carray v)
{   // Print Array as a Column Vector

    int i;

    if (n < 30)
    {   // print all numbers for short arrays
        for (i = 0; i < n; i++) { printf("\n\t"); cPrint(v[i]); }
    }

    else
    {   // print first and last 10 elements
        for (i = 0; i < 10; i++)     printf("\n\t"); cPrint(v[i]);
        for (i = 0; i < 5; i++)      printf("\n\t           .");
        for (i = n - 11; i < n; i++) printf("\n\t"); cPrint(v[i]);
    }

    printf("\n");
}

void rarrPrint(int n, Rarray v)
{   // Print Array as a column vector

    int i;

    if (n < 30)
    {   // print all numbers for short arrays
        for (i = 0; i < n; i++) printf("\n\t%9.2E", v[i]);
    }

    else
    {   // print first and last 10 elements
        for (i = 0; i < 10; i++)     printf("\n\t%9.2E", v[i]);
        for (i = 0; i < 5; i++)      printf("\n\t     .");
        for (i = n - 11; i < n; i++) printf("\n\t%9.2E", v[i]);
    }

    printf("\n");
}

void carr_txt(char fname [], int M, Carray v)
{
    int j;

    FILE * data_file = fopen(fname, "w");

    if (data_file == NULL)
    {   // impossible to open file with the given name
        printf("ERROR: impossible to open file %s\n", fname);
        return;
    }

    for (j = 0; j < M; j ++)
    {
        if (cimag(v[j]) > 0)
        {
            fprintf(data_file, "(%.15E+%.15Ej) ", creal(v[j]), cimag(v[j]));
        }
        else
        {
            if (cimag(v[j]) == 0)
                fprintf(data_file, "(%.15E+%.15Ej) ", creal(v[j]), 0.0);
            else
                fprintf(data_file, "(%.15E%.15Ej) ", creal(v[j]), cimag(v[j]));
        }
    }

    fclose(data_file);
}

void cmat_txt(char fname [],
              int N, int row_step, int M, int col_step, Cmatrix S)
{
    int i, j;

    FILE * data_file = fopen(fname, "w");

    if (data_file == NULL)
    {   // impossible to open file with the given name
        printf("ERROR: impossible to open file %s\n", fname);
        return;
    }

    for (i = 0; i < N; i += row_step) {
        for (j = 0; j < M; j += col_step) {
            if (cimag(S[i][j]) > 0) {
                fprintf(data_file, "(%.15E+%.15Ej) ",
                creal(S[i][j]), cimag(S[i][j]));
            }
            else {
                if (cimag(S[i][j]) == 0) {
                    fprintf(data_file, "(%.15E+%.15Ej) ",
                    creal(S[i][j]), 0.0);
                }
                else {
                    fprintf(data_file, "(%.15E%.15Ej) ",
                    creal(S[i][j]), cimag(S[i][j]));
                }
            }
        }
        fprintf(data_file, "\n");
    }
    fclose(data_file);
}
