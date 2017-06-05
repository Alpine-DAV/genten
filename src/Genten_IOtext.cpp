//@HEADER
// ************************************************************************
//     Genten Tensor Toolbox
//     Software package for tensor math by Sandia National Laboratories
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
// ************************************************************************
//@HEADER

/*!
  @file Genten_IOtext.cpp
  @brief Implement methods for I/O of Genten classes.
*/

#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>

using namespace std;

#include "Genten_FacMatrix.h"
#include "Genten_IOtext.h"
#include "Genten_Ktensor.h"
#include "Genten_Sptensor.h"
#include "Genten_Sptensor_perm.h"
#include "Genten_Sptensor_row.h"
#include "Genten_Util.h"

#include "CMakeInclude.h"
#ifdef HAVE_BOOST
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#endif


//----------------------------------------------------------------------
//  INTERNAL METHODS WITH FILE SCOPE
//----------------------------------------------------------------------

//! Get type of file to be imported and the index base.
/*!
 *  Read and verify the data type line, a single keyword plus an optional
 *  designator of the index base.  The designator is either
 *  'indices-start-at-zero' or 'indices-start-at-one' (if missing then
 *  the default is to start at zero).
 *
 *  @param[in] fIn  File stream pointing at start of data type header.
 *                  The file is not closed by this method.
 *  @param[out] sType  Data file type, which must be checked by the caller.
 *  @param[out] bStartAtZero  True if indices start with zero,
 *                            false if indices start with one.
 *  @throws string  If file read error or unknown format.
 */
static void  get_import_type (istream  & fIn,
                              string   & sType,
                              bool     & bStartAtZero)
{
  string  s;
  if (Genten::getLineContent(fIn,s) == 0)
  {
    Genten::error("Genten::get_import_type - cannot read from file.");
  }
  vector<string>  tokens;
  Genten::splitStr(s, tokens);
  if (tokens.size() > 2)
  {
    Genten::error("Genten::get_import_type - bad format for first line.");
  }

  bStartAtZero = true;
  if (tokens.size() == 2)
  {
    if (tokens[1].compare("indices-start-at-zero") == 0)
      bStartAtZero = true;
    else if (tokens[1].compare("indices-start-at-one") == 0)
      bStartAtZero = false;
    else
    {
      ostringstream  sErrMsg;
      sErrMsg << "Genten::get_import_type - 2nd word on first line"
              << " must be 'indices-start-at-zero' or 'indices-start-at-one'";
      Genten::error(sErrMsg.str());
    }
  }

  sType = tokens[0];
  return;
}

//! Read a line and parse a single positive integer.
/*!
 *  Read and verify a line containing a specific number of positive integers.
 *
 *  @param[in] fIn        File stream pointing at start of data type header.
 *                        One line is read by this method.
 *  @param[out] naResult  Array for values read, already sized to the
 *                        expected number of integers.
 *  @throws string  If file read error.
 */
static void  read_positive_ints (      istream        & fIn,
                                       Genten::IndxArray & naResult,
                                       const char * const     sMsgPrefix)
{
  string s;
  if (Genten::getLineContent(fIn,s) == 0)
  {
    ostringstream  sErrMsg;
    sErrMsg << sMsgPrefix << " - cannot read line from file.";
    Genten::error(sErrMsg.str());
  }

  istringstream ss(s);

  for (ttb_indx  i = 0; i < naResult.size(); i++)
  {
    if (!(ss >> naResult[i]))
    {
      ostringstream  sErrMsg;
      sErrMsg << sMsgPrefix << " - line does not contain enough integers"
              << ", expecting " << naResult.size();
      Genten::error(sErrMsg.str());
    }
    if (naResult[i] <= 0)
    {
      ostringstream  sErrMsg;
      sErrMsg << sMsgPrefix << " - line must contain positive integers"
              << ", [" << i << "] is not";
      Genten::error(sErrMsg.str());
    }
  }
  if (ss.eof() == false)
  {
    ostringstream  sErrMsg;
    sErrMsg << sMsgPrefix << " - line contains too many integers"
            << " (or extra characters)"
            << ", expecting " << naResult.size();
    Genten::error(sErrMsg.str());
  }

  return;
}

//! Verify the stream is at its end.
/*!
 *  @param[in] fIn  File stream to check.
 *  @throws string  If stream is not at its end.
 */
static void  verifyEOF (      istream &     fIn,
                              const char * const  sMsgPrefix)
{
  string  s;
  if (Genten::getLineContent(fIn,s) > 0)
  {
    ostringstream  sErrMsg;
    sErrMsg << sMsgPrefix << " - extra lines found after last element";
    Genten::error(sErrMsg.str());
  }
  return;
}

//----------------------------------------------------------------------
//  METHODS FOR Sptensor (type "sptensor")
//----------------------------------------------------------------------

template <typename tensor_type>
void Genten::import_sptensor (std::istream& fIn,
                              tensor_type& X,
                              const ttb_indx index_base,
                              const bool verbose)
{
  // Read the first line, this tells us if we have a header, and if not,
  // how many modes there are
  std::string s;
  if (getLineContent(fIn,s) == 0)
  {
    std::ostringstream  sErrMsg;
    sErrMsg << "Genten::import_sptensor - tensor must have at "
            << "least one nonzero or a header!";
    Genten::error(sErrMsg.str());
  }
  std::vector<std::string>  tokens;
  Genten::splitStr(s, tokens);

  if (tokens.size() == 0)
  {
    std::ostringstream  sErrMsg;
    sErrMsg << "Genten::import_sptensor - invalid line:  " << s;
    Genten::error(sErrMsg.str());
  }

  ttb_indx offset = index_base;
  ttb_indx nModes = 0;
  ttb_indx nnz = 0;
  std::vector< ttb_indx> dims;
  std::vector< ttb_indx> sub_row;
  ttb_real val_row = 0.0;
  std::vector< std::vector<ttb_indx> > subs;
  std::vector< ttb_real > vals;
  bool compute_dims = true;

  // Get tensor dimensions and index base from header if we have one
  if (tokens[0] == "sptensor")
  {
    if (tokens.size() > 2)
    {
      Genten::error("Genten::get_import_type - bad format for first line.");
    }

    if (tokens.size() == 2)
    {
      if (tokens[1].compare("indices-start-at-zero") == 0)
        offset = 0;
      else if (tokens[1].compare("indices-start-at-one") == 0)
        offset = 1;
      else
      {
        ostringstream  sErrMsg;
        sErrMsg << "Genten::get_import_type - 2nd word on first line"
                << " must be 'indices-start-at-zero' or 'indices-start-at-one'";
        Genten::error(sErrMsg.str());
      }
    }

    // Read the number of modes, dimensions, and number of nonzeros
    Genten::IndxArray  naModes(1);
    read_positive_ints (fIn, naModes, "Genten::import_sptensor, line 2");
    Genten::IndxArray  naSizes(naModes[0]);
    read_positive_ints (fIn, naSizes, "Genten::import_sptensor, line 3");
    Genten::IndxArray  naNnz(1);
    read_positive_ints (fIn, naNnz, "Genten::import_sptensor, line 4");

    // Reserve space based on the supplied tensor dimensions
    nModes = naModes[0];
    sub_row.resize(nModes);
    dims.resize(nModes);
    for (ttb_indx i=0; i<nModes; ++i)
      dims[i] = naSizes[i];
    compute_dims = false;
    subs.reserve(naNnz[0]);
    vals.reserve(naNnz[0]);
  }

  // Otherwise this is the first nonzero and we compute the dimensions as we go
  else
  {
    nModes = tokens.size()-1;
    sub_row.resize(nModes);
    dims.resize(nModes);
    for (ttb_indx i=0; i<nModes; ++i)
    {
      sub_row[i] = std::stol(tokens[i]) - offset;
      dims[i] = sub_row[i]+1;
    }
    compute_dims = true;
    subs.push_back(sub_row);
    val_row = std::stod(tokens[nModes]);
    vals.push_back(val_row);
    ++nnz;
  }

  while (getLineContent(fIn,s))
  {
    ++nnz;
    tokens.resize(0);
    Genten::splitStr(s, tokens);
    if (tokens.size() != nModes+1)
    {
      std::ostringstream  sErrMsg;
      sErrMsg << "Genten::import_sptensor - error reading nonzero " << nnz
              << ":  " << s;
      Genten::error(sErrMsg.str());
    }
    for (ttb_indx i=0; i<nModes; ++i)
    {
      sub_row[i] = std::stol(tokens[i]) - offset;
      if (compute_dims)
        dims[i] = std::max(dims[i], sub_row[i]+1);
    }
    val_row = std::stod(tokens[nModes]);
    subs.push_back(sub_row);
    vals.push_back(val_row);
  }

  verifyEOF(fIn, "Genten::import_sptensor");

  X = tensor_type(dims, vals, subs);

  if (verbose) {
    std::cout << "Read tensor with " << nnz << " nonzeros, dimensions [ ";
    for (ttb_indx i=0; i<nModes; ++i)
      std::cout << dims[i] << " ";
    std::cout << "], and starting index " << offset << std::endl;
  }
}

template void
Genten::import_sptensor<Genten::Sptensor>(std::istream& fIn,
                                          Genten::Sptensor& X,
                                          const ttb_indx index_base,
                                          const bool verbose);
template void
Genten::import_sptensor<Genten::Sptensor_perm>(std::istream& fIn,
                                               Genten::Sptensor_perm& X,
                                               const ttb_indx index_base,
                                               const bool verbose);
template void
Genten::import_sptensor<Genten::Sptensor_row>(std::istream& fIn,
                                              Genten::Sptensor_row& X,
                                              const ttb_indx index_base,
                                              const bool verbose);

template <typename tensor_type>
void Genten::import_sptensor (const std::string& fName,
                              tensor_type& X,
                              const ttb_indx index_base,
                              const bool bCompressed,
                              const bool verbose)
{
  if (bCompressed)
  {
#ifdef HAVE_BOOST
    std::ifstream fIn(fName.c_str(), ios_base::in | ios_base::binary);
    if (!fIn.is_open())
    {
      Genten::error("Genten::import_sptensor - cannot open input file.");
    }
    boost::iostreams::filtering_stream<boost::iostreams::input> in;
    in.push(boost::iostreams::gzip_decompressor());
    in.push(fIn);
    import_sptensor(in, X, index_base, verbose);
    fIn.close();
#else
    Genten::error("Genten::import_sptensor - compression option requires Boost enabled.");
#endif
  }
  else
  {
    std::ifstream fIn(fName.c_str());
    if (!fIn.is_open())
    {
      Genten::error("Genten::import_sptensor - cannot open input file.");
    }
    import_sptensor(fIn, X, index_base, verbose);
    fIn.close();
  }
}

template void
Genten::import_sptensor<Genten::Sptensor>(const std::string& fName,
                                          Genten::Sptensor& X,
                                          const ttb_indx index_base,
                                          const bool bCompressed,
                                          const bool verbose);
template void
Genten::import_sptensor<Genten::Sptensor_perm>(const std::string& fName,
                                               Genten::Sptensor_perm& X,
                                               const ttb_indx index_base,
                                               const bool bCompressed,
                                               const bool verbose);
template void
Genten::import_sptensor<Genten::Sptensor_row>(const std::string& fName,
                                              Genten::Sptensor_row& X,
                                              const ttb_indx index_base,
                                              const bool bCompressed,
                                              const bool verbose);

void Genten::export_sptensor (const std::string   & fName,
                              const Genten::Sptensor & X,
                              const bool            bStartAtZero)
{
  export_sptensor (fName, X, true, 15, bStartAtZero);
  return;
}

void Genten::export_sptensor (const std::string   & fName,
                              const Genten::Sptensor & X,
                              const bool          & bUseScientific,
                              const int           & nDecimalDigits,
                              const bool            bStartAtZero)
{
  ofstream fOut(fName.c_str());
  if (fOut.is_open() == false)
  {
    Genten::error("Genten::export_sptensor - cannot create output file.");
  }

  export_sptensor(fOut, X, bUseScientific, nDecimalDigits, bStartAtZero);
  fOut.close();
  return;
}

void Genten::export_sptensor (      std::ofstream & fOut,
                                    const Genten::Sptensor & X,
                                    const bool          & bUseScientific,
                                    const int           & nDecimalDigits,
                                    const bool            bStartAtZero)
{
  if (fOut.is_open() == false)
  {
    Genten::error("Genten::export_sptensor - cannot create output file.");
  }

  // Write the data type header.
  if (bStartAtZero)
    fOut << "sptensor" << endl;
  else
    fOut << "sptensor indices-start-at-one" << endl;

  // Write the header lines containing sizes.
  fOut << X.ndims() << endl;
  for (ttb_indx  i = 0; i < X.ndims(); i++)
  {
    if (i > 0)
      fOut << " ";
    fOut << X.size(i);
  }
  fOut << endl;
  fOut << X.nnz() << endl;

  // Apply formatting rules for elements.
  if (bUseScientific)
    fOut << setiosflags(ios::scientific);
  else
    fOut << fixed;
  fOut << setprecision(nDecimalDigits);

  // Write the nonzero elements, one per line.
  for (ttb_indx  i = 0; i < X.nnz(); i++)
  {
    for (ttb_indx  j = 0; j < X.ndims(); j++)
    {
      if (bStartAtZero)
        fOut << X.subscript(i,j) << " ";
      else
        fOut << X.subscript(i,j) + 1 << " ";
    }
    fOut << X.value(i) << endl;
  }

  return;
}

void Genten::print_sptensor (const Genten::Sptensor & X,
                             ostream       & fOut,
                             const string          name)
{
  fOut << "-----------------------------------" << endl;
  if (name.empty())
    fOut << "sptensor" << endl;
  else
    fOut << name << endl;
  fOut << "-----------------------------------" << endl;

  ttb_indx  nDims = X.ndims();

  fOut << "Ndims = " << nDims << endl;
  fOut << "Size = [ ";
  for (ttb_indx  i = 0; i < nDims; i++)
  {
    fOut << X.size(i) << " ";
  }
  fOut << "]" << endl;

  fOut << "NNZ = " << X.nnz() << endl;

  // Write out each element.
  for (ttb_indx  i = 0; i < X.nnz(); i++)
  {
    fOut << "X(";
    for (ttb_indx  j = 0; j < nDims; j++)
    {
      fOut << X.subscript(i,j);
      if (j == (nDims - 1))
      {
        fOut << ") = ";
      }
      else
      {
        fOut << ",";
      }
    }
    fOut << X.value(i) << endl;
  }

  fOut << "-----------------------------------" << endl;
  return;
}


//----------------------------------------------------------------------
//  METHODS FOR FacMatrix (type "matrix")
//----------------------------------------------------------------------

void Genten::import_matrix (const string         & fName,
                            Genten::FacMatrix & X)
{
  ifstream fIn(fName.c_str());
  import_matrix(fIn, X);

  verifyEOF(fIn, "Genten::import_matrix");
  fIn.close();
  return;
}

void Genten::import_matrix (ifstream       & fIn,
                            Genten::FacMatrix & X)
{
  if (fIn.is_open() == false)
  {
    Genten::error("Genten::import_matrix - cannot open input file.");
  }

  string  sType;
  bool    bStartAtZero;
  get_import_type(fIn, sType, bStartAtZero);
  if ((sType != "facmatrix") && (sType != "matrix"))
  {
    Genten::error("Genten::import_matrix - data type header is not 'matrix'.");
  }
  //TBD support bStartAtZero

  // Process second and third lines.
  Genten::IndxArray  naModes(1);
  read_positive_ints (fIn, naModes, "Genten::import_matrix, number of dimensions should be 2");
  if (naModes[0] != 2)
  {
    Genten::error("Genten::import_matrix - illegal number of dimensions");
  }
  Genten::IndxArray  naTmp(2);
  read_positive_ints (fIn, naTmp, "Genten::import_matrix, line 3");
  ttb_indx nRows = naTmp[0];
  ttb_indx nCols = naTmp[1];

  X = Genten::FacMatrix(nRows, nCols);

  // Read the remaining lines, expecting one row of values per line.
  // Extra lines are ignored (allowing for multiple matrices in one file).
  string s;
  for (ttb_indx  i = 0; i < nRows; i++)
  {
    if (getLineContent(fIn,s) == 0)
    {
      ostringstream  sErrMsg;
      sErrMsg << "Genten::import_matrix - error reading row " << i
              << " of " << nRows;
      Genten::error(sErrMsg.str());
    }
    istringstream ss(s);
    for (ttb_indx  j = 0; j < nCols; j++)
    {
      if (!(ss >> X.entry(i,j)))
      {
        ostringstream  sErrMsg;
        sErrMsg << "Genten::import_matrix - error reading column " << j
                << " of row " << i << " (out of " << nRows << " rows)";
        Genten::error(sErrMsg.str());
      }
    }
    if (ss.eof() == false)
    {
      ostringstream  sErrMsg;
      sErrMsg << "Genten::import_matrix - too many values"
              << " (or extra characters) in row " << i;
      Genten::error(sErrMsg.str());
    }
  }

  return;
}

void Genten::export_matrix (const std::string    & fName,
                            const Genten::FacMatrix & X)
{
  export_matrix (fName, X, true, 15);
  return;
}

void Genten::export_matrix (const std::string    & fName,
                            const Genten::FacMatrix & X,
                            const bool           & bUseScientific,
                            const int            & nDecimalDigits)
{
  ofstream fOut(fName.c_str());
  if (fOut.is_open() == false)
  {
    Genten::error("Genten::export_matrix - cannot create output file.");
  }

  export_matrix(fOut, X, bUseScientific, nDecimalDigits);
  fOut.close();
  return;
}

void Genten::export_matrix (      std::ofstream  & fOut,
                                  const Genten::FacMatrix & X,
                                  const bool           & bUseScientific,
                                  const int            & nDecimalDigits)
{
  if (fOut.is_open() == false)
  {
    Genten::error("Genten::export_matrix - cannot create output file.");
  }

  // Write the data type header.
  fOut << "matrix" << endl;

  // Write the number of modes and size of each mode,
  // consistent with Matlab Genten.
  fOut << "2" << endl;
  fOut << X.nRows() << " " << X.nCols() << endl;

  // Apply formatting rules for elements.
  if (bUseScientific)
    fOut << setiosflags(ios::scientific);
  else
    fOut << fixed;
  fOut << setprecision(nDecimalDigits);

  // Write elements for each row on one line.
  for (ttb_indx  i = 0; i < X.nRows(); i++)
  {
    for (ttb_indx  j = 0; j < X.nCols(); j++)
    {
      if (j > 0)
        fOut << " ";
      fOut << X.entry(i,j);
    }
    fOut << endl;
  }

  return;
}

void Genten::print_matrix (const Genten::FacMatrix & X,
                           ostream        & fOut,
                           const string           name)
{
  fOut << "-----------------------------------" << endl;
  if (name.empty())
    fOut << "matrix" << endl;
  else
    fOut << name << endl;
  fOut << "-----------------------------------" << endl;

  fOut << "Size = [ " << X.nRows() << " " << X.nCols() << " ]" << endl;

  for (ttb_indx  j = 0; j < X.nCols(); j++)
  {
    for (ttb_indx  i = 0; i < X.nRows(); i++)
    {
      fOut << "X(" << i << "," << j << ") = " << X.entry(i,j) << endl;
    }
  }

  fOut << "-----------------------------------" << endl;
  return;
}


//----------------------------------------------------------------------
//  METHODS FOR Ktensor (type "ktensor")
//----------------------------------------------------------------------

void Genten::import_ktensor (const std::string  & fName,
                             Genten::Ktensor & X)
{
  ifstream fIn(fName.c_str());
  import_ktensor (fIn, X);

  verifyEOF(fIn, "Genten::import_ktensor");
  fIn.close();
  return;
}

void Genten::import_ktensor (std::ifstream & fIn,
                             Genten::Ktensor  & X)
{
  if (fIn.is_open() == false)
  {
    Genten::error("Genten::import_ktensor - cannot open input file.");
  }

  string  sType;
  bool    bStartAtZero;
  get_import_type(fIn, sType, bStartAtZero);
  if (sType != "ktensor")
  {
    Genten::error("Genten::import_ktensor - data type header is not 'ktensor'.");
  }
  //TBD support bStartAtZero

  Genten::IndxArray  naModes(1);
  read_positive_ints (fIn, naModes, "Genten::import_ktensor, line 2");
  Genten::IndxArray  naSizes(naModes[0]);
  read_positive_ints (fIn, naSizes, "Genten::import_ktensor, line 3");
  Genten::IndxArray  naComps(1);
  read_positive_ints (fIn, naComps, "Genten::import_ktensor, line 4");

  X = Genten::Ktensor(naComps[0], naModes[0]);

  // Read the factor weights.
  string  s;
  if (getLineContent(fIn,s) == 0)
  {
    Genten::error("Genten::import_ktensor - cannot read line with weights");
  }
  Genten::Array  daWeights(naComps[0]);
  istringstream ss(s);
  for (ttb_indx  i = 0; i < naComps[0]; i++)
  {
    if (!(ss >> daWeights[i]))
    {
      ostringstream  sErrMsg;
      sErrMsg << "Genten::import_ktensor - error reading weight " << i;
      Genten::error(sErrMsg.str());
    }
    if (daWeights[i] < 0.0)
    {
      Genten::error("Genten::import_ktensor - factor weight cannot be negative");
    }
  }
  if (ss.eof() == false)
  {
    ostringstream  sErrMsg;
    sErrMsg << "Genten::import_ktensor - too many values"
            << " (or extra characters) in weights vector";
    Genten::error(sErrMsg.str());
  }
  X.setWeights(daWeights);

  // Read the factor matrices.
  for (ttb_indx  i = 0; i < naModes[0]; i++)
  {
    Genten::FacMatrix  nextFactor;
    import_matrix (fIn, nextFactor);
    if (   (nextFactor.nRows() != naSizes[i])
           || (nextFactor.nCols() != naComps[0]) )
    {
      ostringstream  sErrMsg;
      sErrMsg << "Genten::import_ktensor - factor matrix " << i
              << " is not the correct size"
              << ", expecting " << naSizes[i] << " by " << naComps[0];
      Genten::error(sErrMsg.str());
    }
    X[i] = nextFactor;
  }

  return;
}

void Genten::export_ktensor (const std::string  & fName,
                             const Genten::Ktensor & X)
{
  export_ktensor (fName, X, true, 15);
  return;
}

void Genten::export_ktensor (const std::string  & fName,
                             const Genten::Ktensor & X,
                             const bool         & bUseScientific,
                             const int          & nDecimalDigits)
{
  ofstream fOut(fName.c_str());
  if (fOut.is_open() == false)
  {
    Genten::error("Genten::export_ktensor - cannot create output file.");
  }

  export_ktensor(fOut, X, bUseScientific, nDecimalDigits);
  fOut.close();
  return;
}

void Genten::export_ktensor (      std::ofstream & fOut,
                                   const Genten::Ktensor  & X,
                                   const bool          & bUseScientific,
                                   const int           & nDecimalDigits)
{
  if (fOut.is_open() == false)
  {
    Genten::error("Genten::export_ktensor - cannot create output file.");
  }

  // Write the data type header.
  fOut << "ktensor" << endl;

  // Write the header lines containing sizes.
  fOut << X.ndims() << endl;
  for (ttb_indx  i = 0; i < X.ndims(); i++)
  {
    if (i > 0)
      fOut << " ";
    fOut << X[i].nRows();
  }
  fOut << endl;
  fOut << X.ncomponents() << endl;

  // Apply formatting rules for elements.
  if (bUseScientific)
    fOut << setiosflags(ios::scientific);
  else
    fOut << fixed;
  fOut << setprecision(nDecimalDigits);

  // Write the weights on one line.
  for (ttb_indx  i = 0; i < X.ncomponents(); i++)
  {
    if (i > 0)
      fOut << " ";
    fOut << X.weights(i);
  }
  fOut << endl;

  // Write the factor matrices.
  for (ttb_indx  i = 0; i < X.ndims(); i++)
  {
    export_matrix(fOut, X[i], bUseScientific, nDecimalDigits);
  }

  return;
}

void Genten::print_ktensor (const Genten::Ktensor & X,
                            std::ostream & fOut,
                            const std::string    name)
{
  fOut << "-----------------------------------" << endl;
  if (name.empty())
    fOut << "ktensor" << endl;
  else
    fOut << name << endl;
  fOut << "-----------------------------------" << endl;

  ttb_indx nd = X.ndims();
  ttb_indx nc = X.ncomponents();
  fOut << "Ndims = " << nd <<"    Ncomps = " << nc << endl;

  fOut << "Size = [ ";
  for (ttb_indx  k = 0; k < nd; k++)
  {
    fOut << X[k].nRows() << ' ';
  }
  fOut << "]" << endl;

  fOut << "Weights = [ ";
  for (ttb_indx  k = 0; k < nc; k++)
  {
    fOut << X.weights(k) << ' ';
  }
  fOut << "]" << endl;

  for (ttb_indx  k = 0; k < nd; k++)
  {
    fOut << "Factor " << k << endl;
    for (ttb_indx  j = 0; j < X[k].nCols(); j++)
    {
      for (ttb_indx  i = 0; i < X[k].nRows(); i++)
      {
        fOut << "f" << k << "(" << i << "," << j << ") = "
             << X[k].entry(i,j) << endl;
      }
    }
  }

  fOut << "-----------------------------------" << endl;
  return;
}

//----------------------------------------------------------------------
//  UTILITY METHODS
//----------------------------------------------------------------------

//! Read the next line with useful content from an opened stream.
/*!
 *  Read a line from an opened stream, dropping the '\n' character.
 *  Skip over empty lines or lines containing only white space
 *  (blank and tab).  Skip over lines containing comments that begin
 *  with "//".
 *  For platform independence, check if the last character is '\r'
 *  (Windows text file) and remove if it is.
 *
 *  @param[in]  fIn  Input stream.
 *  @param[out] str  String variable where content is put.
 *  @return          number of lines read, including the content line,
 *                   or zero if EOF reacehd.
 */
int Genten::getLineContent (istream  & fIn,
                            string   & str)
{
  const string &  sWhitespace = " \t";

  string  sTmp;
  int  nNumLines = 0;
  while (true)
  {
    getline(fIn, sTmp);
    if (fIn.eof() == true)
    {
      str = "";
      return( 0 );
    }
    nNumLines++;

    // Remove end-of-line character(s).
    int  nLast = (int) sTmp.size();
    if (sTmp.c_str()[nLast-1] == '\r')
      sTmp.erase(nLast-1, 1);

    if (sTmp.size() > 0)
    {
      // Trim leading white space.
      size_t  nBegin = sTmp.find_first_not_of (sWhitespace);
      if (nBegin != string::npos)
      {
        // Trim trailing white space.
        size_t  nEnd = sTmp.find_last_not_of (sWhitespace);
        str = sTmp.substr (nBegin, nEnd - nBegin + 1);
        if (str.size() > 0)
        {
          // Check if the line is a comment.
          if ((str[0] != '/') || (str[1] != '/'))
            return( nNumLines );
        }
      }
    }
  }
}

//! Split a string based on white space characters into tokens.
/*!
 *  Find tokens separated by white space (blank and tab).  Consecutive
 *  white space characters are treated as a single separator.
 *
 *  @param[in] str      String to split.
 *  @param[out] tokens  Vector of token strings.
 *  @param[in] sDelims  String of single-character delimiters,
 *                      defaults to blank and tab.
 */
void Genten::splitStr (const string         &  str,
                       vector<string> &  tokens,
                       const string         &  sDelims)
{
  size_t  nStart;
  size_t  nEnd = 0;
  while (nEnd < str.size())
  {
    nStart = nEnd;
    // Skip past any initial delimiter characters.
    while (   (nStart < str.size())
              && (sDelims.find(str[nStart]) != string::npos))
    {
      nStart++;
    }
    nEnd = nStart;

    // Find the end of the token.
    while (   (nEnd < str.size())
              && (sDelims.find(str[nEnd]) == string::npos))
    {
      nEnd++;
    }

    // Save the token if nonempty.
    if (nEnd - nStart != 0)
    {
      tokens.push_back (string(str, nStart, nEnd - nStart));
    }
  }

  return;
}
