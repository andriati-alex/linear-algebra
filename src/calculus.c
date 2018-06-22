#include "../include/calculus.h"

double complex Csimps(int n, Carray f, double dx)
{
    if (n == 1) return 0; // integrating over empty set

    int i, m = n - 1;
    double complex sums = 0;

    #pragma omp parallel for private(i) reduction(+:sums)
    for (i = 0; i < m / 2 - 1; i++) sums += 2 * f[2*(i+1)] + 4 * f[2*i+1];

    if (m % 2 == 0) { return (sums + f[0] + f[m] + 4 * f[m-1]) * dx / 3; }

    /* case odd number of intervals integrate last by trapezium */
    if (m > 2) sums = (sums + f[0] + f[m-1] + 4 * f[m-2]) * dx / 3;
    return sums + (f[m-1] + f[m]) * dx * 0.5;
}

double Rsimps(int n, Rarray f, double dx)
{
    if (n == 1) return 0; // integrating over empty set

    int i, m = n - 1;
    double sums = 0;

    #pragma omp parallel for private(i) reduction(+:sums)
    for (i = 0; i < m / 2 - 1; i++) sums += 2 * f[2*(i+1)] + 4 * f[2*i+1];

    if (m % 2 == 0) return (sums + f[0] + f[m] + 4 * f[m-1]) * dx / 3;

    /* case odd number of intervals integrate last by trapezium */
    if (m > 2) sums = (sums + f[0] + f[m-1] + 4 * f[m-2]) * dx / 3;
    return sums + (f[m-1] + f[m]) * dx * 0.5;
}

void dxFFT(int n, Carray f, double dx, Carray dfdx)
{
    int i;

    Carray outFFT = carrDef(n);
    double freq;
    
    MKL_LONG s; // status of called MKL FFT functions

    DFTI_DESCRIPTOR_HANDLE desc;
    s = DftiCreateDescriptor(&desc, DFTI_DOUBLE, DFTI_COMPLEX, 1, n);
    s = DftiSetValue(desc, DFTI_FORWARD_SCALE, 1.0 / sqrt((double) n));
    s = DftiSetValue(desc, DFTI_BACKWARD_SCALE, 1.0 / sqrt((double) n));
    s = DftiSetValue(desc, DFTI_PLACEMENT, DFTI_NOT_INPLACE);
    s = DftiCommitDescriptor(desc);

    s = DftiComputeForward(desc, f, outFFT);

    for (i = 0; i < n; i++) {
        if (i <= (n - 1) / 2) { freq = (2 * PI * i) / (n * dx);       }
        else                  { freq = (2 * PI * (i - n)) / (n * dx); }
        outFFT[i] *= freq * I;
    }

    s = DftiComputeBackward(desc, outFFT, dfdx);
    s = DftiFreeDescriptor(&desc);
    free(outFFT);
}

void dxCyclic(int n, Carray f, double dx, Carray dfdx)
{
    int i;
    double r = 1.0 / (2 * dx);

    dfdx[0]   = (f[1] - f[n-1]) * r;
    dfdx[n-1] = (f[0] - f[n-2]) * r;
    for (i = 1; i < n - 1; i++) { dfdx[i] = (f[i+1] - f[i-1]) * r; }
}
