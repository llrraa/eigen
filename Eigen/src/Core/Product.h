// This file is part of Eigen, a lightweight C++ template library
// for linear algebra.
//
// Copyright (C) 2006-2008 Benoit Jacob <jacob.benoit.1@gmail.com>
// Copyright (C) 2008 Gael Guennebaud <g.gael@free.fr>
//
// Eigen is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 3 of the License, or (at your option) any later version.
//
// Alternatively, you can redistribute it and/or
// modify it under the terms of the GNU General Public License as
// published by the Free Software Foundation; either version 2 of
// the License, or (at your option) any later version.
//
// Eigen is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License or the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License and a copy of the GNU General Public License along with
// Eigen. If not, see <http://www.gnu.org/licenses/>.

#ifndef EIGEN_PRODUCT_H
#define EIGEN_PRODUCT_H

/***************************
*** Forward declarations ***
***************************/

template<int VectorizationMode, int Index, typename Lhs, typename Rhs, typename RetScalar>
struct ei_product_coeff_impl;

template<int StorageOrder, int Index, typename Lhs, typename Rhs, typename PacketScalar, int LoadMode>
struct ei_product_packet_impl;

/** \class ProductReturnType
  *
  * \brief Helper class to get the correct and optimized returned type of operator*
  *
  * \param Lhs the type of the left-hand side
  * \param Rhs the type of the right-hand side
  * \param ProductMode the type of the product (determined automatically by ei_product_mode)
  *
  * This class defines the typename Type representing the optimized product expression
  * between two matrix expressions. In practice, using ProductReturnType<Lhs,Rhs>::Type
  * is the recommended way to define the result type of a function returning an expression
  * which involve a matrix product. The class Product should never be
  * used directly.
  *
  * \sa class Product, MatrixBase::operator*(const MatrixBase<OtherDerived>&)
  */
template<typename Lhs, typename Rhs, int ProductMode>
struct ProductReturnType
{
  typedef typename ei_nested<Lhs,Rhs::ColsAtCompileTime>::type LhsNested;
  typedef typename ei_nested<Rhs,Lhs::RowsAtCompileTime>::type RhsNested;

  typedef Product<LhsNested, RhsNested, ProductMode> Type;
};

// cache friendly specialization
template<typename Lhs, typename Rhs>
struct ProductReturnType<Lhs,Rhs,CacheFriendlyProduct>
{
  typedef typename ei_nested<Lhs,1>::type LhsNested;
  typedef typename ei_nested<Rhs,1,
                             typename ei_plain_matrix_type_column_major<Rhs>::type
                   >::type RhsNested;

  typedef Product<LhsNested, RhsNested, CacheFriendlyProduct> Type;
};

/*  Helper class to determine the type of the product, can be either:
 *    - NormalProduct
 *    - CacheFriendlyProduct
 */
template<typename Lhs, typename Rhs> struct ei_product_mode
{
  enum{
    // workaround sun studio:
    LhsIsVectorAtCompileTime = ei_traits<Lhs>::ColsAtCompileTime==1 || ei_traits<Rhs>::ColsAtCompileTime==1,
    value = ei_traits<Lhs>::MaxColsAtCompileTime == Dynamic
            && ( ei_traits<Lhs>::MaxRowsAtCompileTime == Dynamic
              || ei_traits<Rhs>::MaxColsAtCompileTime == Dynamic )
            && (!(Rhs::IsVectorAtCompileTime && (ei_traits<Lhs>::Flags&RowMajorBit)  && (!(ei_traits<Lhs>::Flags&DirectAccessBit))))
            && (!(LhsIsVectorAtCompileTime && (!(ei_traits<Rhs>::Flags&RowMajorBit)) && (!(ei_traits<Rhs>::Flags&DirectAccessBit))))
            && (ei_is_same_type<typename ei_traits<Lhs>::Scalar, typename ei_traits<Rhs>::Scalar>::ret)
          ? CacheFriendlyProduct
          : NormalProduct };
};

/** \class Product
  *
  * \brief Expression of the product of two matrices
  *
  * \param LhsNested the type used to store the left-hand side
  * \param RhsNested the type used to store the right-hand side
  * \param ProductMode the type of the product
  *
  * This class represents an expression of the product of two matrices.
  * It is the return type of the operator* between matrices. Its template
  * arguments are determined automatically by ProductReturnType. Therefore,
  * Product should never be used direclty. To determine the result type of a
  * function which involves a matrix product, use ProductReturnType::Type.
  *
  * \sa ProductReturnType, MatrixBase::operator*(const MatrixBase<OtherDerived>&)
  */
template<typename LhsNested, typename RhsNested, int ProductMode>
struct ei_traits<Product<LhsNested, RhsNested, ProductMode> >
{
  // clean the nested types:
  typedef typename ei_cleantype<LhsNested>::type _LhsNested;
  typedef typename ei_cleantype<RhsNested>::type _RhsNested;
  typedef typename ei_scalar_product_traits<typename _LhsNested::Scalar, typename _RhsNested::Scalar>::ReturnType Scalar;

  enum {
    LhsCoeffReadCost = _LhsNested::CoeffReadCost,
    RhsCoeffReadCost = _RhsNested::CoeffReadCost,
    LhsFlags = _LhsNested::Flags,
    RhsFlags = _RhsNested::Flags,

    RowsAtCompileTime = _LhsNested::RowsAtCompileTime,
    ColsAtCompileTime = _RhsNested::ColsAtCompileTime,
    InnerSize = EIGEN_ENUM_MIN(_LhsNested::ColsAtCompileTime, _RhsNested::RowsAtCompileTime),

    MaxRowsAtCompileTime = _LhsNested::MaxRowsAtCompileTime,
    MaxColsAtCompileTime = _RhsNested::MaxColsAtCompileTime,

    LhsRowMajor = LhsFlags & RowMajorBit,
    RhsRowMajor = RhsFlags & RowMajorBit,

    CanVectorizeRhs = RhsRowMajor && (RhsFlags & PacketAccessBit)
                    && (ColsAtCompileTime == Dynamic || (ColsAtCompileTime % ei_packet_traits<Scalar>::size) == 0),

    CanVectorizeLhs = (!LhsRowMajor) && (LhsFlags & PacketAccessBit)
                    && (RowsAtCompileTime == Dynamic || (RowsAtCompileTime % ei_packet_traits<Scalar>::size) == 0),

    EvalToRowMajor = RhsRowMajor && (ProductMode==(int)CacheFriendlyProduct ? LhsRowMajor : (!CanVectorizeLhs)),

    RemovedBits = ~(EvalToRowMajor ? 0 : RowMajorBit),

    Flags = ((unsigned int)(LhsFlags | RhsFlags) & HereditaryBits & RemovedBits)
          | EvalBeforeAssigningBit
          | EvalBeforeNestingBit
          | (CanVectorizeLhs || CanVectorizeRhs ? PacketAccessBit : 0)
          | (LhsFlags & RhsFlags & AlignedBit),

    CoeffReadCost = InnerSize == Dynamic ? Dynamic
                  : InnerSize * (NumTraits<Scalar>::MulCost + LhsCoeffReadCost + RhsCoeffReadCost)
                    + (InnerSize - 1) * NumTraits<Scalar>::AddCost,

    /* CanVectorizeInner deserves special explanation. It does not affect the product flags. It is not used outside
     * of Product. If the Product itself is not a packet-access expression, there is still a chance that the inner
     * loop of the product might be vectorized. This is the meaning of CanVectorizeInner. Since it doesn't affect
     * the Flags, it is safe to make this value depend on ActualPacketAccessBit, that doesn't affect the ABI.
     */
    CanVectorizeInner = LhsRowMajor && (!RhsRowMajor) && (LhsFlags & RhsFlags & ActualPacketAccessBit)
                      && (InnerSize % ei_packet_traits<Scalar>::size == 0)
  };
};

template<typename LhsNested, typename RhsNested, int ProductMode> class Product : ei_no_assignment_operator,
  public MatrixBase<Product<LhsNested, RhsNested, ProductMode> >
{
  public:

    EIGEN_GENERIC_PUBLIC_INTERFACE(Product)

  private:

    typedef typename ei_traits<Product>::_LhsNested _LhsNested;
    typedef typename ei_traits<Product>::_RhsNested _RhsNested;

    enum {
      PacketSize = ei_packet_traits<Scalar>::size,
      InnerSize  = ei_traits<Product>::InnerSize,
      Unroll = CoeffReadCost <= EIGEN_UNROLLING_LIMIT,
      CanVectorizeInner = ei_traits<Product>::CanVectorizeInner
    };

    typedef ei_product_coeff_impl<CanVectorizeInner ? InnerVectorization : NoVectorization,
                                  Unroll ? InnerSize-1 : Dynamic,
                                  _LhsNested, _RhsNested, Scalar> ScalarCoeffImpl;

  public:

    template<typename Lhs, typename Rhs>
    inline Product(const Lhs& lhs, const Rhs& rhs)
      : m_lhs(lhs), m_rhs(rhs)
    {
      // we don't allow taking products of matrices of different real types, as that wouldn't be vectorizable.
      // We still allow to mix T and complex<T>.
      EIGEN_STATIC_ASSERT((ei_is_same_type<typename Lhs::RealScalar, typename Rhs::RealScalar>::ret),
        YOU_MIXED_DIFFERENT_NUMERIC_TYPES__YOU_NEED_TO_USE_THE_CAST_METHOD_OF_MATRIXBASE_TO_CAST_NUMERIC_TYPES_EXPLICITLY)
      ei_assert(lhs.cols() == rhs.rows()
        && "invalid matrix product"
        && "if you wanted a coeff-wise or a dot product use the respective explicit functions");
    }

    /** \internal
      * compute \a res += \c *this using the cache friendly product.
      */
    template<typename DestDerived>
    void _cacheFriendlyEvalAndAdd(DestDerived& res, Scalar alpha) const;

    /** \internal
      * \returns whether it is worth it to use the cache friendly product.
      */
    EIGEN_STRONG_INLINE bool _useCacheFriendlyProduct() const
    {
      // TODO do something more accurate here (especially for mat-vec products)
      return m_lhs.cols()>=EIGEN_CACHEFRIENDLY_PRODUCT_THRESHOLD
              && (  rows()>=EIGEN_CACHEFRIENDLY_PRODUCT_THRESHOLD
                 || cols()>=EIGEN_CACHEFRIENDLY_PRODUCT_THRESHOLD);
    }

    EIGEN_STRONG_INLINE int rows() const { return m_lhs.rows(); }
    EIGEN_STRONG_INLINE int cols() const { return m_rhs.cols(); }

    EIGEN_STRONG_INLINE const Scalar coeff(int row, int col) const
    {
      Scalar res;
      ScalarCoeffImpl::run(row, col, m_lhs, m_rhs, res);
      return res;
    }

    /* Allow index-based non-packet access. It is impossible though to allow index-based packed access,
     * which is why we don't set the LinearAccessBit.
     */
    EIGEN_STRONG_INLINE const Scalar coeff(int index) const
    {
      Scalar res;
      const int row = RowsAtCompileTime == 1 ? 0 : index;
      const int col = RowsAtCompileTime == 1 ? index : 0;
      ScalarCoeffImpl::run(row, col, m_lhs, m_rhs, res);
      return res;
    }

    template<int LoadMode>
    EIGEN_STRONG_INLINE const PacketScalar packet(int row, int col) const
    {
      PacketScalar res;
      ei_product_packet_impl<Flags&RowMajorBit ? RowMajor : ColMajor,
                                   Unroll ? InnerSize-1 : Dynamic,
                                   _LhsNested, _RhsNested, PacketScalar, LoadMode>
        ::run(row, col, m_lhs, m_rhs, res);
      return res;
    }

    EIGEN_STRONG_INLINE const _LhsNested& lhs() const { return m_lhs; }
    EIGEN_STRONG_INLINE const _RhsNested& rhs() const { return m_rhs; }

  protected:
    const LhsNested m_lhs;
    const RhsNested m_rhs;
};

/** \returns the matrix product of \c *this and \a other.
  *
  * \note If instead of the matrix product you want the coefficient-wise product, see Cwise::operator*().
  *
  * \sa lazy(), operator*=(const MatrixBase&), Cwise::operator*()
  */
template<typename Derived>
template<typename OtherDerived>
inline const typename ProductReturnType<Derived,OtherDerived>::Type
MatrixBase<Derived>::operator*(const MatrixBase<OtherDerived> &other) const
{
  enum {
    ProductIsValid =  Derived::ColsAtCompileTime==Dynamic
                   || OtherDerived::RowsAtCompileTime==Dynamic
                   || int(Derived::ColsAtCompileTime)==int(OtherDerived::RowsAtCompileTime),
    AreVectors = Derived::IsVectorAtCompileTime && OtherDerived::IsVectorAtCompileTime,
    SameSizes = EIGEN_PREDICATE_SAME_MATRIX_SIZE(Derived,OtherDerived)
  };
  // note to the lost user:
  //    * for a dot product use: v1.dot(v2)
  //    * for a coeff-wise product use: v1.cwise()*v2
  EIGEN_STATIC_ASSERT(ProductIsValid || !(AreVectors && SameSizes),
    INVALID_VECTOR_VECTOR_PRODUCT__IF_YOU_WANTED_A_DOT_OR_COEFF_WISE_PRODUCT_YOU_MUST_USE_THE_EXPLICIT_FUNCTIONS)
  EIGEN_STATIC_ASSERT(ProductIsValid || !(SameSizes && !AreVectors),
    INVALID_MATRIX_PRODUCT__IF_YOU_WANTED_A_COEFF_WISE_PRODUCT_YOU_MUST_USE_THE_EXPLICIT_FUNCTION)
  EIGEN_STATIC_ASSERT(ProductIsValid || SameSizes, INVALID_MATRIX_PRODUCT)
  return typename ProductReturnType<Derived,OtherDerived>::Type(derived(), other.derived());
}

/** replaces \c *this by \c *this * \a other.
  *
  * \returns a reference to \c *this
  */
template<typename Derived>
template<typename OtherDerived>
inline Derived &
MatrixBase<Derived>::operator*=(const MultiplierBase<OtherDerived> &other)
{
  return derived() = derived() * other.derived();
}

/***************************************************************************
* Normal product .coeff() implementation (with meta-unrolling)
***************************************************************************/

/**************************************
*** Scalar path  - no vectorization ***
**************************************/

template<int Index, typename Lhs, typename Rhs, typename RetScalar>
struct ei_product_coeff_impl<NoVectorization, Index, Lhs, Rhs, RetScalar>
{
  EIGEN_STRONG_INLINE static void run(int row, int col, const Lhs& lhs, const Rhs& rhs, RetScalar &res)
  {
    ei_product_coeff_impl<NoVectorization, Index-1, Lhs, Rhs, RetScalar>::run(row, col, lhs, rhs, res);
    res += lhs.coeff(row, Index) * rhs.coeff(Index, col);
  }
};

template<typename Lhs, typename Rhs, typename RetScalar>
struct ei_product_coeff_impl<NoVectorization, 0, Lhs, Rhs, RetScalar>
{
  EIGEN_STRONG_INLINE static void run(int row, int col, const Lhs& lhs, const Rhs& rhs, RetScalar &res)
  {
    res = lhs.coeff(row, 0) * rhs.coeff(0, col);
  }
};

template<typename Lhs, typename Rhs, typename RetScalar>
struct ei_product_coeff_impl<NoVectorization, Dynamic, Lhs, Rhs, RetScalar>
{
  EIGEN_STRONG_INLINE static void run(int row, int col, const Lhs& lhs, const Rhs& rhs, RetScalar& res)
  {
    ei_assert(lhs.cols()>0 && "you are using a non initialized matrix");
    res = lhs.coeff(row, 0) * rhs.coeff(0, col);
      for(int i = 1; i < lhs.cols(); ++i)
        res += lhs.coeff(row, i) * rhs.coeff(i, col);
  }
};

// prevent buggy user code from causing an infinite recursion
template<typename Lhs, typename Rhs, typename RetScalar>
struct ei_product_coeff_impl<NoVectorization, -1, Lhs, Rhs, RetScalar>
{
  EIGEN_STRONG_INLINE static void run(int, int, const Lhs&, const Rhs&, RetScalar&) {}
};

/*******************************************
*** Scalar path with inner vectorization ***
*******************************************/

template<int Index, typename Lhs, typename Rhs, typename PacketScalar>
struct ei_product_coeff_vectorized_unroller
{
  enum { PacketSize = ei_packet_traits<typename Lhs::Scalar>::size };
  EIGEN_STRONG_INLINE static void run(int row, int col, const Lhs& lhs, const Rhs& rhs, typename Lhs::PacketScalar &pres)
  {
    ei_product_coeff_vectorized_unroller<Index-PacketSize, Lhs, Rhs, PacketScalar>::run(row, col, lhs, rhs, pres);
    pres = ei_padd(pres, ei_pmul( lhs.template packet<Aligned>(row, Index) , rhs.template packet<Aligned>(Index, col) ));
  }
};

template<typename Lhs, typename Rhs, typename PacketScalar>
struct ei_product_coeff_vectorized_unroller<0, Lhs, Rhs, PacketScalar>
{
  EIGEN_STRONG_INLINE static void run(int row, int col, const Lhs& lhs, const Rhs& rhs, typename Lhs::PacketScalar &pres)
  {
    pres = ei_pmul(lhs.template packet<Aligned>(row, 0) , rhs.template packet<Aligned>(0, col));
  }
};

template<int Index, typename Lhs, typename Rhs, typename RetScalar>
struct ei_product_coeff_impl<InnerVectorization, Index, Lhs, Rhs, RetScalar>
{
  typedef typename Lhs::PacketScalar PacketScalar;
  enum { PacketSize = ei_packet_traits<typename Lhs::Scalar>::size };
  EIGEN_STRONG_INLINE static void run(int row, int col, const Lhs& lhs, const Rhs& rhs, RetScalar &res)
  {
    PacketScalar pres;
    ei_product_coeff_vectorized_unroller<Index+1-PacketSize, Lhs, Rhs, PacketScalar>::run(row, col, lhs, rhs, pres);
    ei_product_coeff_impl<NoVectorization,Index,Lhs,Rhs,RetScalar>::run(row, col, lhs, rhs, res);
    res = ei_predux(pres);
  }
};

template<typename Lhs, typename Rhs, int LhsRows = Lhs::RowsAtCompileTime, int RhsCols = Rhs::ColsAtCompileTime>
struct ei_product_coeff_vectorized_dyn_selector
{
  EIGEN_STRONG_INLINE static void run(int row, int col, const Lhs& lhs, const Rhs& rhs, typename Lhs::Scalar &res)
  {
    res = ei_dot_impl<
      Block<Lhs, 1, ei_traits<Lhs>::ColsAtCompileTime>,
      Block<Rhs, ei_traits<Rhs>::RowsAtCompileTime, 1>,
      LinearVectorization, NoUnrolling>::run(lhs.row(row), rhs.col(col));
  }
};

// NOTE the 3 following specializations are because taking .col(0) on a vector is a bit slower
// NOTE maybe they are now useless since we have a specialization for Block<Matrix>
template<typename Lhs, typename Rhs, int RhsCols>
struct ei_product_coeff_vectorized_dyn_selector<Lhs,Rhs,1,RhsCols>
{
  EIGEN_STRONG_INLINE static void run(int /*row*/, int col, const Lhs& lhs, const Rhs& rhs, typename Lhs::Scalar &res)
  {
    res = ei_dot_impl<
      Lhs,
      Block<Rhs, ei_traits<Rhs>::RowsAtCompileTime, 1>,
      LinearVectorization, NoUnrolling>::run(lhs, rhs.col(col));
  }
};

template<typename Lhs, typename Rhs, int LhsRows>
struct ei_product_coeff_vectorized_dyn_selector<Lhs,Rhs,LhsRows,1>
{
  EIGEN_STRONG_INLINE static void run(int row, int /*col*/, const Lhs& lhs, const Rhs& rhs, typename Lhs::Scalar &res)
  {
    res = ei_dot_impl<
      Block<Lhs, 1, ei_traits<Lhs>::ColsAtCompileTime>,
      Rhs,
      LinearVectorization, NoUnrolling>::run(lhs.row(row), rhs);
  }
};

template<typename Lhs, typename Rhs>
struct ei_product_coeff_vectorized_dyn_selector<Lhs,Rhs,1,1>
{
  EIGEN_STRONG_INLINE static void run(int /*row*/, int /*col*/, const Lhs& lhs, const Rhs& rhs, typename Lhs::Scalar &res)
  {
    res = ei_dot_impl<
      Lhs,
      Rhs,
      LinearVectorization, NoUnrolling>::run(lhs, rhs);
  }
};

template<typename Lhs, typename Rhs, typename RetScalar>
struct ei_product_coeff_impl<InnerVectorization, Dynamic, Lhs, Rhs, RetScalar>
{
  EIGEN_STRONG_INLINE static void run(int row, int col, const Lhs& lhs, const Rhs& rhs, typename Lhs::Scalar &res)
  {
    ei_product_coeff_vectorized_dyn_selector<Lhs,Rhs>::run(row, col, lhs, rhs, res);
  }
};

/*******************
*** Packet path  ***
*******************/

template<int Index, typename Lhs, typename Rhs, typename PacketScalar, int LoadMode>
struct ei_product_packet_impl<RowMajor, Index, Lhs, Rhs, PacketScalar, LoadMode>
{
  EIGEN_STRONG_INLINE static void run(int row, int col, const Lhs& lhs, const Rhs& rhs, PacketScalar &res)
  {
    ei_product_packet_impl<RowMajor, Index-1, Lhs, Rhs, PacketScalar, LoadMode>::run(row, col, lhs, rhs, res);
    res =  ei_pmadd(ei_pset1(lhs.coeff(row, Index)), rhs.template packet<LoadMode>(Index, col), res);
  }
};

template<int Index, typename Lhs, typename Rhs, typename PacketScalar, int LoadMode>
struct ei_product_packet_impl<ColMajor, Index, Lhs, Rhs, PacketScalar, LoadMode>
{
  EIGEN_STRONG_INLINE static void run(int row, int col, const Lhs& lhs, const Rhs& rhs, PacketScalar &res)
  {
    ei_product_packet_impl<ColMajor, Index-1, Lhs, Rhs, PacketScalar, LoadMode>::run(row, col, lhs, rhs, res);
    res =  ei_pmadd(lhs.template packet<LoadMode>(row, Index), ei_pset1(rhs.coeff(Index, col)), res);
  }
};

template<typename Lhs, typename Rhs, typename PacketScalar, int LoadMode>
struct ei_product_packet_impl<RowMajor, 0, Lhs, Rhs, PacketScalar, LoadMode>
{
  EIGEN_STRONG_INLINE static void run(int row, int col, const Lhs& lhs, const Rhs& rhs, PacketScalar &res)
  {
    res = ei_pmul(ei_pset1(lhs.coeff(row, 0)),rhs.template packet<LoadMode>(0, col));
  }
};

template<typename Lhs, typename Rhs, typename PacketScalar, int LoadMode>
struct ei_product_packet_impl<ColMajor, 0, Lhs, Rhs, PacketScalar, LoadMode>
{
  EIGEN_STRONG_INLINE static void run(int row, int col, const Lhs& lhs, const Rhs& rhs, PacketScalar &res)
  {
    res = ei_pmul(lhs.template packet<LoadMode>(row, 0), ei_pset1(rhs.coeff(0, col)));
  }
};

template<typename Lhs, typename Rhs, typename PacketScalar, int LoadMode>
struct ei_product_packet_impl<RowMajor, Dynamic, Lhs, Rhs, PacketScalar, LoadMode>
{
  EIGEN_STRONG_INLINE static void run(int row, int col, const Lhs& lhs, const Rhs& rhs, PacketScalar& res)
  {
    ei_assert(lhs.cols()>0 && "you are using a non initialized matrix");
    res = ei_pmul(ei_pset1(lhs.coeff(row, 0)),rhs.template packet<LoadMode>(0, col));
      for(int i = 1; i < lhs.cols(); ++i)
        res =  ei_pmadd(ei_pset1(lhs.coeff(row, i)), rhs.template packet<LoadMode>(i, col), res);
  }
};

template<typename Lhs, typename Rhs, typename PacketScalar, int LoadMode>
struct ei_product_packet_impl<ColMajor, Dynamic, Lhs, Rhs, PacketScalar, LoadMode>
{
  EIGEN_STRONG_INLINE static void run(int row, int col, const Lhs& lhs, const Rhs& rhs, PacketScalar& res)
  {
    ei_assert(lhs.cols()>0 && "you are using a non initialized matrix");
    res = ei_pmul(lhs.template packet<LoadMode>(row, 0), ei_pset1(rhs.coeff(0, col)));
      for(int i = 1; i < lhs.cols(); ++i)
        res =  ei_pmadd(lhs.template packet<LoadMode>(row, i), ei_pset1(rhs.coeff(i, col)), res);
  }
};

/***************************************************************************
* Cache friendly product callers and specific nested evaluation strategies
***************************************************************************/

// Forward declarations

// This helper class aims to determine which optimized product to call,
// and how to call it. We have to distinghish three major cases:
//  1 - matrix-matrix
//  2 - matrix-vector
//  3 - vector-matrix
// The storage order, and direct-access criteria are also important for in last 2 cases.
// For instance, with a mat-vec product, the matrix coeff are evaluated only once, and
// therefore it is useless to first evaluated it to next being able to directly access
// its coefficient.
template<typename ProductType,
  int LhsRows  = ei_traits<ProductType>::RowsAtCompileTime,
  int LhsOrder = int(ei_traits<ProductType>::LhsFlags)&RowMajorBit ? RowMajor : ColMajor,
  int LhsHasDirectAccess = ei_blas_traits<typename ei_traits<ProductType>::_LhsNested>::ActualAccess,
  int RhsCols  = ei_traits<ProductType>::ColsAtCompileTime,
  int RhsOrder = int(ei_traits<ProductType>::RhsFlags)&RowMajorBit ? RowMajor : ColMajor,
  int RhsHasDirectAccess = ei_blas_traits<typename ei_traits<ProductType>::_RhsNested>::ActualAccess>
struct ei_cache_friendly_product_selector
{
  template<typename DestDerived>
  inline static void run(DestDerived& res, const ProductType& product, typename ProductType::Scalar alpha)
  {
    product._cacheFriendlyEvalAndAdd(res, alpha);
  }
};

// optimized colmajor * vector path
template<typename ProductType, int LhsRows, int RhsOrder, int RhsAccess>
struct ei_cache_friendly_product_selector<ProductType,LhsRows,ColMajor,NoDirectAccess,1,RhsOrder,RhsAccess>
{
  template<typename DestDerived>
  inline static void run(DestDerived& res, const ProductType& product, typename ProductType::Scalar alpha)
  {
    ei_assert(alpha==typename ProductType::Scalar(1));
    const int size = product.rhs().rows();
    for (int k=0; k<size; ++k)
      res += product.rhs().coeff(k) * product.lhs().col(k);
  }
};

// optimized cache friendly colmajor * vector path for matrix with direct access flag
// NOTE this path could also be enabled for expressions if we add runtime align queries
template<typename ProductType, int LhsRows, int RhsOrder, int RhsAccess>
struct ei_cache_friendly_product_selector<ProductType,LhsRows,ColMajor,HasDirectAccess,1,RhsOrder,RhsAccess>
{
  typedef typename ProductType::Scalar Scalar;
  typedef ei_blas_traits<typename ei_traits<ProductType>::_LhsNested> LhsProductTraits;
  typedef ei_blas_traits<typename ei_traits<ProductType>::_RhsNested> RhsProductTraits;

  typedef typename LhsProductTraits::ExtractType ActualLhsType;
  typedef typename RhsProductTraits::ExtractType ActualRhsType;

  template<typename DestDerived>
  inline static void run(DestDerived& res, const ProductType& product, typename ProductType::Scalar alpha)
  {
    ActualLhsType actualLhs = LhsProductTraits::extract(product.lhs());
    ActualRhsType actualRhs = RhsProductTraits::extract(product.rhs());

    Scalar actualAlpha = alpha * LhsProductTraits::extractScalarFactor(product.lhs())
                               * RhsProductTraits::extractScalarFactor(product.rhs());

    enum {
      EvalToRes = (ei_packet_traits<Scalar>::size==1)
                ||((DestDerived::Flags&ActualPacketAccessBit) && (!(DestDerived::Flags & RowMajorBit))) };
    Scalar* EIGEN_RESTRICT _res;
    if (EvalToRes)
       _res = &res.coeffRef(0);
    else
    {
      _res = ei_aligned_stack_new(Scalar,res.size());
      Map<Matrix<Scalar,DestDerived::RowsAtCompileTime,1> >(_res, res.size()) = res;
    }
//     std::cerr << "colmajor * vector " << EvalToRes << "\n";
    ei_cache_friendly_product_colmajor_times_vector
      <LhsProductTraits::NeedToConjugate,RhsProductTraits::NeedToConjugate>(
      res.size(),
      &actualLhs.const_cast_derived().coeffRef(0,0), actualLhs.stride(),
      actualRhs, _res, actualAlpha);

    if (!EvalToRes)
    {
      res = Map<Matrix<Scalar,DestDerived::SizeAtCompileTime,1> >(_res, res.size());
      ei_aligned_stack_delete(Scalar, _res, res.size());
    }
  }
};

// optimized vector * rowmajor path
template<typename ProductType, int LhsOrder, int LhsAccess, int RhsCols>
struct ei_cache_friendly_product_selector<ProductType,1,LhsOrder,LhsAccess,RhsCols,RowMajor,NoDirectAccess>
{
  template<typename DestDerived>
  inline static void run(DestDerived& res, const ProductType& product, typename ProductType::Scalar alpha)
  {
    ei_assert(alpha==typename ProductType::Scalar(1));
    const int cols = product.lhs().cols();
    for (int j=0; j<cols; ++j)
      res += product.lhs().coeff(j) * product.rhs().row(j);
  }
};

// optimized cache friendly vector * rowmajor path for matrix with direct access flag
// NOTE this path coul also be enabled for expressions if we add runtime align queries
template<typename ProductType, int LhsOrder, int LhsAccess, int RhsCols>
struct ei_cache_friendly_product_selector<ProductType,1,LhsOrder,LhsAccess,RhsCols,RowMajor,HasDirectAccess>
{
  typedef typename ProductType::Scalar Scalar;
  typedef ei_blas_traits<typename ei_traits<ProductType>::_LhsNested> LhsProductTraits;
  typedef ei_blas_traits<typename ei_traits<ProductType>::_RhsNested> RhsProductTraits;

  typedef typename LhsProductTraits::ExtractType ActualLhsType;
  typedef typename RhsProductTraits::ExtractType ActualRhsType;

  template<typename DestDerived>
  inline static void run(DestDerived& res, const ProductType& product, typename ProductType::Scalar alpha)
  {
    ActualLhsType actualLhs = LhsProductTraits::extract(product.lhs());
    ActualRhsType actualRhs = RhsProductTraits::extract(product.rhs());

    Scalar actualAlpha = alpha * LhsProductTraits::extractScalarFactor(product.lhs())
                               * RhsProductTraits::extractScalarFactor(product.rhs());

    enum {
      EvalToRes = (ei_packet_traits<Scalar>::size==1)
                ||((DestDerived::Flags & ActualPacketAccessBit) && (DestDerived::Flags & RowMajorBit)) };
    Scalar* EIGEN_RESTRICT _res;
    if (EvalToRes)
       _res = &res.coeffRef(0);
    else
    {
      _res = ei_aligned_stack_new(Scalar, res.size());
      Map<Matrix<Scalar,DestDerived::SizeAtCompileTime,1> >(_res, res.size()) = res;
    }

    ei_cache_friendly_product_colmajor_times_vector
      <RhsProductTraits::NeedToConjugate,LhsProductTraits::NeedToConjugate>(res.size(),
      &actualRhs.const_cast_derived().coeffRef(0,0), actualRhs.stride(),
      actualLhs.transpose(), _res, actualAlpha);

    if (!EvalToRes)
    {
      res = Map<Matrix<Scalar,DestDerived::SizeAtCompileTime,1> >(_res, res.size());
      ei_aligned_stack_delete(Scalar, _res, res.size());
    }
  }
};

// optimized rowmajor - vector product
template<typename ProductType, int LhsRows, int RhsOrder, int RhsAccess>
struct ei_cache_friendly_product_selector<ProductType,LhsRows,RowMajor,HasDirectAccess,1,RhsOrder,RhsAccess>
{
  typedef typename ProductType::Scalar Scalar;

  typedef ei_blas_traits<typename ei_traits<ProductType>::_LhsNested> LhsProductTraits;
  typedef ei_blas_traits<typename ei_traits<ProductType>::_RhsNested> RhsProductTraits;

  typedef typename LhsProductTraits::ExtractType ActualLhsType;
  typedef typename RhsProductTraits::ExtractType ActualRhsType;
  typedef typename ei_cleantype<ActualRhsType>::type _ActualRhsType;

  enum {
      UseRhsDirectly = ((ei_packet_traits<Scalar>::size==1) || (_ActualRhsType::Flags&ActualPacketAccessBit))
                     && (!(_ActualRhsType::Flags & RowMajorBit)) };

  template<typename DestDerived>
  inline static void run(DestDerived& res, const ProductType& product, typename ProductType::Scalar alpha)
  {
    ActualLhsType actualLhs = LhsProductTraits::extract(product.lhs());
    ActualRhsType actualRhs = RhsProductTraits::extract(product.rhs());

    Scalar actualAlpha = alpha * LhsProductTraits::extractScalarFactor(product.lhs())
                               * RhsProductTraits::extractScalarFactor(product.rhs());

    Scalar* EIGEN_RESTRICT _rhs;
    if (UseRhsDirectly)
       _rhs = &actualRhs.const_cast_derived().coeffRef(0);
    else
    {
      _rhs = ei_aligned_stack_new(Scalar, actualRhs.size());
      Map<Matrix<Scalar,_ActualRhsType::SizeAtCompileTime,1> >(_rhs, actualRhs.size()) = actualRhs;
    }

    ei_cache_friendly_product_rowmajor_times_vector
      <LhsProductTraits::NeedToConjugate,RhsProductTraits::NeedToConjugate>(
        &actualLhs.const_cast_derived().coeffRef(0,0), actualLhs.stride(),
        _rhs, product.rhs().size(), res, actualAlpha);

    if (!UseRhsDirectly) ei_aligned_stack_delete(Scalar, _rhs, product.rhs().size());
  }
};

// optimized vector - colmajor product
template<typename ProductType, int LhsOrder, int LhsAccess, int RhsCols>
struct ei_cache_friendly_product_selector<ProductType,1,LhsOrder,LhsAccess,RhsCols,ColMajor,HasDirectAccess>
{
  typedef typename ProductType::Scalar Scalar;

  typedef ei_blas_traits<typename ei_traits<ProductType>::_LhsNested> LhsProductTraits;
  typedef ei_blas_traits<typename ei_traits<ProductType>::_RhsNested> RhsProductTraits;

  typedef typename LhsProductTraits::ExtractType ActualLhsType;
  typedef typename RhsProductTraits::ExtractType ActualRhsType;
  typedef typename ei_cleantype<ActualLhsType>::type _ActualLhsType;

  enum {
      UseLhsDirectly = ((ei_packet_traits<Scalar>::size==1) || (_ActualLhsType::Flags&ActualPacketAccessBit))
                     && (_ActualLhsType::Flags & RowMajorBit) };

  template<typename DestDerived>
  inline static void run(DestDerived& res, const ProductType& product, typename ProductType::Scalar alpha)
  {
    ActualLhsType actualLhs = LhsProductTraits::extract(product.lhs());
    ActualRhsType actualRhs = RhsProductTraits::extract(product.rhs());

    Scalar actualAlpha = alpha * LhsProductTraits::extractScalarFactor(product.lhs())
                               * RhsProductTraits::extractScalarFactor(product.rhs());

    Scalar* EIGEN_RESTRICT _lhs;
    if (UseLhsDirectly)
       _lhs = &actualLhs.const_cast_derived().coeffRef(0);
    else
    {
      _lhs = ei_aligned_stack_new(Scalar, actualLhs.size());
      Map<Matrix<Scalar,_ActualLhsType::SizeAtCompileTime,1> >(_lhs, actualLhs.size()) = actualLhs;
    }

    ei_cache_friendly_product_rowmajor_times_vector
      <RhsProductTraits::NeedToConjugate, LhsProductTraits::NeedToConjugate>(
        &actualRhs.const_cast_derived().coeffRef(0,0), actualRhs.stride(),
        _lhs, product.lhs().size(), res, actualAlpha);

    if(!UseLhsDirectly) ei_aligned_stack_delete(Scalar, _lhs, product.lhs().size());
  }
};

// discard this case which has to be handled by the default path
// (we keep it to be sure to hit a compilation error if this is not the case)
template<typename ProductType, int LhsRows, int RhsOrder, int RhsAccess>
struct ei_cache_friendly_product_selector<ProductType,LhsRows,RowMajor,NoDirectAccess,1,RhsOrder,RhsAccess>
{};

// discard this case which has to be handled by the default path
// (we keep it to be sure to hit a compilation error if this is not the case)
template<typename ProductType, int LhsOrder, int LhsAccess, int RhsCols>
struct ei_cache_friendly_product_selector<ProductType,1,LhsOrder,LhsAccess,RhsCols,ColMajor,NoDirectAccess>
{};


/** \internal
  * Overloaded to perform an efficient C += A*B */
template<typename Derived>
template<typename Lhs,typename Rhs>
inline Derived&
MatrixBase<Derived>::operator+=(const Flagged<Product<Lhs,Rhs,CacheFriendlyProduct>, 0, EvalBeforeNestingBit | EvalBeforeAssigningBit>& other)
{//std::cerr << "operator+=\n";
  if (other._expression()._useCacheFriendlyProduct())
    ei_cache_friendly_product_selector<Product<Lhs,Rhs,CacheFriendlyProduct> >::run(const_cast_derived(), other._expression(), Scalar(1));
  else { //std::cerr << "no cf\n";
    lazyAssign(derived() + other._expression());
  }
  return derived();
}

/** \internal
  * Overloaded to perform an efficient C -= A*B */
template<typename Derived>
template<typename Lhs,typename Rhs>
inline Derived&
MatrixBase<Derived>::operator-=(const Flagged<Product<Lhs,Rhs,CacheFriendlyProduct>, 0, EvalBeforeNestingBit | EvalBeforeAssigningBit>& other)
{
  if (other._expression()._useCacheFriendlyProduct())
    ei_cache_friendly_product_selector<Product<Lhs,Rhs,CacheFriendlyProduct> >::run(const_cast_derived(), other._expression(), Scalar(-1));
  else
    lazyAssign(derived() - other._expression());
  return derived();
}

/** \internal
  * Overloaded to perform an efficient C = A*B */
template<typename Derived>
template<typename Lhs, typename Rhs>
inline Derived& MatrixBase<Derived>::lazyAssign(const Product<Lhs,Rhs,CacheFriendlyProduct>& product)
{
  if (product._useCacheFriendlyProduct())
  {
    setZero();
    ei_cache_friendly_product_selector<Product<Lhs,Rhs,CacheFriendlyProduct> >::run(const_cast_derived(), product, Scalar(1));
  }
  else
  {
    lazyAssign(Product<Lhs,Rhs,NormalProduct>(product.lhs(),product.rhs()));
  }
  return derived();
}

template<typename Lhs, typename Rhs, int ProductMode>
template<typename DestDerived>
inline void Product<Lhs,Rhs,ProductMode>::_cacheFriendlyEvalAndAdd(DestDerived& res, Scalar alpha) const
{
  typedef ei_blas_traits<_LhsNested> LhsProductTraits;
  typedef ei_blas_traits<_RhsNested> RhsProductTraits;

  typedef typename LhsProductTraits::DirectLinearAccessType ActualLhsType;
  typedef typename RhsProductTraits::DirectLinearAccessType ActualRhsType;

  typedef typename ei_cleantype<ActualLhsType>::type _ActualLhsType;
  typedef typename ei_cleantype<ActualRhsType>::type _ActualRhsType;

  const ActualLhsType lhs = LhsProductTraits::extract(m_lhs);
  const ActualRhsType rhs = RhsProductTraits::extract(m_rhs);

  Scalar actualAlpha = alpha * LhsProductTraits::extractScalarFactor(m_lhs)
                             * RhsProductTraits::extractScalarFactor(m_rhs);

  ei_general_matrix_matrix_product<
      Scalar,
      (_ActualLhsType::Flags&RowMajorBit)?RowMajor:ColMajor, bool(LhsProductTraits::NeedToConjugate),
      (_ActualRhsType::Flags&RowMajorBit)?RowMajor:ColMajor, bool(RhsProductTraits::NeedToConjugate),
      (DestDerived::Flags&RowMajorBit)?RowMajor:ColMajor>
    ::run(
        rows(), cols(), lhs.cols(),
        (const Scalar*)&(lhs.const_cast_derived().coeffRef(0,0)), lhs.stride(),
        (const Scalar*)&(rhs.const_cast_derived().coeffRef(0,0)), rhs.stride(),
        (Scalar*)&(res.coeffRef(0,0)), res.stride(),
        actualAlpha);
}

#endif // EIGEN_PRODUCT_H
