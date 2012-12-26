// Gmsh - Copyright (C) 1997-2012 C. Geuzaine, J.-F. Remacle
//
// See the LICENSE.txt file for license information. Please report all
// bugs and problems to <gmsh@geuz.org>.

#include "GmshConfig.h"
#if defined(HAVE_PETSC)
#include "petsc.h"
#include "linearSystemPETSc.h"
#include "fullMatrix.h"
#include <stdlib.h>
#include "GmshMessage.h"

#include "linearSystemPETSc.hpp"

template class linearSystemPETSc<double>;
#ifdef PETSC_USE_COMPLEX
template class linearSystemPETSc<std::complex<double> >;
#endif

void linearSystemPETScBlockDouble::_kspCreate()
{
  KSPCreate(_sequential ? PETSC_COMM_SELF : PETSC_COMM_WORLD, &_ksp);
  if (this->_parameters.count("petscPrefix"))
    KSPAppendOptionsPrefix(_ksp, this->_parameters["petscPrefix"].c_str());
  KSPSetFromOptions(_ksp);
  _kspAllocated = true;
}

void linearSystemPETScBlockDouble::addToMatrix(int row, int col,
                                               const fullMatrix<double> &val)
{
  if (!_entriesPreAllocated)
    preAllocateEntries();
  #ifdef PETSC_USE_COMPLEX
  fullMatrix<std::complex<double> > modval(val.size1(), val.size2());
  for (int ii = 0; ii < val.size1(); ii++) {
    for (int jj = 0; jj < val.size1(); jj++) {
      modval(ii, jj) = val (jj, ii);
      modval(jj, ii) = val (ii, jj);
    }
  }
  #else
  fullMatrix<double> &modval = *const_cast<fullMatrix<double> *>(&val);
  for (int ii = 0; ii < val.size1(); ii++) {
    for (int jj = 0; jj < ii; jj++) {
      PetscScalar buff = modval(ii, jj);
      modval(ii, jj) = modval (jj, ii);
      modval(jj, ii) = buff;
    }
  }
  #endif
  PetscInt i = row, j = col;
  MatSetValuesBlocked(_a, 1, &i, 1, &j, &modval(0,0), ADD_VALUES);
  //transpose back so that the original matrix is not modified
  #ifndef PETSC_USE_COMPLEX
  for (int ii = 0; ii < val.size1(); ii++)
    for (int jj = 0; jj < ii; jj++) {
      PetscScalar buff = modval(ii,jj);
      modval(ii, jj) = modval (jj,ii);
      modval(jj, ii) = buff;
    }
  #endif
  _matrixModified=true;
}

void linearSystemPETScBlockDouble::addToRightHandSide(int row,
                                                      const fullMatrix<double> &val)
{
  for (int ii = 0; ii < _blockSize; ii++) {
    PetscInt i = row * _blockSize + ii;
    PetscScalar v = val(ii, 0);
    VecSetValues(_b, 1, &i, &v, ADD_VALUES);
  }
}

void linearSystemPETScBlockDouble::getFromMatrix(int row, int col,
                                                 fullMatrix<double> &val ) const
{
  Msg::Error("getFromMatrix not implemented for PETSc");
}

void linearSystemPETScBlockDouble::getFromRightHandSide(int row,
                                                        fullMatrix<double> &val) const
{
  for (int i = 0; i < _blockSize; i++) {
    int ii = row*_blockSize +i;
    #ifdef PETSC_USE_COMPLEX
    PetscScalar s;
    VecGetValues ( _b, 1, &ii, &s);
    val(i,0) = s.real();
    #else
    VecGetValues ( _b, 1, &ii, &val(i,0));
    #endif
  }
}

void linearSystemPETScBlockDouble::addToSolution(int row, const fullMatrix<double> &val)
{
  for (int ii = 0; ii < _blockSize; ii++) {
    PetscInt i = row * _blockSize + ii;
    PetscScalar v = val(ii, 0);
    VecSetValues(_x, 1, &i, &v, ADD_VALUES);
  }
}

void linearSystemPETScBlockDouble::getFromSolution(int row, fullMatrix<double> &val) const
{
  for (int i = 0; i < _blockSize; i++) {
    int ii = row*_blockSize +i;
    #ifdef PETSC_USE_COMPLEX
    PetscScalar s;
    VecGetValues ( _x, 1, &ii, &s);
    val(i,0) = s.real();
    #else
    VecGetValues ( _x, 1, &ii, &val(i,0));
    #endif
  }
}


void linearSystemPETScBlockDouble::allocate(int nbRows)
{
  MPI_Comm comm = _sequential ? PETSC_COMM_SELF: PETSC_COMM_WORLD;
  if (this->_parameters.count("petscOptions"))
    PetscOptionsInsertString(this->_parameters["petscOptions"].c_str());
  _blockSize = strtol (_parameters["blockSize"].c_str(), NULL, 10);
  if (_blockSize == 0)
    Msg::Error ("'blockSize' parameters must be set for linearSystemPETScBlock");
  clear();
  MatCreate(comm, &_a);
  MatSetSizes(_a,nbRows * _blockSize, nbRows * _blockSize, PETSC_DETERMINE, PETSC_DETERMINE);
  if (Msg::GetCommSize() > 1 && !_sequential) {
    MatSetType(_a, MATMPIBAIJ);
  }
  else {
    MatSetType(_a, MATSEQBAIJ);
  }
  if (_parameters.count("petscPrefix"))
    MatAppendOptionsPrefix(_a, _parameters["petscPrefix"].c_str());
  MatSetFromOptions(_a);
  //since PETSc 3.3 GetOwnershipRange and MatGetSize() cannot be called before SetPreallocation
  _localSize = nbRows;
  _localRowStart = 0;
  _localRowEnd = nbRows;
  _globalSize = _localSize;
  #ifdef HAVE_MPI
  if (!_sequential) {
    _localRowStart = 0;
    if (Msg::GetCommRank() != 0) {
      MPI_Status status;
      MPI_Recv((void*)&_localRowStart, 1, MPI_INT, Msg::GetCommRank() - 1, 1, MPI_COMM_WORLD, &status);
    }
    _localRowEnd = _localRowStart + nbRows;
    if (Msg::GetCommRank() != Msg::GetCommSize() - 1) {
      MPI_Send((void*)&_localRowEnd, 1, MPI_INT, Msg::GetCommRank() + 1, 1, MPI_COMM_WORLD);
    }
    MPI_Allreduce((void*)&_localSize, (void*)&_globalSize, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
  }
  #endif
  // override the default options with the ones from the option
  // database (if any)
  VecCreate(comm, &_x);
  VecSetSizes(_x, nbRows * _blockSize, PETSC_DETERMINE);
  // override the default options with the ones from the option
  // database (if any)
  if (_parameters.count("petscPrefix"))
    VecAppendOptionsPrefix(_x, _parameters["petscPrefix"].c_str());
  VecSetFromOptions(_x);
  VecDuplicate(_x, &_b);
  _isAllocated = true;
}

bool linearSystemPETScBlockDouble::isAllocated() const
{
  return _isAllocated;
}

void linearSystemPETScBlockDouble::clear()
{
  if(_isAllocated){
#if (PETSC_VERSION_RELEASE == 0 || ((PETSC_VERSION_MAJOR == 3) && (PETSC_VERSION_MINOR >= 2)))
    MatDestroy(&_a);
    VecDestroy(&_x);
    VecDestroy(&_b);
#else
    MatDestroy(_a);
    VecDestroy(_x);
    VecDestroy(_b);
#endif
  }
  _isAllocated = false;
}

void linearSystemPETScBlockDouble::print()
{
  _try(MatAssemblyBegin(_a, MAT_FINAL_ASSEMBLY));
  _try(MatAssemblyEnd(_a, MAT_FINAL_ASSEMBLY));
  _try(VecAssemblyBegin(_b));
  _try(VecAssemblyEnd(_b));
  if(Msg::GetCommRank()==0)
    printf("a :\n");
  MatView(_a, PETSC_VIEWER_STDOUT_WORLD);
  if(Msg::GetCommRank()==0)
    printf("b :\n");
  VecView(_b, PETSC_VIEWER_STDOUT_WORLD);
  if(Msg::GetCommRank()==0)
    printf("x :\n");
  VecView(_x, PETSC_VIEWER_STDOUT_WORLD);
}


int linearSystemPETScBlockDouble::systemSolve()
{
  if (!_kspAllocated)
    _kspCreate();
  if (!_matrixModified || _parameters["matrix_reuse"] == "same_matrix")
    KSPSetOperators(_ksp, _a, _a, SAME_PRECONDITIONER);
  else if (_parameters["matrix_reuse"] == "same_sparsity")
    KSPSetOperators(_ksp, _a, _a, SAME_NONZERO_PATTERN);
  else
    KSPSetOperators(_ksp, _a, _a, DIFFERENT_NONZERO_PATTERN);
  MatAssemblyBegin(_a, MAT_FINAL_ASSEMBLY);
  MatAssemblyEnd(_a, MAT_FINAL_ASSEMBLY);
  _matrixModified=false;
  VecAssemblyBegin(_b);
  VecAssemblyEnd(_b);
  KSPSolve(_ksp, _b, _x);
  return 1;
}

void linearSystemPETScBlockDouble::insertInSparsityPattern (int i, int j)
{
  i -= _localRowStart;
  if (i<0 || i>= _localSize) return;
  _sparsity.insertEntry (i,j);
}

void linearSystemPETScBlockDouble::preAllocateEntries()
{
  if (_entriesPreAllocated) return;
  if (!_isAllocated) Msg::Fatal("system must be allocated first");
  if (_sparsity.getNbRows() == 0) {
    PetscInt prealloc = 300;
    PetscTruth set;
    PetscOptionsGetInt(PETSC_NULL, "-petsc_prealloc", &prealloc, &set);
    if (_blockSize == 0) {
      MatSeqAIJSetPreallocation(_a, prealloc, PETSC_NULL);
    } else {
      MatSeqBAIJSetPreallocation(_a, _blockSize, 5, PETSC_NULL);
    }
  } else {
    std::vector<int> nByRowDiag (_localSize), nByRowOffDiag (_localSize);
    for (int i = 0; i < _localSize; i++) {
      int n;
      const int *r = _sparsity.getRow(i, n);
      for (int j = 0; j < n; j++) {
        if (r[j] >= _localRowStart && r[j] < _localRowEnd)
          nByRowDiag[i] ++;
        else
          nByRowOffDiag[i] ++;
      }
    }
    if (_blockSize == 0) {
      MatSeqAIJSetPreallocation(_a, 0, &nByRowDiag[0]);
      MatMPIAIJSetPreallocation(_a, 0, &nByRowDiag[0], 0, &nByRowOffDiag[0]);
    } else {
      MatSeqBAIJSetPreallocation(_a, _blockSize, 0, &nByRowDiag[0]);
      MatMPIBAIJSetPreallocation(_a, _blockSize, 0, &nByRowDiag[0], 0, &nByRowOffDiag[0]);
    }
    _sparsity.clear();
  }
  _entriesPreAllocated = true;
}

void linearSystemPETScBlockDouble::zeroMatrix()
{
  if (_isAllocated && _entriesPreAllocated) {
    MatAssemblyBegin(_a, MAT_FINAL_ASSEMBLY);
    MatAssemblyEnd(_a, MAT_FINAL_ASSEMBLY);
    MatZeroEntries(_a);
  }
}

void linearSystemPETScBlockDouble::zeroRightHandSide()
{
  if (_isAllocated) {
    VecAssemblyBegin(_b);
    VecAssemblyEnd(_b);
    VecZeroEntries(_b);
  }
}

void linearSystemPETScBlockDouble::zeroSolution()
{
  if (_isAllocated) {
    VecAssemblyBegin(_x);
    VecAssemblyEnd(_x);
    VecZeroEntries(_x);
  }
}

linearSystemPETScBlockDouble::linearSystemPETScBlockDouble(bool sequential)
{
  _entriesPreAllocated = false;
  _isAllocated = false;
  _kspAllocated = false;
  _matrixModified=true;
  _sequential = sequential;
}

double linearSystemPETScBlockDouble::normInfRightHandSide() const
{
  PetscReal nor;
  VecAssemblyBegin(_b);
  VecAssemblyEnd(_b);
  VecNorm(_b, NORM_INFINITY, &nor);
  return nor;
}

#endif // HAVE_PETSC
