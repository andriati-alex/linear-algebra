#include "../include/matrix_operations.h"



/*          ***********************************************          */
/*                   SETUP VALUES IN MATRIX ENTRIES                  */
/*          ***********************************************          */



void cmatFill(int m, int n, double complex z, Cmatrix M)
{   /* Fill all matrix with a single constant value */
    unsigned int i, j;
    for (i = 0; i < m; i++) { for (j = 0; j < n; j++) { M[i][j] = z; } }
}

void cmatFillDK(int n, int k, Carray z, Cmatrix M)
{   /* Fill a diagonal starting from k-column or row */
    unsigned int i;
    if (k >= 0) { for (i = 0; i < n - k; i++) M[i][i+k] = z[i]; }
    else        { for (i = 0; i < n + k; i++) M[i-k][i] = z[i]; }
}

void cmatFillTri(int n, Carray upper, Carray mid, Carray lower, Cmatrix M)
{   /* Fill trigiaonal matrix */
    cmatFill(n, n, 0, M);
    cmatFillDK(n, -1, lower, M);
    cmatFillDK(n,  1, upper, M);
    cmatFillDK(n,  0, mid, M);
}

CCSmat emptyCCS(int n, int max_nonzeros)
{
    CCSmat M = (struct CCS * restrict) malloc(sizeof(struct CCS));
    M->m = max_nonzeros;
    M->vec = carrDef(max_nonzeros * n);
    M->col = (int *) malloc(max_nonzeros * n * sizeof(int));

    return M;
}

void setValueCCS(int n, int i, int j, int col, double complex z, CCSmat M)
{   /* j-th non-zero term in the row i and col its true column position */
    M->vec[i + n * j] = z;
    M->col[i + n * j] = col;
}

CCSmat triToCCS(int n, Carray upper, Carray lower, Carray mid)
{   /* Given three diagonals construct CCS matrix format */
    unsigned int j;

    CCSmat M = (struct CCS * restrict)malloc(sizeof(struct CCS));

    M->m = 3;
    M->vec = carrDef(3 * n);
    M->col = (int *) malloc(3 * n * sizeof(int));

    M->vec[0]         = mid[0];
    M->vec[n]         = upper[0];
    M->vec[2 * n]     = 0;
    M->vec[3 * n - 1] = 0;

    for (j = 1; j < n; j++)                 { M->vec[j] = lower[j - 1];     }
    for (j = n + 1; j < 2 * n; j++)         { M->vec[j] = mid[j - n];       }
    for (j = 2 * n + 1; j < 3 * n - 1; j++) { M->vec[j] = upper[j - 2 * n]; }
    
    M->col[0]         = 0;
    M->col[n]         = 1;
    M->col[2 * n]     = 0;
    M->col[3 * n - 1] = 0;
    for (j = 1; j < n; j++)                 { M->col[j] = j - 1; }
    for (j = n + 1; j < 2 * n; j++)         { M->col[j] = j - n; }
    for (j = 2 * n + 1; j < 3 * n - 1; j++) { M->col[j] = j + 1 - 2 * n; }

    return M;
}

CCSmat CyclicToCCS(int n, Carray upper, Carray lower, Carray mid)
{   /* Given three diagonals and cyclic terms construct CCS format */
    unsigned int j;
    
    CCSmat M = (struct CCS * restrict)malloc(sizeof(struct CCS));

    M->m = 3;
    M->vec = carrDef(3 * n);
    M->col = (int *)malloc(3 * n * sizeof(int));

    M->vec[0]         = mid[0];
    M->vec[n]         = upper[0];
    M->vec[2 * n]     = upper[n-1]; // Cyclic term
    M->vec[n - 1]     = lower[n-1]; // Cyclic term
    M->vec[2*n - 1]   = lower[n-2];
    M->vec[3 * n - 1] = mid[n-1];

    // In the last and first line we need to take of bottom cyclic term
    for (j = 1; j < n - 1; j++)             { M->vec[j] = lower[j - 1];     }
    for (j = n + 1; j < 2 * n - 1; j++)     { M->vec[j] = mid[j - n];       }
    for (j = 2 * n + 1; j < 3 * n - 1; j++) { M->vec[j] = upper[j - 2 * n]; }
    
    
    M->col[0]         = 0;
    M->col[n]         = 1;
    M->col[2 * n]     = n - 1; // Cyclic term
    M->col[n - 1]     = 0;     // Cyclic Term
    M->col[2 * n - 1] = n - 2;
    M->col[3 * n - 1] = n - 1;
    for (j = 1; j < n - 1; j++)             { M->col[j] = j - 1;         }
    for (j = n + 1; j < 2 * n - 1; j++)     { M->col[j] = j - n;         }
    for (j = 2 * n + 1; j < 3 * n - 1; j++) { M->col[j] = j + 1 - 2 * n; }

    return M;
}



/*          **********************************************          */
/*          MATRIX-VECTOR AND MATRIX-MATRIX MULTIPLICATION          */
/*          **********************************************          */



void cmatvec(int m, int n, Cmatrix M, Carray v, Carray ans) {
    unsigned int i, j;
    for (i = 0; i < m; i++) {
        ans[i] = M[i][0] * v[0];
        for (j = 1; j < n; j++) { ans[i] += M[i][j] * v[j]; }
    }
}

void cmatmat(int m, int n, int l, Cmatrix M, Cmatrix A, Cmatrix ans)
{
    unsigned int i, j, k;
    for (i = 0; i < m; i++) { 
        for (j = 0; j < l; j++) {
            for (k = 0; k < n; k++) ans[i][j] += M[i][k] * A[k][j];
        }
    }
}

void CCSvec(int n, Carray vals, int * restrict cols, int m,
            Carray vec, Carray ans)
{
    unsigned int i, l;
    #pragma omp parallel for private(l, i)
    for (i = 0; i < n; i++) {
        ans[i] = vals[i] * vec[cols[i]];
        for (l = 1; l < m; l++) {
            ans[i] += vals[i + l * n] * vec[cols[i + l * n]];
        }
    }
}



/*          ***********************************************          */
/*          DETERMINANT AND INVERSION OF TRIDIAGONAL SYSTEM          */
/*          ***********************************************          */



double complex detTrik(int k, Carray upper, Carray lower, Carray mid)
{
    double complex det0 = 1;
    double complex det1 = mid[0];
    double complex detK;
    if (k == -1) { return 0; }
    if (k ==  0) { return 1; }
    return mid[k-1] * detTrik(k - 1, upper, lower, mid) - \
           lower[k-2] * upper[k-2] * detTrik(k - 2, upper, lower, mid);
}

Carray detTri(int n, Carray upper, Carray lower, Carray mid)
{
    int i;
    Carray theta = carrDef(n + 1);

    theta[0] = 1;
    theta[1] = mid[0];
    for (i = 1; i < n; i++) {
        theta[i+1] = mid[i] * theta[i] - lower[i-1] * upper[i-1] * theta[i-1];
    }
    return theta;
}

Carray phiTri(int n, Carray upper, Carray lower, Carray mid)
{
    int i;
    Carray phi = carrDef(n + 1);

    phi[n]   = 1;
    phi[n-1] = mid[n-1];
    for (i = n - 2; i >= 0; i--) {
        phi[i] = mid[i] * phi[i+1] - lower[i] * upper[i] * phi[i+2];
    }
    return phi;
}

void invTri(int n, Carray upper, Carray lower, Carray mid, Cmatrix ans)
{
    int i, j;

    Carray phi = phiTri(n, upper, lower, mid);
    Carray theta = detTri(n, upper, lower, mid);

    double complex thetaN = theta[n];
    double complex prodU, prodL;

    for (i = 0; i < n; i++) {
        ans[i][i] = theta[i] * phi[i+1] / thetaN;
        prodU = 1;
        prodL = 1;
        for (j = i - 1; j >= 0; j--) {
            prodU *= (-1) * upper[j];
            prodL *= (-1) * lower[j];
            ans[j][i] = prodU * theta[j] * phi[i+1] / thetaN;
            ans[i][j] = prodL * theta[j] * phi[i+1] / thetaN;
        }
    }

    free(phi);
    free(theta);
}