//@HEADER
// ************************************************************************
//     Genten Tensor Toolbox
//     Software package for tensor math by Sandia National Laboratories
//
// Sandia National Laboratories is a multimission laboratory managed
// and operated by National Technology and Engineering Solutions of Sandia,
// LLC, a wholly owned subsidiary of Honeywell International, Inc., for the
// U.S. Department of Energy’s National Nuclear Security Administration under
// contract DE-NA0003525.
//
// Copyright 20XX, Sandia Corporation.
// ************************************************************************
//@HEADER


/*!
  @file Genten_CpAls.h
  @brief CP-ALS algorithm, in template form to allow different data tensor types.
*/

#pragma once
#include "Genten_Sptensor.h"
#include "Genten_Ktensor.h"

namespace Genten {

  //! Contains CP-ALS performance information for one iteration.
  /*!
   * The container is used by cpals_core to return information about CP-ALS
   * algorithm performance.  An array of this type is passed to cpals_core,
   * and elements filled with information for a particular iteration.
   * For examples, see Genten_Test/Genten_Test_CpAls.cpp and
   * Genten_PerfTest/Genten_CpAlsRandomKtensor.cpp.
   *
   * @param nIter     The completed CP-ALS iteration that this information
   *                  refers to.  If zero, then information is for the
   *                  starting point.  If -1, then the container has no
   *                  valid information.
   * @param dResNorm  Square root of the Frobenius norm of the residual at
   *                  the end of iteration nIter.
   * @param dFit      Fit number (see Genten::cpals_core) at the end
   *                  of iteration nIter.
   * @param dCumTime  Wall clock time in seconds measured from the start of
   *                  cpals_core to the end of iteration nIter.
   */
  typedef struct
  {
    int       nIter;
    ttb_real  dResNorm;
    ttb_real  dFit;
    ttb_real  dCumTime;
  } CpAlsPerfInfo;


  //! Compute the CP decomposition of a tensor based on a least squares objective.
  /*!
   *  Compute an estimate of the best rank-R CP model of a tensor X
   *  using an alternating least-squares algorithm.  The input X can be
   *  a (dense) Tensor or (sparse) Sptensor.  The result is a Ktensor.
   *
   *  The CP-ALS objective function minimizes the least squares difference
   *  between X and the factorization (i.e., the Frobenius norm).
   *  Convergence is based on change in the "fit function"
   *    1 - (residual norm / ||X||_F)
   *  This function is loosely the proportion of data described by the
   *  CP model.  A fit of 1 is perfect, 0 is poor.
   *
   *  An initial guess of factor matrices must be provided.  Note that the
   *  Matlab cp_als call overwrites the weights in the guess to be 1.
   *  If the initial guess leads to a singular RxR intermediate matrix,
   *  then the factorization will fail; for example, an initial factor
   *  matrix cannot be all zeroes.
   *
   *  @param[in] x          Data tensor to be fit by the model.
   *  @param[in,out] u      Input contains an initial guess for the factors.
   *                        The size of each mode must match the corresponding
   *                        mode of x, and the number of components determines
   *                        how many will be in the result.
   *                        Output contains resulting factorization Ktensor.
   *  @param[in] tol        Stop tolerance for convergence of "fit function".
   *  @param[in] maxIters   Maximum number of iterations allowed.
   *  @param[in] maxSecs    Maximum execution time allowed (CP-ALS will finish
   *                        the current iteration before exiting).
   *                        If negative, execute without a time limit.
   *  @param[in] printIter  Print progress every n iterations.
   *                        If zero, print nothing.
   *  @param[out] numIters  Number of iterations actually completed.
   *  @param[out] resNorm   Square root of Frobenius norm of the residual.
   *  @param[in] perfIter   Add performance information every n iterations.
   *                        If zero, do not collect info.
   *  @param[out] perfInfo  Performance information array.  Must allocate
   *                        (maxIters / perfIter) + 2 elements of type
   *                        CpAlsPerfInfo.
   *                        Can be NULL if perfIter is zero.
   *
   *  @throws string        if internal linear solve detects singularity,
   *                        or tensor arguments are incompatible.
   */

  template<class T>
  void cpals_core (const T             & x,
                   Genten::Ktensor  & u,
                   const ttb_real        tol,
                   const ttb_indx        maxIters,
                   const ttb_real        maxSecs,
                   const ttb_indx        printIter,
                   ttb_indx      & numIters,
                   ttb_real      & resNorm,
                   const ttb_indx        perfIter,
                   CpAlsPerfInfo   perfInfo[]);

}
