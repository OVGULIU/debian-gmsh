// Gmsh - Copyright (C) 1997-2008 C. Geuzaine, J.-F. Remacle
//
// See the LICENSE.txt file for license information. Please report all
// bugs and problems to <gmsh@geuz.org>.

#include "GFace.h"
#include "GEdge.h"
#include "gmshVertex.h"
#include "Geo.h"
#include "GeoInterpolation.h"
#include "Message.h"

SPoint2 gmshVertex::reparamOnFace(GFace *face, int dir) const
{
  Surface *s = (Surface*)face->getNativePtr();

  if(s->geometry){
    // It is not always right if it is periodic.
    if(l_edges.size() == 1 && 
       (*l_edges.begin())->getBeginVertex() ==
       (*l_edges.begin())->getEndVertex()){
      Range<double> bb = (*l_edges.begin())->parBounds(0);
      return (*l_edges.begin())->reparamOnFace(face, bb.low(), dir);
    } 
    return v->pntOnGeometry;
  }

  if(s->Typ ==  MSH_SURF_REGL){
    Curve *C[4];
    for(int i = 0; i < 4; i++)
      List_Read(s->Generatrices, i, &C[i]);

    double U, V;    
    if ((C[0]->beg == v && C[3]->beg == v) ||
        (C[0]->end == v && C[3]->beg == v) ||
        (C[0]->beg == v && C[3]->end == v) ||
        (C[0]->end == v && C[3]->end == v)){
      U = V = 0;
    }
    else if ((C[0]->beg == v && C[1]->beg == v) ||
             (C[0]->end == v && C[1]->beg == v) ||
             (C[0]->beg == v && C[1]->end == v) ||
             (C[0]->end == v && C[1]->end == v)){
      U = 1;
      V = 0;
    }
    else if ((C[2]->beg == v && C[1]->beg == v) ||
             (C[2]->end == v && C[1]->beg == v) ||
             (C[2]->beg == v && C[1]->end == v) ||
             (C[2]->end == v && C[1]->end == v)){
      U = 1;
      V = 1;
    }
    else if ((C[2]->beg == v && C[3]->beg == v) ||
             (C[2]->end == v && C[3]->beg == v) ||
             (C[2]->beg == v && C[3]->end == v) ||
             (C[2]->end == v && C[3]->end == v)){
      U = 0;
      V = 1;
    }
    else{
      Msg::Info("Reparameterizing point %d on face %d", v->Num, s->Num);
      return GVertex::reparamOnFace(face, dir);
    }
    return SPoint2(U, V);
  }
  else{
    return GVertex::reparamOnFace(face, dir);
  }
}

GEntity::GeomType gmshVertex::geomType() const
{
  if(v->Typ == MSH_POINT_BND_LAYER)
    return BoundaryLayerPoint;
  else
    return Point;
}
