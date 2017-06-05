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

#include "Genten_Sptensor_row.h"

// Implementation of createRow().  Has to be done as a non-member function
// because lambda capture of *this doesn't work on Cuda.
template <typename row_ptr_type,
          typename perm_type,
          typename subs_type,
          typename siz_type>
row_ptr_type
createRowPtrImpl(const perm_type& perm,
                 const subs_type& subs,
                 const siz_type& siz)
{
  typedef typename subs_type::execution_space ExecSpace;

  // Create rowptr array with the starting index of each row
  const ttb_indx sz = perm.dimension_0();
  const ttb_indx nNumDims = perm.dimension_1();
  row_ptr_type rowptr("Genten::Sptensor_kokkos::rowptr",nNumDims);

  for (ttb_indx n = 0; n < nNumDims; ++n) {
    Kokkos::View<ttb_indx*,ExecSpace> rowptr_n(
      Kokkos::view_alloc(Kokkos::WithoutInitializing,"rowptr_n"),siz[n]+1);

    Kokkos::parallel_for( Kokkos::RangePolicy<ExecSpace>(0,sz+1),
                          KOKKOS_LAMBDA(const ttb_indx i)
    {
      if (i == 0) {
        // The first nonzero row is subs(perm(0,n),n).  The for-loop handles
        // the case when this row != 0.
        const ttb_indx s = subs(perm(0,n),n);
        for (ttb_indx k=0; k<=s; ++k)
          rowptr_n(k) = 0;
      }
      else if (i == sz) {
        // The last nonzero row is subs(perm(sz-1,n),n).  The for-loop handles
        // the case when this row != siz[n]-1.
        const ttb_indx sm = subs(perm(sz-1,n),n);
        for (ttb_indx k=sm+1; k<=siz[n]; ++k)
          rowptr_n(k) = sz;
      }
      else {
        // A row starts when subs(perm(i,n),n) != subs(perm(i-1,n),n).
        // The inner for-loop handles the case when
        // subs(perm(i,n),n) != subs(perm(i-1,n),n)+1
        const ttb_indx s  = subs(perm(i,n),n);
        const ttb_indx sm = subs(perm(i-1,n),n);
        if (s != sm) {
          for (ttb_indx k=sm+1; k<=s; ++k)
            rowptr_n(k) = i;
        }
      }
    });

    rowptr(n) = rowptr_n;
  }


  // Check
  const bool check = false;
  if (check) {
    for (ttb_indx n=0; n<nNumDims; ++n) {
      for (ttb_indx i=0; i<siz[n]; ++i) {
        const ttb_indx r_beg = rowptr(n)(i);
        const ttb_indx r_end = rowptr(n)(i+1);
        for (ttb_indx r=r_beg; r<r_end; ++r) {
          if (subs(perm(r,n),n) != i)
            std::cout << "Check failed:  Expected " << i
                      << " got " << subs(perm(r,n),n) << std::endl;
        }
      }
    }
  }

  return rowptr;
}

void Genten::Sptensor_row::
createRowPtr()
{
  rowptr = createRowPtrImpl<row_ptr_type>(perm, subs, siz);
}
