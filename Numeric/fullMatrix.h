// Gmsh - Copyright (C) 1997-2009 C. Geuzaine, J.-F. Remacle
//
// See the LICENSE.txt file for license information. Please report all
// bugs and problems to <gmsh@geuz.org>.

#ifndef _FULL_MATRIX_H_
#define _FULL_MATRIX_H_

#include <math.h>
#include <stdio.h>
#include "GmshConfig.h"
#include "GmshMessage.h"

template <class scalar> class fullMatrix;

template <class scalar>
class fullVector
{
 private:
  int _r;
  scalar *_data;
  friend class fullMatrix<scalar>;
 public:
  fullVector(int r) : _r(r)
  {
    _data = new scalar[_r];
    scale(0.);
  }
  fullVector(const fullVector<scalar> &other) : _r(other._r)
  {
    _data = new scalar[_r];
    for(int i = 0; i < _r; ++i) _data[i] = other._data[i];
  }
  ~fullVector() { if(_data) delete [] _data; }
  inline scalar operator () (int i) const
  {
    return _data[i];
  }
  inline int size() const { return _r; }
  inline scalar & operator () (int i)
  {
    return _data[i];
  }
  inline scalar norm()
  {
    scalar n = 0.;
    for(int i = 0; i < _r; ++i) n += _data[i] * _data[i];
    return sqrt(n);
  }
  inline void scale(const scalar s)
  {
    if(s == 0.) 
      for(int i = 0; i < _r; ++i) _data[i] = 0.;
    else 
      for(int i = 0; i < _r; ++i) _data[i] *= s;
  }

  inline scalar operator *(const fullVector<scalar> & other)
  {
    scalar s = 0.0;
    for(int i = 0; i < _r; ++i) s += _data[i]*other._data[i];
    return s;
  }
  void print(const char * name="") const 
  {
    printf("Printing vector %s:\n", name);
    printf("  ");
    for(int I = 0; I < size(); I++){
      printf("%12.5E ", (*this)(I));
    }
    printf("\n");
  }
};

template <class scalar>
class fullMatrix
{
 private:
  int _r, _c;
  scalar *_data;
 public:
  fullMatrix(int r, int c) : _r(r), _c(c)
  {
    _data = new scalar[_r * _c];
    scale(0.);
  }
  fullMatrix(const fullMatrix<scalar> &other) : _r(other._r), _c(other._c)
  {
    _data = new scalar[_r * _c];
    gM_memcpy(other);
  }
  fullMatrix() : _r(0), _c(0), _data(0) {}
  ~fullMatrix() { if(_data) delete [] _data; }
  inline int size1() const { return _r; }
  inline int size2() const { return _c; }
  fullMatrix<scalar> & operator = (const fullMatrix<scalar> &other)
  {
    if(this != &other){
      _r = other._r; 
      _c = other._c;
      _data = new scalar[_r * _c];
      gM_memcpy(other);
    }
    return *this;
  }
  void gM_memcpy(const fullMatrix<scalar> &other)
  {
    for(int i = 0; i < _r * _c; ++i) _data[i] = other._data[i];
  }
  inline scalar operator () (int i, int j) const
  {
    return _data[i + _r * j];
  }
  inline scalar & operator () (int i, int j)
  {
    return _data[i + _r * j];
  }
  void copy(const fullMatrix<scalar> &a, int i0, int ni, int j0, int nj, 
            int desti0, int destj0)
  {
    for(int i = i0, desti = desti0; i < i0 + ni; i++, desti++)
      for(int j = j0, destj = destj0; j < j0 + nj; j++, destj++)
        (*this)(desti, destj) = a(i, j);
  }
  void mult(const fullMatrix<scalar> &b, fullMatrix<scalar> &c)
#if !defined(HAVE_BLAS)
  {
    c.scale(0.);
    for(int i = 0; i < _r; i++)
      for(int j = 0; j < b.size2(); j++)
        for(int k = 0; k < _c; k++)
          c._data[i + _r * j] += (*this)(i, k) * b(k, j);
  }
#endif
  ;
  void gemm(fullMatrix<scalar> &a, fullMatrix<scalar> &b, 
            scalar alpha=1., scalar beta=1.)
#if !defined(HAVE_BLAS)
  {
    fullMatrix<scalar> temp(a.size1(), b.size2());
    a.mult(b, temp);
    temp.scale(alpha);
    scale(beta);
    add(temp);
  }
#endif
  ;
  inline void set_all(const scalar &m) 
  {
    for(int i = 0; i < _r * _c; i++) _data[i] = m;
  }
  inline void scale(const double s)
  {
    if(s == 0.)
      for(int i = 0; i < _r * _c; ++i) _data[i] = 0.;
    else
      for(int i = 0; i < _r * _c; ++i) _data[i] *= s;
  }
  inline void add(const double &a) 
  {
    for(int i = 0; i < _r * _c; ++i) _data[i] += a;
  }
  inline void add(const fullMatrix<scalar> &m) 
  {
    for(int i = 0; i < size1(); i++)
      for(int j = 0; j < size2(); j++)
        (*this)(i, j) += m(i, j);
  }
  void mult(const fullVector<scalar> &x, fullVector<scalar> &y)
#if !defined(HAVE_BLAS)
  {
    y.scale(0.);
    for(int i = 0; i < _r; i++)
      for(int j = 0; j < _c; j++)
        y._data[i] += (*this)(i, j) * x(j);
  }
#endif
  ;
  inline fullMatrix<scalar> transpose()
  {
    fullMatrix<scalar> T(size2(), size1());
    for(int i = 0; i < size1(); i++)
      for(int j = 0; j < size2(); j++)
        T(j, i) = (*this)(i, j);
    return T;
  }
  inline void transposeInPlace()
  {
    if ( size1() != size2() ) {
      Msg::Error("Not a square matrix (size1: %d, size2: %d)",size1(),size2());
    }
    scalar t;
    for(int i = 0; i < size1(); i++)
      for(int j = 0; j < i; j++) {
        t = _data[i + _r * j];
        _data[i + _r * j] = _data[j + _r * i];
        _data[j + _r * i] = t;
      }
  }
  bool lu_solve(const fullVector<scalar> &rhs, fullVector<scalar> &result)
#if !defined(HAVE_LAPACK)
  {
    Msg::Error("LU factorization requires LAPACK");
    return false;
  }
#endif
  ;
  bool invertInPlace()
#if !defined(HAVE_LAPACK)
  {
    Msg::Error("Matrix inversion requires LAPACK");
    return false;
  }
#endif
  ;
  bool eig(fullMatrix<scalar> &VL, // left eigenvectors 
           fullVector<double> &DR, // Real part of eigenvalues
           fullVector<double> &DI, // Im part of eigen
           fullMatrix<scalar> &VR,
           bool sortRealPart=false) // if true: sorted from min 'DR' to max 'DR'
#if !defined(HAVE_LAPACK)
  {
    Msg::Error("Eigenvalue computations requires LAPACK");
    return false;
  }
#endif
  ;
  bool invert(fullMatrix<scalar> &result)
#if !defined(HAVE_LAPACK)
  {
    Msg::Error("LU factorization requires LAPACK");
    return false;
  }
#endif
  ;
  fullMatrix<scalar> cofactor(int i, int j) const 
  {
    int ni = size1();
    int nj = size2();
    fullMatrix<scalar> cof(ni - 1, nj - 1);
    for(int I = 0; I < ni; I++){
      for(int J = 0; J < nj; J++){
        if(J != j && I != i)
          cof(I < i ? I : I - 1, J < j ? J : J - 1) = (*this)(I, J);
      }
    }
    return cof;
  }
  scalar determinant() const
#if !defined(HAVE_LAPACK)
  {
    Msg::Error("Determinant computation requires LAPACK");
    return 0.;
  }
#endif
  ;
  bool svd(fullMatrix<scalar> &V, fullVector<scalar> &S)
#if !defined(HAVE_LAPACK)
  {
    Msg::Error("Singular value decomposition requires LAPACK");
    return false;
  }
#endif
  ;
  void print(const char * name="") const 
  {
    printf("Printing matrix %s:\n",name);
    int ni = size1();
    int nj = size2();
    for(int I = 0; I < ni; I++){
      printf("  ");
      for(int J = 0; J < nj; J++){
        printf("%12.5E ", (*this)(I, J));
      }
      printf("\n");
    }
  }
};

#endif
