// Gmsh - Copyright (C) 1997-2009 C. Geuzaine, J.-F. Remacle
//
// See the LICENSE.txt file for license information. Please report all
// bugs and problems to <gmsh@geuz.org>.

#include "PViewData.h"
#include "adaptiveData.h"
#include "Numeric.h"
#include "GmshMessage.h"

PViewData::PViewData()
  : _dirty(true), _fileIndex(0), _adaptive(0)
{
}

PViewData::~PViewData()
{
  if(_adaptive) delete _adaptive;
  for(std::map<int, std::vector<fullMatrix<double>*> >::iterator it = _interpolation.begin();
      it != _interpolation.end(); it++)
    for(unsigned int i = 0; i < it->second.size(); i++)
      delete it->second[i];
}

bool PViewData::finalize()
{ 
  _dirty = false;
  return true;
}

void PViewData::initAdaptiveData(int step, int level, double tol)
{
  if(!_adaptive){
    Msg::Info("Initializing adaptive data %p interp size=%d", this, _interpolation.size());
    _adaptive = new adaptiveData(this);
    _adaptive->changeResolution(step, level, tol);
  }
}

void PViewData::destroyAdaptiveData()
{
  if(_adaptive) delete _adaptive;
  _adaptive = 0;
}

bool PViewData::empty()
{
  return (!getNumElements() && !getNumStrings2D() && !getNumStrings3D());
}

void PViewData::getScalarValue(int step, int ent, int ele, int nod, double &val)
{
  int numComp = getNumComponents(step, ent, ele);
  if(numComp == 1){
    getValue(step, ent, ele, nod, 0, val);
  }
  else{
    std::vector<double> d(numComp);
    for(int comp = 0; comp < numComp; comp++)
      getValue(step, ent, ele, nod, comp, d[comp]);
    val = ComputeScalarRep(numComp, &d[0]);
  }
}

void PViewData::setNode(int step, int ent, int ele, int nod, double x, double y, double z)
{
  Msg::Error("Cannot change node coordinates in this view");
}

void PViewData::setValue(int step, int ent, int ele, int nod, int comp, double val)
{
  Msg::Error("Cannot change field value in this view");
}

void PViewData::setInterpolationMatrices(int type, 
                                         const fullMatrix<double> &coefVal,
                                         const fullMatrix<double> &expVal)
{
  if(!type || _interpolation[type].size()) return;
  _interpolation[type].push_back(new fullMatrix<double>(coefVal));
  _interpolation[type].push_back(new fullMatrix<double>(expVal));
}

void PViewData::setInterpolationMatrices(int type, 
                                         const fullMatrix<double> &coefVal,
                                         const fullMatrix<double> &expVal, 
                                         const fullMatrix<double> &coefGeo,
                                         const fullMatrix<double> &expGeo)
{
  if(!type || _interpolation[type].size()) return;
  _interpolation[type].push_back(new fullMatrix<double>(coefVal));
  _interpolation[type].push_back(new fullMatrix<double>(expVal));
  _interpolation[type].push_back(new fullMatrix<double>(coefGeo));
  _interpolation[type].push_back(new fullMatrix<double>(expGeo));
}

int PViewData::getInterpolationMatrices(int type, std::vector<fullMatrix<double>*> &p)
{
  if(_interpolation.count(type)){
    p = _interpolation[type];
    return p.size();
  }
  return 0;
}

void PViewData::smooth()
{
  Msg::Error("Smoothing is not implemented for this type of data");
}

bool PViewData::combineTime(nameData &nd)
{ 
  Msg::Error("Combine time is not implemented for this type of data");
  return false; 
}

bool PViewData::combineSpace(nameData &nd)
{ 
  Msg::Error("Combine space is not implemented for this type of data");
  return false; 
}
