// Gmsh - Copyright (C) 1997-2008 C. Geuzaine, J.-F. Remacle
//
// See the LICENSE.txt file for license information. Please report all
// bugs and problems to <gmsh@geuz.org>.

#ifndef _FOURIER_PROJECTION_FACE_H_
#define _FOURIER_PROJECTION_FACE_H_

#include "GModel.h"
#include "Range.h"

#if defined(HAVE_FOURIER_MODEL)

#include "FM_ProjectionSurface.h"

class fourierProjectionFace : public GFace {
 protected:
  FM::ProjectionSurface *ps_;
 public:
  fourierProjectionFace(GModel *m, int num, FM::ProjectionSurface* ps);
  ~fourierProjectionFace();
  Range<double> parBounds(int i) const;
  GPoint point(double par1, double par2) const; 
  SVector3 normal(const SPoint2 &param) const; 
  Pair<SVector3,SVector3> firstDer(const SPoint2 &param) const; 
  SPoint2 parFromPoint(const SPoint3 &) const;
  virtual GEntity::GeomType geomType() const { return GEntity::ProjectionFace; }
  ModelType getNativeType() const { return UnknownModel; }
  void *getNativePtr() const { return ps_; } 
};

#endif

#endif
