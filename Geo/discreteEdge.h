// Gmsh - Copyright (C) 1997-2009 C. Geuzaine, J.-F. Remacle
//
// See the LICENSE.txt file for license information. Please report all
// bugs and problems to <gmsh@geuz.org>.

#ifndef _DISCRETE_EDGE_H_
#define _DISCRETE_EDGE_H_

#include "GModel.h"
#include "GEdge.h"
#include "discreteVertex.h"

class discreteEdge : public GEdge {
 protected:
  std::vector<double> _pars;
  std::vector<int> _orientation;
  std::map<MVertex*,MLine*> boundv;
  mutable std::map<MVertex*,SVector3> _normals;
  bool createdTopo;
 public:
  discreteEdge(GModel *model, int num, GVertex *_v0, GVertex *_v1);
  virtual ~discreteEdge() {}
  void getLocalParameter(const double &t, int &iEdge, double &tLoc) const;
  virtual GeomType geomType() const { return DiscreteCurve; }
  virtual GPoint point(double p) const;
  virtual SVector3 firstDer(double par) const;
  virtual Range<double> parBounds(int) const;
  void parametrize();
  void orderMLines();
  void setBoundVertices();
  void createTopo();
  void computeNormals () const;
};

#endif
