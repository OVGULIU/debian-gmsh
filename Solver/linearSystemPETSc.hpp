#ifdef HAVE_PETSC
#include <petsc.h>
#include <petscksp.h>
#include "linearSystemPETSc.h"

#if (PETSC_VERSION_RELEASE == 0) // petsc-dev
#define PetscTruth PetscBool
#define PetscOptionsGetTruth PetscOptionsGetBool
#endif

static void  _try(int ierr)
{
  CHKERRABORT(PETSC_COMM_WORLD, ierr);
}

template <class scalar>
void linearSystemPETSc<scalar>::_kspCreate()
{
  _try(KSPCreate(PETSC_COMM_WORLD, &_ksp));
  PC pc;
  _try(KSPGetPC(_ksp, &pc));
  // set some default options
  //_try(PCSetType(pc, PCLU));//LU for direct solver and PCILU for indirect solver
  /*    _try(PCFactorSetMatOrderingType(pc, MATORDERING_RCM));
        _try(PCFactorSetLevels(pc, 1));*/
  _try(KSPSetTolerances(_ksp, 1.e-8, PETSC_DEFAULT, PETSC_DEFAULT, PETSC_DEFAULT));
  // override the default options with the ones from the option
  // database (if any)
  if (this->_parameters.count("petscPrefix"))
    _try(KSPAppendOptionsPrefix(_ksp, this->_parameters["petscPrefix"].c_str()));
  _try(KSPSetFromOptions(_ksp));
  _try(PCSetFromOptions(pc));
  _kspAllocated = true;
}

template <class scalar>
linearSystemPETSc<scalar>::linearSystemPETSc()
{
  _isAllocated = false;
  _blockSize = 0;
  _kspAllocated = false;
}

template <class scalar>
linearSystemPETSc<scalar>::~linearSystemPETSc()
{
  clear();
  if(_kspAllocated)
#if (PETSC_VERSION_RELEASE == 0) // petsc-dev
    _try(KSPDestroy(&_ksp));
#else
    _try(KSPDestroy(_ksp));
#endif
}

template <class scalar>
void linearSystemPETSc<scalar>::insertInSparsityPattern (int i, int j)
{
  i -= _localRowStart;
  if (i<0 || i>= _localSize) return;
  _sparsity.insertEntry (i,j);
}

template <class scalar>
void linearSystemPETSc<scalar>::preAllocateEntries()
{
  if (_entriesPreAllocated) return;
  if (!_isAllocated) Msg::Fatal("system must be allocated first");
  if (_sparsity.getNbRows() == 0) {
    PetscInt prealloc = 300;
    PetscTruth set;
    PetscOptionsGetInt(PETSC_NULL, "-petsc_prealloc", &prealloc, &set);
    if (_blockSize == 0) {
      _try(MatSeqAIJSetPreallocation(_a, prealloc, PETSC_NULL));
    } else {
      _try(MatSeqBAIJSetPreallocation(_a, _blockSize, 5, PETSC_NULL));
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
      _try(MatSeqAIJSetPreallocation(_a, 0, &nByRowDiag[0]));
      _try(MatMPIAIJSetPreallocation(_a, 0, &nByRowDiag[0], 0, &nByRowOffDiag[0]));
    } else {
      _try(MatSeqBAIJSetPreallocation(_a, _blockSize, 0, &nByRowDiag[0]));
      _try(MatMPIBAIJSetPreallocation(_a, _blockSize, 0, &nByRowDiag[0], 0, &nByRowOffDiag[0]));
    }
    _sparsity.clear();
  }
  _entriesPreAllocated = true;
}

template <class scalar>
void linearSystemPETSc<scalar>::allocate(int nbRows)
{
  clear();
  _try(MatCreate(PETSC_COMM_WORLD, &_a));
  _try(MatSetSizes(_a, nbRows, nbRows, PETSC_DETERMINE, PETSC_DETERMINE));
  // override the default options with the ones from the option
  // database (if any)
  if (this->_parameters.count("petscOptions"))
    _try(PetscOptionsInsertString(this->_parameters["petscOptions"].c_str()));
  if (this->_parameters.count("petscPrefix"))
    _try(MatAppendOptionsPrefix(_a, this->_parameters["petscPrefix"].c_str()));
  _try(MatSetFromOptions(_a));
  _try(MatGetOwnershipRange(_a, &_localRowStart, &_localRowEnd));
  int nbColumns;
  _localSize = _localRowEnd - _localRowStart;
  _try(MatGetSize(_a, &_globalSize, &nbColumns));
  // preallocation option must be set after other options
  _try(VecCreate(PETSC_COMM_WORLD, &_x));
  _try(VecSetSizes(_x, nbRows, PETSC_DETERMINE));
  // override the default options with the ones from the option
  // database (if any)
  if (this->_parameters.count("petscPrefix"))
    _try(VecAppendOptionsPrefix(_x, this->_parameters["petscPrefix"].c_str()));
  _try(VecSetFromOptions(_x));
  _try(VecDuplicate(_x, &_b));
  _isAllocated = true;
  _entriesPreAllocated = false;
}

template <class scalar>
void linearSystemPETSc<scalar>::print()
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

template <class scalar>
void linearSystemPETSc<scalar>::clear()
{
  if(_isAllocated){
#if (PETSC_VERSION_RELEASE == 0) // petsc-dev
    _try(MatDestroy(&_a));
    _try(VecDestroy(&_x));
    _try(VecDestroy(&_b));
#else
    _try(MatDestroy(_a));
    _try(VecDestroy(_x));
    _try(VecDestroy(_b));
#endif
  }
  _isAllocated = false;
}

template <class scalar>
void linearSystemPETSc<scalar>::getFromMatrix(int row, int col, scalar &val) const
{
  Msg::Error("getFromMatrix not implemented for PETSc");
}

template <class scalar>
void linearSystemPETSc<scalar>::addToRightHandSide(int row, const scalar &val)
{
  PetscInt i = row;
  PetscScalar s = val;
  _try(VecSetValues(_b, 1, &i, &s, ADD_VALUES));
}

template <class scalar>
void linearSystemPETSc<scalar>::getFromRightHandSide(int row, scalar &val) const
{
#if defined(PETSC_USE_COMPLEX)
  PetscScalar *tmp;
  _try(VecGetArray(_b, &tmp));
  PetscScalar s = tmp[row];
  _try(VecRestoreArray(_b, &tmp));
  // FIXME specialize this routine
  val = s.real();
#else
  _try(VecGetValues(_b, 1, &row, &val));
#endif
}

template <class scalar>
double linearSystemPETSc<scalar>::normInfRightHandSide() const
{
  PetscReal nor;
  VecAssemblyBegin(_b);
  VecAssemblyEnd(_b);
  _try(VecNorm(_b, NORM_INFINITY, &nor));
  return nor;
}

template <class scalar>
void linearSystemPETSc<scalar>::addToMatrix(int row, int col, const scalar &val)
{
  if (!_entriesPreAllocated)
    preAllocateEntries();
  PetscInt i = row, j = col;
  PetscScalar s = val;
  _try(MatSetValues(_a, 1, &i, 1, &j, &s, ADD_VALUES));
}

template <class scalar>
void linearSystemPETSc<scalar>::getFromSolution(int row, scalar &val) const
{
#if defined(PETSC_USE_COMPLEX)
  PetscScalar *tmp;
  _try(VecGetArray(_x, &tmp));
  PetscScalar s = tmp[row];
  _try(VecRestoreArray(_x, &tmp));
  val = s.real();
#else
  _try(VecGetValues(_x, 1, &row, &val));
#endif
}

template <class scalar>
void linearSystemPETSc<scalar>::zeroMatrix()
{
  if (_isAllocated && _entriesPreAllocated) {
    _try(MatAssemblyBegin(_a, MAT_FINAL_ASSEMBLY));
    _try(MatAssemblyEnd(_a, MAT_FINAL_ASSEMBLY));
    _try(MatZeroEntries(_a));
  }
}

template <class scalar>
void linearSystemPETSc<scalar>::zeroRightHandSide()
{
  if (_isAllocated) {
    _try(VecAssemblyBegin(_b));
    _try(VecAssemblyEnd(_b));
    _try(VecZeroEntries(_b));
  }
}

template <class scalar>
int linearSystemPETSc<scalar>::systemSolve()
{
  if (!_kspAllocated)
    _kspCreate();
  if (linearSystem<scalar>::_parameters["matrix_reuse"] == "same_sparsity")
    _try(KSPSetOperators(_ksp, _a, _a, SAME_NONZERO_PATTERN));
  else if (linearSystem<scalar>::_parameters["matrix_reuse"] == "same_matrix")
    _try(KSPSetOperators(_ksp, _a, _a, SAME_PRECONDITIONER));
  else
    _try(KSPSetOperators(_ksp, _a, _a, DIFFERENT_NONZERO_PATTERN));
  _try(MatAssemblyBegin(_a, MAT_FINAL_ASSEMBLY));
  _try(MatAssemblyEnd(_a, MAT_FINAL_ASSEMBLY));
  /*MatInfo info;
    MatGetInfo(_a, MAT_LOCAL, &info);
    printf("mallocs %.0f    nz_allocated %.0f    nz_used %.0f    nz_unneeded %.0f\n", info.mallocs, info.nz_allocated, info.nz_used, info.nz_unneeded);*/
  _try(VecAssemblyBegin(_b));
  _try(VecAssemblyEnd(_b));
  _try(KSPSolve(_ksp, _b, _x));
  //_try(KSPView(ksp, PETSC_VIEWER_STDOUT_SELF));
  //PetscInt its;
  //_try(KSPGetIterationNumber(ksp, &its));
  //Msg::Info("%d iterations", its);
  return 1;
}

template <class scalar>
std::vector<scalar> linearSystemPETSc<scalar>::getData()
{
  _try(MatAssemblyBegin(_a, MAT_FINAL_ASSEMBLY));
  _try(MatAssemblyEnd(_a, MAT_FINAL_ASSEMBLY));
  PetscScalar *v;
  _try(MatGetArray(_a,&v));
  MatInfo info;
  _try(MatGetInfo(_a,MAT_LOCAL,&info));
  std::vector<scalar> data; // Maybe I should reserve or resize (SAM)

#if defined(PETSC_USE_COMPLEX)
  for (int i = 0; i < info.nz_allocated; i++)
    data.push_back(v[i].real());
#else
  for (int i = 0; i < info.nz_allocated; i++)
    data.push_back(v[i]);
#endif
  _try(MatRestoreArray(_a,&v));
  return data;
}

template <class scalar>
std::vector<int> linearSystemPETSc<scalar>::getRowPointers()
{
  _try(MatAssemblyBegin(_a, MAT_FINAL_ASSEMBLY));
  _try(MatAssemblyEnd(_a, MAT_FINAL_ASSEMBLY));
  PetscInt *rows;
  PetscInt *columns;
  PetscInt n;
  PetscTruth done;
  _try(MatGetRowIJ(_a,0,PETSC_FALSE,PETSC_FALSE,&n,&rows,&columns,&done));        //case done == PETSC_FALSE should be handled
  std::vector<int> rowPointers; // Maybe I should reserve or resize (SAM)
  for (int i = 0; i <= n; i++)
    rowPointers.push_back(rows[i]);
  _try(MatRestoreRowIJ(_a,0,PETSC_FALSE,PETSC_FALSE,&n,&rows,&columns,&done));
  return rowPointers;
}

template <class scalar>
std::vector<int> linearSystemPETSc<scalar>::getColumnsIndices()
{
  _try(MatAssemblyBegin(_a, MAT_FINAL_ASSEMBLY));
  _try(MatAssemblyEnd(_a, MAT_FINAL_ASSEMBLY));
  PetscInt *rows;
  PetscInt *columns;
  PetscInt n;
  PetscTruth done;
  _try(MatGetRowIJ(_a,0,PETSC_FALSE,PETSC_FALSE,&n,&rows,&columns,&done));        //case done == PETSC_FALSE should be handled
  MatInfo info;
  _try(MatGetInfo(_a,MAT_LOCAL,&info));
  std::vector<int> columnIndices; // Maybe I should reserve or resize (SAM)
  for (int i = 0; i <  info.nz_allocated; i++)
    columnIndices.push_back(columns[i]);
  _try(MatRestoreRowIJ(_a,0,PETSC_FALSE,PETSC_FALSE,&n,&rows,&columns,&done));
  return columnIndices;
}
#endif
