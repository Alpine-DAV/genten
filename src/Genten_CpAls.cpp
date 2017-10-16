//@HEADER
// ************************************************************************
//     Genten: Software for Generalized Tensor Decompositions
//     by Sandia National Laboratories
//
// Sandia National Laboratories is a multimission laboratory managed
// and operated by National Technology and Engineering Solutions of Sandia,
// LLC, a wholly owned subsidiary of Honeywell International, Inc., for the
// U.S. Department of Energy's National Nuclear Security Administration under
// contract DE-NA0003525.
//
// Copyright 2017 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// 1. Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
// ************************************************************************
//@HEADER


/*!
  @file Genten_CpAls.cpp
  @brief CP-ALS algorithm, in template form to allow different data tensor types.
*/

#include <assert.h>
#include <float.h>
#include <stdio.h>
#include <sstream>

#include "Genten_Array.hpp"
#include "Genten_CpAls.hpp"
#include "Genten_FacMatrix.hpp"
#include "Genten_Ktensor.hpp"
#include "Genten_MixedFormatOps.hpp"
#include "Genten_Sptensor.hpp"
#include "Genten_Sptensor_perm.hpp"
#include "Genten_Sptensor_row.hpp"
#include "Genten_SystemTimer.hpp"
#include "Genten_Util.hpp"

using namespace std;


namespace Genten {

  //------------------------------------------------------
  // FORWARD REFERENCES
  //------------------------------------------------------
  static ttb_real computeResNorm(const ttb_real  xNorm,
                                 const ttb_real  mNorm,
                                 const ttb_real  xDotm);


  //------------------------------------------------------
  // PUBLIC FUNCTIONS
  //------------------------------------------------------

  /*
   *  Copied from the header file:
   *
   *  @param[in] x          Data tensor to be fit by the model.
   *  @param[in/out] u      Input contains an initial guess for the factors.
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
                   CpAlsPerfInfo   perfInfo[])
  {

    // Check size compatibility of the arguments.
    if (u.isConsistent() == false)
      Genten::error("Genten::cpals_core - ktensor u is not consistent");
    if (x.ndims() != u.ndims())
      Genten::error("Genten::cpals_core - u and x have different num dims");
    for (ttb_indx  i = 0; i < x.ndims(); i++)
    {
      if (x.size(i) != u[i].nRows())
        Genten::error("Genten::cpals_core - u and x have different size");
    }

    // Start timer for total execution time of the algorithm.
    const int timer_cpals = 0;
    const int timer_mttkrp = 1;
    const int timer_ip = 2;
    const int timer_gramian = 3;
    const int timer_solve = 4;
    const int timer_scale = 5;
    const int timer_norm = 6;
    Genten::SystemTimer  timer(7);

    timer.start(timer_cpals);

    // Initialize CpAlsPerfInfo.
    if ((perfIter > 0) && (perfInfo == NULL))
    {
      Genten::error("Genten::cpals_core - perfInfo requested but needs to be allocated");
    }
    int  nNextPerf = 0;
    if (perfIter > 0)
    {
      ttb_indx  nPossibleInfo = 2 + (maxIters / perfIter);
      for (ttb_indx  i = 0; i < nPossibleInfo; i++)
      {
        perfInfo[i].nIter = -1;
        perfInfo[i].dResNorm = -1.0;
        perfInfo[i].dFit = -1.0;
        perfInfo[i].dCumTime = -1.0;
        perfInfo[i].dmttkrp_gflops = -1.0;
      }
    }

    ttb_indx nc = u.ncomponents();     // number of components
    ttb_indx nd = x.ndims();           // number of dimensions

    // Distribute the initial guess to have weights of one.
    u.distribute(0);
    Genten::Array lambda(nc, (ttb_real) 1.0);

    // Define gamma, an array of Gramian Matrices corresponding to the
    // factor matrices in u.
    // Note that we can skip the first entry of gamma since
    // it's not used in the first inner iteration.
    Genten::FacMatArray gamma(nd);
    for (ttb_indx n = 0; n < nd; n ++)
    {
      gamma[n] = FacMatrix(u[n].nCols(), u[n].nCols());
    }
    for (ttb_indx n = 1; n < nd; n ++)
    {
      gamma[n].gramian(u[n]);
    }

    // Define upsilon to store Hadamard products of the gamma matrices.
    // The matrix is called Z in the Matlab Computations paper.
    Genten::FacMatrix upsilon(nc,nc);

    // Define a temporary matrix used in the loop.
    Genten::FacMatrix tmpMat(nc,nc);

    // Pre-calculate the Frobenius norm of the tensor x.
    ttb_real xNorm = x.norm();

    ttb_real fit;
    ttb_real fitold;
    if (perfInfo > 0)
    {
      // Compute residual norm and fit of the initial guess.
      // Fit can be a huge negative number for bad start points,
      // so bound it at zero.
      ttb_real  dUnorm = u.normFsq();
      ttb_real  dXtU = innerprod (x, u, lambda);
      //ttb_real  d = (xNorm * xNorm) + (dUnorm * dUnorm) - (2 * dXtU);
      perfInfo[nNextPerf].dResNorm = computeResNorm(xNorm, dUnorm, dXtU);
      fit = 1.0 - (perfInfo[nNextPerf].dResNorm / xNorm);
      if (fit < 0.0)
        fit = 0.0;
      perfInfo[nNextPerf].dFit = fit;

      perfInfo[nNextPerf].nIter = 0;
      perfInfo[nNextPerf].dCumTime = timer.getTotalTime(timer_cpals);
      nNextPerf++;
    }
    else
    {
      // Initial fit is unknown, assume the worst case.
      fit = 0.0;
    }


    //--------------------------------------------------
    // Main algorithm loop.
    //--------------------------------------------------
    for (numIters = 0; numIters < maxIters; numIters++)
    {

//            cout<<"TBD +++++ begin cpals iter " << numIters << endl;
      fitold = fit;

      // Iterate over all N modes of the tensor
      for (ttb_indx n = 0; n < nd; n++)
      {
//                cout<<"TBD   begin inner iter over mode "<<n<<"\n";
        // Update u[n] via MTTKRP with x (Khattri-Rao product).
        // The size of u[n] is dim(n) rows by R columns.
        timer.start(timer_mttkrp);
        Genten::mttkrp (x, u, n);
        timer.stop(timer_mttkrp);

        // Compute the matrix of coefficients in the solve step.
        upsilon = 1;
        for (ttb_indx idx = 0; idx < nd; idx ++)
        {
          if (idx != n)
          {
            upsilon.times(gamma[idx]);
          }
        }
//                print_matrix(upsilon,cout,"TBD upsilon");

        // Solve upsilon * X = u[n]' for X, and overwrite u[n]
        // with the result.  Equivalent to the Matlab operation
        //   u[n] = (upsilon \ u[n]')'.
        timer.start(timer_solve);
        u[n].solveTransposeRHS (upsilon);
        timer.stop(timer_solve);
//                print_matrix(u[n],cout,"TBD after solve");

        // Compute lambda.
        timer.start(timer_norm);
        if (numIters == 0)
        {
          // L2 norm on first iteration.
          u[n].colNorms(NormTwo, lambda, 0.0);
        }
        else
        {
          // L0 norm (max) on other iterations.
          u[n].colNorms(NormInf, lambda, 1.0);
        }
        timer.stop(timer_norm);

        // Scale u[n] by the inverse of lambda.
        //TBD...this can throw an exception, divide-by-zero,
        //      if a column's norm is zero
        //      I caused it with an unfortunate initial ktensor guess
        timer.start(timer_scale);
        u[n].colScale(lambda, true);
        timer.stop(timer_scale);
//                print_ktensor(u,cout,"TBD u after rescaling");

        // Update u[n]'s corresponding Gramian matrix.
        timer.start(timer_gramian);
        gamma[n].gramian(u[n]);
        timer.stop(timer_gramian);
      }


      // Compute Frobenius norm of "p", the current factorization
      // consisting of lambda and u.
      upsilon.times(gamma[nd-1]);
      tmpMat.oprod(lambda);
      upsilon.times(tmpMat);
      ttb_real pNorm = sqrt(fabs(upsilon.sum()));

      // Compute inner product of input data x with "p".
      timer.start(timer_ip);
      ttb_real xpip = innerprod (x, u, lambda);
      timer.stop(timer_ip);

      // Compute the Frobenius norm of the residual using quantities
      // formed earlier.
      resNorm = computeResNorm(xNorm, pNorm, xpip);

      // Compute the relative fit and change since the last iteration.
      fit = 1 - (resNorm / xNorm);
      ttb_real fitchange = fabs(fitold - fit);

      // Print progress of the current iteration.
      if ((printIter > 0) && (((numIters + 1) % printIter) == 0))
      {
        printf ("Iter %2d: fit = %13.6e  fitdelta = %8.1e\n",
                (int) (numIters + 1), fit, fitchange);
      }

      // Fill in performance information if requested.
      if ((perfIter > 0) && ((numIters + 1) % perfIter == 0))
      {
        perfInfo[nNextPerf].nIter = (int) (numIters + 1);
        perfInfo[nNextPerf].dResNorm = resNorm;
        perfInfo[nNextPerf].dFit = fit;
        perfInfo[nNextPerf].dCumTime = timer.getTotalTime(timer_cpals);
        nNextPerf++;
      }

      // Check for convergence.
      if ( ((numIters > 0) && (fitchange < tol)) ||
           ((maxSecs >= 0.0) && (timer.getTotalTime(timer_cpals) > maxSecs)) )
      {
        break;
      }
    }

    // Increment so the count starts from one.
    numIters++;

    // Normalize the final result, incorporating the final lambda values.
    u.normalize(Genten::NormTwo);
    lambda.times(u.weights());
    //lambda.print(std::cout);
    u.setWeights(lambda);
    u.arrange();

    //TBD...matlab calls "fixsigns"

    timer.stop(timer_cpals);

    // Compute MTTKRP floating-point throughput
    const ttb_real mttkrp_total_time = timer.getTotalTime(timer_mttkrp);
    const ttb_real mttkrp_avg_time = timer.getAvgTime(timer_mttkrp);

    // Use double for these to ensure sufficient precision
    const double atomic = 1.0; // cost of atomic measured in flops
    const double mttkrp_flops =
      x.nnz()*nc*(nd+atomic);
    const double mttkrp_reads =
      x.nnz()*((nd*nc+3)*sizeof(ttb_real)+nd*sizeof(ttb_indx));
    const double mttkrp_tput =
      ( mttkrp_flops / mttkrp_avg_time ) / (1024.0 * 1024.0 * 1024.0);
    const double mttkrp_factor = mttkrp_flops / mttkrp_reads;

    // Fill in performance information if requested.
    if (perfIter > 0)
    {
      perfInfo[nNextPerf].nIter = (int) numIters;
      perfInfo[nNextPerf].dResNorm = resNorm;
      perfInfo[nNextPerf].dFit = fit;
      perfInfo[nNextPerf].dCumTime = timer.getTotalTime(timer_cpals);
      perfInfo[nNextPerf].dmttkrp_gflops = mttkrp_tput;
    }

    if (printIter > 0)
    {
      printf ("CpAls completed %d iterations in %.3f seconds\n",
              (int) numIters, timer.getTotalTime(timer_cpals));
      printf ("\tMTTKRP total time = %.3f seconds, average time = %.3f seconds\n",
              mttkrp_total_time, mttkrp_avg_time);
      printf ("\tMTTKRP throughput = %.3f GFLOP/s, bandwidth factor = %.3f\n",
              mttkrp_tput, mttkrp_factor);
      printf ("\tInner product total time = %.3f seconds, average time = %.3f seconds\n",
              timer.getTotalTime(timer_ip),
              timer.getAvgTime(timer_ip));
      printf ("\tGramian total time = %.3f seconds, average time = %.3f seconds\n",
              timer.getTotalTime(timer_gramian),
              timer.getAvgTime(timer_gramian));
      printf ("\tSolve total time = %.3f seconds, average time = %.3f seconds\n",
              timer.getTotalTime(timer_solve),
              timer.getAvgTime(timer_solve));
      printf ("\tScale total time = %.3f seconds, average time = %.3f seconds\n",
              timer.getTotalTime(timer_scale),
              timer.getAvgTime(timer_scale));
      printf ("\tNorm total time = %.3f seconds, average time = %.3f seconds\n",
              timer.getTotalTime(timer_norm),
              timer.getAvgTime(timer_norm));
    }

    return;
  }



  // Declare all possible template instantiations for the compiler.

  template
  void cpals_core<Genten::Sptensor>(const Sptensor& x,
                                    Genten::Ktensor  & u,
                                    const ttb_real        tol,
                                    const ttb_indx        maxIters,
                                    const ttb_real        maxSecs,
                                    const ttb_indx        printIter,
                                    ttb_indx      & numIters,
                                    ttb_real      & resNorm,
                                    const ttb_indx        perfIter,
                                    CpAlsPerfInfo   perfInfo[]);

  template
  void cpals_core<Genten::Sptensor_perm>(const Sptensor_perm & x,
                                         Genten::Ktensor  & u,
                                         const ttb_real        tol,
                                         const ttb_indx        maxIters,
                                         const ttb_real        maxSecs,
                                         const ttb_indx        printIter,
                                         ttb_indx      & numIters,
                                         ttb_real      & resNorm,
                                         const ttb_indx        perfIter,
                                         CpAlsPerfInfo   perfInfo[]);

  template
  void cpals_core<Genten::Sptensor_row>(const Sptensor_row  & x,
                                        Genten::Ktensor  & u,
                                        const ttb_real        tol,
                                        const ttb_indx        maxIters,
                                        const ttb_real        maxSecs,
                                        const ttb_indx        printIter,
                                        ttb_indx      & numIters,
                                        ttb_real      & resNorm,
                                        const ttb_indx        perfIter,
                                        CpAlsPerfInfo   perfInfo[]);


  //------------------------------------------------------
  // PRIVATE FUNCTIONS
  //------------------------------------------------------

  /*
   * Compute the residual Frobenius norm between data tensor X
   * and Ktensor model M as
   *     sqrt{ |X|^2_F - 2(X . M) + |M|^2_F }
   *
   * The residual can be slightly negative due to roundoff errors
   * if the model is a nearly exact fit to the data.  The threshold
   * for fatal error was determined from experimental observations.
   */
  static ttb_real computeResNorm(const ttb_real  xNorm,
                                 const ttb_real  mNorm,
                                 const ttb_real  xDotm)
  {
    ttb_real  d = (xNorm * xNorm) + (mNorm * mNorm) - (2 * xDotm);

    ttb_real  result;
    ttb_real dSmallNegThresh = -(xDotm * sqrt(MACHINE_EPSILON) * 1e3);
    if (d > DBL_MIN)
      result = sqrt(d);
    else if (d > dSmallNegThresh)
      result = 0.0;
    else
    {
      ostringstream  sMsg;
      sMsg << "Genten::cpals_core - residual norm is negative: " << d;
      Genten::error(sMsg.str());
    }
    return( result );
  }

}
