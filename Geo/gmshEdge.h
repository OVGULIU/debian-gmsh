// Gmsh - Copyright (C) 1997-2008 C. Geuzaine, J.-F. Remacle
//
// See the LICENSE.txt file for license information. Please report all
// bugs and problems to <gmsh@geuz.org>.

#ifndef _GMSH_EDGE_H_
#define _GMSH_EDGE_H_

#include "Geo.h"
#include "GEdge.h"

class gmshEdge : public GEdge {
 protected:
  Curve *c; 

 public:
  gmshEdge(GModel *model, Curve *edge, GVertex *v1, GVertex *v2);
  virtual ~gmshEdge() {}
  virtual Range<double> parBounds(int i) const;
  virtual GeomType geomType() const;
  virtual GPoint point(double p) const;
  virtual GPoint closestPoint(const SPoint3 & queryPoint) const;
  virtual SVector3 firstDer(double par) const;
  ModelType getNativeType() const { return GmshModel; }
  void * getNativePtr() const { return c; }
  virtual double parFromPoint(const SPoint3 &pt) const;
  virtual int minimumMeshSegments() const;
  virtual int minimumDrawSegments() const;
  virtual void resetMeshAttributes();
  virtual SPoint2 reparamOnFace(GFace *face, double epar, int dir) const;
};

#endif
