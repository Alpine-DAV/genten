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


#include <stdio.h>

#include "Genten_Array.h"
#include "Genten_DiscreteCDF.h"
#include "Genten_FacMatrix.h"

using namespace std;


//-----------------------------------------------------------------------------
//  Constructor
//-----------------------------------------------------------------------------
Genten::DiscreteCDF::DiscreteCDF (void)
{
  return;
}


//-----------------------------------------------------------------------------
//  Destructor
//-----------------------------------------------------------------------------
Genten::DiscreteCDF::~DiscreteCDF (void)
{
  return;
}


//-----------------------------------------------------------------------------
//  Public method
//-----------------------------------------------------------------------------
bool Genten::DiscreteCDF::load (const Array &  cPDF)
{
  _cCDF = Array (cPDF.size());

  if (cPDF.size() == 1)
  {
    _cCDF[0] = 1.0;
    return( true );
  }

  for (ttb_indx  i = 0; i < cPDF.size(); i++)
  {
    ttb_real  dNext = cPDF[i];
    if ((dNext < 0.0) || (dNext >= 1.0))
    {
      cout << "*** Bad input to DiscreteCDF.load:  ("
           << i << ") = " << dNext << "\n";
      return( false );
    }
    if (i == 0)
      _cCDF[i] = dNext;
    else
      _cCDF[i] = dNext + _cCDF[i-1];
  }

  ttb_real  dTotal = _cCDF[_cCDF.size() - 1];
  if ((sizeof(ttb_real) == 8 && fabs (dTotal - 1.0) > 1.0e-14) ||
      (sizeof(ttb_real) == 4 && fabs (dTotal - 1.0) > 1.0e-6))
  {
    printf ("*** Bad input to DiscreteCDF.load: "
            " sums to %24.16f instead of 1 (error %e).\n",
            dTotal, fabs (dTotal - 1.0));
    return( false );
  }

  return( true );
}


//-----------------------------------------------------------------------------
//  Public method
//-----------------------------------------------------------------------------
bool Genten::DiscreteCDF::load (const FacMatrix &  cPDF,
                                const ttb_indx     nColumn)
{
  _cCDF = Array (cPDF.nRows());

  for (ttb_indx  r = 0; r < cPDF.nRows(); r++)
  {
    ttb_real  dNext = cPDF.entry (r, nColumn);
    if ((dNext < 0.0) || (dNext >= 1.0))
    {
      cout << "*** Bad input to DiscreteCDF.load:  ("
           << r << "," << nColumn << ") = " << dNext << "\n";
      return( false );
    }
    if (r == 0)
      _cCDF[r] = dNext;
    else
      _cCDF[r] = dNext + _cCDF[r-1];
  }

  ttb_real  dTotal = _cCDF[_cCDF.size() - 1];
  if ((sizeof(ttb_real) == 8 && fabs (dTotal - 1.0) > 1.0e-12) ||
      (sizeof(ttb_real) == 4 && fabs (dTotal - 1.0) > 1.0e-4))
  {
    printf ("*** Bad input to DiscreteCDF.load: "
            " sums to %18.16f instead of 1 (error %e).\n",
            dTotal, fabs (dTotal - 1.0));
    return( false );
  }

  return( true );
}


//-----------------------------------------------------------------------------
//  Public method
//-----------------------------------------------------------------------------
ttb_indx  Genten::DiscreteCDF::getRandomSample (ttb_real  dRandomNumber)
{
  const ttb_indx  nMAXLEN_FULLSEARCH = 16;

  // If the histogram is short, then just walk thru it to find the index.
  if (_cCDF.size() < nMAXLEN_FULLSEARCH)
  {
    for (ttb_indx  i = 0; i < _cCDF.size(); i++)
    {
      if (dRandomNumber < _cCDF[i])
        return(i);
    }
    return( _cCDF.size() - 1 );
  }

  // For a longer histogram, use a binary search.
  ttb_indx  nStart = 0;
  ttb_indx  nEnd = _cCDF.size() - 1;
  while (true)
  {
    if ((nEnd - nStart) <= 1)
    {
      if (dRandomNumber < _cCDF[nStart])
        return( nStart );
      else
        return( nEnd);
    }
    ttb_indx  nMiddle = (nStart + nEnd) / 2;
    if (dRandomNumber < _cCDF[nMiddle])
      nEnd = nMiddle;
    else
      nStart = nMiddle;
  }
}
