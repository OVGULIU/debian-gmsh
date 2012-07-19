// Gmsh - Copyright (C) 1997-2012 C. Geuzaine, J.-F. Remacle
//
// See the LICENSE.txt file for license information. Please report all
// bugs and problems to <gmsh@geuz.org>.

#ifndef _DOF_MANAGER_H_
#define _DOF_MANAGER_H_

#include <vector>
#include <string>
#include <complex>
#include <map>
#include <list>
#include <iostream>
#include "MVertex.h"
#include "linearSystem.h"
#include "fullMatrix.h"

class Dof{
 protected:
  // v(x) = \sum_f \sum_i v_{fi} s^f_i(x)
  long int _entity; // "i": node, edge, group, etc.
  int _type; // "f": basis function type index, etc.
 public:
  Dof(long int entity, int type) : _entity(entity), _type(type) {}
  inline long int getEntity() const { return _entity; }
  inline int getType() const { return _type; }
  inline static int createTypeWithTwoInts(int i1, int i2)
  {
    return i1 + 10000 * i2;
  }
  inline static void getTwoIntsFromType(int t, int &i1, int &i2)
  {
    i1 = t % 10000;
    i2 = t / 10000;
  }
  bool operator < (const Dof &other) const
  {
    if(_entity < other._entity) return true;
    if(_entity > other._entity) return false;
    if(_type < other._type) return true;
    return false;
  }
  bool operator == (const Dof &other) const{
    return (_entity == other._entity && _type == other._type);
  }
};

template<class T> struct dofTraits
{
  typedef T VecType;
  typedef T MatType;
  inline static void gemm(VecType &r, const MatType &m, const VecType &v,
                          double alpha, double beta)
  {
    r = beta * r + alpha * m * v;
  }
};

template<class T> struct dofTraits<fullMatrix<T> >
{
  typedef fullMatrix<T> VecType;
  typedef fullMatrix<T> MatType;
  inline static void gemm (VecType &r, const MatType &m, const VecType &v,
                           double alpha, double beta)
  {
    r.gemm(m, v, alpha,beta);
  }
};

/*
template<> struct dofTraits<fullVector<std::complex<double> > >
{
  typedef fullVector<std::complex<double> > VecType;
  typedef fullMatrix<std::complex<double> > MatType;
};
*/

template<class T>
class DofAffineConstraint{
 public:
  std::vector<std::pair<Dof, typename dofTraits<T>::MatType> > linear;
  typename dofTraits<T>::VecType shift;
};

//non template part that can be implemented in the cxx file (and so avoid to include mpi.h in the .h file)
class dofManagerBase{
  protected:
  // numbering of unknown dof blocks
  std::map<Dof, int> unknown;

  // associatations (not used ?)
  std::map<Dof, Dof> associatedWith;

  // parallel section
  // those dof are images of ghost located on another proc (id givent by the map).
  // this is a first try, maybe not the final implementation
  std::map<Dof, std::pair<int, int> > ghostByDof;  // dof => procId, globalId
  std::vector<std::vector<Dof> > ghostByProc, parentByProc;
  int _localSize;
  bool _parallelFinalized;
  bool _isParallel;
  void _parallelFinalize();
  dofManagerBase(bool isParallel) {
    _isParallel = isParallel;
    _parallelFinalized = false;
  }
};

// A manager for degrees of freedoms, templated on the value of a dof
// (what the functional returns): float, double, complex<double>,
// fullVecor<double>, ...
template <class T>
class dofManager : public dofManagerBase{
 public:
  typedef typename dofTraits<T>::VecType dataVec;
  typedef typename dofTraits<T>::MatType dataMat;
 protected:
  // general affine constraint on sub-blocks, treated by adding
  // equations:
  //   Dof = \sum_i dataMat_i x Dof_i + dataVec
  std::map<Dof, DofAffineConstraint< dataVec > > constraints;

  // fixations on full blocks, treated by eliminating equations:
  //   DofVec = dataVec
  std::map<Dof, dataVec> fixed;

  // initial conditions (not used ?)
  std::map<Dof, std::vector<dataVec> > initial;

  // linearSystems
  linearSystem<dataMat> *_current;
  std::map<const std::string, linearSystem<dataMat>*> _linearSystems;

  std::map<Dof, T> ghostValue;
  public:
  void scatterSolution();

 public:
  dofManager(linearSystem<dataMat> *l, bool isParallel=false)
    :dofManagerBase(isParallel), _current(l)
  {
    _linearSystems["A"] = l;
  }
  dofManager(linearSystem<dataMat> *l1, linearSystem<dataMat> *l2)
    :dofManagerBase(false), _current(l1)
  {
    _linearSystems.insert(std::make_pair("A", l1));
    _linearSystems.insert(std::make_pair("B", l2));
  }
  virtual ~dofManager(){}
  virtual inline void fixDof(Dof key, const dataVec &value)
  {
    if(unknown.find(key) != unknown.end())
      return;
    fixed[key] = value;
  }
  inline void fixDof(long int ent, int type, const dataVec &value)
  {
    fixDof(Dof(ent, type), value);
  }
  inline void fixVertex(MVertex*v, int iComp, int iField, const dataVec &value)
  {
    fixDof(v->getNum(), Dof::createTypeWithTwoInts(iComp, iField), value);
  }
  virtual inline bool isFixed(Dof key) const
  {
    if(fixed.find(key) != fixed.end()){
      return true;
    }
    return false;
  }

  virtual inline bool isAnUnknown(Dof key) const
  {
    if(ghostValue.find(key) == ghostValue.end())
    {
      if(unknown.find(key) != unknown.end())
        return true;
    }
    return false;
  }

  inline bool isFixed(long int ent, int type) const
  {
    return isFixed(Dof(ent, type));
  }
  inline bool isFixed(MVertex*v, int iComp, int iField) const
  {
    return isFixed(v->getNum(), Dof::createTypeWithTwoInts(iComp, iField));
  }
  virtual inline void numberGhostDof (Dof key, int procId) {
    if (fixed.find(key) != fixed.end()) return;
    if (constraints.find(key) != constraints.end()) return;
    if (ghostByDof.find(key) != ghostByDof.end()) return;
    ghostByDof[key] = std::make_pair(procId, 0);
  }
  virtual inline void numberDof(Dof key)
  {
    if (fixed.find(key) != fixed.end()) return;
    if (constraints.find(key) != constraints.end()) return;
    if (ghostByDof.find(key) != ghostByDof.end()) return;

    std::map<Dof, int> :: iterator it = unknown.find(key);
    if (it == unknown.end()) {
      unsigned int size = unknown.size();
      unknown[key] = size;
    }
  }
  virtual inline void numberDof(const std::vector<Dof> &R)
  {
    for(unsigned int i=0;i<R.size();i++)
      this->numberDof(R[i]);
  }
  inline void numberDof(long int ent, int type)
  {
    numberDof(Dof(ent, type));
  }
  inline void numberVertex(MVertex*v, int iComp, int iField)
  {
    numberDof(v->getNum(), Dof::createTypeWithTwoInts(iComp, iField));
  }
  virtual inline void getDofValue(std::vector<Dof> &keys, std::vector<dataVec> &Vals)
  {
    int ndofs = keys.size();
    size_t originalSize = Vals.size();
    Vals.resize(originalSize + ndofs);
    for (int i = 0; i < ndofs; ++i) getDofValue(keys[i], Vals[originalSize+i]);
  }

  virtual inline bool getAnUnknown(Dof key,  dataVec &val) const
  {
    if(ghostValue.find(key) == ghostValue.end())
    {
      std::map<Dof, int>::const_iterator it = unknown.find(key);
      if (it != unknown.end())
      {
        _current->getFromSolution(it->second, val);
        return true;
      }
    }
    return false;
  }

  virtual inline void getDofValue(Dof key,  dataVec &val) const
  {
    {
      typename std::map<Dof, dataVec>::const_iterator it = ghostValue.find(key);
      if (it != ghostValue.end()) {
        val =  it->second;
        return;
      }
    }
    {
      std::map<Dof, int>::const_iterator it = unknown.find(key);
      if (it != unknown.end()) {
        _current->getFromSolution(it->second, val);
        return;
      }
    }
    {
      typename std::map<Dof, dataVec>::const_iterator it = fixed.find(key);
      if (it != fixed.end()) {
        val =  it->second;
        return;
      }
    }
    {
      typename std::map<Dof, DofAffineConstraint< dataVec > >::const_iterator it =
        constraints.find(key);
      if (it != constraints.end()){
        dataVec tmp(val);
        val = it->second.shift;
        for (unsigned i = 0; i < (it->second).linear.size(); i++){
          /* gcc: warning: variable ‘itu’ set but not used
          std::map<Dof, int>::const_iterator itu = unknown.find
            (((it->second).linear[i]).first);*/
          getDofValue(((it->second).linear[i]).first, tmp);
          dofTraits<T>::gemm(val, ((it->second).linear[i]).second, tmp, 1, 1);
        }
        return ;
      }
    }
  }
  inline void getDofValue(int ent, int type, dataVec &v) const
  {
    getDofValue(Dof(ent, type), v);
  }
  inline void getDofValue(MVertex *v, int iComp, int iField, dataVec &value) const
  {
    getDofValue(v->getNum(), Dof::createTypeWithTwoInts(iComp, iField), value);
  }

  virtual inline void insertInSparsityPatternLinConst(const Dof &R, const Dof &C)
  {
    std::map<Dof, int>::iterator itR = unknown.find(R);
    if (itR != unknown.end())
    {
      typename std::map<Dof, DofAffineConstraint<dataVec> >::iterator itConstraint;
      itConstraint = constraints.find(C);
      if (itConstraint != constraints.end()){
        for (unsigned i = 0; i < (itConstraint->second).linear.size(); i++){
          insertInSparsityPattern(R, (itConstraint->second).linear[i].first);
        }
      }
    }
    else{  // test function ; (no shift ?)
      typename std::map<Dof, DofAffineConstraint<dataVec> >::iterator itConstraint;
      itConstraint = constraints.find(R);
      if (itConstraint != constraints.end()){
        for (unsigned i = 0; i < (itConstraint->second).linear.size(); i++){
          insertInSparsityPattern((itConstraint->second).linear[i].first, C);
        }
      }
    }
  }

  virtual inline void insertInSparsityPattern(const Dof &R, const Dof &C)
  {
    if (_isParallel && !_parallelFinalized) _parallelFinalize();
    if (!_current->isAllocated()) _current->allocate (sizeOfR());
    std::map<Dof, int>::iterator itR = unknown.find(R);
    if (itR != unknown.end()){
      std::map<Dof, int>::iterator itC = unknown.find(C);
      if (itC != unknown.end()){
        _current->insertInSparsityPattern(itR->second, itC->second);
      }
      else{
        typename std::map<Dof, dataVec>::iterator itFixed = fixed.find(C);
        if (itFixed != fixed.end()) {
        }
        else insertInSparsityPatternLinConst(R, C);
      }
    }
    if (itR == unknown.end())
    {
      insertInSparsityPatternLinConst(R, C);
    }
  }

  virtual inline void assemble(const Dof &R, const Dof &C, const dataMat &value)
  {
    if (_isParallel && !_parallelFinalized) _parallelFinalize();
    if (!_current->isAllocated()) _current->allocate (sizeOfR());
    std::map<Dof, int>::iterator itR = unknown.find(R);
    if (itR != unknown.end()){
      std::map<Dof, int>::iterator itC = unknown.find(C);
      if (itC != unknown.end()){
        _current->addToMatrix(itR->second, itC->second, value);
      }
      else{
        typename std::map<Dof, dataVec>::iterator itFixed = fixed.find(C);
        if (itFixed != fixed.end()) {
          // tmp = -value * itFixed->second
          dataVec tmp(itFixed->second);
          dofTraits<T>::gemm(tmp, value, itFixed->second, -1, 0);
          _current->addToRightHandSide(itR->second, tmp);
        }
        else assembleLinConst(R, C, value);
      }
    }
    if (itR == unknown.end())
    {
      assembleLinConst(R, C, value);
    }
  }
  virtual inline void assemble(std::vector<Dof> &R, std::vector<Dof> &C,
                       const fullMatrix<dataMat> &m)
  {
    if (_isParallel && !_parallelFinalized) _parallelFinalize();
    if (!_current->isAllocated()) _current->allocate(sizeOfR());

    std::vector<int> NR(R.size()), NC(C.size());

    for (unsigned int i = 0; i < R.size(); i++){
      std::map<Dof, int>::iterator itR = unknown.find(R[i]);
      if (itR != unknown.end()) NR[i] = itR->second;
      else NR[i] = -1;
    }
    for (unsigned int i = 0; i < C.size(); i++){
      std::map<Dof, int>::iterator itC = unknown.find(C[i]);
      if (itC != unknown.end()) NC[i] = itC->second;
      else NC[i] = -1;
    }
    for (unsigned int i = 0; i < R.size(); i++){
      if (NR[i] != -1){
        for (unsigned int j = 0; j < C.size(); j++){
          if (NC[j] != -1){
            _current->addToMatrix(NR[i], NC[j], m(i, j));
          }
          else{
            typename std::map<Dof,  dataVec>::iterator itFixed = fixed.find(C[j]);
            if (itFixed != fixed.end()){
              // tmp = -m(i,j) * itFixed->second
              dataVec tmp(itFixed->second);
              dofTraits<T>::gemm(tmp, m(i, j), itFixed->second, -1, 0);
              _current->addToRightHandSide(NR[i], tmp);
            }
            else assembleLinConst(R[i], C[j], m(i, j));
          }
        }
      }
      else{
        for (unsigned int j = 0; j < C.size(); j++){
          assembleLinConst(R[i], C[j], m(i, j));
        }
      }
    }
  }
  // for linear forms
  virtual inline void assemble(std::vector<Dof> &R, const fullVector<dataMat> &m)
  {
    if (_isParallel && !_parallelFinalized) _parallelFinalize();
    if (!_current->isAllocated()) _current->allocate(sizeOfR());
    std::vector<int> NR(R.size());
    for (unsigned int i = 0; i < R.size(); i++){
      std::map<Dof, int>::iterator itR = unknown.find(R[i]);
      if (itR != unknown.end()) NR[i] = itR->second;
      else NR[i] = -1;
    }
    for (unsigned int i = 0; i < R.size(); i++){
      if (NR[i] != -1){
        _current->addToRightHandSide(NR[i], m(i));
      }
      else{
        typename std::map<Dof, DofAffineConstraint<dataVec> >::iterator itConstraint;
        itConstraint = constraints.find(R[i]);
        if (itConstraint != constraints.end()){
          for (unsigned j = 0; j < (itConstraint->second).linear.size(); j++){
            dataMat tmp;
            dofTraits<T>::gemm(tmp, (itConstraint->second).linear[j].second, m(i), 1, 0);
            assemble((itConstraint->second).linear[j].first, tmp);
          }
        }
      }
    }
  }
  virtual inline void assemble(std::vector<Dof> &R, const fullMatrix<dataMat> &m)
  {
    if (_isParallel && !_parallelFinalized) _parallelFinalize();
    if (!_current->isAllocated()) _current->allocate(sizeOfR());
    std::vector<int> NR(R.size());
    for (unsigned int i = 0; i < R.size(); i++){
      std::map<Dof, int>::iterator itR = unknown.find(R[i]);
      if (itR != unknown.end()) NR[i] = itR->second;
      else NR[i] = -1;
    }
    for (unsigned int i = 0; i < R.size(); i++){
      if (NR[i] != -1){
        for (unsigned int j = 0; j < R.size(); j++){
          if (NR[j] != -1){
            _current->addToMatrix(NR[i], NR[j], m(i, j));
          }
          else{
            typename std::map<Dof,  dataVec>::iterator itFixed = fixed.find(R[j]);
            if (itFixed != fixed.end()){
              // tmp = -m(i,j) * itFixed->second
              dataVec tmp(itFixed->second);
              dofTraits<T>::gemm(tmp, m(i, j), itFixed->second, -1, 0);
              _current->addToRightHandSide(NR[i], tmp);
            } else assembleLinConst(R[i], R[j], m(i, j));
          }
        }
      }
      else{
        for (unsigned int j = 0; j < R.size(); j++){
          assembleLinConst(R[i], R[j], m(i, j));
        }
      }
    }
  }
  inline void assemble(int entR, int typeR, int entC, int typeC, const dataMat &value)
  {
    assemble(Dof(entR, typeR), Dof(entC, typeC), value);
  }
  inline void assemble(MVertex *vR, int iCompR, int iFieldR,
                       MVertex *vC, int iCompC, int iFieldC,
                       const dataMat &value)
  {
    assemble(vR->getNum(), Dof::createTypeWithTwoInts(iCompR, iFieldR),
             vC->getNum(), Dof::createTypeWithTwoInts(iCompC, iFieldC),
             value);
  }
  virtual inline void assemble(const Dof &R, const dataMat &value)
  {
    if (_isParallel && !_parallelFinalized) _parallelFinalize();
    if(!_current->isAllocated()) _current->allocate(sizeOfR());
    std::map<Dof, int>::iterator itR = unknown.find(R);
    if(itR != unknown.end()){
      _current->addToRightHandSide(itR->second, value);
    }
    else{
      typename std::map<Dof, DofAffineConstraint<dataVec> >::iterator itConstraint;
      itConstraint = constraints.find(R);
      if (itConstraint != constraints.end()){
        for (unsigned j = 0; j < (itConstraint->second).linear.size(); j++){
          dataMat tmp;
          dofTraits<T>::gemm(tmp, (itConstraint->second).linear[j].second, value, 1, 0);
          assemble((itConstraint->second).linear[j].first, tmp);
        }
      }
    }
  }
  inline void assemble(int entR, int typeR, const dataMat &value)
  {
    assemble(Dof(entR, typeR), value);
  }
  inline void assemble(MVertex *vR, int iCompR, int iFieldR,
                        const dataMat &value)
  {
    assemble(vR->getNum(), Dof::createTypeWithTwoInts(iCompR, iFieldR), value);
  }
  virtual int sizeOfR() const { return _isParallel ? _localSize : unknown.size(); }
  virtual int sizeOfF() const { return fixed.size(); }
  virtual void systemSolve(){ _current->systemSolve(); }
  virtual void systemClear()
  {
    _current->zeroMatrix();
    _current->zeroRightHandSide();
  }
  virtual inline void setCurrentMatrix(std::string name)
  {
    typename std::map<const std::string, linearSystem<dataMat>*>::iterator it =
      _linearSystems.find(name);
    if(it != _linearSystems.end())
      _current = it->second;
    else{
      Msg::Error("Current matrix %s not found ", name.c_str());
      throw;
    }
  }
  virtual linearSystem<dataMat> *getLinearSystem(std::string &name)
  {
    typename std::map<const std::string, linearSystem<dataMat>*>::iterator it =
      _linearSystems.find(name);
    if(it != _linearSystems.end())
      return it->second;
    else
      return 0;
  }
  virtual inline void setLinearConstraint (Dof key, DofAffineConstraint<dataVec> &affineconstraint)
  {
    constraints[key] = affineconstraint;
    // constraints.insert(std::make_pair(key, affineconstraint));
  }
  virtual inline void assembleLinConst(const Dof &R, const Dof &C, const dataMat &value)
  {
    std::map<Dof, int>::iterator itR = unknown.find(R);
    if (itR != unknown.end())
    {
      typename std::map<Dof, DofAffineConstraint<dataVec> >::iterator itConstraint;
      itConstraint = constraints.find(C);
      if (itConstraint != constraints.end()){
        dataMat tmp(value);
        for (unsigned i = 0; i < (itConstraint->second).linear.size(); i++){
          dofTraits<T>::gemm(tmp, (itConstraint->second).linear[i].second, value, 1, 0);
          assemble(R, (itConstraint->second).linear[i].first, tmp);
        }
        dataMat tmp2(value);
        dofTraits<T>::gemm(tmp2, value, itConstraint->second.shift, -1, 0);
        _current->addToRightHandSide(itR->second, tmp2);
      }
    }
    else{  // test function ; (no shift ?)
      typename std::map<Dof, DofAffineConstraint<dataVec> >::iterator itConstraint;
      itConstraint = constraints.find(R);
      if (itConstraint != constraints.end()){
        dataMat tmp(value);
        for (unsigned i = 0; i < (itConstraint->second).linear.size(); i++){
          dofTraits<T>::gemm(tmp, itConstraint->second.linear[i].second, value, 1, 0);
          assemble((itConstraint->second).linear[i].first, C, tmp);
        }
      }
    }
  }
  virtual void getFixedDof(std::vector<Dof> &R)
  {
    R.clear();
    R.reserve(fixed.size());
    typename std::map<Dof, dataVec>::iterator it;
    for(it = fixed.begin(); it != fixed.end(); ++it){
      R.push_back(it->first);
    }
  }

  virtual int getDofNumber(const Dof& key){
		std::map<Dof,int>::iterator it = unknown.find(key);
		if (it == unknown.end()) {
			return -1;
		}
		else return it->second;
	};
};

#endif
