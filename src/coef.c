#include "../include/coef_routines.h"

long fac(int n)
{
    long n_fac = 1;
    for (int i = 1; i < n; i++) n_fac = n_fac * (i + 1);
    return n_fac;
}

long NC(int N, int M)
{
    long n = 1;
    for (int i = N + M - 1; i > N; i --) n = n * i;
    return n / fac(M - 1);
}

long ** MountNCmat(int N, int M)
{
    int i,
        j;

    long ** NCmat = (long ** ) malloc((N + 1) * sizeof(long * ));

    for (i = 0; i < N + 1; i++)
        NCmat[i] = (long * ) malloc((M + 1) * sizeof(long));

    for (i = 0; i < N + 1; i++)
    {
        NCmat[i][0] = 0;
        for (j = 1; j < M + 1; j++) NCmat[i][j] = NC(i, j);
    }

    return NCmat;
}

int ** MountFocks(int N, int M, long ** NCmat)
{
    long k;

    int ** ItoFock = (int **) malloc(NC(N, M) * sizeof(int *));

    for (k = 0; k < NC(N, M); k++)
    {
        ItoFock[k] = (int * ) malloc(M * sizeof(int));
        IndexToFock(k, N, M, NCmat, ItoFock[k]);
    }

    return ItoFock;
}

void IndexToFock(long k, int N, int M, long ** NCmat, int * v)
{
    long x;

    int  i,
         m = M - 1;

    for (i = 0; i < M; i++) v[i] = 0;

    /***  Put the particles in orbitals while has combinations to spend  ***/

    while ( k > 0 )
    {
        while ( k - NCmat[N][m] < 0 ) m = m - 1;
        x = k - NCmat[N][m];
        while ( x >= 0 )
        {
            v[m] = v[m] + 1; // One more particle in orbital m
            N = N - 1;       // Less one particle still to setup
            k = x;
            x = x - NCmat[N][m];
        }
    }

    /***         Put the rest of particles in the first orbital         ***/

    for (i = N; i > 0; i--) v[0] = v[0] + 1;
}

long FockToIndex(int N, int M, long ** NCmat, int * v)
{
    int  i, n;
    long k = 0;

    /*******  Empty one by one orbital starting from the last one  *******/

    for (i = M - 1; i > 0; i--)
    {
        n = v[i]; // Number of particles in the orbital
        while (n > 0)
        {
            k = k + NCmat[N][i]; // number of combinations needed
            N = N - 1;           // decrease the number of particles
            n = n - 1;
        }
    }
    return k;
}

void JumpMapping(int N, int M, long ** NCmat, int ** IF, long * Map)
{
    long i,
         nc = NCmat[N][M];

    int  q,
         k,
         l;

    int * v = (int *) malloc(M * sizeof(int));

    for (i = 0; i < nc; i++)
    {
        for (k = 0; k < M; k++)
        {
            if (v[k] < 1) continue;
            for (l = 0; l < M; l++)
            {
                for (q = 0; q < M; q++) v[q] = IF[i][q];
                v[k] -= 1;
                v[l] += 1;
                Map[i + k * nc + l * M * nc] = FockToIndex(N, M, NCmat, v);
            }
        }
    }

    free(v);
}


void OBrho(int N, int M, long ** NCmat, int ** IF, Carray C, Cmatrix rho)
{
    long i, // long indices to number coeficients
         j,
         nc = NCmat[N][M];
    int  k, // int indices to density matrix (M x M)
         l,
         q;

    double mod2,
           sqrtOf;

    double complex RHO_kk,
                   RHO_kl;

    int * v;

    for (k = 0; k < M; k++)
    {
        RHO_kk = 0;
        #pragma omp parallel shared(k,nc,C,IF) private(i,mod2) \
        reduction(+:RHO_kk)
        {
            #pragma omp for schedule(static)
            for (i = 0; i < nc; i++)
            {
                mod2 = creal(C[i]) * creal(C[i]) + cimag(C[i]) * cimag(C[i]);
                RHO_kk += mod2 * IF[i][k];
            }
        }
        rho[k][k] = RHO_kk;

        for (l = k + 1; l < M; l++)
        {
            RHO_kl = 0;
            #pragma omp parallel shared(l,k,M,N,nc,C,NCmat,IF) \
            private(i,j,q,v) reduction(+:RHO_kl)
            {
                v = (int *) malloc(M * sizeof(int));
                #pragma omp for schedule(static)
                for (i = 0; i < nc; i++)
                {
                    if (IF[i][k] < 1) continue;
                    for (q = 0; q < M; q++) v[q] = IF[i][q];
                    v[k] -= 1;
                    v[l] += 1;
                    j = FockToIndex(N, M, NCmat, v);
                    v[k] += 1;
                    v[l] -= 1;
                    RHO_kl += conj(C[i]) * C[j] * sqrt((v[l] + 1) * v[k]);
                }
                free(v); // Each thread release its vector
            }
            rho[k][l] = RHO_kl;
        }
    }

    /*******  Use hermiticity of density matrix to fill lower part  *******/

    for (k = 0; k < M; k++)
    {
        for (l = k + 1; l < M; l++) rho[l][k] = conj(rho[k][l]);
    }

}

void TBrho(int N, int M, long ** NCmat, int ** IF, Carray C, Carray rho)
{
    long i, // long indices to number coeficients
         j,
         nc = NCmat[N][M];
    int  k, // int indices to density matrix (M x M x M x M)
         s,
         q,
         l,
         t; // To copy the fock vector

    int M2 = M * M,
        M3 = M * M * M;

    double mod2,   // |Cj| ^ 2
           sqrtOf; // Factors from the action of creation/annihilation

    double complex RHO;

    int * v; // occupation number vector in each iteration


    /****************************** Rule 1 ******************************/

    for (k = 0; k < M; k++)
    {
        RHO = 0;
        #pragma omp parallel for private(i,mod2) reduction(+:RHO)
        for (i = 0; i < nc; i++)
        {
            mod2 = creal(C[i]) * creal(C[i]) + cimag(C[i]) * cimag(C[i]);
            RHO += mod2 * IF[i][k] * (IF[i][k] - 1);
        }
        rho[k + M * k + M2 * k + M3 * k] = RHO;
    }

    /****************************** Rule 2 ******************************/

    for (k = 0; k < M; k++)
    {
        for (s = k + 1; s < M; s++)
        {
            RHO = 0;
            #pragma omp parallel for private(i,mod2) reduction(+:RHO)
            for (i = 0; i < nc; i++)
            {
                mod2 = creal(C[i]) * creal(C[i]) + cimag(C[i]) * cimag(C[i]);
                RHO += mod2 * IF[i][k] * IF[i][s];
            }
            rho[k + s * M + k * M2 + s * M3] = RHO;
            rho[s + k * M + k * M2 + s * M3] = RHO;
            rho[s + k * M + s * M2 + k * M3] = RHO;
            rho[k + s * M + s * M2 + k * M3] = RHO;
        }
    }
    
    /****************************** Rule 3 ******************************/

    for (k = 0; k < M; k++)
    {
        for (q = k + 1; q < M; q++)
        {
            RHO = 0;
            #pragma omp parallel private(i,j,t,sqrtOf,v) reduction(+:RHO)
            {
                v = (int *) malloc(M * sizeof(int));
                #pragma omp for
                for (i = 0; i < nc; i++)
                {
                    if (IF[i][k] < 2) continue;
                    for (t = 0; t < M; t++) v[t] = IF[i][t];
                    sqrtOf = sqrt((v[k] - 1) * v[k] * (v[q] + 1) * (v[q] + 2));
                    v[k] -= 2;
                    v[q] += 2;
                    j = FockToIndex(N, M, NCmat, v);
                    RHO += conj(C[i]) * C[j] * sqrtOf;
                }
                free(v);
            }
            rho[k + k * M + q * M2 + q * M3] = RHO;
        }
    }
        
    /****************************** Rule 4 ******************************/

    for (k = 0; k < M; k++)
    {
        for (l = k + 1; l < M; l++)
        {
            RHO = 0;
            #pragma omp parallel private(i,j,t,sqrtOf,v) reduction(+:RHO)
            {
                v = (int *) malloc(M * sizeof(int));
                #pragma omp for
                for (i = 0; i < nc; i++)
                {
                    if (IF[i][k] < 1) continue;
                    for (t = 0; t < M; t++) v[t] = IF[i][t];
                    sqrtOf = (v[k] - 1) * sqrt(v[k] * (v[l] + 1));
                    v[k] -= 1;
                    v[l] += 1;
                    j = FockToIndex(N, M, NCmat, v);
                    RHO += conj(C[i]) * C[j] * sqrtOf;
                }
                free(v);
            }
            rho[k + k * M + k * M2 + l * M3] = RHO;
        }
    }
    
    /****************************** Rule 5 ******************************/

    for (k = 0; k < M; k++)
    {
        for (s = k + 1; s < M; s++)
        {
            RHO = 0;
            #pragma omp parallel private(i,j,t,sqrtOf,v) reduction(+:RHO)
            {
                v = (int *) malloc(M * sizeof(int));
                #pragma omp for
                for (i = 0; i < nc; i++)
                {
                    if (IF[i][k] < 1) continue;
                    for (t = 0; t < M; t++) v[t] = IF[i][t];
                    sqrtOf = v[s] * sqrt(v[k] * (v[s] + 1));
                    v[k] -= 1;
                    v[s] += 1;
                    j = FockToIndex(N, M, NCmat, v);
                    RHO += conj(C[i]) * C[j] * sqrtOf;
                }
                free(v);
            }
            rho[k + s * M + s * M2 + s * M3] = RHO;
        }
    }

    /****************************** Rule 6 ******************************/
        
    for (k = 0; k < M; k++)
    {
        for (q = k + 1; q < M; q++)
        {
            for (l = q + 1; l < M; l++)
            {
                RHO = 0;
                #pragma omp parallel private(i,j,t,sqrtOf,v) reduction(+:RHO)
                {
                    v = (int *) malloc(M * sizeof(int));
                    #pragma omp for
                    for (i = 0; i < nc; i++)
                    {
                        if (IF[i][k] < 2) continue;
                        for (t = 0; t < M; t++) v[t] = IF[i][t];
                        sqrtOf = sqrt( v[k] * (v[k] - 1) * 
                                 (v[q] + 1) * (v[l] + 1) );
                        v[k] -= 2;
                        v[l] += 1;
                        v[q] += 1;
                        j = FockToIndex(N, M, NCmat, v);
                        RHO += conj(C[i]) * C[j] * sqrtOf;
                    }
                    free(v);
                }
                rho[k + k*M + q*M2 + l*M3] = RHO;
            }
        }
    }

    for (q = 0; q < M; q++)
    {
        for (k = q + 1; k < M; k++)
        {
            for (l = k + 1; l < M; l++)
            {
                RHO = 0;
                #pragma omp parallel private(i,j,t,sqrtOf,v) reduction(+:RHO)
                {
                    v = (int *) malloc(M * sizeof(int));
                    #pragma omp for
                    for (i = 0; i < nc; i++)
                    {
                        if (IF[i][k] < 2) continue;
                        for (t = 0; t < M; t++) v[t] = IF[i][t];
                        sqrtOf = sqrt( v[k] * (v[k] - 1) * 
                                 (v[q] + 1) * (v[l] + 1) );
                        v[k] -= 2;
                        v[l] += 1;
                        v[q] += 1;
                        j = FockToIndex(N, M, NCmat, v);
                        RHO += conj(C[i]) * C[j] * sqrtOf;
                    }
                    free(v);
                }
                rho[k + k*M + q*M2 + l*M3] = RHO;
            }
        }
    }
    
    for (q = 0; q < M; q++)
    {
        for (l = q + 1; l < M; l++)
        {
            for (k = l + 1; k < M; k++)
            {
                RHO = 0;
                #pragma omp parallel private(i,j,t,sqrtOf,v) reduction(+:RHO)
                {
                    v = (int *) malloc(M * sizeof(int));
                    #pragma omp for
                    for (i = 0; i < nc; i++)
                    {
                        if (IF[i][k] < 2) continue;
                        for (t = 0; t < M; t++) v[t] = IF[i][t];
                        sqrtOf = sqrt( v[k] * (v[k] - 1) *
                                 (v[q] + 1) * (v[l] + 1) );
                        v[k] -= 2;
                        v[l] += 1;
                        v[q] += 1;
                        j = FockToIndex(N, M, NCmat, v);
                        RHO += conj(C[i]) * C[j] * sqrtOf;
                    }
                    free(v);
                }
                rho[k + k*M + q*M2 + l*M3] = RHO;
            }
        }
    }

    /****************************** Rule 7 ******************************/

    for (s = 0; s < M; s++)
    {
        for (k = s + 1; k < M; k++)
        {
            for (l = k + 1; l < M; l++)
            {
                RHO = 0;
                #pragma omp parallel private(i,j,t,sqrtOf,v) reduction(+:RHO)
                {
                    v = (int *) malloc(M * sizeof(int));
                    #pragma omp for
                    for (i = 0; i < nc; i++)
                    {
                        if (IF[i][k] < 1) continue;
                        for (t = 0; t < M; t++) v[t] = IF[i][t];
                        sqrtOf = v[s] * sqrt(v[k] * (v[l] + 1));
                        v[k] -= 1;
                        v[l] += 1;
                        j = FockToIndex(N, M, NCmat, v);
                        RHO += conj(C[i]) * C[j] * sqrtOf;
                    }
                    free(v);
                }
                rho[k + s*M + s*M2 + l*M3] = RHO;
            }
        }
    }

    for (k = 0; k < M; k++)
    {
        for (s = k + 1; s < M; s++)
        {
            for (l = s + 1; l < M; l++)
            {
                RHO = 0;
                #pragma omp parallel private(i,j,t,sqrtOf,v) reduction(+:RHO)
                {
                    v = (int *) malloc(M * sizeof(int));
                    #pragma omp for
                    for (i = 0; i < nc; i++)
                    {
                        if (IF[i][k] < 1) continue;
                        for (t = 0; t < M; t++) v[t] = IF[i][t];
                        sqrtOf = v[s] * sqrt(v[k] * (v[l] + 1));
                        v[k] -= 1;
                        v[l] += 1;
                        j = FockToIndex(N, M, NCmat, v);
                        RHO += conj(C[i]) * C[j] * sqrtOf;
                    }
                    free(v);
                }
                rho[k + s*M + s*M2 + l*M3] = RHO;
            }
        }
    }

    for (k = 0; k < M; k++)
    {
        for (l = k + 1; l < M; l++)
        {
            for (s = l + 1; s < M; s++)
            {
                RHO = 0;
                #pragma omp parallel private(i,j,t,sqrtOf,v) reduction(+:RHO)
                {
                    v = (int *) malloc(M * sizeof(int));
                    #pragma omp for
                    for (i = 0; i < nc; i++)
                    {
                        if (IF[i][k] < 1) continue;
                        for (t = 0; t < M; t++) v[t] = IF[i][t];
                        sqrtOf = v[s] * sqrt(v[k] * (v[l] + 1));
                        v[k] -= 1;
                        v[l] += 1;
                        j = FockToIndex(N, M, NCmat, v);
                        RHO += conj(C[i]) * C[j] * sqrtOf;
                    }
                    free(v);
                }
                rho[k + s*M + s*M2 + l*M3] = RHO;
            }
        }
    }
        
    /****************************** Rule 8 ******************************/
 
    for (k = 0; k < M; k++)
    {
        for (s = 0; s < M; s++)
        {
            if (s == k) continue;
            for (q = 0; q < M; q++)
            {
                if (q == s || q == k) continue;
                for (l = 0; l < M; l ++)
                {
                    RHO = 0;
                    if (l == k || l == s || l == q) continue;
                    #pragma omp parallel private(i,j,t,sqrtOf,v) \
                    reduction(+:RHO)
                    {
                        v = (int *) malloc(M * sizeof(int));
                        #pragma omp for
                        for (i = 0; i < nc; i++)
                        {
                            if (IF[i][k] < 1 || IF[i][s] < 1) continue;
                            for (t = 0; t < M; t++) v[t] = IF[i][t];
                            sqrtOf = sqrt(v[k]*v[s]*(v[q] + 1)*(v[l] + 1));
                            v[k] -= 1;
                            v[s] -= 1;
                            v[q] += 1;
                            v[l] += 1;
                            j = FockToIndex(N, M, NCmat, v);
                            RHO += conj(C[i]) * C[j] * sqrtOf;
                        }
                        free(v);
                    }
                    rho[k+s*M+q*M2+l*M3] = RHO;
                } // Finish l loop
            } // Finish q loop
        } // Finish s loop
    } // Finish k loop

    /****************************** CC Rule 3 ******************************/

    for (k = 0; k < M; k++)
    {
        for (q = k + 1; q < M; q++)
            rho[q + q*M + k*M2 + k*M3] = conj(rho[k + k*M + q*M2 + q*M3]);
    }
    
    /****************************** CC Rule 4 ******************************/

    for (k = 0; k < M; k++)
    {
        for (l = k + 1; l < M; l++)
        {
            rho[k + k*M + l*M2 + k*M3] = rho[k + k*M + k*M2 + l*M3];
            rho[l + k*M + k*M2 + k*M3] = conj(rho[k + k*M + k*M2 + l*M3]);
            rho[k + l*M + k*M2 + k*M3] = rho[l + k*M + k*M2 + k*M3];
        }
    }
    
    /****************************** CC Rule 5 ******************************/

    for (k = 0; k < M; k++)
    {
        for (s = k + 1; s < M; s++)
        {
            rho[s + k*M + s*M2 + s*M3] = rho[k + s*M + s*M2 + s*M3];
            rho[s + s*M + s*M2 + k*M3] = conj(rho[k + s*M + s*M2 + s*M3]);
            rho[s + s*M + k*M2 + s*M3] = rho[s + s*M + s*M2 + k*M3];
        }
    }
    
    /****************************** CC Rule 6 ******************************/

    for (k = 0; k < M; k++)
    {
        for (q = k + 1; q < M; q++)
        {
            for (l = q + 1; l < M; l++)
            {
                rho[k + k*M + l*M2 + q*M3] = rho[k + k*M + q*M2 + l*M3];
                rho[l + q*M + k*M2 + k*M3] = conj(rho[k + k*M + q*M2 + l*M3]);
                rho[q + l*M + k*M2 + k*M3] = rho[l + q*M + k*M2 + k*M3];
            }
        }
    }

    for (q = 0; q < M; q++)
    {
        for (k = q + 1; k < M; k++)
        {
            for (l = k + 1; l < M; l++)
            {
                rho[k + k*M + l*M2 + q*M3] = rho[k + k*M + q*M2 + l*M3];
                rho[l + q*M + k*M2 + k*M3] = conj(rho[k + k*M + q*M2 + l*M3]);
                rho[q + l*M + k*M2 + k*M3] = rho[l + q*M + k*M2 + k*M3];
            }
        }
    }
    
    for (q = 0; q < M; q++)
    {
        for (l = q + 1; l < M; l++)
        {
            for (k = l + 1; k < M; k++)
            {
                rho[k + k*M + l*M2 + q*M3] = rho[k + k*M + q*M2 + l*M3];
                rho[l + q*M + k*M2 + k*M3] = conj(rho[k + k*M + q*M2 + l*M3]);
                rho[q + l*M + k*M2 + k*M3] = rho[l + q*M + k*M2 + k*M3];
            }
        }
    }
    
    /****************************** CC Rule 7 ******************************/
        
    for (s = 0; s < M; s++)
    {
        for (k = s + 1; k < M; k++)
        {
            for (l = k + 1; l < M; l++)
            {
                rho[k + s*M + l*M2 + s*M3] = rho[k + s*M + s*M2 + l*M3];
                rho[s + k*M + l*M2 + s*M3] = rho[k + s*M + s*M2 + l*M3];
                rho[s + k*M + s*M2 + l*M3] = rho[k + s*M + s*M2 + l*M3];
                rho[l + s*M + s*M2 + k*M3] = conj(rho[k + s*M + s*M2 + l*M3]);
                rho[s + l*M + s*M2 + k*M3] = rho[l + s*M + s*M2 + k*M3];
                rho[s + l*M + k*M2 + s*M3] = rho[l + s*M + s*M2 + k*M3];
                rho[l + s*M + k*M2 + s*M3] = rho[l + s*M + s*M2 + k*M3];
            }
        }
    }
    
    for (k = 0; k < M; k++)
    {
        for (s = k + 1; s < M; s++)
        {
            for (l = s + 1; l < M; l++)
            {
                rho[k + s*M + l*M2 + s*M3] = rho[k + s*M + s*M2 + l*M3];
                rho[s + k*M + l*M2 + s*M3] = rho[k + s*M + s*M2 + l*M3];
                rho[s + k*M + s*M2 + l*M3] = rho[k + s*M + s*M2 + l*M3];
                rho[l + s*M + s*M2 + k*M3] = conj(rho[k + s*M + s*M2 + l*M3]);
                rho[s + l*M + s*M2 + k*M3] = rho[l + s*M + s*M2 + k*M3];
                rho[s + l*M + k*M2 + s*M3] = rho[l + s*M + s*M2 + k*M3];
                rho[l + s*M + k*M2 + s*M3] = rho[l + s*M + s*M2 + k*M3];
            }
        }
    }
    
    for (k = 0; k < M; k++)
    {
        for (l = k + 1; l < M; l++)
        {
            for (s = l + 1; s < M; s++)
            {
                rho[k + s*M + l*M2 + s*M3] = rho[k + s*M + s*M2 + l*M3];
                rho[s + k*M + l*M2 + s*M3] = rho[k + s*M + s*M2 + l*M3];
                rho[s + k*M + s*M2 + l*M3] = rho[k + s*M + s*M2 + l*M3];
                rho[l + s*M + s*M2 + k*M3] = conj(rho[k + s*M + s*M2 + l*M3]);
                rho[s + l*M + s*M2 + k*M3] = rho[l + s*M + s*M2 + k*M3];
                rho[s + l*M + k*M2 + s*M3] = rho[l + s*M + s*M2 + k*M3];
                rho[l + s*M + k*M2 + s*M3] = rho[l + s*M + s*M2 + k*M3];
            }
        }
    }

    /**********                  END OF ROUTINE                  **********/
}

