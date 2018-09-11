#ifndef _MCTDHB_observables_h
#define _MCTDHB_observables_h

#ifdef _OPENMP
    #include <omp.h>
#endif

#include "calculus.h"
#include "array_memory.h"
#include "coef_routines.h"
#include "MCTDHB_datatype.h"



/* ======================================================================== */
/*                                                                          */
/*                           FUNCTION PROTOTYPES                            */
/*                                                                          */
/* ======================================================================== */



/* ========================================================================
 *                                                                *
 *        Setup matrix elements of one-body Hamiltonian Ho        *
 *        ------------------------------------------------        *
 *                                                                */



void SetupHo
(
    int Morb,
    int Mpos,
    Cmatrix Omat,
    double dx,
    double a2,
    double complex a1,
    Rarray V,
    Cmatrix Ho
);



/* ========================================================================
 *                                                                  *
 *        Setup matrix elements of two-body Hamiltonian Hint        *
 *        --------------------------------------------------        *
 *                                                                  */



void SetupHint
(
    int Morb,
    int Mpos,
    Cmatrix Omat,
    double dx,
    double inter,
    Carray Hint
);



/* ========================================================================
 *                                                                  *
 *        Energy for a given set of orbitals and coeficients        *
 *        --------------------------------------------------        *
 *                                                                  */



double complex Energy (MCTDHBsetup mc, Cmatrix Orb, Carray C);

#endif