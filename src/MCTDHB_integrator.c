#include "../include/MCTDHB_integrator.h"





void RecordArray(FILE * f, int M, Carray v)
{   // Given an open file f write the complex array v in a line
    int j;

    for (j = 0; j < M; j ++)
    {   // write in a format suitable to import with numpy
        if (cimag(v[j]) >= 0)
        {
            fprintf(f, "(%.15E+%.15Ej) ", creal(v[j]), cimag(v[j]));
        }
        else
        {
            fprintf(f, "(%.15E%.15Ej) ", creal(v[j]), cimag(v[j]));
        }
    }

    fprintf(f, "\n");
}





void RecordMatrix(FILE * f, int M, Cmatrix A)
{   // Given an open file f write the matrix in Row-major format in a line
    int i, j;

    for (i = 0; i < M; i ++)
    {   // write in a format suitable to import with numpy
        for (j = 0; j < M; j++)
        {
            if (cimag(A[i][j]) >= 0)
            {
                fprintf(f, "(%.15E+%.15Ej) ", creal(A[i][j]), cimag(A[i][j]));
            }
            else
            {
                fprintf(f, "(%.15E%.15Ej) ", creal(A[i][j]), cimag(A[i][j]));
            }
        }
    }

    fprintf(f, "\n");
}





/* ========================================================================
 *
 *                    APPLY THE MANY BODY HAMILTONIAN
 *                    -------------------------------
 *
 * Once defined a set of Single-Particle Wave Functions (SPWF) a many
 * body state  can be expanded  in  a  Occupation Number Basis  (ONB)
 * whose vector are also named Fock states.Then to apply an  operator
 * on a state we need  its  coefficients in this basis  (Cj)  and the 
 * matrix elements of the operator that is done below.
 *
 * ======================================================================== */



void applyHconf (MCTDHBsetup MC, Carray C, Cmatrix Ho, Carray Hint, Carray out)
{ // Apply Many-Body Hamiltonian on a state in configuration basis
    
    int // Index of coeficients
        i,
        j,
        nc = MC->nc;

    int // enumerate orbitals
        k,
        l,
        s,
        q;

    /* ==================================================================== *
     *                                                                      *
     *                         Auxiliary variables                          *
     *                                                                      *
     * ==================================================================== */

    int  M  = MC->Morb,
         N  = MC->Npar,
         M2 = M * M,
         M3 = M * M * M,
         ** IF = MC->IF;
    
    int ** NCmat = MC->NCmat;

    int * v;             // Occupation vector on each iteration

    double sqrtOf;       // Factor from creation/annihilation operator

    double complex rhsI; // line step value of right-hand-side for C

    /* ==================================================================== *
     *                                                                      *
     *    Apply Hamiltonian in a state expanded in Fock-occupation basis    *
     *                                                                      *
     * ==================================================================== */

    #pragma omp parallel firstprivate(N, M, M2, M3) \
    private(i, j, k, l, s, q, rhsI, sqrtOf, v)
    {

    v = (int * ) malloc(M * sizeof(int));

    #pragma omp for
    for (i = 0; i < nc; i++)
    {
        rhsI = 0;

        for (k = 0; k < M; k++) v[k] = IF[i][k];
    
        /* ================================================================ *
         *                                                                  *
         *                       One-body contribution                      *
         *                                                                  *
         * ================================================================ */

        for (k = 0; k < M; k++)
        {
            if (v[k] < 1) continue;
            rhsI += Ho[k][k] * v[k] * C[i];
            for (l = 0; l < M; l++)
            {
                if (l == k) continue;
                sqrtOf = sqrt(v[k] * (v[l] + 1));
                v[k] -= 1;
                v[l] += 1;
                j = FockToIndex(N, M, NCmat, v);
                rhsI += Ho[k][l] * sqrtOf * C[j];
                v[k] += 1;
                v[l] -= 1;
            }
        }


        /* ================================================================ *
         *                                                                  *
         *                       Two-body contribution                      *
         *                                                                  *
         * ================================================================ */


        /* ---------------------------------------------
         * Rule 1: Creation on k k / Annihilation on k k
        ------------------------------------------------------------------- */
        for (k = 0; k < M; k++)
        {
            sqrtOf = v[k] * (v[k] - 1);
            rhsI += Hint[k + M * k + M2 * k + M3 * k] * C[i] * sqrtOf;
        }
        /* ---------------------------------------------------------------- */


        /* ---------------------------------------------
         * Rule 2: Creation on k s / Annihilation on k s
        ------------------------------------------------------------------- */
        for (k = 0; k < M; k++)
        {
            for (s = k + 1; s < M; s++)
            {
                sqrtOf = v[k] * v[s];
                rhsI += Hint[k + s*M + k*M2 + s*M3] * sqrtOf * C[i];
                rhsI += Hint[s + k*M + k*M2 + s*M3] * sqrtOf * C[i];
                rhsI += Hint[s + k*M + s*M2 + k*M3] * sqrtOf * C[i];
                rhsI += Hint[k + s*M + s*M2 + k*M3] * sqrtOf * C[i];
            }
        }
        /* ---------------------------------------------------------------- */


        /* ---------------------------------------------
         * Rule 3: Creation on k k / Annihilation on q q
        ------------------------------------------------------------------- */
        for (k = 0; k < M; k++)
        {
            if (v[k] < 2) continue;
            for (q = 0; q < M; q++)
            {
                if (q == k) continue;
                sqrtOf = sqrt((v[k] - 1) * v[k] * (v[q] + 1) * (v[q] + 2));
                v[k] -= 2;
                v[q] += 2;
                j = FockToIndex(N, M, NCmat, v);
                rhsI += Hint[k + k * M + q * M2 + q * M3] * C[j] * sqrtOf;
                v[k] += 2;
                v[q] -= 2;
            }
        }
        /* ---------------------------------------------------------------- */


        /* ---------------------------------------------
         * Rule 4: Creation on k k / Annihilation on k l
        ------------------------------------------------------------------- */
        for (k = 0; k < M; k++)
        {
            if (v[k] < 2) continue;
            for (l = 0; l < M; l++)
            {
                if (l == k) continue;
                sqrtOf = (v[k] - 1) * sqrt(v[k] * (v[l] + 1));
                v[k] -= 1;
                v[l] += 1;
                j = FockToIndex(N, M, NCmat, v);
                rhsI += Hint[k + k * M + k * M2 + l * M3] * C[j] * sqrtOf;
                rhsI += Hint[k + k * M + l * M2 + k * M3] * C[j] * sqrtOf;
                v[k] += 1;
                v[l] -= 1;
            }
        }
        /* ---------------------------------------------------------------- */


        /* ---------------------------------------------
         * Rule 5: Creation on k s / Annihilation on s s
        ------------------------------------------------------------------- */
        for (k = 0; k < M; k++)
        {
            if (v[k] < 1) continue;
            for (s = 0; s < M; s++)
            {
                if (s == k) continue;
                sqrtOf = v[s] * sqrt(v[k] * (v[s] + 1));
                v[k] -= 1;
                v[s] += 1;
                j = FockToIndex(N, M, NCmat, v);
                rhsI += Hint[k + s * M + s * M2 + s * M3] * C[j] * sqrtOf;
                rhsI += Hint[s + k * M + s * M2 + s * M3] * C[j] * sqrtOf;
                v[k] += 1;
                v[s] -= 1;
            }
        }
        /* ---------------------------------------------------------------- */


        /* -----------------------------------------------------------
         * Rule 6.0: Creation on k k / Annihilation on q l (k < q < l)
        ------------------------------------------------------------------- */
        for (k = 0; k < M; k++)
        {
            if (v[k] < 2) continue;
            for (q = k + 1; q < M; q++)
            {
                for (l = q + 1; l < M; l++)
                {
                    sqrtOf = sqrt(v[k] * (v[k] - 1) * (v[q] + 1) * (v[l] + 1));
                    v[k] -= 2;
                    v[l] += 1;
                    v[q] += 1;
                    j = FockToIndex(N, M, NCmat, v);
                    rhsI += Hint[k + k*M + q*M2 + l*M3] * C[j] * sqrtOf;
                    rhsI += Hint[k + k*M + l*M2 + q*M3] * C[j] * sqrtOf;
                    v[k] += 2;
                    v[l] -= 1;
                    v[q] -= 1;
                }
            }
        }
        /* ---------------------------------------------------------------- */


        /* -----------------------------------------------------------
         * Rule 6.1: Creation on k k / Annihilation on q l (q < k < l)
        ------------------------------------------------------------------- */
        for (q = 0; q < M; q++)
        {
            for (k = q + 1; k < M; k++)
            {
                if (v[k] < 2) continue;
                for (l = k + 1; l < M; l++)
                {
                    sqrtOf = sqrt(v[k] * (v[k] - 1) * (v[q] + 1) * (v[l] + 1));
                    v[k] -= 2;
                    v[l] += 1;
                    v[q] += 1;
                    j = FockToIndex(N, M, NCmat, v);
                    rhsI += Hint[k + k*M + q*M2 + l*M3] * C[j] * sqrtOf;
                    rhsI += Hint[k + k*M + l*M2 + q*M3] * C[j] * sqrtOf;
                    v[k] += 2;
                    v[l] -= 1;
                    v[q] -= 1;
                }
            }
        }
        /* ---------------------------------------------------------------- */


        /* -----------------------------------------------------------
         * Rule 6.2: Creation on k k / Annihilation on q l (q < l < k)
        ------------------------------------------------------------------- */
        for (q = 0; q < M; q++)
        {
            for (l = q + 1; l < M; l++)
            {
                for (k = l + 1; k < M; k++)
                {
                    if (v[k] < 2) continue;
                    sqrtOf = sqrt(v[k] * (v[k] - 1) * (v[q] + 1) * (v[l] + 1));
                    v[k] -= 2;
                    v[l] += 1;
                    v[q] += 1;
                    j = FockToIndex(N, M, NCmat, v);
                    rhsI += Hint[k + k*M + q*M2 + l*M3] * C[j] * sqrtOf;
                    rhsI += Hint[k + k*M + l*M2 + q*M3] * C[j] * sqrtOf;
                    v[k] += 2;
                    v[l] -= 1;
                    v[q] -= 1;
                }
            }
        }
        /* ---------------------------------------------------------------- */


        /* -----------------------------------------------------------
         * Rule 7.0: Creation on k s / Annihilation on q q (q > k > s)
        ------------------------------------------------------------------- */
        for (q = 0; q < M; q++)
        {
            for (k = q + 1; k < M; k++)
            {
                if (v[k] < 1) continue;
                for (s = k + 1; s < M; s++)
                {
                    if (v[s] < 1) continue;
                    sqrtOf = sqrt(v[k] * v[s] * (v[q] + 1) * (v[q] + 2));
                    v[k] -= 1;
                    v[s] -= 1;
                    v[q] += 2;
                    j = FockToIndex(N, M, NCmat, v);
                    rhsI += Hint[k + s*M + q*M2 + q*M3] * C[j] * sqrtOf;
                    rhsI += Hint[s + k*M + q*M2 + q*M3] * C[j] * sqrtOf;
                    v[k] += 1;
                    v[s] += 1;
                    v[q] -= 2;
                }
            }
        }
        /* ---------------------------------------------------------------- */


        /* -----------------------------------------------------------
         * Rule 7.1: Creation on k s / Annihilation on q q (k > q > s)
        ------------------------------------------------------------------- */
        for (k = 0; k < M; k++)
        {
            if (v[k] < 1) continue;
            for (q = k + 1; q < M; q++)
            {
                for (s = q + 1; s < M; s++)
                {
                    if (v[s] < 1) continue;
                    sqrtOf = sqrt(v[k] * v[s] * (v[q] + 1) * (v[q] + 2));
                    v[k] -= 1;
                    v[s] -= 1;
                    v[q] += 2;
                    j = FockToIndex(N, M, NCmat, v);
                    rhsI += Hint[k + s*M + q*M2 + q*M3] * C[j] * sqrtOf;
                    rhsI += Hint[s + k*M + q*M2 + q*M3] * C[j] * sqrtOf;
                    v[k] += 1;
                    v[s] += 1;
                    v[q] -= 2;
                }
            }
        }
        /* ---------------------------------------------------------------- */


        /* -----------------------------------------------------------
         * Rule 7.2: Creation on k s / Annihilation on q q (k > s > q)
        ------------------------------------------------------------------- */
        for (k = 0; k < M; k++)
        {
            if (v[k] < 1) continue;
            for (s = k + 1; s < M; s++)
            {
                if (v[s] < 1) continue;
                for (q = s + 1; q < M; q++)
                {
                    sqrtOf = sqrt(v[k] * v[s] * (v[q] + 1) * (v[q] + 2));
                    v[k] -= 1;
                    v[s] -= 1;
                    v[q] += 2;
                    j = FockToIndex(N, M, NCmat, v);
                    rhsI += Hint[k + s*M + q*M2 + q*M3] * C[j] * sqrtOf;
                    rhsI += Hint[s + k*M + q*M2 + q*M3] * C[j] * sqrtOf;
                    v[k] += 1;
                    v[s] += 1;
                    v[q] -= 2;
                }
            }
        }
        /* ---------------------------------------------------------------- */


        /* ---------------------------------------------
         * Rule 8: Creation on k s / Annihilation on s l
        ------------------------------------------------------------------- */
        for (s = 0; s < M; s++)
        {
            if (v[s] < 1) continue; // may improve performance
            for (k = 0; k < M; k++)
            {
                if (v[k] < 1 || k == s) continue;
                for (l = 0; l < M; l++)
                {
                    if (l == k || l == s) continue;
                    sqrtOf = v[s] * sqrt(v[k] * (v[l] + 1));
                    v[k] -= 1;
                    v[l] += 1;
                    j = FockToIndex(N, M, NCmat, v);
                    rhsI += Hint[k + s*M + s*M2 + l*M3] * C[j] * sqrtOf;
                    rhsI += Hint[s + k*M + s*M2 + l*M3] * C[j] * sqrtOf;
                    rhsI += Hint[s + k*M + l*M2 + s*M3] * C[j] * sqrtOf;
                    rhsI += Hint[k + s*M + l*M2 + s*M3] * C[j] * sqrtOf;
                    v[k] += 1;
                    v[l] -= 1;
                }
            }
        }


        /* ---------------------------------------------
         * Rule 9: Creation on k s / Annihilation on q l
        ------------------------------------------------------------------- */
        for (k = 0; k < M; k++)
        {
            if (v[k] < 1) continue;
            for (s = 0; s < M; s++)
            {
                if (v[s] < 1 || s == k) continue;
                for (q = 0; q < M; q++)
                {
                    if (q == s || q == k) continue;
                    for (l = 0; l < M; l ++)
                    {
                        if (l == k || l == s || l == q) continue;
                        sqrtOf = sqrt(v[k] * v[s] * (v[q] + 1) * (v[l] + 1));
                        v[k] -= 1;
                        v[s] -= 1;
                        v[q] += 1;
                        v[l] += 1;
                        j = FockToIndex(N, M, NCmat, v);
                        rhsI += Hint[k + s*M + q*M2 + l*M3] * C[j] * sqrtOf;
                        v[k] += 1;
                        v[s] += 1;
                        v[q] -= 1;
                        v[l] -= 1;
                    }   // Finish l
                }       // Finish q
            }           // Finish s
        }               // Finish k

        out[i] = rhsI;
    }

    free(v);

    } // End of parallel region

}





double complex Proj_Hint (int M, int k, int i, Cmatrix rho_inv, Carray rho2,
       Carray Hint )
{   // k and i enumerate orbitals maintaned fixed
    int j,
        s,
        q,
        l,
        i_rho2,
        i_Hint;

    double complex ans = 0;

    for (j = 0; j < M; j++)
    {
        for (s = 0; s < M; s++)
        {
            for (q = 0; q < M; q++)
            {
                for (l = 0; l < M; l++)
                {
                    i_rho2 = j + s * M + q * M * M + l * M * M * M;
                    i_Hint = i + s * M + l * M * M + q * M * M * M;
                    ans += rho_inv[k][j] * rho2[i_rho2] * Hint[i_Hint];
                }
            }
        }
    }

    return ans;
}





double complex NonLinear (int M, int k, int n, Cmatrix Omat,
               Cmatrix rho_inv, Carray rho2 )
{   // k enumerate orbital and n a discretized position
    int j,
        s,
        q,
        l,
        i_rho2;

    double complex ans = 0;

    for (j = 0; j < M; j++)
    {
        for (s = 0; s < M; s++)
        {
            for (q = 0; q < M; q++)
            {
                for (l = 0; l < M; l++)
                {
                    i_rho2 = j + s * M + q * M * M + l * M * M * M;
                    ans += rho_inv[k][j] * rho2[i_rho2] * \
                           conj(Omat[s][n]) * Omat[l][n] * Omat[q][n];
                }
            }
        }
    }

    return ans;
}





void OrbDDT (MCTDHBsetup MC, Carray C, Cmatrix Orb, Cmatrix newOrb,
     Cmatrix Ho, Carray Hint)
{   // Right-Hand-Side when isolate orbital's time derivative

    int
        k,
        s,
        j,
        M  = MC->Morb,
        N  = MC->Npar,
        Mpos  = MC->Mpos,
        ** IF = MC->IF,
        ** NCmat = MC->NCmat;

    double
        dx = MC->dx;
    
    double complex
        Proj,
        g = MC->inter;

    /* ==================================================================== *
     *                                                                      *
     *        Setup single/two particle hamiltonian matrix elements         *
     *                                                                      *
     * ==================================================================== */

    SetupHo(M, Mpos, Orb, dx, MC->a2, MC->a1, MC->V, Ho);
    SetupHint(M, Mpos, Orb, dx, g, Hint);

    Cmatrix rho = cmatDef(M, M);
    Cmatrix rho_inv = cmatDef(M, M);
    Carray rho2 = carrDef(M * M * M * M);

    OBrho(N, M, NCmat, IF, C, rho);
    TBrho(N, M, NCmat, IF, C, rho2);

    s = HermitianInv(M, rho, rho_inv);
    if (s != 0)
    { printf("\n\n\t\tFailed on Lapack inversion routine!\n\n"); }

    for (k = 0; k < M; k++)
    {   // Take k orbital
        for (j = 0; j < Mpos; j++)
        {   // At discretized position j
            Proj = 0;
            for (s = 0; s < M; s++)
            {   // projection over 's' orbitals
                Proj += Ho[s][k] * Orb[s][j]; 
                Proj += Proj_Hint(M, k, s, rho_inv, rho2, Hint) * Orb[s][j];
            }
            newOrb[k][j] = \
            -I * (g * NonLinear(M, k, j, Orb, rho_inv, rho2) - Proj);
        }
    }

    free(rho2);
    cmatFree(M, rho);
    cmatFree(M, rho_inv);
}





void OrbConfDDT (MCTDHBsetup MC, Carray C, Cmatrix Orb, Cmatrix Ho,
     Carray Hint, Carray dCdt, Cmatrix dOdt )
{   // Right-Hand-Side of time derivatives both for coefficients and orbitals
    int i;
    OrbDDT(MC, C, Orb, dOdt, Ho, Hint);
    applyHconf(MC, C, Ho, Hint, dCdt);
    for (i = 0; i < MC->nc; i++) dCdt[i] = - I * dCdt[i];
}





void lanczos(MCTDHBsetup MCdata, Cmatrix Ho, Carray Hint,
     int lm, Carray diag, Carray offdiag, Cmatrix lvec)
{   // Improved lanczos iterations  with  reorthogonalization
    // lanczos vectors are store in lvec and diag and offdiag
    // hold the values of tridiagonal real matrix

    int i,
        j,
        k,
        nc = MCdata->nc;

    Carray out = carrDef(nc);
    Carray ortho = carrDef(lm);

    applyHconf(MCdata, lvec[0], Ho, Hint, out);
    diag[0] = carrDot(nc, lvec[0], out);

    for (j = 0; j < nc; j++) out[j] = out[j] - diag[0] * lvec[0][j];

    for (i = 0; i < lm - 1; i++)
    {
        offdiag[i] = carrMod(nc, out);
        carrScalarMultiply(nc, out, 1.0 / offdiag[i], lvec[i + 1]);
        applyHconf(MCdata, lvec[i + 1], Ho, Hint, out);
        for (j = 0; j < nc; j++)
        {
            out[j] = out[j] - offdiag[i]*lvec[i][j];
        }
        diag[i + 1] = carrDot(nc, lvec[i + 1], out);
        for (j = 0; j < nc; j++)
        {
            out[j] = out[j] - diag[i+1]*lvec[i+1][j];
        }
        // Additional re-orthogonalization procedure
        carrFill(lm, 0, ortho);
        for (k = 0; k < i + 2; k++) ortho[k] += carrDot(nc, lvec[k], out);
        for (j = 0; j < nc; j++)
        {
            for (k = 0; k < i + 2; k++) out[j] -= lvec[k][j] * ortho[k];
        }
    }

    free(ortho);
    free(out);
}





void RK4lanczosAfter (MCTDHBsetup MC, Cmatrix Orb, Carray C, double dt)
{   // Use C in next time-step only in k4 on RK4 method for orbitals. Note
    // that lvec[0]  ( initial lanczos vector )  holds current time-step C

    int
        i,
        k,  // Counter
        j,  // Counter
        lm, // Number of lanczos iterations
        M = MC->Morb,
        Mpos = MC->Mpos,
        Npar = MC->Npar;

    lm = 4; // Follows the order of Runge-Kutta



    /* -------------------------------------------------------------
    variables to call lapack diagonalization routine for tridiagonal
    symmetric matrix
    ---------------------------------------------------------------- */
    double
        * d = malloc(lm * sizeof(double)),
        * e = malloc(lm * sizeof(double)),
        * eigvec = malloc(lm * lm * sizeof(double));
    /* ------------------------------------------------------------- */



    /* -----------------------------------------------------
    variables to store lanczos vectors and matrix iterations
    -------------------------------------------------------- */
    // Lanczos Vectors (organize aint rows)
    Cmatrix lvec = cmatDef(lm, MC->nc);
    // Elements of tridiagonal lanczos matrix
    Carray diag = carrDef(lm);
    Carray offdiag = carrDef(lm);
    // Solve system of ODEs in lanczos vector space
    Carray Clanczos = carrDef(lm);
    Carray aux = carrDef(lm);
    /* ----------------------------------------------------- */



    /* ----------------------------------------------------
    Variables to evolve 4-th order Runge-Kutta for Orbitals
    ------------------------------------------------------- */
    Cmatrix Orhs = cmatDef(M, Mpos);
    Cmatrix Onew = cmatDef(M, Mpos);
    Cmatrix Oarg = cmatDef(M, Mpos);
    /* ---------------------------------------------------- */



    /* ---------------------------------------
    One/Two-body Hamiltonian matrices elements
    ------------------------------------------ */
    Cmatrix  Ho = cmatDef(M, M);
    Carray Hint = carrDef(M * M * M *M);
    /* --------------------------------------- */
    
    
    
    /* ---------------------------------------------
    Setup values needed to solve the equations for C
    ------------------------------------------------ */
    offdiag[lm-1] = 0; // Useless
    SetupHo(M, Mpos, Orb, MC->dx, MC->a2, MC->a1, MC->V, Ho);
    SetupHint(M, Mpos, Orb, MC->dx, MC->inter, Hint);
    // Setup initial lanczos vector
    carrCopy(MC->nc, C, lvec[0]);
    /* --------------------------------------------- */



    /* ================================================================= *
    
            SOLVE ODE FOR COEFFICIENTS USING LANCZOS VECTOR SPACE

     * ================================================================= */



    /* --------------------------------------------------------------
    Call Lanczos what setup tridiagonal symmetric and lanczos vectors
    ----------------------------------------------------------------- */
    lanczos(MC, Ho, Hint, lm, diag, offdiag, lvec);
    /* -------------------------------------------------------------- */



    /* --------------------------------------------------------------
    Transfer data to use lapack routine
    ----------------------------------------------------------------- */
    for (k = 0; k < lm; k++)
    {
        d[k] = creal(diag[k]);    // Supposed to be real
        e[k] = creal(offdiag[k]); // Supposed to be real
        for (j = 0; j < lm; j++) eigvec[k * lm + j] = 0;
    }

    k = LAPACKE_dstev(LAPACK_ROW_MAJOR, 'V', lm, d, e, eigvec, lm);
    if (k != 0)
    { printf("\n\n\t\tERROR IN DIAGONALIZATION\n\n"); }
    /* -------------------------------------------------------------- */



    /* --------------------------------------------------------------
    Solve exactly the equation in lanczos vector space using 
    matrix-eigenvalues to exactly exponentiate
    ----------------------------------------------------------------- */
    // Initial condition in Lanczos vector space
    carrFill(lm, 0, Clanczos); Clanczos[0] = 1.0;

    for (k = 0; k < lm; k++)
    {   // Solve in diagonal basis and for this apply eigvec trasformation
        aux[k] = 0;
        for (j = 0; j < lm; j++) aux[k] += eigvec[j*lm + k] * Clanczos[j];
        aux[k] = aux[k] * cexp(- I * d[k] * dt);
    }

    for (k = 0; k < lm; k++)
    {   // Backward transformation from diagonal matrix
        Clanczos[k] = 0;
        for (j = 0; j < lm; j++) Clanczos[k] += eigvec[k*lm + j] * aux[j];
    }

    for (i = 0; i < MC->nc; i++)
    {   // Matrix multiplication by lanczos vector give the solution
        C[i] = 0;
        for (j = 0; j < lm; j++) C[i] += lvec[j][i] * Clanczos[j];
    }
    /* -------------------------------------------------------------- */



    /* ================================================================= *
    
            COMPUTE AND SUM UP FOUR Ks OF RUNGE-KUTTA for ORBITALS

     * ================================================================= */



    /* ------------------------------------------------------------------
    COMPUTE K1 in Orhs
    --------------------------------------------------------------------- */
    OrbDDT(MC, lvec[0], Orb, Orhs, Ho, Hint);
    for (k = 0; k < M; k++)
    {
        for (j = 0; j < Mpos; j++)
        {   // Add K1 contribution
            Onew[k][j] = Orhs[k][j];
            // Prepare next argument to compute K2
            Oarg[k][j] = Orb[k][j] + Orhs[k][j] * 0.5 * dt;
        }
    }



    /* ------------------------------------------------------------------
    COMPUTE K2 in Orhs
    --------------------------------------------------------------------- */
    OrbDDT(MC, lvec[0], Oarg, Orhs, Ho, Hint);
    for (k = 0; k < M; k++)
    {
        for (j = 0; j < Mpos; j++)
        {   // Add K2 contribution
            Onew[k][j] += 2 * Orhs[k][j];
            // Prepare next argument to compute K3
            Oarg[k][j] = Orb[k][j] + Orhs[k][j] * 0.5 * dt;
        }
    }



    /* ------------------------------------------------------------------
    COMPUTE K3 in Orhs
    --------------------------------------------------------------------- */
    OrbDDT(MC, lvec[0], Oarg, Orhs, Ho, Hint);
    for (k = 0; k < M; k++)
    {
        for (j = 0; j < Mpos; j++)
        {   // Add K3 contribution
            Onew[k][j] += 2 * Orhs[k][j];
            // Prepare next argument to compute K4
            Oarg[k][j] = Orb[k][j] + Orhs[k][j] * dt;
        }
    }



    /* ------------------------------------------------------------------
    COMPUTE K4 in Orhs
    --------------------------------------------------------------------- */
    OrbDDT(MC, C, Oarg, Orhs, Ho, Hint);
    for (k = 0; k < M; k++)
    {   // Add k4 contribution
        for (j = 0; j < Mpos; j++) Onew[k][j] += Orhs[k][j];
    }



    /* ------------------------------------------------------------------
    Update orbitals in the given time-step
    --------------------------------------------------------------------- */
    // Until now Onew  holds the sum K1 + 2 * K2 + 2 * K3 + K4
    // from the Fourth order Runge-Kutta algorithm. Therefore:
    for (k = 0; k < M; k++)
    {   // Update Orbitals
        for (j = 0; j < Mpos; j++)
        {
            Orb[k][j] = Orb[k][j] + Onew[k][j] * dt / 6;
        }
    }



    /* ================================================================= *
    
                                RELEASE MEMORY

     * ================================================================= */

    free(d);
    free(e);
    free(eigvec);
    free(diag);
    free(offdiag);
    free(Clanczos);
    free(aux);

    cmatFree(lm, lvec);
    cmatFree(M, Orhs);
    cmatFree(M, Onew);
    cmatFree(M, Oarg);

    cmatFree(M, Ho);
    free(Hint);
}





void RK4lanczosBefore (MCTDHBsetup MC, Cmatrix Orb, Carray C, double dt)
{   // Use C in next time-step in k2 k3 and k4 on  RK4  method for orbitals
    // Note that lvec[0] (initial lanczos vector) holds current time-step C
    
    int
        i,
        k,  // Counter
        j,  // Counter
        lm, // Number of lanczos iterations
        M = MC->Morb,
        Mpos = MC->Mpos,
        Npar = MC->Npar;

    lm = 4; // Follows the order of Runge-Kutta



    /* -------------------------------------------------------------
    variables to call lapack diagonalization routine for tridiagonal
    symmetric matrix
    ---------------------------------------------------------------- */
    double
        * d = malloc(lm * sizeof(double)),
        * e = malloc(lm * sizeof(double)),
        * eigvec = malloc(lm * lm * sizeof(double));
    /* ------------------------------------------------------------- */



    /* -----------------------------------------------------
    variables to store lanczos vectors and matrix iterations
    -------------------------------------------------------- */
    // Lanczos Vectors (organize aint rows)
    Cmatrix lvec = cmatDef(lm, MC->nc);
    // Elements of tridiagonal lanczos matrix
    Carray diag = carrDef(lm);
    Carray offdiag = carrDef(lm);
    // Solve system of ODEs in lanczos vector space
    Carray Clanczos = carrDef(lm);
    Carray aux = carrDef(lm);
    /* ----------------------------------------------------- */



    /* ----------------------------------------------------
    Variables to evolve 4-th order Runge-Kutta for Orbitals
    ------------------------------------------------------- */
    Cmatrix Orhs = cmatDef(M, Mpos);
    Cmatrix Onew = cmatDef(M, Mpos);
    Cmatrix Oarg = cmatDef(M, Mpos);
    /* ---------------------------------------------------- */



    /* ---------------------------------------------
    Setup values needed to solve the equations for C
    ------------------------------------------------ */
    offdiag[lm-1] = 0; // Useless
    Cmatrix  Ho = cmatDef(M, M);
    Carray Hint = carrDef(M * M * M *M);
    SetupHo(M, Mpos, Orb, MC->dx, MC->a2, MC->a1, MC->V, Ho);
    SetupHint(M, Mpos, Orb, MC->dx, MC->inter, Hint);
    // Setup initial lanczos vector
    carrCopy(MC->nc, C, lvec[0]);
    /* --------------------------------------------- */



    /* ================================================================= *
    
            SOLVE ODE FOR COEFFICIENTS USING LANCZOS VECTOR SPACE

     * ================================================================= */



    /* --------------------------------------------------------------
    Call Lanczos what setup tridiagonal symmetric and lanczos vectors
    ----------------------------------------------------------------- */
    lanczos(MC, Ho, Hint, lm, diag, offdiag, lvec);
    /* -------------------------------------------------------------- */



    /* --------------------------------------------------------------
    Transfer data to use lapack routine
    ----------------------------------------------------------------- */
    for (k = 0; k < lm; k++)
    {
        d[k] = creal(diag[k]);    // Supposed to be real
        e[k] = creal(offdiag[k]); // Supposed to be real
        for (j = 0; j < lm; j++) eigvec[k * lm + j] = 0;
    }

    k = LAPACKE_dstev(LAPACK_ROW_MAJOR, 'V', lm, d, e, eigvec, lm);
    if (k != 0)
    { printf("\n\n\t\tERROR IN DIAGONALIZATION\n\n"); }
    /* -------------------------------------------------------------- */



    /* --------------------------------------------------------------
    Solve exactly the equation in lanczos vector space using 
    matrix-eigenvalues to exactly exponentiate
    ----------------------------------------------------------------- */
    // Initial condition in Lanczos vector space
    carrFill(lm, 0, Clanczos); Clanczos[0] = 1.0;

    for (k = 0; k < lm; k++)
    {   // Solve in diagonal basis and for this apply eigvec trasformation
        aux[k] = 0;
        for (j = 0; j < lm; j++) aux[k] += eigvec[j*lm + k] * Clanczos[j];
        aux[k] = aux[k] * cexp(- I * d[k] * dt);
    }

    for (k = 0; k < lm; k++)
    {   // Backward transformation from diagonal matrix
        Clanczos[k] = 0;
        for (j = 0; j < lm; j++) Clanczos[k] += eigvec[k*lm + j] * aux[j];
    }

    for (i = 0; i < MC->nc; i++)
    {   // Matrix multiplication by lanczos vector give the solution
        C[i] = 0;
        for (j = 0; j < lm; j++) C[i] += lvec[j][i] * Clanczos[j];
    }
    /* -------------------------------------------------------------- */



    /* ================================================================= *
    
            COMPUTE AND SUM UP FOUR Ks OF RUNGE-KUTTA for ORBITALS

     * ================================================================= */



    /* ------------------------------------------------------------------
    COMPUTE K1 in Orhs
    --------------------------------------------------------------------- */
    OrbDDT(MC, lvec[0], Orb, Orhs, Ho, Hint);
    for (k = 0; k < M; k++)
    {
        for (j = 0; j < Mpos; j++)
        {   // Add K1 contribution
            Onew[k][j] = Orhs[k][j];
            // Prepare next argument to compute K2
            Oarg[k][j] = Orb[k][j] + Orhs[k][j] * 0.5 * dt;
        }
    }



    /* ------------------------------------------------------------------
    COMPUTE K2 in Orhs
    --------------------------------------------------------------------- */
    OrbDDT(MC, C, Oarg, Orhs, Ho, Hint);
    for (k = 0; k < M; k++)
    {
        for (j = 0; j < Mpos; j++)
        {   // Add K2 contribution
            Onew[k][j] += 2 * Orhs[k][j];
            // Prepare next argument to compute K3
            Oarg[k][j] = Orb[k][j] + Orhs[k][j] * 0.5 * dt;
        }
    }



    /* ------------------------------------------------------------------
    COMPUTE K3 in Orhs
    --------------------------------------------------------------------- */
    OrbDDT(MC, C, Oarg, Orhs, Ho, Hint);
    for (k = 0; k < M; k++)
    {
        for (j = 0; j < Mpos; j++)
        {   // Add K3 contribution
            Onew[k][j] += 2 * Orhs[k][j];
            // Prepare next argument to compute K4
            Oarg[k][j] = Orb[k][j] + Orhs[k][j] * dt;
        }
    }



    /* ------------------------------------------------------------------
    COMPUTE K4 in Orhs
    --------------------------------------------------------------------- */
    OrbDDT(MC, C, Oarg, Orhs, Ho, Hint);
    for (k = 0; k < M; k++)
    {   // Add k4 contribution
        for (j = 0; j < Mpos; j++) Onew[k][j] += Orhs[k][j];
    }



    /* ------------------------------------------------------------------
    Update orbitals in the given time-step
    --------------------------------------------------------------------- */
    // Until now Onew  holds the sum K1 + 2 * K2 + 2 * K3 + K4
    // from the Fourth order Runge-Kutta algorithm. Therefore:
    for (k = 0; k < M; k++)
    {   // Update Orbitals
        for (j = 0; j < Mpos; j++)
        {
            Orb[k][j] = Orb[k][j] + Onew[k][j] * dt / 6;
        }
    }



    /* ================================================================= *
    
                                RELEASE MEMORY

     * ================================================================= */

    free(d);
    free(e);
    free(eigvec);
    free(diag);
    free(offdiag);
    free(Clanczos);
    free(aux);

    cmatFree(lm, lvec);
    cmatFree(M, Orhs);
    cmatFree(M, Onew);
    cmatFree(M, Oarg);

    cmatFree(M, Ho);
    free(Hint);
}





void RK4step (MCTDHBsetup MC, Cmatrix Orb, Carray C, double dt)
{   // Apply 4-th order Runge-Kutta routine given a (COMPLEX)time step

    int i; // Coeficient Index Counter

    int  k, // Orbital counter
         j, // discretized position counter
         M = MC->Morb,
         Mpos = MC->Mpos,
         Npar = MC->Npar;

    Carray Crhs = carrDef(MC->nc);
    Carray Cnew = carrDef(MC->nc);
    Carray Carg = carrDef(MC->nc);

    Cmatrix Orhs = cmatDef(M, Mpos);
    Cmatrix Onew = cmatDef(M, Mpos);
    Cmatrix Oarg = cmatDef(M, Mpos);

    Cmatrix  Ho = cmatDef(M, M);
    Carray Hint = carrDef(M * M * M *M);



    /* ================================================================= *
    
                  COMPUTE AND SUM UP FOUR Ks OF RUNGE-KUTTA

     * ================================================================= */



    /* ------------------------------------------------------------------
    COMPUTE K1 in ?rhs variables
    --------------------------------------------------------------------- */
    OrbConfDDT(MC, C, Orb, Ho, Hint, Crhs, Orhs);

    for (i = 0; i < MC->nc; i++)
    {   // Add K1 contribution
        Cnew[i] = Crhs[i];
        // Prepare next argument to compute K2
        Carg[i] = C[i] + Crhs[i] * 0.5 * dt;
    }

    for (k = 0; k < M; k++)
    {
        for (j = 0; j < Mpos; j++)
        {   // Add K1 contribution
            Onew[k][j] = Orhs[k][j];
            // Prepare next argument to compute K2
            Oarg[k][j] = Orb[k][j] + Orhs[k][j] * 0.5 * dt;
        }
    }



    /* ------------------------------------------------------------------
    COMPUTE K2 in ?rhs variables
    --------------------------------------------------------------------- */
    OrbConfDDT(MC, Carg, Oarg, Ho, Hint, Crhs, Orhs);

    for (i = 0; i < MC->nc; i++)
    {   // Add K2 contribution
        Cnew[i] += 2 * Crhs[i];
        // Prepare next argument to compute K3
        Carg[i] = C[i] + Crhs[i] * 0.5 * dt;
    }

    for (k = 0; k < M; k++)
    {
        for (j = 0; j < Mpos; j++)
        {   // Add K2 contribution
            Onew[k][j] += 2 * Orhs[k][j];
            // Prepare next argument to compute K3
            Oarg[k][j] = Orb[k][j] + Orhs[k][j] * 0.5 * dt;
        }
    }



    /* ------------------------------------------------------------------
    COMPUTE K3 in ?rhs variables
    --------------------------------------------------------------------- */
    OrbConfDDT(MC, Carg, Oarg, Ho, Hint, Crhs, Orhs);

    for (i = 0; i < MC->nc; i++)
    {   // Add K3 contribution
        Cnew[i] += 2 * Crhs[i];
        // Prepare next argument to compute K4
        Carg[i] = C[i] + Crhs[i] * dt;
    }

    for (k = 0; k < M; k++)
    {
        for (j = 0; j < Mpos; j++)
        {   // Add K3 contribution
            Onew[k][j] += 2 * Orhs[k][j];
            // Prepare next argument to compute K4
            Oarg[k][j] = Orb[k][j] + Orhs[k][j] * dt;
        }
    }



    /* ------------------------------------------------------------------
    COMPUTE K4 in ?rhs variables
    --------------------------------------------------------------------- */
    OrbConfDDT(MC, Carg, Oarg, Ho, Hint, Crhs, Orhs);

    for (i = 0; i < MC->nc; i++)
    {   // Add K4 contribution
        Cnew[i] += Crhs[i];
    }

    for (k = 0; k < M; k++)
    {
        for (j = 0; j < Mpos; j++)
        {   // Add K4 contribution
            Onew[k][j] += Orhs[k][j];
        }
    }

    // Until now ?new  holds the sum K1 + 2 * K2 + 2 * K3 + K4
    // from the Fourth order Runge-Kutta algorithm. Therefore:

    for (i = 0; i < MC->nc; i++)
    {   // Update Coeficients
        C[i] = C[i] + Cnew[i] * dt / 6;
    }

    for (k = 0; k < M; k++)
    {   // Update Orbitals
        for (j = 0; j < Mpos; j++)
        {
            Orb[k][j] = Orb[k][j] + Onew[k][j] * dt / 6;
        }
    }

    free(Cnew);
    free(Crhs);
    free(Carg);
    
    cmatFree(M, Orhs);
    cmatFree(M, Onew);
    cmatFree(M, Oarg);

    cmatFree(M, Ho);
    free(Hint);
}





void IRK4step (MCTDHBsetup MC, Cmatrix Orb, Carray C, double complex dt)
{   // Apply 4-th order Runge-Kutta routine given a (COMPLEX)time step

    int
        i,
        k, // Orbital counter
        j, // discretized position counter
        M = MC->Morb,
        Mpos = MC->Mpos,
        Npar = MC->Npar;

    Carray Crhs = carrDef(MC->nc);
    Carray Cnew = carrDef(MC->nc);
    Carray Carg = carrDef(MC->nc);

    Cmatrix Orhs = cmatDef(M, Mpos);
    Cmatrix Onew = cmatDef(M, Mpos);
    Cmatrix Oarg = cmatDef(M, Mpos);

    Cmatrix  Ho = cmatDef(M, M);
    Carray Hint = carrDef(M * M * M *M);



    /* ------------------------------------------------------------------
    COMPUTE K1 in ?rhs variables
    --------------------------------------------------------------------- */
    OrbConfDDT(MC, C, Orb, Ho, Hint, Crhs, Orhs);

    for (i = 0; i < MC->nc; i++)
    {   // Add K1 contribution
        Cnew[i] = Crhs[i];
        // Prepare next argument to compute K2
        Carg[i] = C[i] + Crhs[i] * 0.5 * dt;
    }

    for (k = 0; k < M; k++)
    {
        for (j = 0; j < Mpos; j++)
        {   // Add K1 contribution
            Onew[k][j] = Orhs[k][j];
            // Prepare next argument to compute K2
            Oarg[k][j] = Orb[k][j] + Orhs[k][j] * 0.5 * dt;
        }
    }



    /* ------------------------------------------------------------------
    COMPUTE K2 in ?rhs variables
    --------------------------------------------------------------------- */
    OrbConfDDT(MC, Carg, Oarg, Ho, Hint, Crhs, Orhs);

    for (i = 0; i < MC->nc; i++)
    {   // Add K2 contribution
        Cnew[i] += 2 * Crhs[i];
        // Prepare next argument to compute K3
        Carg[i] = C[i] + Crhs[i] * 0.5 * dt;
    }

    for (k = 0; k < M; k++)
    {
        for (j = 0; j < Mpos; j++)
        {   // Add K2 contribution
            Onew[k][j] += 2 * Orhs[k][j];
            // Prepare next argument to compute K3
            Oarg[k][j] = Orb[k][j] + Orhs[k][j] * 0.5 * dt;
        }
    }



    /* ------------------------------------------------------------------
    COMPUTE K3 in ?rhs variables
    --------------------------------------------------------------------- */
    OrbConfDDT(MC, Carg, Oarg, Ho, Hint, Crhs, Orhs);

    for (i = 0; i < MC->nc; i++)
    {   // Add K3 contribution
        Cnew[i] += 2 * Crhs[i];
        // Prepare next argument to compute K4
        Carg[i] = C[i] + Crhs[i] * dt;
    }

    for (k = 0; k < M; k++)
    {
        for (j = 0; j < Mpos; j++)
        {   // Add K3 contribution
            Onew[k][j] += 2 * Orhs[k][j];
            // Prepare next argument to compute K4
            Oarg[k][j] = Orb[k][j] + Orhs[k][j] * dt;
        }
    }



    /* ------------------------------------------------------------------
    COMPUTE K4 in ?rhs variables
    --------------------------------------------------------------------- */
    OrbConfDDT(MC, Carg, Oarg, Ho, Hint, Crhs, Orhs);

    for (i = 0; i < MC->nc; i++)
    {   // Add K4 contribution
        Cnew[i] += Crhs[i];
    }

    for (k = 0; k < M; k++)
    {
        for (j = 0; j < Mpos; j++)
        {   // Add K4 contribution
            Onew[k][j] += Orhs[k][j];
        }
    }

    // Until now ?new  holds the sum K1 + 2 * K2 + 2 * K3 + K4
    // from the Fourth order Runge-Kutta algorithm. Therefore:

    for (i = 0; i < MC->nc; i++)
    {   // Update Coeficients
        C[i] = C[i] + Cnew[i] * dt / 6;
    }

    for (k = 0; k < M; k++)
    {   // Update Orbitals
        for (j = 0; j < Mpos; j++)
        {
            Orb[k][j] = Orb[k][j] + Onew[k][j] * dt / 6;
        }
    }

    free(Cnew);
    free(Crhs);
    free(Carg);
    
    cmatFree(M, Orhs);
    cmatFree(M, Onew);
    cmatFree(M, Oarg);

    cmatFree(M, Ho);
    free(Hint);
}





void LinearPartSM (int Mpos, int Morb, CCSmat rhs_mat, Carray upper,
     Carray lower, Carray mid, Cmatrix Orb)
{   // Laplacian part. Solve using CN-discretization
    int k, size = Mpos - 1;

    Carray rhs = carrDef(size);

    for (k = 0; k < Morb; k++)
    {   // For each orbital k solve a tridiagonal system obtained by CN
        CCSvec(size, rhs_mat->vec, rhs_mat->col, rhs_mat->m, Orb[k], rhs);
        triCyclicSM(size, upper, lower, mid, rhs, Orb[k]);
    }

    free(rhs);
}





void LinearPartLU (int Mpos, int Morb, CCSmat rhs_mat, Carray upper,
     Carray lower, Carray mid, Cmatrix Orb )
{   // Laplacian part. Solve using CN-discretization
    int k, size = Mpos - 1;

    Carray rhs = carrDef(size);

    for (k = 0; k < Morb; k++)
    {   // For each orbital k solve a tridiagonal system obtained by CN
        CCSvec(size, rhs_mat->vec, rhs_mat->col, rhs_mat->m, Orb[k], rhs);
        triCyclicLU(size, upper, lower, mid, rhs, Orb[k]);
    }

    free(rhs);
}





    /* ================================================================== *
     *                                                                    *
     *              Give the solution after some time steps               *
     *                                                                    *
     * Given the structure MCTDHBsetup whose contains all  relevant       *
     * parameters, do the time (real or complex) evolution  calling       *
     * separetely the nonlinear part together with the  coeficients       *
     * in half of the time step. Then It evolves an entire step the       *
     * linear part of orbital's equation. Finally evolve  one  more       *
     * half time step the nonlinear part.                                 */





void MCTDHB_CN_REAL (MCTDHBsetup MC, Cmatrix Orb, Carray C, double dt,
     int Nsteps, int method, int cyclic, char fname [], int n)
{
    int
        i,
        k,
        l,
        s,
        q,
        Mpos;

    Mpos = MC->Mpos;

    double
        dx = MC->dx,
        a2 = MC->a2;

    double complex
        tr,
        a1 = MC->a1;

    // used to store matrix elements of linear part
    Carray
        to_int = carrDef(Mpos),
        upper  = carrDef(Mpos - 1),
        lower  = carrDef(Mpos - 1),
        mid    = carrDef(Mpos - 1);

    Cmatrix
        rho = cmatDef(MC->Morb, MC->Morb);

    CCSmat
        rhs_mat;

    char
        fname_orb[120],
        fname_rho[120];

    FILE
        * out_orb,
        * out_rho;



    strcpy(fname_orb, fname);
    strcat(fname_orb, "_orb_realtime.dat");
    strcpy(fname_rho, fname);
    strcat(fname_rho, "_rho_realtime.dat");

    out_orb = fopen(fname_orb, "w");
    if (out_orb == NULL)
    {
        printf("\n\nError: Impossible to open %s\n", fname_orb);
        exit(EXIT_FAILURE);
    }

    out_rho = fopen(fname_rho, "w");
    if (out_orb == NULL)
    {
        printf("\n\nError: Impossible to open %s\n", fname_rho);
        exit(EXIT_FAILURE);
    }



    // Record initial data
    OBrho(MC->Npar, MC->Morb, MC->NCmat, MC->IF, C, rho);
    for (k = 0; k < MC->Morb; k++) RecordArray(out_orb, Mpos, Orb[k]);
    RecordMatrix(out_rho, MC->Morb, rho);



    /* ------------------------------------------------------------------- *
     *         Setup Right-Hand-Side matrix of linear part of PDE          *
     * ------------------------------------------------------------------- */

    // fill main diagonal (use upper as auxiliar array)
    carrFill(Mpos - 1, - a2 * dt / dx / dx + I, upper);
    rcarrUpdate(Mpos - 1, upper, dt, MC->V, mid);

    // fill upper diagonal
    carrFill(Mpos - 1, a2 * dt / dx / dx / 2 + a1 * dt / dx / 4, upper);
    if (cyclic) { upper[Mpos-2] = a2 * dt / dx / dx / 2 - a1 * dt / dx / 4; }
    else        { upper[Mpos-2] = 0;                                        }

    // fill lower diagonal
    carrFill(Mpos - 1, a2 * dt / dx / dx / 2 - a1 * dt / dx / 4, lower);
    if (cyclic) { lower[Mpos-2] = a2 * dt / dx / dx / 2 + a1 * dt / dx / 4; }
    else        { lower[Mpos-2] = 0;                                        }

    // Store in CCS format the RHS of discretized system of equations
    rhs_mat = CyclicToCCS(Mpos - 1, upper, lower, mid);



    /* ------------------------------------------------------------------- *
     *                    Setup Cyclic tridiagonal matrix                  *
     * ------------------------------------------------------------------- */

    // fill main diagonal (use upper as auxiliar array)
    carrFill(Mpos - 1, a2 * dt / dx / dx + I, upper);
    rcarrUpdate(Mpos - 1, upper, -dt, MC->V, mid);

    // fill upper diagonal
    carrFill(Mpos - 1, - a2 * dt / dx / dx / 2 - a1 * dt / dx / 4, upper);
    if (cyclic) { upper[Mpos-2] = - a2 * dt / dx / dx / 2 + a1 * dt / dx / 4; }
    else        { upper[Mpos-2] = 0;                                          }

    // fill lower diagonal
    carrFill(Mpos - 1, - a2 * dt / dx / dx / 2 + a1 * dt / dx / 4, lower);
    if (cyclic) { lower[Mpos-2] = - a2 * dt / dx / dx / 2 - a1 * dt / dx / 4; }
    else        { lower[Mpos-2] = 0;                                          }



    q = 0;
    for (i = 0; i < Nsteps; i++)
    {
        /* Half step nonlinear part
         * -------------------------------------- */
        if (method == 12 || method == 22)
            RK4lanczosBefore(MC, Orb, C, dt / 2);
        else
            RK4step(MC, Orb, C, dt / 2);
        /* -------------------------------------- */


        /* One step linear part
         * --------------------------------------------------------------- */
        if (method == 21 || method == 22)
            LinearPartLU(Mpos, MC->Morb, rhs_mat, upper, lower, mid, Orb);
        else
            LinearPartSM(Mpos, MC->Morb, rhs_mat, upper, lower, mid, Orb);
        // The boundary
        if (cyclic)
        { for (k = 0; k < MC->Morb; k++) Orb[k][Mpos-1] = Orb[k][0]; }
        else
        { for (k = 0; k < MC->Morb; k++) Orb[k][Mpos-1] = 0;         }
        /* --------------------------------------------------------------- */


        /* Another Half step nonlinear part
         * -------------------------------------- */
        if (method == 12 || method == 22)
            RK4lanczosAfter(MC, Orb, C, dt / 2);
        else
            RK4step(MC, Orb, C, dt / 2);
        /* -------------------------------------- */


        // Build rho after have evolved one step
        OBrho(MC->Npar, MC->Morb, MC->NCmat, MC->IF, C, rho);
        tr = 0;
        for (k = 0; k < MC->Morb; k++) tr = tr + rho[k][k];



        /* ----------------------------------------------------------------
         * print to check orthogonality/Energy
        ------------------------------------------------------------------- */
        printf("\n\nAfter %d time steps, Tr(rho) = ", i + 1); cPrint(tr);
        printf(". Orthogonality matrix is:\n\n");
        for (k = 0; k < MC->Morb; k++)
        {
            printf("\n\t");
            for (l = 0; l < MC->Morb; l++)
            {
                for (s = 0; s < MC->Mpos; s++)
                {
                    to_int[s] = conj(Orb[k][s]) * Orb[l][s];
                }
                printf(" "); cPrint(Csimps(MC->Mpos, to_int, MC->dx));
            }
        }
        printf("\n\n|| C || = %.6lf", carrMod(MC->nc, C));
        /* ---------------------------------------------------------------- */



        // record data every n steps
        if (q == n)
        {
            q = 0;
            RecordMatrix(out_rho, MC->Morb, rho);
            for (k = 0; k < MC->Morb; k++)
            {
                RecordArray(out_orb, Mpos, Orb[k]);
            }
        }
        else { q = q + 1; }
    }

    fclose(out_orb);
    fclose(out_rho);

    cmatFree(MC->Morb, rho);
    CCSFree(rhs_mat);
    free(to_int);
    free(upper);
    free(lower);
    free(mid);
}





void MCTDHB_CN_IMAG (MCTDHBsetup MC, Cmatrix Orb, Carray C, Carray E,
     double dT, int Nsteps, int cyclic)
{
    int i,
        k,
        l,
        s,
        Mpos = MC->Mpos;

    double
        dx = MC->dx,
        a2 = MC->a2,
        inter = MC->inter,
        * V = MC->V;

    double complex
        a1 = MC->a1,
        dt = - I * dT;

    // used to store matrix elements of linear part
    Carray
        upper  = carrDef(Mpos - 1),
        lower  = carrDef(Mpos - 1),
        mid    = carrDef(Mpos - 1),
        to_int = carrDef(Mpos);

    CCSmat
        rhs_mat;
    
    

    // Store the initial guess energy
    E[0] = Energy(MC, Orb, C);

    // Configure the linear system from Crank-Nicolson scheme
    rhs_mat = conf_linear(Mpos, dx, dt, a2, a1, inter, V, cyclic, upper, lower, mid);



    for (i = 0; i < Nsteps; i++)
    {
        IRK4step(MC, Orb, C, dt / 2);

        LinearPartSM(Mpos, MC->Morb, rhs_mat, upper, lower, mid, Orb);

        // The boundary
        if (cyclic)
        { for (k = 0; k < MC->Morb; k++) Orb[k][Mpos-1] = Orb[k][0]; }
        else
        { for (k = 0; k < MC->Morb; k++) Orb[k][Mpos-1] = 0;         }

        IRK4step(MC, Orb, C, dt / 2);

        // Loss of Norm => undefined behavior on orthogonalization
        Ortonormalize(MC->Morb, Mpos, dx, Orb);
        // Renormalize coeficients
        renormalizeVector(MC->nc, C, 1.0);
        // Store energy
        E[i + 1] = Energy(MC, Orb, C);
        
        
        // Adapt time step
        if ((i+1) % 2000 == 0)
        {
            dt = dt * (1 + 0.2);
            dT = dT * (1 + 0.2);
            CCSFree(rhs_mat); // Erase old matrix to setup new one
            rhs_mat = conf_linear(Mpos, dx, dt, a2, a1, inter,
                      V, cyclic, upper, lower, mid);
        }



        /* ----------------------------------------------------------------
         * print to check orthogonality/Energy
        ------------------------------------------------------------------- */
        printf("\n\nAfter %d time steps, Energy = ", i + 1);
        cPrint(E[i + 1]);
        printf(". Orthogonality matrix is:\n\n");
        for (k = 0; k < MC->Morb; k++)
        {
            printf("\n\t");
            for (l = 0; l < MC->Morb; l++)
            {
                for (s = 0; s < MC->Mpos; s++)
                {
                    to_int[s] = conj(Orb[k][s]) * Orb[l][s];
                }
                printf(" "); cPrint(Csimps(MC->Mpos, to_int, MC->dx));
            }
        }
        printf("\n\n|| C || = %.6lf", carrMod(MC->nc, C));
        /* ---------------------------------------------------------------- */
    }

    CCSFree(rhs_mat);
    free(to_int);
    free(upper);
    free(lower);
    free(mid);
}
