// Gmsh - Copyright (C) 1997-2008 C. Geuzaine, J.-F. Remacle
//
// See the LICENSE.txt file for license information. Please report all
// bugs and problems to <gmsh@geuz.org>.

#include "GModel.h"
#include "Message.h"
#include "OCCEdge.h"
#include "OCCFace.h"
#include "Context.h"
extern Context_T CTX;

#if defined(HAVE_OCC)
#include "Geom2dLProp_CLProps2d.hxx"
#include "Geom_BezierCurve.hxx"
#include "Geom_OffsetCurve.hxx"
#include "Geom_Ellipse.hxx"
#include "Geom_Parabola.hxx"
#include "Geom_Hyperbola.hxx"
#include "Geom_TrimmedCurve.hxx"
#include "Geom_Circle.hxx"
#include "Geom_Line.hxx"
#include "Geom_Conic.hxx"

OCCEdge::OCCEdge(GModel *model, TopoDS_Edge edge, int num, GVertex *v1, GVertex *v2)
  : GEdge(model, num, v1, v2), c(edge), trimmed(0)
{
  curve = BRep_Tool::Curve(c, s0, s1);
  // build the reverse curve
  c_rev = c;
  c_rev.Reverse();
}

Range<double> OCCEdge::parBounds(int i) const
{ 
  return Range<double>(s0, s1);
}

void OCCEdge::setTrimmed (OCCFace *f)
{
  if (!trimmed){
    trimmed = f;
    const TopoDS_Face *s = (TopoDS_Face*) trimmed->getNativePtr();
    curve2d = BRep_Tool::CurveOnSurface(c, *s, s0, s1);
    if(curve2d.IsNull()) trimmed = 0;
  }
}

SPoint2 OCCEdge::reparamOnFace(GFace *face, double epar, int dir) const
{
  const TopoDS_Face *s = (TopoDS_Face*) face->getNativePtr();
  double t0, t1;
  Handle(Geom2d_Curve) c2d;

  if(dir == 1){
    c2d = BRep_Tool::CurveOnSurface(c, *s, t0, t1);
  }
  else{
    c2d = BRep_Tool::CurveOnSurface(c_rev, *s, t0, t1);
  }
  
  if(c2d.IsNull()){
    Msg::Fatal("Reparam on face failed: curve %d is not on surface %d",
	       tag(), face->tag());
  }

  double u, v;
  gp_Pnt2d pnt = c2d->Value(epar);
  pnt.Coord(u, v);

  // sometimes OCC miserably fails ...
  GPoint p1 = point(epar);
  GPoint p2 = face->point(u, v);
  const double dx = p1.x()-p2.x();
  const double dy = p1.y()-p2.y();
  const double dz = p1.z()-p2.z();
  if(sqrt(dx * dx + dy * dy + dz * dz) > 1.e-4 * CTX.lc){
    // return reparamOnFace(face, epar,-1);      
    Msg::Warning("Reparam on face partially failed for curve %d surface %d at point %g",
		 tag(), face->tag(), epar);
    Msg::Warning("On the face %d local (%g %g) global (%g %g %g)",
		 face->tag(), u, v, p2.x(), p2.y(), p2.z());
    Msg::Warning("On the edge %d local (%g) global (%g %g %g)",
		 tag(), epar, p1.x(), p1.y(), p1.z());
    // GPoint ppp = face->closestPoint(SPoint3(p1.x(), p1.y(), p1.z()));
    // return SPoint2(ppp.u(), ppp.v());
  }
  return SPoint2(u, v);
}

// True if the edge is a seam for the given face
bool OCCEdge::isSeam(GFace *face) const
{
  const TopoDS_Face *s = (TopoDS_Face*) face->getNativePtr();
  BRepAdaptor_Surface surface(*s);
  //  printf("asking if edge %d is a seam of face %d\n",tag(),face->tag());
  //  printf("periodic %d %d\n",surface.IsUPeriodic(),surface.IsVPeriodic());
  //  if(surface.IsUPeriodic() || surface.IsVPeriodic()){
  return BRep_Tool::IsClosed(c, *s);
  //  }
  // return false;
}

GPoint OCCEdge::point(double par) const
{
  if(trimmed){
    double u, v;
    curve2d->Value(par).Coord(u, v);
    return trimmed->point(u, v);
  }
  else if(!curve.IsNull()){
    gp_Pnt pnt = curve->Value (par);
    return GPoint(pnt.X(), pnt.Y(), pnt.Z());
  }
  else{
    Msg::Warning("OCC Curve %d is neither a 3D curve not a trimmed curve", tag());
    return GPoint(0, 0, 0);
  }
}

SVector3 OCCEdge::firstDer(double par) const
{  
  BRepAdaptor_Curve brepc(c);
  BRepLProp_CLProps prop(brepc, 1, 1e-5);
  prop.SetParameter (par);
  gp_Vec d1 = prop.D1();
  return SVector3(d1.X(), d1.Y(), d1.Z());
}

double OCCEdge::parFromPoint(const SPoint3 &pt) const
{
  Msg::Error("parFromPoint not implemented for OCCEdge");
  return 0.;
}

GEntity::GeomType OCCEdge::geomType() const
{
  if(curve.IsNull()){
    if (curve2d->DynamicType() == STANDARD_TYPE(Geom_Circle))
      return Circle;
    else if (curve2d->DynamicType() == STANDARD_TYPE(Geom_Line))
      return Line;
    else if (curve2d->DynamicType() == STANDARD_TYPE(Geom_Ellipse))
      return Ellipse;
    else if (curve2d->DynamicType() == STANDARD_TYPE(Geom_Parabola))
      return Parabola;
    else if (curve2d->DynamicType() == STANDARD_TYPE(Geom_Hyperbola))
      return Hyperbola;
    else if (curve2d->DynamicType() == STANDARD_TYPE(Geom_TrimmedCurve))
      return TrimmedCurve;
    else if (curve2d->DynamicType() == STANDARD_TYPE(Geom_OffsetCurve))
      return OffsetCurve;
    else if (curve2d->DynamicType() == STANDARD_TYPE(Geom_BSplineCurve))
      return BSpline;
    else if (curve2d->DynamicType() == STANDARD_TYPE(Geom_BezierCurve))
      return Bezier;
    else if (curve2d->DynamicType() == STANDARD_TYPE(Geom_Conic))
      return Conic;
    return Unknown;
  }
  else{
    if (curve->DynamicType() == STANDARD_TYPE(Geom_Circle))
      return Circle;
    else if (curve->DynamicType() == STANDARD_TYPE(Geom_Line))
      return Line;
    else if (curve->DynamicType() == STANDARD_TYPE(Geom_Parabola))
      return Parabola;
    else if (curve->DynamicType() == STANDARD_TYPE(Geom_Hyperbola))
      return Hyperbola;
    else if (curve->DynamicType() == STANDARD_TYPE(Geom_TrimmedCurve))
      return TrimmedCurve;
    else if (curve->DynamicType() == STANDARD_TYPE(Geom_OffsetCurve))
      return OffsetCurve;
    else if (curve->DynamicType() == STANDARD_TYPE(Geom_Ellipse))
      return Ellipse;
    else if (curve->DynamicType() == STANDARD_TYPE(Geom_BSplineCurve))
      return BSpline;
    else if (curve->DynamicType() == STANDARD_TYPE(Geom_BezierCurve))
      return Bezier;
    else if (curve2d->DynamicType() == STANDARD_TYPE(Geom_Conic))
      return Conic;
    return Unknown;
  }
}

int OCCEdge::minimumMeshSegments() const
{
  int np;
  if(geomType() == Line)
    np = GEdge::minimumMeshSegments();
  else 
    np = CTX.mesh.min_curv_points - 1;
  
  // if the edge is closed, ensure that at least 3 points are
  // generated in the 1D mesh (4 segments, one of which is
  // degenerated)
  if (getBeginVertex() == getEndVertex()) np = std::max(4, np);

  return std::max(np, meshAttributes.minimumMeshSegments);
}

int OCCEdge::minimumDrawSegments() const
{
  if(geomType() == Line)
    return GEdge::minimumDrawSegments();
  else if(geomType() == Circle || geomType() == Ellipse)
    return CTX.geom.circle_points;
  else
    return 20 * GEdge::minimumDrawSegments();
}

double OCCEdge::curvature(double par) const 
{
  const double eps = 1.e-15;

  Standard_Real Crv;
  if (curve.IsNull()){
    Geom2dLProp_CLProps2d aCLProps(curve2d, 2, eps);
    aCLProps.SetParameter(par);
    if(!aCLProps.IsTangentDefined())
      Crv = eps;
    else
      Crv = aCLProps.Curvature();
  }
  else{
    BRepAdaptor_Curve brepc(c);
    BRepLProp_CLProps prop(brepc, 2, eps);
    prop.SetParameter(par); 
    if(!prop.IsTangentDefined())
      Crv = eps;
    else
      Crv = prop.Curvature();
  }
  if(Crv <= eps) Crv = eps;
  
  // std::list<GFace*> ff = faces();
  // std::list<GFace *>::iterator it =  ff.begin();
  // while (it != ff.end()){
  //   SPoint2 par2 = reparamOnFace((*it),par,1);
  //   const double cc = (*it)->curvature ( par2 );
  //   if (cc > 0)
  //     Crv = std::max( Crv, cc);  
  //   ++it;
  // }  
  // printf("curvature = %12.5E\n",Crv); 

  return Crv;
}

#endif
