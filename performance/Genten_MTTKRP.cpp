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
  @file Genten_CpAlsRandomKtensor.cpp
  @brief Main program that factorizes synthetic data using the CP-ALS algorithm.
*/

#include <iostream>
#include <stdio.h>

#include "Genten_FacTestSetGenerator.hpp"
#include "Genten_IndxArray.hpp"
#include "Genten_IOtext.hpp"
#include "Genten_Ktensor.hpp"
#include "Genten_Sptensor.hpp"
#include "Genten_SystemTimer.hpp"
#include "Genten_Driver_Utils.hpp"
#include "Genten_MixedFormatOps.hpp"

enum SPTENSOR_TYPE {
  SPTENSOR,
  SPTENSOR_PERM,
  SPTENSOR_ROW
};
const unsigned num_sptensor_types = 3;
SPTENSOR_TYPE sptensor_types[] =
{ SPTENSOR, SPTENSOR_PERM, SPTENSOR_ROW };
std::string sptensor_names[] =
{ "kokkos", "perm", "row" };

template <template<class> class Sptensor_template, typename Space>
int run_mttkrp(const std::string& inputfilename,
               const ttb_indx index_base,
               const bool gz,
               const Genten::IndxArray& cFacDims_rnd_host,
               const ttb_indx  nNumComponents,
               const ttb_indx  nMaxNonzeroes,
               const unsigned long  nRNGseed,
               const ttb_indx  nIters,
               const SPTENSOR_TYPE tensor_type,
               const ttb_indx check)
{
  typedef Sptensor_template<Space> Sptensor_type;
  typedef Sptensor_template<Genten::DefaultHostExecutionSpace> Sptensor_host_type;
  typedef Genten::KtensorT<Space> Ktensor_type;
  typedef Genten::KtensorT<Genten::DefaultHostExecutionSpace> Ktensor_host_type;

  // Construct a random number generator that matches Matlab.
  Genten::RandomMT cRNG(nRNGseed);

  Sptensor_host_type cData_host;
  Sptensor_type cData;
  Genten::IndxArray cFacDims_host;
  Genten::IndxArrayT<Space> cFacDims;
  ttb_indx nDims = 0;
  if (inputfilename != "") {
    // Read tensor from file
    std::string fname(inputfilename);
    Genten::SystemTimer read_timer(1);
    read_timer.start(0);
    Genten::import_sptensor(fname, cData_host, index_base, gz, true);
    cData = create_mirror_view( Space(), cData_host );
    deep_copy( cData, cData_host );
    read_timer.stop(0);
    printf("Data import took %6.3f seconds\n", read_timer.getTotalTime(0));
    cFacDims_host = cData_host.size();
    cFacDims = cData.size();
    nDims = cData_host.ndims();
  }
  else {
    // Generate random tensor
    cFacDims_host = cFacDims_rnd_host;
    cFacDims = create_mirror_view( Space(), cFacDims_host );
    deep_copy( cFacDims, cFacDims_host );
    nDims = cFacDims_host.size();

    std::cout << "Will construct a random Ktensor/Sptensor_";
    std::cout << sptensor_names[tensor_type] << " pair:\n";
    std::cout << "  Ndims = " << nDims << ",  Size = [ ";
    for (ttb_indx n=0; n<nDims; ++n)
      std::cout << cFacDims_host[n] << ' ';
    std::cout << "]\n";
    std::cout << "  Ncomps = " << nNumComponents << "\n";
    std::cout << "  Maximum nnz = " << nMaxNonzeroes << "\n";

    // Generate a random Ktensor, and from it a representative sparse
    // data tensor.
    Ktensor_host_type cSol_host;
    Genten::FacTestSetGenerator cTestGen;

    Genten::SystemTimer gen_timer(1);
    gen_timer.start(0);
    if (cTestGen.genSpFromRndKtensor(cFacDims_host, nNumComponents,
                                     nMaxNonzeroes,
                                     cRNG, cData_host, cSol_host) == false)
    {
      std::cout << "*** Call to genSpFromRndKtensor failed.\n";
      return -1;
    }
    cData = create_mirror_view( Space(), cData_host );
    deep_copy( cData, cData_host );
    gen_timer.stop(0);
    std::printf("  (data generation took %6.3f seconds)\n",
                gen_timer.getTotalTime(0));
    std::cout << "  Actual nnz  = " << cData_host.nnz() << "\n";
  }

  Genten::SystemTimer timer(1+nDims);

  // Set a random input Ktensor, matching the Matlab code.
  Ktensor_host_type  cInput_host(nNumComponents, nDims, cFacDims_host);
  cInput_host.setWeights(1.0);
  cInput_host.setMatrices(0.0);
  for (ttb_indx n=0; n<nDims; ++n)
  {
    for (ttb_indx c=0; c<nNumComponents; ++c)
    {
      for (ttb_indx i=0; i<cFacDims_host[n]; ++i)
      {
        cInput_host[n].entry(i,c) = cRNG.genMatlabMT();
      }
    }
  }
  Ktensor_type cInput = create_mirror_view( Space(), cInput_host );
  deep_copy( cInput, cInput_host );

  // Do a pass through the mttkrp to warm up and make sure the tensor
  // is copied to the device before generating any timings.  Use
  // Sptensor mttkrp and do this before fillComplete() so that
  // fillComplete() timings are not polluted by UVM transfers
  Ktensor_type cResult(nNumComponents, nDims, cFacDims);
  Genten::SptensorT<Genten::DefaultExecutionSpace>& cData_tmp = cData;
  for (ttb_indx n=0; n<nDims; ++n)
    Genten::mttkrp(cData_tmp, cInput, n, cResult[n]);

  // Perform any post-processing (e.g., permutation and row ptr generation)
  timer.start(0);
  cData.fillComplete();
  timer.stop(0);
  std::printf("  (fillComplete() took %6.3f seconds)\n", timer.getTotalTime(0));

  // Perform nIters iterations of MTTKRP on each mode, timing performance
  // We do each mode sequentially as this is more representative of CpALS
  // (as opposed to running all nIters iterations on each mode before moving
  // to the next one).
  std::cout << "Performing " << nIters << " iterations of MTTKRP" << std::endl;
  std::cout << "MTTKRP performance:" << std::endl;
  for (ttb_indx iter=0; iter<nIters; ++iter) {
    for (ttb_indx n=0; n<nDims; ++n) {
      timer.start(1+n);
      Genten::mttkrp(cData, cInput, n, cResult[n]);
      timer.stop(1+n);
    }
  }
  const double atomic = 1.0; // cost of atomic measured in flops
  const double mttkrp_flops = cData.nnz()*nNumComponents*(nDims+atomic);
  double mttkrp_total_time = 0.0;
  for (ttb_indx n=0; n<nDims; ++n) {
    const double mttkrp_time = timer.getTotalTime(1+n) / nIters;
    const double mttkrp_throughput =
      ( mttkrp_flops / mttkrp_time ) / (1024.0 * 1024.0 * 1024.0);
    std::printf(
      "\tMode %i: average time = %.3f seconds, throughput = %.3f GFLOP/s\n",
      n, mttkrp_time, mttkrp_throughput);
    mttkrp_total_time += mttkrp_time;
  }
  mttkrp_total_time /= nDims;
  const double mttkrp_total_throughput =
    ( mttkrp_flops / mttkrp_total_time ) / (1024.0 * 1024.0 * 1024.0);
  std::printf(
    "\tTotal:  average time = %.3f seconds, throughput = %.3f GFLOP/s\n",
      mttkrp_total_time, mttkrp_total_throughput);

  if (check != 0) {
    // Check the results using a simple MTTKRP algorithm executed on the host
    std::cout << "Checking result for correctness:  " << std::endl;
    Ktensor_host_type cAnswer_host(nNumComponents, nDims, cFacDims_host);
    auto cResult_host = create_mirror_view(cResult);
    deep_copy( cResult_host, cResult );
    const ttb_indx nnz = cData_host.nnz();
    Kokkos::RangePolicy<Genten::DefaultHostExecutionSpace> policy(0,nnz);
    Kokkos::parallel_for(policy, [=](const ttb_indx i)
    {
      const ttb_real val = cData_host.value(i);
      for (ttb_indx j=0; j<nNumComponents; ++j) {
        for (ttb_indx n=0; n<nDims; ++n) {
          ttb_real tmp = val * cInput_host.weights(j);
          for (ttb_indx m=0; m<nDims; ++m)
            if (m != n)
              tmp *= cInput_host[m].entry(cData_host.subscript(i,m),j);
          Kokkos::atomic_add(
            &(cAnswer_host[n].entry(cData_host.subscript(i,n),j)), tmp);
        }
      }
    });

    // Compare cResult with cAnswer
    const ttb_real tol = MACHINE_EPSILON * 1000;
    ttb_indx num_failures = 0;
    for (ttb_indx n=0; n<nDims; ++n) {
      const ttb_indx nRows = cFacDims_host[n];
      ttb_indx num_failures_n = 0;
      Kokkos::RangePolicy<Genten::DefaultHostExecutionSpace> policy2(0,nRows);
      Kokkos::parallel_reduce(policy2,
                              [=](const ttb_indx i, ttb_indx& nfail)
      {
        for (ttb_indx j=0; j<nNumComponents; ++j) {
          const ttb_real v1 = cResult_host[n].entry(i,j);
          const ttb_real v2 = cAnswer_host[n].entry(i,j);
          const bool isequal = Genten::isEqualToTol(v1, v2, tol);
          if (!isequal)
            ++nfail;
        }
      },num_failures_n);
      num_failures += num_failures_n;
    }
    if (num_failures == 0)
      std::cout << "\tSuccess!" << std::endl;
    else
      std::cout << "\tFailed!" << std::endl;

    // If there were failures, print out the differences (in serial)
    if (num_failures > 0) {
      for (ttb_indx n=0; n<nDims; ++n) {
        const ttb_indx nRows = cFacDims_host[n];
        for (ttb_indx i=0; i<nRows; ++i) {
          for (ttb_indx j=0; j<nNumComponents; ++j) {
            const ttb_real v1 = cResult_host[n].entry(i,j);
            const ttb_real v2 = cAnswer_host[n].entry(i,j);
            const ttb_real diff =
              std::abs(v1-v2)/std::max(std::abs(v1),std::abs(v2));
            const bool isequal = Genten::isEqualToTol(v1, v2, tol);
            if (!isequal) {
              std::cout << "mode " << n << " entry (" << i << "," << j
                        << ") expected " << v2 << ", got " << v1
                        << ", rel. diff. = " << diff
                        << ", tol = " << tol
                        << std::endl;
            }
          }
        }
      }
    }
  }

  return 0;
}

void usage(char **argv)
{
  std::cout << "Usage: "<< argv[0]<<" [options]" << std::endl;
  std::cout << "options: " << std::endl;
  std::cout << "  --input <string>     path to input sptensor data" << std::endl;
  std::cout << "  --index_base <int>   starting index for tensor nonzeros" << std::endl;
  std::cout << "  --gz                 read tensor in gzip compressed format" << std::endl;
  std::cout << "  --dims <[n1,n2,...]> random tensor dimensions" << std::endl;
  std::cout << "  --nnz <int>          maximum number of random tensor nonzeros" << std::endl;
  std::cout << "  --nc <int>           number of factor components" << std::endl;
  std::cout << "  --iters <int>        number of iterations to perform" << std::endl;
  std::cout << "  --seed <int>         seed for random number generator used in initial guess" << std::endl;
  std::cout << "  --check <0/1>        check the result for correctness" << std::endl;
  std::cout << "  --tensor <type>      Sptensor format: ";
  for (unsigned i=0; i<num_sptensor_types; ++i) {
    std::cout << sptensor_names[i];
    if (i != num_sptensor_types-1)
      std::cout << ", ";
  }
  std::cout << std::endl;
  std::cout << "  --vtune              connect to vtune for Intel-based profiling (assumes vtune profiling tool, amplxe-cl, is in your path)" << std::endl;
}

//! Main routine for the executable.
/*!
 *  The test constructs a random Ktensor, derives a sparse data tensor,
 *  and calls MTTKRP.  Parameters allow different
 *  data sizes with the intent of understanding MTTKRP performance issues.
 */
int main(int argc, char* argv[])
{
  Kokkos::initialize(argc, argv);
  int ret = 0;

  try {

    ttb_bool help = parse_ttb_bool(argc, argv, "--help", false);
    if (help) {
      usage(argv);
      Kokkos::finalize();
      return 0;
    }

    ttb_bool vtune = parse_ttb_bool(argc, argv, "--vtune", false);
    if (vtune)
      Genten::connect_vtune();

    // Choose parameters: ndims, dim sizes, ncomps.
    std::string inputfilename =
      parse_string(argc,argv,"--input","");
    ttb_indx index_base =
      parse_ttb_indx(argc, argv, "--index_base", 0, 0, INT_MAX);
    ttb_bool gz =
      parse_ttb_bool(argc, argv, "--gz", false);
    Genten::IndxArray  cFacDims = { 3000, 4000, 5000 };
    cFacDims =
      parse_ttb_indx_array(argc, argv, "--dims", cFacDims, 1, INT_MAX);
    ttb_indx  nNumComponents =
      parse_ttb_indx(argc, argv, "--nc", 32, 1, INT_MAX);
    ttb_indx  nMaxNonzeroes =
      parse_ttb_indx(argc, argv, "--nnz", 1 * 1000 * 1000, 1, INT_MAX);
    unsigned long  nRNGseed =
      parse_ttb_indx(argc, argv, "--seed", 1, 0, INT_MAX);
    ttb_indx  nIters =
      parse_ttb_indx(argc, argv, "--iters", 10, 1, INT_MAX);
    ttb_indx  check =
      parse_ttb_indx(argc, argv, "--check", 1, 0, 1);
    SPTENSOR_TYPE tensor_type =
      parse_ttb_enum(argc, argv, "--tensor", SPTENSOR,
                     num_sptensor_types, sptensor_types, sptensor_names);

    if (tensor_type == SPTENSOR)
      ret = run_mttkrp< Genten::SptensorT, Genten::DefaultExecutionSpace >(
        inputfilename, index_base, gz,
        cFacDims, nNumComponents, nMaxNonzeroes, nRNGseed, nIters, tensor_type,
        check);
    else if (tensor_type == SPTENSOR_PERM)
      ret = run_mttkrp< Genten::SptensorT_perm, Genten::DefaultExecutionSpace >(
        inputfilename, index_base, gz,
        cFacDims, nNumComponents, nMaxNonzeroes, nRNGseed, nIters, tensor_type,
        check);
    else if (tensor_type == SPTENSOR_ROW)
      ret = run_mttkrp< Genten::SptensorT_row, Genten::DefaultExecutionSpace >(
        inputfilename, index_base, gz,
        cFacDims, nNumComponents, nMaxNonzeroes, nRNGseed, nIters, tensor_type,
        check);

  }
  catch(std::string sExc)
  {
    std::cout << "*** Call to cpals_core threw an exception:\n";
    std::cout << "  " << sExc << "\n";
    ret = 0;
  }

  Kokkos::finalize();
  return ret;
}
