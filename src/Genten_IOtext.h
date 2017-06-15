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
// ************************************************************************
//@HEADER

/*!
  @file Genten_IOtext.h
  @brief Declare methods providing I/O for Genten classes.
*/

#pragma once

#include <string>
#include <vector>

#include "Genten_FacMatrix.h"
#include "Genten_Ktensor.h"
#include "Genten_Sptensor.h"


//! Namespace for the Genten C++ project.
namespace Genten
{

  /** ----------------------------------------------------------------
   *  @name I/O Methods for Sptensor (sparse tensor)
   *  @{
   *  ---------------------------------------------------------------- */

  //! Read a Sptensor from a text file, matching export_sptensor().
  /*!
   *  <pre>
   *  The file should have four header lines followed by the entries.
   *    1st line must be the keyword 'sptensor', optionally followed by the keyword 'indices-start-at-one'.
   *    2nd line must provide the number of modes.
   *    3rd line must provide the sizes of all modes.
   *    4th line must provide the number of nonzero elements.
   *  </pre>
   *  Each subsequent line provides values for one nonzero element, with
   *  indices followed by the value.  Indices start numbering at zero.
   *  The elements can be in any order.
   *
   *  @param[in] fName  Input filename.
   *  @param[in,out] X  Sptensor resized and filled with data.
   *  @throws string    for any error.
   */
  template <typename tensor_type>
  void import_sptensor (std::istream& fIn,
                        tensor_type& X,
                        const ttb_indx index_base = 0,
                        const bool verbose = false);
  template <typename tensor_type>
  void import_sptensor (const std::string& fName,
                        tensor_type& X,
                        const ttb_indx index_base = 0,
                        const bool bCompressed = false,
                        const bool verbose = false);

  //! Write a Sptensor to a text file, matching import_sptensor().
  /*!
   *  Elements are output with the default format "%0.15e".
   *
   *  @param[in] fName         Output filename.
   *  @param[in] X             Sptensor to be exported.
   *  @param[in] bStartAtZero  True if indices start at zero,
   *                           false if at one.
   *  @throws string           for any error.
   */
  void export_sptensor (const std::string   & fName,
                        const Genten::Sptensor & X,
                        const bool            bStartAtZero = true);

  //! Write a Sptensor to a text file, matching import_sptensor().
  /*!
   *  @param[in] fName           Output filename.
   *  @param[in] X               Sptensor to export.
   *  @param[in] bUseScientific  True means use "%e" format, false means "%f".
   *  @param[in] nDecimalDigits  Number of digits after the decimal point.
   *  @param[in] bStartAtZero    True if indices start at zero,
   *                             false if at one.
   *  @throws string             for any error.
   */
  void export_sptensor (const std::string   & fName,
                        const Genten::Sptensor & X,
                        const bool          & bUseScientific,
                        const int           & nDecimalDigits,
                        const bool            bStartAtZero = true);

  //! Write a Sptensor to an opened file stream, matching import_sptensor().
  /*!
   *  See previous method for details.
   *
   *  @param[in] fOut            File stream for output.
   *                             The file is not closed by this method.
   *  @param[in] X               Sptensor to export.
   *  @param[in] bUseScientific  True means use "%e" format, false means "%f".
   *  @param[in] nDecimalDigits  Number of digits after the decimal point.
   *  @param[in] bStartAtZero    True if indices start at zero,
   *                             false if at one.
   *  @throws string             for any error.
   */
  void export_sptensor (      std::ofstream & fOut,
                              const Genten::Sptensor & X,
                              const bool          & bUseScientific,
                              const int           & nDecimalDigits,
                              const bool            bStartAtZero = true);

  //! Pretty-print a sparse tensor to an output stream.
  /*!
   *  @param[in] X     Sptensor to print.
   *  @param[in] fOut  Output stream.
   *  @param[in] name  Optional name for the Sptensor.
   */
  void print_sptensor (const Genten::Sptensor & X,
                       std::ostream  & fOut,
                       const std::string     name = "");

  /** @} */

  /** ----------------------------------------------------------------
   *  @name I/O Methods for FacMatrix (dense matrices)
   *  @{
   *  ---------------------------------------------------------------- */

  //! Read a factor matrix from a text file, matching export_matrix().
  /*!
   *  <pre>
   *  The file should have two header lines followed by the entries.
   *    1st line must be the keyword 'matrix' or 'facmatrix'.
   *    2nd line must provide the number of dimensions (always 2).
   *    3rd line must provide the number of rows and columns.
   *  </pre>
   *  Each subsequent line provides values for a row, with values delimited
   *  by one or more space characters.  No index values are given, and
   *  rows are assumed to be dense.
   *
   *  @param[in] fName  Input filename.
   *  @param[in,out] X  Matrix resized and filled with data.
   *  @throws string    for any error.
   */
  void import_matrix(const std::string    & fName,
                     Genten::FacMatrix & X);

  //! Read a factor matrix from an opened file stream, matching export_matrix().
  /*!
   *  See other import_matrix() method for details.
   *
   *  This method reads a factor matrix from the current position and stops
   *  reading after the last element.  The stream is allowed to have
   *  additional content after the factor matrix.
   *
   *  @param[in] fIn    File stream pointing at start of matrix data.
   *                    The file is not closed by this method.
   *  @param[in,out] X  Matrix resized and filled with data.
   *  @throws string    for any error.
   */
  void import_matrix (std::ifstream  & fIn,
                      Genten::FacMatrix & X);

  //! Write a factor matrix to a text file, matching import_matrix().
  /*!
   *  Elements are output with the default format "%0.15e".
   *
   *  @param[in] fName  Output filename.
   *  @param[in] X      Matrix to be exported.
   *  @throws string    for any error.
   */
  void export_matrix (const std::string    & fName,
                      const Genten::FacMatrix & X);

  //! Write a factor matrix to a text file, matching import_matrix().
  /*!
   *  @param[in] fName           Output filename.
   *  @param[in] X               Matrix to export.
   *  @param[in] bUseScientific  True means use "%e" format, false means "%f".
   *  @param[in] nDecimalDigits  Number of digits after the decimal point.
   *  @throws string             for any error.
   */
  void export_matrix (const std::string    & fName,
                      const Genten::FacMatrix & X,
                      const bool           & bUseScientific,
                      const int            & nDecimalDigits);

  //! Write a factor matrix to an opened file stream, matching import_matrix().
  /*!
   *  See previous method for details.
   *
   *  @param[in] fOut            File stream for output.
   *                             The file is not closed by this method.
   *  @param[in] X               Matrix to export.
   *  @param[in] bUseScientific  True means use "%e" format, false means "%f".
   *  @param[in] nDecimalDigits  Number of digits after the decimal point.
   *  @throws string             for any error.
   */
  void export_matrix (      std::ofstream  & fOut,
                            const Genten::FacMatrix & X,
                            const bool           & bUseScientific,
                            const int            & nDecimalDigits);

  //! Pretty-print a matrix to an output stream.
  /*!
   *  @param[in] X     Matrix to print.
   *  @param[in] fOut  Output stream.
   *  @param[in] name  Optional name for the matrix.
   */
  void print_matrix (const Genten::FacMatrix & X,
                     std::ostream   & fOut,
                     const std::string      name = "");

  /** @} */

  /** ----------------------------------------------------------------
   *  @name I/O Methods for KTensor
   *  @{
   *  ---------------------------------------------------------------- */

  //! Read a Ktensor from a text file, matching export_ktensor().
  /*!
   *  <pre>
   *  The file should have four header lines followed by the entries.
   *    1st line must be the keyword 'ktensor'.
   *    2nd line must provide the number of modes.
   *    3rd line must provide the sizes of all modes.
   *    4th line must provide the number of components.
   *  </pre>
   *  Factor weights follow as a row vector, and then each factor matrix
   *  (see import_matrix() for details of their format).
   *
   *  @param[in] fName  Input filename.
   *  @param[in,out] X  Ktensor resized and filled with data.
   *  @throws string    for any error, including extra lines in the file.
   */
  void import_ktensor (const std::string  & fName,
                       Genten::Ktensor & X);

  //! Read a Ktensor from a stream, matching export_ktensor().
  /*!
   *  See other import_ktensor() method for details.
   *
   *  This method reads a ktensor from the current position and stops
   *  reading after the last element.  The stream is allowed to have
   *  additional content after the ktensor.
   *
   *  @param[in] fIn    File stream pointing at start of ktensor data.
   *                    The stream is not closed by this method.
   *  @param[in,out] X  Ktensor resized and filled with data.
   *  @throws string    for any error.
   */
  void import_ktensor (std::ifstream & fIn,
                       Genten::Ktensor  & X);

  //! Write a Ktensor to a text file, matching import_ktensor().
  /*!
   *  Elements are output with the default format "%0.15e".
   *
   *  @param[in] fName  Output filename.
   *  @param[in] X      Ktensor to be exported.
   *  @throws string    for any error.
   */
  void export_ktensor (const std::string  & fName,
                       const Genten::Ktensor & X);

  //! Write a Ktensor to a text file, matching import_ktensor().
  /*!
   *  @param[in] fName           Output filename.
   *  @param[in] X               Ktensor to export.
   *  @param[in] bUseScientific  True means use "%e" format, false means "%f".
   *  @param[in] nDecimalDigits  Number of digits after the decimal point.
   *  @throws string             for any error.
   */
  void export_ktensor (const std::string  & fName,
                       const Genten::Ktensor & X,
                       const bool         & bUseScientific,
                       const int          & nDecimalDigits);

  //! Write a Ktensor to an opened file stream, matching import_ktensor().
  /*!
   *  See previous method for details.
   *
   *  @param[in] fOut            File stream for output.
   *                             The file is not closed by this method.
   *  @param[in] X               Ktensor to export.
   *  @param[in] bUseScientific  True means use "%e" format, false means "%f".
   *  @param[in] nDecimalDigits  Number of digits after the decimal point.
   *  @throws string             for any error.
   */
  void export_ktensor (      std::ofstream & fOut,
                             const Genten::Ktensor  & X,
                             const bool          & bUseScientific,
                             const int           & nDecimalDigits);

  //! Pretty-print a Ktensor to an output stream.
  /*!
   *  @param[in] X     Ktensor to print.
   *  @param[in] fOut  Output stream.
   *  @param[in] name  Optional name for the Ktensor.
   */
  void print_ktensor (const Genten::Ktensor & X,
                      std::ostream & fOut,
                      const std::string    name = "");

  /** @} */

  /** ----------------------------------------------------------------
   *  @name I/O Utilities
   *  @{
   *  ---------------------------------------------------------------- */

  //! Read the next line with useful content from an opened stream.
  /*!
   *  Read a line from an opened stream, dropping terminal LF (ASCII 10)
   *  and CR (ASCII 13) characters.
   *  Skip over empty lines or lines containing only white space
   *  (blank and tab).  Skip over lines containing comments that begin
   *  with "//".
   *
   *  @param[in] fIn   Input stream.
   *  @param[out] str  String variable where content is put.
   *  @return          number of lines read, including the content line,
   *                   or zero if EOF reacehd.
   */
  int getLineContent (std::istream  & fIn,
                      std::string   & str);

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
  void splitStr (const std::string              &  str,
                 std::vector<std::string> &  tokens,
                 const std::string              &  sDelims = " \t");

  /** @} */

}
