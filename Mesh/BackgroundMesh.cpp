// Gmsh - Copyright (C) 1997-2009 C. Geuzaine, J.-F. Remacle
//
// See the LICENSE.txt file for license information. Please report all
// bugs and problems to <gmsh@geuz.org>.

#include "GmshMessage.h"
#include "BackgroundMesh.h"
#include "Numeric.h"
#include "Context.h"
#include "GVertex.h"
#include "GEdge.h"
#include "GFace.h"
#include "GModel.h"
#include "Field.h"

// computes the characteristic length of the mesh at a vertex in order
// to have the geometry captured with accuracy. A parameter called
// CTX::instance()->mesh.minCircPoints tells the minimum number of points per
// radius of curvature

static double max_edge_curvature(const GVertex *gv)
{
  double val = 0;
  std::list<GEdge*> l_edges = gv->edges();
  for (std::list<GEdge*>::const_iterator ite = l_edges.begin(); 
       ite != l_edges.end(); ++ite){
    GEdge *_myGEdge = *ite;
    Range<double> range = _myGEdge->parBounds(0);      
    double cc;
    if (gv == _myGEdge->getBeginVertex()) cc = _myGEdge->curvature(range.low());
    else cc = _myGEdge->curvature(range.high());
    val = std::max(val, cc);
  }
  return val;
}

static double max_surf_curvature(const GEdge *ge, double u)
{
  double val = 0;
  std::list<GFace *> faces = ge->faces();
  std::list<GFace *>::iterator it = faces.begin();
  while(it != faces.end()){
    if ((*it)->geomType() != GEntity::CompoundSurface){
      SPoint2 par = ge->reparamOnFace((*it), u, 1);
      double cc = (*it)->curvature(par);
      val = std::max(cc, val);
    }
    ++it;
  }  
  return val;
}

static double max_surf_curvature(const GVertex *gv)
{
  double val = 0;
  std::list<GEdge*> l_edges = gv->edges();
  for (std::list<GEdge*>::const_iterator ite = l_edges.begin(); 
       ite != l_edges.end(); ++ite){
    GEdge *_myGEdge = *ite;
    Range<double> bounds = _myGEdge->parBounds(0);
    if (gv == _myGEdge->getBeginVertex())
      val = std::max(val, max_surf_curvature(_myGEdge, bounds.low()));
    else
      val = std::max(val, max_surf_curvature(_myGEdge, bounds.high()));
  }
  return val;
}

// the mesh vertex is classified on a model vertex.  we compute the
// maximum of the curvature of model faces surrounding this point if
// it is classified on a model edge, we do the same for all model
// faces surrounding it if it is on a model face, we compute the
// curvature at this location

static double LC_MVertex_CURV(GEntity *ge, double U, double V)
{
  double Crv = 0;
  switch(ge->dim()){
  case 0:        
    //    Crv = max_edge_curvature((const GVertex *)ge);
    //    Crv = std::max(max_surf_curvature((const GVertex *)ge), Crv);
    Crv = max_surf_curvature((const GVertex *)ge);
    break;
  case 1:
    {
      GEdge *ged = (GEdge *)ge;
      //      Crv = ged->curvature(U)*2;
      //      Crv = std::max(Crv, max_surf_curvature(ged, U));
      Crv = max_surf_curvature(ged, U);      
    }
    break;
  case 2:
    {
      GFace *gf = (GFace *)ge;
      Crv = gf->curvature(SPoint2(U, V));
    }
    break;
  }
 
  double lc = Crv > 0 ? 2 * M_PI / Crv / CTX::instance()->mesh.minCircPoints : MAX_LC;
  return lc;
}

// compute the mesh size at a given vertex due to prescribed sizes at
// mesh vertices
static double LC_MVertex_PNTS(GEntity *ge, double U, double V)
{
  switch(ge->dim()){
  case 0:
    {
      GVertex *gv = (GVertex *)ge;
      double lc = gv->prescribedMeshSizeAtVertex();
      // FIXME we might want to remove this to make all lc treatment consistent
      if(lc >= MAX_LC) return CTX::instance()->lc / 10.;
      return lc;
    }
  case 1:
    {
      GEdge *ged = (GEdge *)ge;
      GVertex *v1 = ged->getBeginVertex();
      GVertex *v2 = ged->getEndVertex();
      if (v1 && v2){
        Range<double> range = ged->parBounds(0);      
        double a = (U - range.low()) / (range.high() - range.low()); 
        double lc = (1 - a) * v1->prescribedMeshSizeAtVertex() +
          (a) * v2->prescribedMeshSizeAtVertex() ;
        // FIXME we might want to remove this to make all lc treatment consistent
        if(lc >= MAX_LC) return CTX::instance()->lc / 10.;
        return lc;
      }
      else 
        return MAX_LC; 
    }
  default:
    return MAX_LC;
  }
}

// This is the only function that is used by the meshers
double BGM_MeshSize(GEntity *ge, double U, double V, 
                    double X, double Y, double Z)
{
  // default lc (mesh size == size of the model)
  double l1 = CTX::instance()->lc;

  // lc from points
  double l2 = MAX_LC;
  if(CTX::instance()->mesh.lcFromPoints && ge->dim() < 2) 
    l2 = LC_MVertex_PNTS(ge, U, V);

  // lc from curvature
  double l3 = MAX_LC;
  if(CTX::instance()->mesh.lcFromCurvature && ge->dim() < 3)
    l3 = LC_MVertex_CURV(ge, U, V);

  // lc from fields
  double l4 = MAX_LC;
  FieldManager *fields = GModel::current()->getFields();
  if(fields->background_field > 0){
    Field *f = fields->get(fields->background_field);
    if(f) l4 = (*f)(X, Y, Z, ge);
  }

  // take the minimum, then constrain by lcMin and lcMax
  double lc = std::min(std::min(std::min(l1, l2), l3), l4);
  lc = std::max(lc, CTX::instance()->mesh.lcMin);
  lc = std::min(lc, CTX::instance()->mesh.lcMax);

  if(lc <= 0.){
    Msg::Error("Wrong characteristic length lc = %g (lcmin = %g, lcmax = %g)",
               lc, CTX::instance()->mesh.lcMin, CTX::instance()->mesh.lcMax);
    lc = l1;
  }

  return lc * CTX::instance()->mesh.lcFactor;
}


// anisotropic version of the background field
SMetric3 BGM_MeshMetric(GEntity *ge, 
                        double U, double V, 
                        double X, double Y, double Z)
{
  // default lc (mesh size == size of the model)
  double l1 = CTX::instance()->lc;

  // lc from points            
  double l2 = MAX_LC;
  if(CTX::instance()->mesh.lcFromPoints && ge->dim() < 2) 
    l2 = LC_MVertex_PNTS(ge, U, V);
  
  // lc from curvature
  double l3 = MAX_LC;
  if(CTX::instance()->mesh.lcFromCurvature && ge->dim() < 3)
    l3 = LC_MVertex_CURV(ge, U, V);

  // lc from fields
  SMetric3 l4(MAX_LC);
  FieldManager *fields = GModel::current()->getFields();
  if(fields->background_field > 0){
    Field *f = fields->get(fields->background_field);
    if(f){
      if (!f->isotropic())
        (*f)(X, Y, Z, l4,ge);
      else
        l4 = SMetric3((*f)(X, Y, Z, ge));
    }
  }
  
  // take the minimum, then constrain by lcMin and lcMax
  double lc = std::min(std::min(l1, l2), l3);
  lc = std::max(lc, CTX::instance()->mesh.lcMin);
  lc = std::min(lc, CTX::instance()->mesh.lcMax);

  if(lc <= 0.){
     Msg::Error("Wrong characteristic length lc = %g (lcmin = %g, lcmax = %g)",
               lc, CTX::instance()->mesh.lcMin, CTX::instance()->mesh.lcMax);
     lc = l1;
  }

  SMetric3 LC(lc);
  return intersection (l4, LC);
  //  return lc * CTX::instance()->mesh.lcFactor;
}

bool Extend1dMeshIn2dSurfaces()
{
  return CTX::instance()->mesh.lcExtendFromBoundary ? true : false;
}

bool Extend2dMeshIn3dVolumes()
{
  return CTX::instance()->mesh.lcExtendFromBoundary ? true : false;
}
