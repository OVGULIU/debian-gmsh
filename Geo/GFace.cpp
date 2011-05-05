// Gmsh - Copyright (C) 1997-2008 C. Geuzaine, J.-F. Remacle
//
// See the LICENSE.txt file for license information. Please report all
// bugs and problems to <gmsh@geuz.org>.

#include "GModel.h"
#include "GFace.h"
#include "GEdge.h"
#include "MElement.h"

#if defined(HAVE_GMSH_EMBEDDED)
#  include "GmshEmbedded.h"
#else
#  include "Message.h"
#  include "Numeric.h"
#  include "GaussLegendre1D.h"
#  include "VertexArray.h"
#  include "Context.h"
#  if defined(HAVE_GSL)
#    include <gsl/gsl_vector.h>
#    include <gsl/gsl_linalg.h>
#  else
#    define NRANSI
#    include "nrutil.h"
void dsvdcmp(double **a, int m, int n, double w[], double **v);
#  endif
#endif

extern Context_T CTX;

#define SQU(a)      ((a)*(a))

GFace::GFace(GModel *model, int tag)
  : GEntity(model, tag), r1(0), r2(0), va_geom_triangles(0)
{
  meshStatistics.status = GFace::PENDING;
  resetMeshAttributes();
}

GFace::~GFace()
{
  std::list<GEdge*>::iterator it = l_edges.begin();

  while (it != l_edges.end()){
    (*it)->delFace(this);
    ++it;
  }

  for(unsigned int i = 0; i < mesh_vertices.size(); i++)
    delete mesh_vertices[i];

  for(unsigned int i = 0; i < triangles.size(); i++)
    delete triangles[i];

  for(unsigned int i = 0; i < quadrangles.size(); i++)
    delete quadrangles[i];

  if(va_geom_triangles)
    delete va_geom_triangles;
}

unsigned int GFace::getNumMeshElements()
{ 
  return triangles.size() + quadrangles.size(); 
}

MElement *GFace::getMeshElement(unsigned int index)
{ 
  if(index < triangles.size())
    return triangles[index];
  else if(index < triangles.size() + quadrangles.size())
    return quadrangles[index - triangles.size()];
  return 0;
}

void GFace::resetMeshAttributes()
{
  meshAttributes.recombine = 0;
  meshAttributes.recombineAngle = 0.;
  meshAttributes.Method = MESH_UNSTRUCTURED;
  meshAttributes.transfiniteArrangement = 0;
  meshAttributes.transfiniteSmoothing = -1;
  meshAttributes.extrude = 0;
}

SBoundingBox3d GFace::bounds() const
{
  SBoundingBox3d res;
  if(geomType() != DiscreteSurface){
    std::list<GEdge*>::const_iterator it = l_edges.begin();
    for(; it != l_edges.end(); it++)
      res += (*it)->bounds();
  }
  else{
    for(unsigned int i = 0; i < mesh_vertices.size(); i++)
      res += mesh_vertices[i]->point();
  }
  return res;
}

surface_params GFace::getSurfaceParams() const
{
  surface_params p;
  p.radius = p.radius2 = p.height = p.cx = p.cy = p.cz = 0.;
  Msg::Error("Empty surface parameters for this type of surface");
  return p;
}

std::list<GVertex*> GFace::vertices() const
{
  std::list<GEdge*>::const_iterator it = l_edges.begin();
  std::list<GVertex*>ret;
  while (it != l_edges.end()){
    GVertex *v1 = (*it)->getBeginVertex();
    GVertex *v2 = (*it)->getEndVertex();
    if(v1 && std::find(ret.begin(), ret.end(), v1) == ret.end())
      ret.push_back(v1);
    if(v2 && std::find(ret.begin(), ret.end(), v2) == ret.end())
      ret.push_back(v2);
    ++it;
  }
  return ret;
}

void GFace::setVisibility(char val, bool recursive)
{
  GEntity::setVisibility(val);
  if(recursive){
    std::list<GEdge*>::iterator it = l_edges.begin();
    while (it != l_edges.end()){
      (*it)->setVisibility(val, recursive);
      ++it;
    }
  }
}

void GFace::recomputeMeshPartitions()
{
  for(unsigned int i = 0; i < triangles.size(); i++) {
    int part = triangles[i]->getPartition();
    if(part) model()->getMeshPartitions().insert(part);
  }
  for(unsigned int i = 0; i < quadrangles.size(); i++) {
    int part = quadrangles[i]->getPartition();
    if(part) model()->getMeshPartitions().insert(part);
  }
}

void GFace::deleteMeshPartitions()
{
  for(unsigned int i = 0; i < triangles.size(); i++)
    triangles[i]->setPartition(0);
  for(unsigned int i = 0; i < quadrangles.size(); i++)
    quadrangles[i]->setPartition(0);
}

std::string GFace::getAdditionalInfoString()
{
  if(l_edges.empty()) return std::string("");

  std::string str("{");
  if(l_edges.size() > 10){
    char tmp[256];
    sprintf(tmp, "%d, ..., %d", l_edges.front()->tag(), l_edges.back()->tag());
    str += tmp;
  }
  else{
    std::list<GEdge*>::const_iterator it = l_edges.begin();
    for(; it != l_edges.end(); it++){
      if(it != l_edges.begin()) str += ",";
      char tmp[256];
      sprintf(tmp, "%d", (*it)->tag());
      str += tmp;
    }
  }
  str += "}";
  return str;
}

void GFace::computeMeanPlane()
{
  std::vector<SPoint3> pts;
  std::list<GVertex*> verts = vertices();
  std::list<GVertex*>::const_iterator itv = verts.begin();
  for(; itv != verts.end(); itv++){
    const GVertex *v = *itv;
    pts.push_back(SPoint3(v->x(), v->y(), v->z()));
  }

  if(pts.size() < 3){
    Msg::Info("Adding edge points to compute mean plane of face %d", tag());
    std::list<GEdge*> edg = edges();
    std::list<GEdge*>::const_iterator ite = edg.begin();
    for(; ite != edg.end(); ite++){
      const GEdge *e = *ite;
      Range<double> b = e->parBounds(0);
      GPoint p1 = e->point(b.low() + 0.333 * (b.high() - b.low()));
      pts.push_back(SPoint3(p1.x(), p1.y(), p1.z()));
      GPoint p2 = e->point(b.low() + 0.666 * (b.high() - b.low()));
      pts.push_back(SPoint3(p2.x(), p2.y(), p2.z()));
    }
  }

  computeMeanPlane(pts);
}

void GFace::computeMeanPlane(const std::vector<MVertex*> &points)
{
  std::vector<SPoint3> pts;
  for(unsigned int i = 0; i < points.size(); i++)
    pts.push_back(SPoint3(points[i]->x(), points[i]->y(), points[i]->z()));
  computeMeanPlane(pts);
}

void GFace::computeMeanPlane(const std::vector<SPoint3> &points)
{
#if !defined(HAVE_GMSH_EMBEDDED)
  // The concept of a mean plane computed in the sense of least
  // squares is fine for plane surfaces(!), but not really the best
  // one for non-plane surfaces. Indeed, imagine a quarter of a circle
  // extruded along a very small height: the mean plane will be in the
  // plane of the circle... Until we implement something better, there
  // is a test in this routine that checks the coherence of the
  // computation for non-plane surfaces and reverts to a basic mean
  // plane approximatation if the check fails.

  double xm = 0., ym = 0., zm = 0.;
  int ndata = points.size();
  int na = 3;
  for(int i = 0; i < ndata; i++) {
    xm += points[i].x();
    ym += points[i].y();
    zm += points[i].z();
  }
  xm /= (double)ndata;
  ym /= (double)ndata;
  zm /= (double)ndata;

  int min;
  double res[4], svd[3];
#if defined(HAVE_GSL)
  gsl_matrix *U = gsl_matrix_alloc(ndata, na);
  gsl_matrix *V = gsl_matrix_alloc(na, na);
  gsl_vector *W = gsl_vector_alloc(na);
  gsl_vector *TMPVEC = gsl_vector_alloc(na);
  for(int i = 0; i < ndata; i++) {
    gsl_matrix_set(U, i, 0, points[i].x() - xm);
    gsl_matrix_set(U, i, 1, points[i].y() - ym);
    gsl_matrix_set(U, i, 2, points[i].z() - zm);
  }
  gsl_linalg_SV_decomp(U, V, W, TMPVEC);
  svd[0] = gsl_vector_get(W, 0);
  svd[1] = gsl_vector_get(W, 1);
  svd[2] = gsl_vector_get(W, 2);
  if(fabs(svd[0]) < fabs(svd[1]) && fabs(svd[0]) < fabs(svd[2]))
    min = 0;
  else if(fabs(svd[1]) < fabs(svd[0]) && fabs(svd[1]) < fabs(svd[2]))
    min = 1;
  else
    min = 2;
  res[0] = gsl_matrix_get(V, 0, min);
  res[1] = gsl_matrix_get(V, 1, min);
  res[2] = gsl_matrix_get(V, 2, min);
  norme(res);
  gsl_matrix_free(U);
  gsl_matrix_free(V);
  gsl_vector_free(W);
  gsl_vector_free(TMPVEC);
#else
  double **U = dmatrix(1, ndata, 1, na);
  double **V = dmatrix(1, na, 1, na);
  double *W = dvector(1, na);
  for(int i = 0; i < ndata; i++) {
    U[i + 1][1] = points[i].x() - xm;
    U[i + 1][2] = points[i].y() - ym;
    U[i + 1][3] = points[i].z() - zm;
  }
  dsvdcmp(U, ndata, na, W, V);
  if(fabs(W[1]) < fabs(W[2]) && fabs(W[1]) < fabs(W[3]))
    min = 1;
  else if(fabs(W[2]) < fabs(W[1]) && fabs(W[2]) < fabs(W[3]))
    min = 2;
  else
    min = 3;
  svd[0] = W[1];
  svd[1] = W[2];
  svd[2] = W[3];
  res[0] = V[1][min];
  res[1] = V[2][min];
  res[2] = V[3][min];
  norme(res);
  free_dmatrix(U, 1, ndata, 1, na);
  free_dmatrix(V, 1, na, 1, na);
  free_dvector(W, 1, na);
#endif

  double ex[3], t1[3], t2[3];

  // check coherence of results for non-plane surfaces
  if(geomType() != Plane && geomType() != DiscreteSurface) {
    double res2[3], c[3], cosc, sinc, angplan;
    double eps = 1.e-3;

    GPoint v1 = point(0.5, 0.5);
    GPoint v2 = point(0.5 + eps, 0.5);
    GPoint v3 = point(0.5, 0.5 + eps);
    t1[0] = v2.x() - v1.x();
    t1[1] = v2.y() - v1.y();
    t1[2] = v2.z() - v1.z();
    t2[0] = v3.x() - v1.x();
    t2[1] = v3.y() - v1.y();
    t2[2] = v3.z() - v1.z();
    norme(t1);
    norme(t2);
    // prodve(t1, t2, res2);
    // Warning: the rest of the code assumes res = t2 x t1, not t1 x t2 (WTF?)
    prodve(t2, t1, res2);
    norme(res2);
    prodve(res, res2, c);
    prosca(res, res2, &cosc);
    sinc = sqrt(c[0] * c[0] + c[1] * c[1] + c[2] * c[2]);
    angplan = myatan2(sinc, cosc);
    angplan = angle_02pi(angplan) * 180. / M_PI;
    if((angplan > 70 && angplan < 110) || (angplan > 260 && angplan < 280)) {
      Msg::Info("SVD failed (angle=%g): using rough algo...", angplan);
      res[0] = res2[0];
      res[1] = res2[1];
      res[2] = res2[2];
      goto end;
    }
  }

  ex[0] = ex[1] = ex[2] = 0.0;
  if(res[0] == 0.)
    ex[0] = 1.0;
  else if(res[1] == 0.)
    ex[1] = 1.0;
  else
    ex[2] = 1.0;

  prodve(res, ex, t1);
  norme(t1);
  prodve(t1, res, t2);
  norme(t2);

end:
  res[3] = (xm * res[0] + ym * res[1] + zm * res[2]);

  for(int i = 0; i < 3; i++)
    meanPlane.plan[0][i] = t1[i];
  for(int i = 0; i < 3; i++)
    meanPlane.plan[1][i] = t2[i];
  for(int i = 0; i < 3; i++)
    meanPlane.plan[2][i] = res[i];

  meanPlane.a = res[0];
  meanPlane.b = res[1];
  meanPlane.c = res[2];
  meanPlane.d = res[3];

  meanPlane.x = meanPlane.y = meanPlane.z = 0.;
  if(fabs(meanPlane.a) >= fabs(meanPlane.b) &&
     fabs(meanPlane.a) >= fabs(meanPlane.c) ){
    meanPlane.x = meanPlane.d / meanPlane.a;
  }
  else if(fabs(meanPlane.b) >= fabs(meanPlane.a) &&
          fabs(meanPlane.b) >= fabs(meanPlane.c)){
    meanPlane.y = meanPlane.d / meanPlane.b;
  }
  else{
    meanPlane.z = meanPlane.d / meanPlane.c;
  }

  Msg::Debug("Surface: %d", tag());
  Msg::Debug("SVD    : %g,%g,%g (min=%d)", svd[0], svd[1], svd[2], min);
  Msg::Debug("Plane  : (%g x + %g y + %g z = %g)",
	     meanPlane.a, meanPlane.b, meanPlane.c, meanPlane.d);
  Msg::Debug("Normal : (%g , %g , %g )",
	     meanPlane.a, meanPlane.b, meanPlane.c);
  Msg::Debug("t1     : (%g , %g , %g )", t1[0], t1[1], t1[2]);
  Msg::Debug("t2     : (%g , %g , %g )", t2[0], t2[1], t2[2]);
  Msg::Debug("pt     : (%g , %g , %g )",
	     meanPlane.x, meanPlane.y, meanPlane.z);

  //check coherence for plane surfaces
  if(geomType() == Plane) {
    SBoundingBox3d bb = bounds();
    double lc = norm(SVector3(bb.max(), bb.min()));
    std::list<GVertex*> verts = vertices();
    std::list<GVertex*>::const_iterator itv = verts.begin();
    for(; itv != verts.end(); itv++){
      const GVertex *v = *itv;
      double d = meanPlane.a * v->x() + meanPlane.b * v->y() +
        meanPlane.c * v->z() - meanPlane.d;
      if(fabs(d) > lc * 1.e-3) {
        Msg::Error("Plane surface %d (%gx+%gy+%gz+%g=0) is not plane!",
            tag(), meanPlane.a, meanPlane.b, meanPlane.c, meanPlane.d);
        Msg::Error("Control point %d = (%g,%g,%g), val=%g",
            v->tag(), v->x(), v->y(), v->z(), d);
        return;
      }
    }
  }
#endif
}

void GFace::getMeanPlaneData(double VX[3], double VY[3],
                             double &x, double &y, double &z) const
{
  VX[0] = meanPlane.plan[0][0];
  VX[1] = meanPlane.plan[0][1];
  VX[2] = meanPlane.plan[0][2];
  VY[0] = meanPlane.plan[1][0];
  VY[1] = meanPlane.plan[1][1];
  VY[2] = meanPlane.plan[1][2];
  x = meanPlane.x;
  y = meanPlane.y;
  z = meanPlane.z;
}

void GFace::getMeanPlaneData(double plan[3][3]) const
{
  for(int i = 0; i < 3; i++)
    for(int j = 0; j < 3; j++)
      plan[i][j] = meanPlane.plan[i][j];
}

double GFace::curvature(const SPoint2 &param) const
{
  if (geomType() == Plane) return 0;

  // X=X(u,v) Y=Y(u,v) Z=Z(u,v)
  // curv = div n = dnx/dx + dny/dy + dnz/dz
  // dnx/dx = dnx/du du/dx + dnx/dv dv/dx

  const double eps = 1.e-3;

  Pair<SVector3,SVector3> der = firstDer(param);

  SVector3 du = der.first();
  SVector3 dv = der.second();
  SVector3 nml = crossprod(du, dv);

  double detJ = norm(nml);

  du.normalize();
  dv.normalize();

  SVector3 n1 = normal(SPoint2(param.x() - eps, param.y()));
  SVector3 n2 = normal(SPoint2(param.x() + eps, param.y()));
  SVector3 n3 = normal(SPoint2(param.x(), param.y() - eps));
  SVector3 n4 = normal(SPoint2(param.x(), param.y() + eps));

  SVector3 dndu = 500 * (n2 - n1);
  SVector3 dndv = 500 * (n4 - n3);

  double c = fabs(dot(dndu, du) +  dot(dndv, dv)) / detJ;

  // Msg::Info("c = %g detJ %g", c, detJ);

  return c;
}

double GFace::getMetricEigenvalue(const SPoint2 &)
{
  Msg::Error("Metric eigenvalue is not implemented for this type of surface");
  return 0.;
}

void GFace::XYZtoUV(const double X, const double Y, const double Z,
                    double &U, double &V, const double relax,
                    const bool onSurface) const
{
#if !defined(HAVE_GMSH_EMBEDDED)
  const double Precision = 1.e-8;
  const int MaxIter = 25;
  const int NumInitGuess = 11;

  double Unew = 0., Vnew = 0., err, err2;
  int iter;
  double mat[3][3], jac[3][3];
  double umin, umax, vmin, vmax;
  double initu[NumInitGuess] = {0.487, 0.6, 0.4, 0.7, 0.3, 0.8, 0.2, 1., 0., 0., 1.};
  double initv[NumInitGuess] = {0.487, 0.6, 0.4, 0.7, 0.3, 0.8, 0.2, 0., 1., 0., 1.};

  Range<double> ru = parBounds(0);
  Range<double> rv = parBounds(1);
  umin = ru.low();
  umax = ru.high();
  vmin = rv.low();
  vmax = rv.high();

  for(int i = 0; i < NumInitGuess; i++){
    for(int j = 0; j < NumInitGuess; j++){

      U = initu[i];
      V = initv[j];
      err = 1.0;
      iter = 1;

      GPoint P = point(U, V);
      err2 = sqrt(SQU(X - P.x()) + SQU(Y - P.y()) + SQU(Z - P.z()));
      if (err2 < 1.e-8 * CTX.lc) return;

      while(err > Precision && iter < MaxIter) {
        P = point(U, V);
        Pair<SVector3, SVector3> der = firstDer(SPoint2(U, V));
        mat[0][0] = der.left().x();
        mat[0][1] = der.left().y();
        mat[0][2] = der.left().z();
        mat[1][0] = der.right().x();
        mat[1][1] = der.right().y();
        mat[1][2] = der.right().z();
        mat[2][0] = 0.;
        mat[2][1] = 0.;
        mat[2][2] = 0.;
        invert_singular_matrix3x3(mat, jac);

        Unew = U + relax *
          (jac[0][0] * (X - P.x()) + jac[1][0] * (Y - P.y()) +
           jac[2][0] * (Z - P.z()));
        Vnew = V + relax *
          (jac[0][1] * (X - P.x()) + jac[1][1] * (Y - P.y()) +
           jac[2][1] * (Z - P.z()));

        //if(Unew > umax || Vnew > vmax || Unew < umin || Vnew < vmin) break;

        err = SQU(Unew - U) + SQU(Vnew - V);
        err2 = sqrt(SQU(X - P.x()) + SQU(Y - P.y()) + SQU(Z - P.z()));
        iter++;
        U = Unew;
        V = Vnew;
      }

      //printf("i=%d j=%d err=%g iter=%d err2=%g u=%.16g v=%.16g x=%g y=%g z=%g\n", 
      //     i, j, err, iter, err2, U, V, X, Y, Z);

      if(iter < MaxIter && err <= Precision &&
         Unew <= umax && Vnew <= vmax &&
         Unew >= umin && Vnew >= vmin){
        if (onSurface && err2 > 1.e-4 * CTX.lc)
          Msg::Warning("Converged for i=%d j=%d (err=%g iter=%d) BUT xyz error = %g",
              i, j, err, iter, err2);
        return;
      }
    }
  }

  if(relax < 1.e-6)
    Msg::Error("Could not converge: surface mesh will be wrong");
  else {
    Msg::Info("point %g %g %g : Relaxation factor = %g", X, Y, Z, 0.75 * relax);
    XYZtoUV(X, Y, Z, U, V, 0.75 * relax);
  }
#endif
}

SPoint2 GFace::parFromPoint(const SPoint3 &p) const
{
  double U = 0., V = 0.;
  XYZtoUV(p.x(), p.y(), p.z(), U, V, 1.0);
  return SPoint2(U, V);
}

GPoint GFace::closestPoint(const SPoint3 & queryPoint, const double initialGuess[2]) const
{
  Msg::Error("Closet point not implemented for this type of surface");
  return GPoint(0, 0, 0);
}

bool GFace::containsParam(const SPoint2 &pt) const
{
  Range<double> uu = parBounds(0);
  Range<double> vv = parBounds(1);
  if((pt.x() >= uu.low() && pt.x() <= uu.high()) &&
     (pt.y() >= vv.low() && pt.y() <= vv.high()))
    return true;
  else
    return false;
}

bool GFace::buildRepresentationCross()
{
  if(geomType() != Plane){
    // don't try again
    cross.clear();
    cross.push_back(SPoint3(0., 0., 0.));
    return false;
  }

  std::list<GEdge*> ed = edges();
  SBoundingBox3d bb;
  for(std::list<GEdge*>::iterator it = ed.begin(); it != ed.end(); it++){
    GEdge *ge = *it;
    if(ge->geomType() == GEntity::DiscreteCurve ||
       ge->geomType() == GEntity::BoundaryLayerCurve){
      // don't try again
      cross.clear();
      cross.push_back(SPoint3(0., 0., 0.));
      return false;
    }
    else{
      Range<double> t_bounds = ge->parBounds(0);
      GPoint p[3] = {ge->point(t_bounds.low()),
                     ge->point(0.5 * (t_bounds.low() + t_bounds.high())),
                     ge->point(t_bounds.high())};
      for(int i = 0; i < 3; i++){
        SPoint2 uv = parFromPoint(SPoint3(p[i].x(), p[i].y(), p[i].z()));
        bb += SPoint3(uv.x(), uv.y(), 0.);
      }
    }
  }
  bb *= 1.1;
  GPoint v0 = point(bb.min().x(), bb.min().y());
  GPoint v1 = point(bb.max().x(), bb.min().y());
  GPoint v2 = point(bb.max().x(), bb.max().y());
  GPoint v3 = point(bb.min().x(), bb.max().y());
  const int N = 100;
  for(int dir = 0; dir < 2; dir++) {
    int end_line = 0;
    SPoint3 pt, pt_last_inside;
    for(int i = 0; i < N; i++) {
      double t = (double)i / (double)(N - 1);
      double x, y, z;
      if(!dir){
        x = 0.5 * (t * (v0.x() + v1.x()) + (1. - t) * (v2.x() + v3.x()));
        y = 0.5 * (t * (v0.y() + v1.y()) + (1. - t) * (v2.y() + v3.y()));
        z = 0.5 * (t * (v0.z() + v1.z()) + (1. - t) * (v2.z() + v3.z()));
      }
      else{
        x = 0.5 * (t * (v0.x() + v3.x()) + (1. - t) * (v2.x() + v1.x()));
        y = 0.5 * (t * (v0.y() + v3.y()) + (1. - t) * (v2.y() + v1.y()));
        z = 0.5 * (t * (v0.z() + v3.z()) + (1. - t) * (v2.z() + v1.z()));
      }
      pt.setPosition(x, y, z);
      if(containsPoint(pt)){
        pt_last_inside.setPosition(x, y, z);
        if(!end_line) { cross.push_back(pt); end_line = 1; }
      }
      else {
        if(end_line) { cross.push_back(pt_last_inside); end_line = 0; }
      }
    }
    if(end_line) cross.push_back(pt_last_inside);
  }
  // if we couldn't determine a cross, add a dummy point so that
  // we won't try again
  if(!cross.size()){
    cross.push_back(SPoint3(0., 0., 0.));
    return false;
  }
  return true;
}

struct graphics_point{
  double xyz[3];
  SVector3 n;
};

bool GFace::buildSTLTriangulation()
{
#if defined(HAVE_GMSH_EMBEDDED)
  return false;
#else
  // Build a simple triangulation for surfaces which we know are not
  // trimmed
  if(geomType() != ParametricSurface && geomType() != ProjectionFace)
    return false;

  const int nu = 64, nv = 64;
  graphics_point p[nu][nv];

  if(va_geom_triangles) delete va_geom_triangles;
  va_geom_triangles = new VertexArray(3, 2 * (nu - 1) * (nv - 1));

  Range<double> ubounds = parBounds(0);
  Range<double> vbounds = parBounds(1);
  double umin = ubounds.low(), umax = ubounds.high();
  double vmin = vbounds.low(), vmax = vbounds.high();

  for(int i = 0; i < nu; i++){
    for(int j = 0; j < nv; j++){
      double u = umin + (double)i / (double)(nu - 1) * (umax - umin);
      double v = vmin + (double)j / (double)(nv - 1) * (vmax - vmin);
      GPoint gp = point(u, v);
      p[i][j].xyz[0] = gp.x();
      p[i][j].xyz[1] = gp.y();
      p[i][j].xyz[2] = gp.z();
      p[i][j].n = normal(SPoint2(u, v));
    }
  }

  // i,j+1 *---* i+1,j+1
  //       | / |
  //   i,j *---* i+1,j
  unsigned int c = CTX.color.geom.surface;
  unsigned int col[4] = {c, c, c, c};
  for(int i = 0; i < nu - 1; i++){
    for(int j = 0; j < nv - 1; j++){
      double x1[3] = {p[i][j].xyz[0], p[i + 1][j].xyz[0], p[i + 1][j + 1].xyz[0]};
      double y1[3] = {p[i][j].xyz[1], p[i + 1][j].xyz[1], p[i + 1][j + 1].xyz[1]};
      double z1[3] = {p[i][j].xyz[2], p[i + 1][j].xyz[2], p[i + 1][j + 1].xyz[2]};
      SVector3 n1[3] = {p[i][j].n, p[i + 1][j].n, p[i + 1][j + 1].n};
      va_geom_triangles->add(x1, y1, z1, n1, col);
      double x2[3] = {p[i][j].xyz[0], p[i + 1][j + 1].xyz[0], p[i][j + 1].xyz[0]};
      double y2[3] = {p[i][j].xyz[1], p[i + 1][j + 1].xyz[1], p[i][j + 1].xyz[1]};
      double z2[3] = {p[i][j].xyz[2], p[i + 1][j + 1].xyz[2], p[i][j + 1].xyz[2]};
      SVector3 n2[3] = {p[i][j].n, p[i + 1][j + 1].n, p[i][j + 1].n};
      va_geom_triangles->add(x2, y2, z2, n2, col);
    }
  }
  va_geom_triangles->finalize();
  return true;
#endif
}

// by default we assume that straight lines are geodesics
SPoint2 GFace::geodesic(const SPoint2 &pt1 , const SPoint2 &pt2 , double t)
{
  if(CTX.mesh.second_order_experimental && geomType() != GEntity::Plane){
    // FIXME: this is buggy -- remove the CTX option once we do it in
    // a robust manner
    GPoint gp1 = point(pt1.x(), pt1.y());
    GPoint gp2 = point(pt2.x(), pt2.y());
    SPoint2 guess = pt1 + (pt2 - pt1) * t;
    GPoint gp = closestPoint(SPoint3(gp1.x() + t * (gp2.x() - gp1.x()),
				     gp1.y() + t * (gp2.y() - gp1.y()),
				     gp1.z() + t * (gp2.z() - gp1.z())),
			     (double*)guess);
    return SPoint2(gp.u(), gp.v());
  }
  else{
    return pt1 + (pt2 - pt1) * t;
  }
}

// length of a curve drawn on a surface
// S = (X(u,v), Y(u,v), Z(u,v) );
// u = u(t) , v = v(t)
// C = C ( u(t), v(t) )
// dC/dt = dC/du du/dt + dC/dv dv/dt
double GFace::length(const SPoint2 &pt1, const SPoint2 &pt2, int nbQuadPoints)
{
#if defined(HAVE_GMSH_EMBEDDED)
  return -1.;
#else
  double *t = 0, *w = 0;
  double L = 0.0;
  gmshGaussLegendre1D(nbQuadPoints, &t, &w);
  for (int i = 0; i < nbQuadPoints; i++){
    const double ti = 0.5 * (1. + t[i]);
    SPoint2 pi = geodesic(pt1, pt2, ti);
    Pair<SVector3, SVector3> der2 = firstDer(pi);
    SVector3 der = der2.left() * (pt2.x() - pt1.x()) + der2.right() * (pt2.y() - pt1.y());
    const double d = norm(der);
    L += d * w[i] ;
  }
  return L;
#endif
}
