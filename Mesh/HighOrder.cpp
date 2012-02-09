// Gmsh - Copyright (C) 1997-2012 C. Geuzaine, J.-F. Remacle
//
// See the LICENSE.txt file for license information. Please report all
// bugs and problems to <gmsh@geuz.org>.
//
// Contributor(s):
//   Koen Hillewaert
//

#include "HighOrder.h"
#include "highOrderSmoother.h"
#include "highOrderTools.h"
#include "MLine.h"
#include "MTriangle.h"
#include "MQuadrangle.h"
#include "MTetrahedron.h"
#include "MHexahedron.h"
#include "MPrism.h"
#include "MPyramid.h"
#include "GmshMessage.h"
#include "OS.h"
#include "Numeric.h"
#include "Context.h"
#include "fullMatrix.h"
#include "polynomialBasis.h"

#define SQU(a)      ((a)*(a))

// The aim here is to build a polynomial representation that consist
// in polynomial segments of equal length

static double mylength(GEdge *ge, int i, double *u)
{
  return ge->length(u[i], u[i+1], 10);
}

static void myresid(int N, GEdge *ge, double *u, fullVector<double> &r)
{
  double L[100];
  for (int i = 0; i < N - 1; i++) L[i] = mylength(ge, i, u);
  for (int i = 0; i < N - 2; i++) r(i) = L[i + 1] - L[i];
}

static bool computeEquidistantParameters1(GEdge *ge, double u0, double uN, int N, 
                                         double *u, double underRelax)
{
  GPoint p0 = ge->point(u0);
  GPoint p1 = ge->point(uN);
  double du = 1. / (N - 1);
  u[0] = u0;
  //  printf("starting with %g %g %g\n",p0.x(),p0.y(),u0);
  //  printf("ending   with %g %g %g\n",p1.x(),p1.y(),uN);
  for (int i = 1; i < N; i++){
    SPoint3 pi (p0.x() + i * du * (p1.x()-p0.x()),
		p0.y() + i * du * (p1.y()-p0.y()),
		p0.z() + i * du * (p1.z()-p0.z()));
    double t;
    GPoint gp = ge->closestPoint(pi, t);		
    u[i] = gp.u();
    //    printf("going to %g %g u %g\n",pi.x(),pi.y(),gp.u());
  }  
  return true;
}

static bool computeEquidistantParameters0(GEdge *ge, double u0, double uN, int N, 
                                         double *u, double underRelax)
{
  const double PRECISION = 1.e-6;
  const int MAX_ITER = 50;
  const double eps = (uN - u0) * 1.e-5;

  // newton algorithm
  // N is the total number of points (3 for quadratic, 4 for cubic ...)
  // u[0] = u0;
  // u[N-1] = uN;
  // initialize as equidistant in parameter space
  u[0] = u0;
  double du = (uN - u0) / (N - 1);
  for (int i = 1; i < N; i++){
    u[i] = u[i - 1] + du;
  }

  // create the tangent matrix
  const int M = N - 2;
  fullMatrix<double> J(M, M);
  fullVector<double> DU(M);
  fullVector<double> R(M);
  fullVector<double> Rp(M);
  
  int iter = 1;

  while (iter < MAX_ITER){
    iter++;
    myresid(N, ge, u, R);

    for (int i = 0; i < M; i++){
      u[i + 1] += eps;
      myresid(N, ge, u, Rp);
      for (int j = 0; j < M; j++){
        J(i, j) = (Rp(j) - R(j)) / eps;
      }
      u[i+1] -= eps;
    }

    if (M == 1)
      DU(0) = R(0) / J(0, 0);
    else
      J.luSolve(R, DU);
    
    for (int i = 0; i < M; i++){
      u[i+1] -= underRelax*DU(i);
    }

    if (u[1] < u0) break;
    if (u[N - 2] > uN) break;

    double newt_norm = DU.norm();      
    if (newt_norm < PRECISION) {
      return true; 
    }
  }
  return false;
}
// 1 = geodesics
static int method_for_computing_intermediary_points = 1;
static bool computeEquidistantParameters(GEdge *ge, double u0, double uN, int N, 
                                         double *u, double underRelax){
  if (method_for_computing_intermediary_points == 0) // use linear abscissa
    return computeEquidistantParameters0(ge,u0,uN,N,u,underRelax);
  else if (method_for_computing_intermediary_points == 1) // use projection
    return computeEquidistantParameters1(ge,u0,uN,N,u,underRelax);  
}

static double mylength(GFace *gf, int i, double *u, double *v)
{
  return gf->length(SPoint2(u[i], v[i]), SPoint2(u[i + 1], v[i + 1]), 10);
}

static void myresid(int N, GFace *gf, double *u, double *v, fullVector<double> &r)
{
  double L[100];
  for (int i = 0; i < N - 1; i++) L[i] = mylength(gf, i, u, v);  
  for (int i = 0; i < N - 2; i++) r(i) = L[i + 1] - L[i];
}

static bool computeEquidistantParameters1(GFace *gf, double u0, double uN, 
					  double v0, double vN, int N,
					  double *u, double *v){
  //  printf("coucou\n");
  GPoint p0 = gf->point(u0,v0);
  GPoint p1 = gf->point(uN,vN);
  double du = 1. / (N - 1);
  u[0] = u0;
  u[0] = u0;
  v[0] = v0;
  //  printf("starting with %g %g %g\n",p0.x(),p0.y(),u0);
  //  printf("ending   with %g %g %g\n",p1.x(),p1.y(),uN);
  for (int i = 1; i < N; i++){
    SPoint3 pi (p0.x() + i * du * (p1.x()-p0.x()),
		p0.y() + i * du * (p1.y()-p0.y()),
		p0.z() + i * du * (p1.z()-p0.z()));
    SPoint2 t;
    GPoint gp = gf->closestPoint(pi, t);		
    u[i] = gp.u();
    v[i] = gp.v();
  }  
  return true;
}

static bool computeEquidistantParameters0(GFace *gf, double u0, double uN, 
                                         double v0, double vN, int N,
                                         double *u, double *v)
{
  const double PRECISION = 1.e-6;
  const int MAX_ITER = 50;
  const double eps = 1.e-4;

  double t[100];
  // initialize the points by equal subdivision of geodesics
  u[0] = u0;
  v[0] = v0;
  t[0] = 0;
  for (int i = 1; i < N; i++){
    t[i] = (double)i / (N - 1);
    SPoint2 p = gf->geodesic(SPoint2(u0, v0), SPoint2(uN, vN), t[i]);
    u[i] = p.x();
    v[i] = p.y();
  }
  u[N] = uN;
  v[N] = vN;
  t[N] = 1.0;

  return true;

  // create the tangent matrix
  const int M = N - 2;
  fullMatrix<double> J(M, M);
  fullVector<double> DU(M);
  fullVector<double> R(M);
  fullVector<double> Rp(M);
  
  int iter = 1;

  while (iter < MAX_ITER){
    iter++;
    myresid(N, gf, u, v, R); 

    for (int i = 0; i < M; i++){
      t[i + 1] += eps;
      double tempu = u[i + 1];
      double tempv = v[i + 1];
      SPoint2 p = gf->geodesic(SPoint2(u0, v0), SPoint2(uN, vN), t[i + 1]);
      u[i + 1] = p.x();
      v[i + 1] = p.y();
      myresid(N, gf, u, v, Rp);
      for (int j = 0; j < M; j++){
        J(i, j) = (Rp(j) - R(j)) / eps;
      }
      t[i + 1] -= eps;
      u[i + 1] = tempu;
      v[i + 1] = tempv;
    }

    if (M == 1)
      DU(0) = R(0) / J(0, 0);
    else
      J.luSolve(R, DU);
    
    for (int i = 0; i < M; i++){
      t[i + 1] -= DU(i);
      SPoint2 p = gf->geodesic(SPoint2(u0, v0), SPoint2(uN, vN), t[i + 1]);
      u[i + 1] = p.x();
      v[i + 1] = p.y();
    }
    double newt_norm = DU.norm();      
    if (newt_norm < PRECISION) return true;
  }
  // FAILED, use equidistant in param space
  for (int i = 1; i < N; i++){
    t[i] = (double)i / (N - 1);
    SPoint2 p = gf->geodesic(SPoint2(u0, v0), SPoint2(uN, vN), t[i]);
    u[i] = p.x();
    v[i] = p.y();
  }
  return false;
}

static bool computeEquidistantParameters(GFace *gf, double u0, double uN, 
                                         double v0, double vN, int N,
                                         double *u, double *v){
  if (method_for_computing_intermediary_points == 0) // use linear abscissa
    return computeEquidistantParameters0(gf,u0,uN,v0,vN,N,u,v);
  else if (method_for_computing_intermediary_points == 1) // use projection
    return computeEquidistantParameters1(gf,u0,uN,v0,vN,N,u,v);
}


static void getEdgeVertices(GEdge *ge, MElement *ele, std::vector<MVertex*> &ve,
                            edgeContainer &edgeVertices, bool linear,
                            int nPts = 1, highOrderSmoother *displ2D = 0,
                            highOrderSmoother *displ3D = 0)
{
  if(ge->geomType() == GEntity::DiscreteCurve ||
     ge->geomType() == GEntity::BoundaryLayerCurve)
    linear = true;

  for(int i = 0; i < ele->getNumEdges(); i++){
    MEdge edge = ele->getEdge(i);
    std::pair<MVertex*, MVertex*> p(edge.getMinVertex(), edge.getMaxVertex());
    if(edgeVertices.count(p)){
      if(edge.getVertex(0) == edge.getMinVertex())
        ve.insert(ve.end(), edgeVertices[p].begin(), edgeVertices[p].end());
      else
        ve.insert(ve.end(), edgeVertices[p].rbegin(), edgeVertices[p].rend());
    }
    else{  
      MVertex *v0 = edge.getVertex(0), *v1 = edge.getVertex(1);
      std::vector<MVertex*> temp;
      double u0 = 0., u1 = 0., US[100];
      bool reparamOK = true;
      if(!linear) {
        reparamOK &= reparamMeshVertexOnEdge(v0, ge, u0);
        if(ge->periodic(0) && v1 == ge->getEndVertex()->mesh_vertices[0])
          u1 = ge->parBounds(0).high();
        else
          reparamOK &= reparamMeshVertexOnEdge(v1, ge, u1);
        if(reparamOK){
          double relax = 1.;
          while (1){
            if(computeEquidistantParameters(ge, std::min(u0,u1), std::max(u0,u1), 
                                            nPts + 2, US, relax)) 
              break;
            relax /= 2.0;
            if(relax < 1.e-2) 
              break;
          } 
          if(relax < 1.e-2)
            Msg::Warning
              ("Failed to compute equidistant parameters (relax = %g, value = %g) "
               "for edge %d-%d parametrized with %g %g on GEdge %d linear %d",
               relax, US[1], v0->getNum(), v1->getNum(),u0,u1,ge->tag(), linear);
        }
	else{
	  Msg::Error("Cannot reparam a mesh Vertex in high order meshing");
	}
      }
      for(int j = 0; j < nPts; j++){
        const double t = (double)(j + 1)/(nPts + 1);
        double uc = (1. - t) * u0 + t * u1; // can be wrong, that's ok
        MVertex *v;
        if(linear || !reparamOK || uc < std::min(u0,u1) || uc > std::max(u0,u1)){ 
	  if (!linear)
	    Msg::Warning("We don't have a valid parameter on curve %d-%d",
			 v0->getNum(), v1->getNum());
          // we don't have a (valid) parameter on the curve
          SPoint3 pc = edge.interpolate(t);
          v = new MVertex(pc.x(), pc.y(), pc.z(), ge);	  
        }
        else {          
	  int count = u0<u1? j + 1 : nPts + 1  - (j + 1);
	  GPoint pc = ge->point(US[count]);
          v = new MEdgeVertex(pc.x(), pc.y(), pc.z(), ge,pc.u()); 
	  //	  printf("Edge %d(%g) %d(%g) new vertex %g %g at %g\n",v0->getNum(),u0,v1->getNum(),u1,v->x(),v->y(), US[count]);
          if (displ2D || displ3D){
            SPoint3 pc2 = edge.interpolate(t);          
            if(displ2D) displ2D->add(v, SVector3(pc2.x(), pc2.y(), pc2.z()));
            if(displ3D) displ3D->add(v, SVector3(pc2.x(), pc2.y(), pc2.z()));
          }
        }
        temp.push_back(v);
        // this destroys the ordering of the mesh vertices on the edge
        ge->mesh_vertices.push_back(v);
        ve.push_back(v);
      }
    
      if(edge.getVertex(0) == edge.getMinVertex())
        edgeVertices[p].insert(edgeVertices[p].end(), temp.begin(), temp.end());
      else
        edgeVertices[p].insert(edgeVertices[p].end(), temp.rbegin(), temp.rend());
    }
  }
}

static void getEdgeVertices(GFace *gf, MElement *ele, std::vector<MVertex*> &ve,
                            edgeContainer &edgeVertices, bool linear,
                            int nPts = 1, 
                            highOrderSmoother *displ2D = 0,
                            highOrderSmoother *displ3D = 0)
{
  if(gf->geomType() == GEntity::DiscreteSurface ||
     gf->geomType() == GEntity::BoundaryLayerSurface)
    linear = true;

  for(int i = 0; i < ele->getNumEdges(); i++){
    MEdge edge = ele->getEdge(i);    
    std::pair<MVertex*, MVertex*> p(edge.getMinVertex(), edge.getMaxVertex());
    if(edgeVertices.count(p)){
      if(edge.getVertex(0) == edge.getMinVertex())
        ve.insert(ve.end(), edgeVertices[p].begin(), edgeVertices[p].end());
      else
        ve.insert(ve.end(), edgeVertices[p].rbegin(), edgeVertices[p].rend());
    }
    else{
      MVertex *v0 = edge.getVertex(0), *v1 = edge.getVertex(1);
      SPoint2 p0, p1;
      double US[100], VS[100];
      bool reparamOK = true;
      if(!linear){
        reparamOK = reparamMeshEdgeOnFace(v0, v1, gf, p0, p1);
        if(reparamOK)
          computeEquidistantParameters(gf, p0[0], p1[0], p0[1], p1[1], nPts + 2,
                                       US, VS);
      }
      std::vector<MVertex*> temp;
      for(int j = 0; j < nPts; j++){
        const double t = (double)(j + 1) / (nPts + 1);
        MVertex *v;
        if(linear || !reparamOK){
          // we don't have (valid) parameters on the surface
          SPoint3 pc = edge.interpolate(t);
          v = new MVertex(pc.x(), pc.y(), pc.z(), gf);
        }
        else{
          GPoint pc = gf->point(US[j + 1], VS[j + 1]);
          v = new MFaceVertex(pc.x(), pc.y(), pc.z(), gf, US[j + 1], VS[j + 1]);
          if (displ2D || displ3D){
            SPoint3 pc2 = edge.interpolate(t);          
            if(displ3D) displ3D->add(v, SVector3(pc2.x(), pc2.y(), pc2.z()));
            if(displ2D) displ2D->add(v, SVector3(pc2.x(), pc2.y(), pc2.z()));
          }
        }
        temp.push_back(v);
        gf->mesh_vertices.push_back(v);
        ve.push_back(v);
      }
      if(edge.getVertex(0) == edge.getMinVertex())
        edgeVertices[p].insert(edgeVertices[p].end(), temp.begin(), temp.end());
      else
        edgeVertices[p].insert(edgeVertices[p].end(), temp.rbegin(), temp.rend());
    }
  }
}

static void getEdgeVertices(GRegion *gr, MElement *ele, std::vector<MVertex*> &ve,
                            edgeContainer &edgeVertices, bool linear,
                            int nPts = 1, highOrderSmoother *displ2D = 0,
                            highOrderSmoother *displ3D = 0)
{
  for(int i = 0; i < ele->getNumEdges(); i++){
    MEdge edge = ele->getEdge(i);
    std::pair<MVertex*, MVertex*> p(edge.getMinVertex(), edge.getMaxVertex());
    if(edgeVertices.count(p)){
      if(edge.getVertex(0) == edge.getMinVertex())
        ve.insert(ve.end(), edgeVertices[p].begin(), edgeVertices[p].end());
      else
        ve.insert(ve.end(), edgeVertices[p].rbegin(), edgeVertices[p].rend());
    }
    else{
      std::vector<MVertex*> temp;
      for(int j = 0; j < nPts; j++){
        double t = (double)(j + 1) / (nPts + 1);
        SPoint3 pc = edge.interpolate(t);
        MVertex *v = new MVertex(pc.x(), pc.y(), pc.z(), gr);
        temp.push_back(v);
        gr->mesh_vertices.push_back(v);
        ve.push_back(v);
      }
      if(edge.getVertex(0) == edge.getMinVertex())
        edgeVertices[p].insert(edgeVertices[p].end(), temp.begin(), temp.end());
      else
        edgeVertices[p].insert(edgeVertices[p].end(), temp.rbegin(), temp.rend());
    }
  }
}

static void getFaceVertices(GFace *gf, MElement *incomplete, MElement *ele, 
                            std::vector<MVertex*> &vf, faceContainer &faceVertices, 
                            bool linear, int nPts = 1, highOrderSmoother *displ2D = 0,
                            highOrderSmoother *displ3D = 0)
{
  if(gf->geomType() == GEntity::DiscreteSurface ||
     gf->geomType() == GEntity::BoundaryLayerSurface)
    linear = true;

  for(int i = 0; i < ele->getNumFaces(); i++){
    MFace face = ele->getFace(i);
    faceContainer::iterator fIter = faceVertices.find(face);
    if(fIter != faceVertices.end()){
      vf.insert(vf.end(), fIter->second.begin(), fIter->second.end());
    }
    else{
      std::vector<MVertex*> &vtcs = faceVertices[face];
      SPoint2 pts[1000];
      bool reparamOK = true;
      if(!linear){
        for(int k = 0; k < incomplete->getNumVertices(); k++)
          reparamOK &= reparamMeshVertexOnFace(incomplete->getVertex(k), gf, pts[k]);
      }
      int start = face.getNumVertices() * (nPts + 1);
      const fullMatrix<double> &points = ele->getFunctionSpace(nPts + 1)->points;
      for(int k = start; k < points.size1(); k++){
        MVertex *v;
        const double t1 = points(k, 0);
        const double t2 = points(k, 1);
        if(linear){
          SPoint3 pc = face.interpolate(t1, t2);
          v = new MVertex(pc.x(), pc.y(), pc.z(), gf);
        }
        else{
          double X(0), Y(0), Z(0), GUESS[2] = {0, 0};
          double sf[1256];
          incomplete->getShapeFunctions(t1, t2, 0, sf);
          for (int j = 0; j < incomplete->getNumShapeFunctions(); j++){
            MVertex *vt = incomplete->getShapeFunctionNode(j);
            X += sf[j] * vt->x();
            Y += sf[j] * vt->y();
            Z += sf[j] * vt->z();
            if (reparamOK){
              GUESS[0] += sf[j] * pts[j][0];
              GUESS[1] += sf[j] * pts[j][1];
            }
          }
          if(reparamOK){
            GPoint gp = gf->closestPoint(SPoint3(X, Y, Z), GUESS);
            if (gp.g()){
              v = new MFaceVertex(gp.x(), gp.y(), gp.z(), gf, gp.u(), gp.v());
            }
            else{
              v = new MVertex(X, Y, Z, gf);
            }
          }
          else{
            v = new MVertex(X, Y, Z, gf);
          }
          if(displ3D || displ2D){
            SPoint3 pc2 = face.interpolate(t1, t2);
            if(displ3D) displ3D->add(v, SVector3(pc2.x(), pc2.y(), pc2.z()));
            if(displ2D) displ2D->add(v, SVector3(pc2.x(), pc2.y(), pc2.z()));
          }       
        }
        // should be expensive -> induces a new search each time
        vtcs.push_back(v);
        gf->mesh_vertices.push_back(v);
        vf.push_back(v);
      }
    }
  }  
}

static void reorientTrianglePoints(std::vector<MVertex*> &vtcs, int orientation, 
                                   bool swap)
{
  int nbPts = vtcs.size();
  if (nbPts <= 1) return;
  std::vector<MVertex*> tmp(nbPts);
  int interiorOrder = (int)((sqrt(1. + 8. * nbPts) - 3) / 2);
  int pos = 0;
  for (int o = interiorOrder; o>0; o-=3) {
    if (swap) {
      tmp[pos] = vtcs[pos];
      tmp[pos+1] = vtcs[pos+2];
      tmp[pos+2] = vtcs[pos+1];
      for (int i = 0; i < 3*(o-1); i++)
        tmp[pos+3+i] = vtcs[pos+3*o-i-1];
    }
    else {
      for (int i = 0; i < 3*o; i++)
        tmp[pos+i] = vtcs[pos+i];
    }
    for (int i = 0; i < 3; i++) {
      int ri = (i+orientation)%3;
      vtcs[pos+ri] = tmp[pos+i];
      for (int j = 0; j < o-1; j++)
        vtcs[pos+3+(o-1)*ri+j] = tmp[pos+3+(o-1)*i+j];
    }
    pos += 3*o;
  }
} 

static void reorientQuadPoints(std::vector<MVertex*> &vtcs, int orientation, 
			       bool swap, int order)
{
  int nbPts = vtcs.size();
  if (nbPts <= 1) return;
  std::vector<MVertex*> tmp(nbPts);  

  int start = 0;
  while (1){
    // CORNERS
    int index = 0;
    if (order == 0){
      start++;
    }
    else{
      int i1,i2,i3,i4;
      if (!swap){
	if      (orientation == 0){ i1 = 0; i2 = 1; i3 = 2; i4 = 3; }
	else if (orientation == 1){ i1 = 3; i2 = 0; i3 = 1; i4 = 2; }
	else if (orientation == 2){ i1 = 2; i2 = 3; i3 = 0; i4 = 1; }
	else if (orientation == 3){ i1 = 1; i2 = 2; i3 = 3; i4 = 0; }
      }
      else{
	if      (orientation == 0){ i1 = 0; i2 = 3; i3 = 2; i4 = 1; }
	else if (orientation == 3){ i1 = 3; i2 = 2; i3 = 1; i4 = 0; }
	else if (orientation == 2){ i1 = 2; i2 = 1; i3 = 0; i4 = 3; }
	else if (orientation == 1){ i1 = 1; i2 = 0; i3 = 3; i4 = 2; }
      }

      int indices[4] = {i1, i2, i3, i4};
      for (int i = 0; i < 4; i++) tmp[i] = vtcs[start + indices[i]];
      for (int i = 0; i < 4; i++) vtcs[start + i] = tmp[i];

      // EDGES
      for (int iEdge=0;iEdge<4;iEdge++){
	int p1 = indices[iEdge];
	int p2 = indices[(iEdge+1)%4];
	int nbP = order-1;
	if      (p1 == 0 && p2 == 1){
          for (int i = 4+0*nbP; i < 4+1*nbP; i++) tmp[index++] = vtcs[i];
        }
	else if (p1 == 1 && p2 == 2){
          for (int i = 4+1*nbP; i< 4+2*nbP; i++) tmp[index++] = vtcs[i];
        }
	else if (p1 == 2 && p2 == 3){
          for (int i = 4+2*nbP; i< 4+3*nbP; i++) tmp[index++] = vtcs[i];
        }
	else if (p1 == 3 && p2 == 0){
          for (int i = 4+3*nbP; i< 4+4*nbP; i++) tmp[index++] = vtcs[i];
        }
	else if (p1 == 1 && p2 == 0){
          for (int i = 4+1*nbP-1; i >= 4+0*nbP; i--) tmp[index++] = vtcs[i];
        }
	else if (p1 == 2 && p2 == 1){
          for (int i = 4+2*nbP-1; i >= 4+1*nbP; i--) tmp[index++] = vtcs[i];
        }
	else if (p1 == 3 && p2 == 2){
          for (int i = 4+3*nbP-1; i >= 4+2*nbP; i--) tmp[index++] = vtcs[i];
        }
	else if (p1 == 0 && p2 == 3){
          for (int i = 4+4*nbP-1; i >= 4+3*nbP; i--) tmp[index++] = vtcs[i];
        }
	else Msg::Error("Something wrong in reorientQuadPoints");
      }	
      for (int i = 0; i < index; i++)vtcs[start+4+i] = tmp[i];      
      start += (4 + index);
    }

    order -= 2;
    if (start >= vtcs.size()) break;
  }
} 

static int getNewFacePoints(int numPrimaryFacePoints, int nPts, fullMatrix<double> &points)
{
  int start = 0;
  switch(numPrimaryFacePoints){
  case 3:
    switch (nPts){
    case 0 : break;
    case 1 : break;
    case 2 : points = polynomialBases::find(MSH_TRI_10)->points; start = 9; break;
    case 3 : points = polynomialBases::find(MSH_TRI_15)->points; start = 12; break;
    case 4 : points = polynomialBases::find(MSH_TRI_21)->points; start = 15; break;
    case 5 : points = polynomialBases::find(MSH_TRI_28)->points; start = 18; break;
    case 6 : points = polynomialBases::find(MSH_TRI_36)->points; start = 21; break;
    case 7 : points = polynomialBases::find(MSH_TRI_45)->points; start = 24; break;
    case 8 : points = polynomialBases::find(MSH_TRI_55)->points; start = 27; break;
    case 9 : points = polynomialBases::find(MSH_TRI_66)->points; start = 30; break;
    default :
      Msg::Error("getFaceVertices not implemented for order %i", nPts +1);
      break;
    }
    break;
  case 4:
    switch (nPts){
    case 0 : break;
    case 1 : points = polynomialBases::find(MSH_QUA_9)->points; break;
    case 2 : points = polynomialBases::find(MSH_QUA_16)->points; break;
    case 3 : points = polynomialBases::find(MSH_QUA_25)->points; break;
    case 4 : points = polynomialBases::find(MSH_QUA_36)->points; break;
    case 5 : points = polynomialBases::find(MSH_QUA_49)->points; break;
    case 6 : points = polynomialBases::find(MSH_QUA_64)->points; break;
    case 7 : points = polynomialBases::find(MSH_QUA_81)->points; break;
    case 8 : points = polynomialBases::find(MSH_QUA_100)->points; break;
    default :  
      Msg::Error("getFaceVertices not implemented for order %i", nPts +1);
      break;
    }
    start = (nPts + 2) * (nPts + 2) - nPts * nPts;
    break;
  default:
    Msg::Error("getFaceVertices not implemented for %d-node faces", numPrimaryFacePoints);
    break;
  }
  return start;
}

static void getFaceVertices(GRegion *gr, MElement *ele, std::vector<MVertex*> &vf,
                            faceContainer &faceVertices, edgeContainer &edgeVertices,
                            bool linear, int nPts = 1)
{
  for(int i = 0; i < ele->getNumFaces(); i++){
    MFace face = ele->getFace(i);
    faceContainer::iterator fIter = faceVertices.find(face);
    if(fIter != faceVertices.end()) {
      std::vector<MVertex*> vtcs = fIter->second;
      if(face.getNumVertices() == 3 && nPts > 1){ // tri face
        int orientation;
        bool swap;
        if (fIter->first.computeCorrespondence(face, orientation, swap))
          reorientTrianglePoints(vtcs, orientation, swap);
        else
          Msg::Error("Error in face lookup for recuperation of high order face nodes");
      }
      else if(face.getNumVertices() == 4){ // quad face
        int orientation;
        bool swap;
        if (fIter->first.computeCorrespondence(face, orientation, swap)){
          reorientQuadPoints(vtcs, orientation, swap, nPts-1);
	}
        else
          Msg::Error("Error in face lookup for recuperation of high order face nodes");
      }
      vf.insert(vf.end(), vtcs.begin(), vtcs.end());
    }
    else{
      std::vector<MVertex*> &vtcs = faceVertices[face];
      fullMatrix<double> points;
      int start = getNewFacePoints(face.getNumVertices(), nPts, points);
      if(face.getNumVertices() == 3 && nPts > 1){ // tri face
        // construct incomplete element to take into account curved
        // edges on surface boundaries
        std::vector<MVertex*> hoEdgeNodes;
        for (int i = 0; i < 3; i++) {
          MVertex* v0 = face.getVertex(i);
          MVertex* v1 = face.getVertex((i + 1) % 3);
          edgeContainer::iterator eIter = edgeVertices.find
            (std::pair<MVertex*,MVertex*>(std::min(v0, v1), std::max(v0, v1)));
          if (eIter == edgeVertices.end())
            Msg::Error("Could not find ho nodes for an edge");
          if (v0 == eIter->first.first) 
            hoEdgeNodes.insert(hoEdgeNodes.end(), eIter->second.begin(),
                               eIter->second.end());
          else                    
            hoEdgeNodes.insert(hoEdgeNodes.end(), eIter->second.rbegin(), 
                               eIter->second.rend());
        }
        MTriangleN incomplete(face.getVertex(0), face.getVertex(1),
                              face.getVertex(2), hoEdgeNodes, nPts + 1);
        for (int k = start; k < points.size1(); k++) {
          double t1 = points(k, 0);
          double t2 = points(k, 1);
          SPoint3 pos;
          incomplete.pnt(t1, t2, 0, pos);
          MVertex* v = new MVertex(pos.x(), pos.y(), pos.z(), gr);
          vtcs.push_back(v);
          gr->mesh_vertices.push_back(v);
          vf.push_back(v);
        }         
      }
      else if(face.getNumVertices() == 4){ // quad face
        for (int k = start; k < points.size1(); k++) {
          double t1 = points(k, 0);
          double t2 = points(k, 1);
          SPoint3 pc = face.interpolate(t1, t2);
          MVertex *v = new MVertex(pc.x(), pc.y(), pc.z(), gr);
          vtcs.push_back(v);
          gr->mesh_vertices.push_back(v);
          vf.push_back(v);
        }
      }
    }
  }
}

static void getRegionVertices(GRegion *gr, MElement *incomplete, MElement *ele, 
                              std::vector<MVertex*> &vr, bool linear, int nPts = 1)
{
  fullMatrix<double> points;
  int start = 0;

  switch (incomplete->getType()){
  case TYPE_TET : 
    switch (nPts){
    case 0: return;
    case 1: return;	
    case 2: points = polynomialBases::find(MSH_TET_20)->points; break;
    case 3: points = polynomialBases::find(MSH_TET_35)->points; break;
    case 4: points = polynomialBases::find(MSH_TET_56)->points; break;
    case 5: points = polynomialBases::find(MSH_TET_84)->points; break;
    case 6: points = polynomialBases::find(MSH_TET_120)->points; break;
    case 7: points = polynomialBases::find(MSH_TET_165)->points; break;
    case 8: points = polynomialBases::find(MSH_TET_220)->points; break;
    case 9: points = polynomialBases::find(MSH_TET_286)->points; break;
    default:
      Msg::Error("getRegionVertices not implemented for order %i", nPts+1);
      break;
    }
    start = ((nPts+2)*(nPts+3)*(nPts+4)-(nPts-2)*(nPts-1)*(nPts))/6;
    break;
  case TYPE_HEX :
    switch (nPts){
    case 0: return;
    case 1: points = polynomialBases::find(MSH_HEX_27)->points; break;
    case 2: points = polynomialBases::find(MSH_HEX_64)->points; break;
    case 3: points = polynomialBases::find(MSH_HEX_125)->points; break;
    case 4: points = polynomialBases::find(MSH_HEX_216)->points; break;
    case 5: points = polynomialBases::find(MSH_HEX_343)->points; break;
    case 6: points = polynomialBases::find(MSH_HEX_512)->points; break;
    case 7: points = polynomialBases::find(MSH_HEX_729)->points; break;
    case 8: points = polynomialBases::find(MSH_HEX_1000)->points; break;
    default :  
      Msg::Error("getRegionVertices not implemented for order %i", nPts+1);
      break;
    }
    start = (nPts+2)*(nPts+2)*(nPts+2) - (nPts)*(nPts)*(nPts) ;
    break;
  }

  for(int k = start; k < points.size1(); k++){
    MVertex *v;
    const double t1 = points(k, 0);
    const double t2 = points(k, 1);
    const double t3 = points(k, 2);
    SPoint3 pos;
    incomplete->pnt(t1, t2, t3, pos);
    v = new MVertex(pos.x(), pos.y(), pos.z(), gr);
    gr->mesh_vertices.push_back(v);
    vr.push_back(v);
  }
}

static void setHighOrder(GEdge *ge, edgeContainer &edgeVertices, bool linear, 
                         int nbPts = 1, highOrderSmoother *displ2D = 0,
                         highOrderSmoother *displ3D = 0)
{
  std::vector<MLine*> lines2;
  for(unsigned int i = 0; i < ge->lines.size(); i++){
    MLine *l = ge->lines[i];
    std::vector<MVertex*> ve;
    getEdgeVertices(ge, l, ve, edgeVertices, linear, nbPts, displ2D, displ3D);
    if(nbPts == 1)
      lines2.push_back(new MLine3(l->getVertex(0), l->getVertex(1), ve[0]));
    else
      lines2.push_back(new MLineN(l->getVertex(0), l->getVertex(1), ve));      
    delete l;
  }
  ge->lines = lines2;
  ge->deleteVertexArrays();
}

MTriangle *setHighOrder(MTriangle *t, GFace *gf, 
                        edgeContainer &edgeVertices, 
                        faceContainer &faceVertices, 
                        bool linear, bool incomplete, int nPts, 
                        highOrderSmoother *displ2D,
                        highOrderSmoother *displ3D)
{
  std::vector<MVertex*> ve, vf;
  getEdgeVertices(gf, t, ve, edgeVertices, linear, nPts, displ2D, displ3D);
  if(nPts == 1){
    return new MTriangle6(t->getVertex(0), t->getVertex(1), t->getVertex(2),
                          ve[0], ve[1], ve[2]);
  }
  else{
    if(incomplete){
      return new MTriangleN(t->getVertex(0), t->getVertex(1), t->getVertex(2),
                            ve, nPts + 1);
    }
    else{
      if (displ2D && gf->geomType() == GEntity::Plane){
        MTriangle incpl(t->getVertex(0), t->getVertex(1), t->getVertex(2));
        getFaceVertices(gf, &incpl, t, vf, faceVertices, linear, nPts, displ2D, 
                        displ3D);
      }
      else{
        MTriangleN incpl(t->getVertex(0), t->getVertex(1), t->getVertex(2),
                         ve, nPts + 1);
        getFaceVertices(gf, &incpl, t, vf, faceVertices, linear, nPts, displ2D, 
                        displ3D);
      }
      ve.insert(ve.end(), vf.begin(), vf.end());
      return new MTriangleN(t->getVertex(0), t->getVertex(1), t->getVertex(2),
                            ve, nPts + 1);
    }
  }  
}

static MQuadrangle *setHighOrder(MQuadrangle *q, GFace *gf,
                                 edgeContainer &edgeVertices, 
                                 faceContainer &faceVertices, 
                                 bool linear, bool incomplete, int nPts, 
                                 highOrderSmoother *displ2D,
                                 highOrderSmoother *displ3D)
{
  std::vector<MVertex*> ve, vf;
  getEdgeVertices(gf, q, ve, edgeVertices, linear, nPts, displ2D, displ3D);
  if(incomplete){
    if(nPts == 1){
      return new MQuadrangle8(q->getVertex(0), q->getVertex(1), q->getVertex(2), 
                              q->getVertex(3), ve[0], ve[1], ve[2], ve[3]);
    }
    else{
      return new MQuadrangleN(q->getVertex(0), q->getVertex(1), q->getVertex(2),
                              q->getVertex(3), ve, nPts + 1);
    }
  } 
  else {
    if (displ2D && gf->geomType() == GEntity::Plane){
      MQuadrangle incpl(q->getVertex(0), q->getVertex(1), q->getVertex(2), 
                        q->getVertex(3));
      getFaceVertices(gf, &incpl, q, vf, faceVertices, linear, nPts, displ2D, displ3D);
    }else{
      MQuadrangleN incpl(q->getVertex(0), q->getVertex(1), q->getVertex(2), 
                         q->getVertex(3), ve, nPts + 1);
      getFaceVertices(gf, &incpl, q, vf, faceVertices, linear, nPts, displ2D, displ3D);
    }
    ve.insert(ve.end(), vf.begin(), vf.end());
    if(nPts == 1){
      return new MQuadrangle9(q->getVertex(0), q->getVertex(1), q->getVertex(2),
                              q->getVertex(3), ve[0], ve[1], ve[2], ve[3], vf[0]);
    }
    else{
      return new MQuadrangleN(q->getVertex(0), q->getVertex(1), q->getVertex(2),
                              q->getVertex(3), ve, nPts + 1);
    }
  }
}  

static void setHighOrder(GFace *gf, edgeContainer &edgeVertices, 
                         faceContainer &faceVertices, bool linear, bool incomplete,
                         int nPts = 1, highOrderSmoother *displ2D = 0,
                         highOrderSmoother *displ3D = 0)
{
  std::vector<MTriangle*> triangles2;
  for(unsigned int i = 0; i < gf->triangles.size(); i++){
    MTriangle *t = gf->triangles[i];
    MTriangle *tNew = setHighOrder(t, gf,edgeVertices, faceVertices, linear, incomplete,
                                   nPts, displ2D, displ3D);
    triangles2.push_back(tNew);
    delete t;
  }
  gf->triangles = triangles2;
  std::vector<MQuadrangle*> quadrangles2;
  for(unsigned int i = 0; i < gf->quadrangles.size(); i++){
    MQuadrangle *q = gf->quadrangles[i];
    MQuadrangle *qNew = setHighOrder(q, gf, edgeVertices, faceVertices, linear,
                                     incomplete, nPts, displ2D, displ3D);
    quadrangles2.push_back(qNew);
    delete q;
  }
  gf->quadrangles = quadrangles2;
  gf->deleteVertexArrays();
}

static void setHighOrder(GRegion *gr, edgeContainer &edgeVertices, 
                         faceContainer &faceVertices, bool linear, bool incomplete,
                         int nPts = 1, highOrderSmoother *displ2D = 0,
                         highOrderSmoother *displ3D = 0)
{
  std::vector<MTetrahedron*> tetrahedra2;
  for(unsigned int i = 0; i < gr->tetrahedra.size(); i++){
    MTetrahedron *t = gr->tetrahedra[i];
    std::vector<MVertex*> ve, vf, vr;
    getEdgeVertices(gr, t, ve, edgeVertices, linear, nPts, displ2D, displ3D);
    if(nPts == 1){
      tetrahedra2.push_back
        (new MTetrahedron10(t->getVertex(0), t->getVertex(1), t->getVertex(2), 
                            t->getVertex(3), ve[0], ve[1], ve[2], ve[3], ve[4], ve[5]));
    }
    else{
      getFaceVertices(gr, t, vf, faceVertices, edgeVertices, linear, nPts);
      ve.insert(ve.end(), vf.begin(), vf.end());     
      MTetrahedronN incpl(t->getVertex(0), t->getVertex(1), t->getVertex(2), 
                          t->getVertex(3), ve, nPts + 1);
      getRegionVertices(gr, &incpl, t, vr, linear, nPts); 
      ve.insert(ve.end(), vr.begin(), vr.end());
      MTetrahedron *n = new MTetrahedronN(t->getVertex(0), t->getVertex(1), 
                                          t->getVertex(2), t->getVertex(3), ve, nPts + 1);
      tetrahedra2.push_back(n);
    }
    delete t;
  }
  gr->tetrahedra = tetrahedra2;

  std::vector<MHexahedron*> hexahedra2;
  for(unsigned int i = 0; i < gr->hexahedra.size(); i++){
    MHexahedron *h = gr->hexahedra[i];
    std::vector<MVertex*> ve, vf, vr;
    getEdgeVertices(gr, h, ve, edgeVertices, linear, nPts, displ2D, displ3D);
    if(nPts == 1){
      if(incomplete){
	hexahedra2.push_back
	  (new MHexahedron20(h->getVertex(0), h->getVertex(1), h->getVertex(2), 
			     h->getVertex(3), h->getVertex(4), h->getVertex(5), 
			     h->getVertex(6), h->getVertex(7), ve[0], ve[1], ve[2], 
			     ve[3], ve[4], ve[5], ve[6], ve[7], ve[8], ve[9], ve[10], 
			     ve[11]));
      }
      else{
	getFaceVertices(gr, h, vf, faceVertices, edgeVertices, linear, nPts);
	SPoint3 pc = h->barycenter();
	MVertex *v = new MVertex(pc.x(), pc.y(), pc.z(), gr);
	gr->mesh_vertices.push_back(v);
	hexahedra2.push_back
	  (new MHexahedron27(h->getVertex(0), h->getVertex(1), h->getVertex(2), 
			     h->getVertex(3), h->getVertex(4), h->getVertex(5), 
			     h->getVertex(6), h->getVertex(7), ve[0], ve[1], ve[2], 
			     ve[3], ve[4], ve[5], ve[6], ve[7], ve[8], ve[9], ve[10], 
			     ve[11], vf[0], vf[1], vf[2], vf[3], vf[4], vf[5], v));
      }
    }
    else {
      getFaceVertices(gr, h, vf, faceVertices, edgeVertices, linear, nPts);
      ve.insert(ve.end(), vf.begin(), vf.end());     
      MHexahedronN incpl(h->getVertex(0), h->getVertex(1), h->getVertex(2),
                         h->getVertex(3), h->getVertex(4), h->getVertex(5),
                         h->getVertex(6), h->getVertex(7), ve, nPts + 1);
      getRegionVertices(gr, &incpl, h, vr, linear, nPts); 
      ve.insert(ve.end(), vr.begin(), vr.end());
      MHexahedron *n = new MHexahedronN(h->getVertex(0), h->getVertex(1), h->getVertex(2), 
                                        h->getVertex(3), h->getVertex(4), h->getVertex(5),
                                        h->getVertex(6), h->getVertex(7), ve, nPts + 1);
      hexahedra2.push_back(n);     
    }
    delete h;
  }
  gr->hexahedra = hexahedra2;

  std::vector<MPrism*> prisms2;
  for(unsigned int i = 0; i < gr->prisms.size(); i++){
    MPrism *p = gr->prisms[i];
    std::vector<MVertex*> ve, vf;
    getEdgeVertices(gr, p, ve, edgeVertices, linear, nPts, displ2D, displ3D);
    if(incomplete){
      prisms2.push_back
        (new MPrism15(p->getVertex(0), p->getVertex(1), p->getVertex(2), 
                      p->getVertex(3), p->getVertex(4), p->getVertex(5), 
                      ve[0], ve[1], ve[2], ve[3], ve[4], ve[5], ve[6], ve[7], ve[8]));
    }
    else{
      getFaceVertices(gr, p, vf, faceVertices, edgeVertices, linear, nPts);
      prisms2.push_back
        (new MPrism18(p->getVertex(0), p->getVertex(1), p->getVertex(2), 
                      p->getVertex(3), p->getVertex(4), p->getVertex(5), 
                      ve[0], ve[1], ve[2], ve[3], ve[4], ve[5], ve[6], ve[7], ve[8],
                      vf[0], vf[1], vf[2]));
    }
    delete p;
  }
  gr->prisms = prisms2;

  std::vector<MPyramid*> pyramids2;
  for(unsigned int i = 0; i < gr->pyramids.size(); i++){
    MPyramid *p = gr->pyramids[i];
    std::vector<MVertex*> ve, vf;
    getEdgeVertices(gr, p, ve, edgeVertices, linear, nPts, displ2D, displ3D);
    if(incomplete){
      pyramids2.push_back
        (new MPyramid13(p->getVertex(0), p->getVertex(1), p->getVertex(2), 
                        p->getVertex(3), p->getVertex(4), ve[0], ve[1], ve[2], 
                        ve[3], ve[4], ve[5], ve[6], ve[7]));
    }
    else{
      getFaceVertices(gr, p, vf, faceVertices, edgeVertices, linear, nPts);
      pyramids2.push_back
        (new MPyramid14(p->getVertex(0), p->getVertex(1), p->getVertex(2), 
                        p->getVertex(3), p->getVertex(4), ve[0], ve[1], ve[2], 
                        ve[3], ve[4], ve[5], ve[6], ve[7], vf[0]));
    }
    delete p;
  }
  gr->pyramids = pyramids2;
  gr->deleteVertexArrays();
}

template<class T>
static void setFirstOrder(GEntity *e, std::vector<T*> &elements)
{
  std::vector<T*> elements1;
  for(unsigned int i = 0; i < elements.size(); i++){
    T *ele = elements[i];
    int n = ele->getNumPrimaryVertices();
    std::vector<MVertex*> v1;
    for(int j = 0; j < n; j++)
      v1.push_back(ele->getVertex(j));
    elements1.push_back(new T(v1));
    delete ele;
  }
  elements = elements1;
  e->deleteVertexArrays();
}

static void removeHighOrderVertices(GEntity *e)
{
  std::vector<MVertex*> v1;
  for(unsigned int i = 0; i < e->mesh_vertices.size(); i++){
    if(e->mesh_vertices[i]->getPolynomialOrder() > 1)
      delete e->mesh_vertices[i];
    else
      v1.push_back(e->mesh_vertices[i]);
  }
  e->mesh_vertices = v1;
}

void SetOrder1(GModel *m)
{
  m->destroyMeshCaches();

  // replace all elements with first order elements
  for(GModel::eiter it = m->firstEdge(); it != m->lastEdge(); ++it){
    setFirstOrder(*it, (*it)->lines);
  }
  for(GModel::fiter it = m->firstFace(); it != m->lastFace(); ++it){
    setFirstOrder(*it, (*it)->triangles);
    setFirstOrder(*it, (*it)->quadrangles);
  }
  for(GModel::riter it = m->firstRegion(); it != m->lastRegion(); ++it){
    setFirstOrder(*it, (*it)->tetrahedra);
    setFirstOrder(*it, (*it)->hexahedra);
    setFirstOrder(*it, (*it)->prisms);
    setFirstOrder(*it, (*it)->pyramids);
  }

  // remove all high order vertices
  for(GModel::eiter it = m->firstEdge(); it != m->lastEdge(); ++it)
    removeHighOrderVertices(*it);
  for(GModel::fiter it = m->firstFace(); it != m->lastFace(); ++it)
    removeHighOrderVertices(*it);
  for(GModel::riter it = m->firstRegion(); it != m->lastRegion(); ++it)
    removeHighOrderVertices(*it);
}

void checkHighOrderTriangles(const char* cc, GModel *m, 
                             std::vector<MElement*> &bad, double &minJGlob)
{
  bad.clear();
  minJGlob = 1.0;
  double minGGlob = 1.0;
  double avg = 0.0;
  int count = 0, nbfair=0;
  for(GModel::fiter it = m->firstFace(); it != m->lastFace(); ++it){
    for(unsigned int i = 0; i < (*it)->triangles.size(); i++){
      MTriangle *t = (*it)->triangles[i];
      double disto_ = t->distoShapeMeasure();
      double gamma_ = t->gammaShapeMeasure();
      double disto = disto_;
      minJGlob = std::min(minJGlob, disto);
      minGGlob = std::min(minGGlob, gamma_);
      avg += disto; count++;
      if (disto < 0) bad.push_back(t);
      else if (disto < 0.2) nbfair++;
    }
    for(unsigned int i = 0; i < (*it)->quadrangles.size(); i++){
      MQuadrangle *t = (*it)->quadrangles[i];
      double disto_ = t->distoShapeMeasure();
      double gamma_ = t->gammaShapeMeasure();
      double disto = disto_;
      minJGlob = std::min(minJGlob, disto);
      minGGlob = std::min(minGGlob, gamma_);
      avg += disto; count++;
      if (disto < 0) bad.push_back(t);
      else if (disto < 0.2) nbfair++;
    }
  }
  if(!count) return;
  if (minJGlob > 0) 
    Msg::Info("%s : Worst Face Distorsion Mapping %g Gamma %g Nb elem. (0<d<0.2) = %d", 
              cc, minJGlob, minGGlob,nbfair );
  else
    Msg::Warning("%s : Worst Face Distorsion Mapping %g (%d negative jacobians) "
                 "Worst Gamma %g Avg Smoothness %g", cc, minJGlob, bad.size(),
                 minGGlob, avg / (count ? count : 1));
}

static void checkHighOrderTetrahedron(const char* cc, GModel *m, 
                                      std::vector<MElement*> &bad, double &minJGlob)
{
  bad.clear();
  minJGlob = 1.0;
  double minGGlob = 1.0;
  double avg = 0.0;
  int count = 0, nbfair=0;
  for(GModel::riter it = m->firstRegion(); it != m->lastRegion(); ++it){
    for(unsigned int i = 0; i < (*it)->tetrahedra.size(); i++){
      MTetrahedron *t = (*it)->tetrahedra[i];
      double disto_ = t->distoShapeMeasure();
      double gamma_ = t->gammaShapeMeasure();
      double disto = disto_;
      minJGlob = std::min(minJGlob, disto);
      minGGlob = std::min(minGGlob, gamma_);
      avg += disto; count++;
      if (disto < 0) bad.push_back(t);
      else if (disto < 0.2) nbfair++;
    }
  }
  if(!count) return;
  if (minJGlob < 0)
    Msg::Warning("%s : Worst Tetrahedron Smoothness %g Gamma %g NbFair = %d NbBad = %d", 
              cc, minJGlob, minGGlob, nbfair, bad.size());
  else 
    Msg::Info("%s : Worst Tetrahedron Smoothness %g (%d negative jacobians) "
                 "Worst Gamma %g Avg Smoothness %g", cc, minJGlob, bad.size(),
                 minGGlob, avg / (count ? count : 1));
}

extern double mesh_functional_distorsion(MElement *t, double u, double v);

void printJacobians(GModel *m, const char *nm)
{
  const int n = 100;
  double D[n][n], X[n][n], Y[n][n], Z[n][n];

  FILE *f = fopen(nm,"w");
  fprintf(f,"View \"\"{\n");
  for(GModel::fiter it = m->firstFace(); it != m->lastFace(); ++it){
    for(unsigned int j = 0; j < (*it)->triangles.size(); j++){
      MTriangle *t = (*it)->triangles[j];
      for(int i = 0; i < n; i++){
        for(int k = 0; k < n - i; k++){
          SPoint3 pt;
          double u = (double)i / (n - 1);
          double v = (double)k / (n - 1);         
          t->pnt(u, v, 0, pt);
          D[i][k] = mesh_functional_distorsion(t, u, v);
          //X[i][k] = u;
          //Y[i][k] = v;
          //Z[i][k] = 0.0;
          X[i][k] = pt.x();
          Y[i][k] = pt.y();
          Z[i][k] = pt.z();
        }
      }
      for(int i= 0; i < n -1; i++){
        for(int k = 0; k < n - i -1; k++){
          fprintf(f,"ST(%g,%g,%g,%g,%g,%g,%g,%g,%g){%22.15E,%22.15E,%22.15E};\n",
                  X[i][k],Y[i][k],Z[i][k],
                  X[i+1][k],Y[i+1][k],Z[i+1][k],
                  X[i][k+1],Y[i][k+1],Z[i][k+1],
                  D[i][k],
                  D[i+1][k],
                  D[i][k+1]);
          if (i != n-2 && k != n - i -2)
            fprintf(f,"ST(%g,%g,%g,%g,%g,%g,%g,%g,%g){%22.15E,%22.15E,%22.15E};\n",
                    X[i+1][k],Y[i+1][k],Z[i+1][k],
                    X[i+1][k+1],Y[i+1][k+1],Z[i+1][k+1],
                    X[i][k+1],Y[i][k+1],Z[i][k+1],
                    D[i+1][k],
                    D[i+1][k+1],
                    D[i][k+1]);
        }
      }
    }
  }
  fprintf(f,"};\n");
  fclose(f);
}

void SetOrderN(GModel *m, int order, bool linear, bool incomplete)
{
  // replace all the elements in the mesh with second order elements
  // by creating unique vertices on the edges/faces of the mesh:
  //
  // - if linear is set to true, new vertices are created by linear
  //   interpolation between existing ones. If not, new vertices are
  //   created on the exact geometry, provided that the geometrical
  //   edges/faces are discretized with 1D/2D elements. (I.e., if
  //   there are only 3D elements in the mesh then any new vertices on
  //   the boundary will always be created by linear interpolation,
  //   whether linear is set or not.)
  //
  // - if incomplete is set to true, we only create new vertices on 
  //   edges (creating 8-node quads, 20-node hexas, etc., instead of
  //   9-node quads, 27-node hexas, etc.)

  int nPts = order - 1;

  if (!linear)
    Msg::StatusBar(2, true, "Meshing order %d, curvilinear ON...", order);
  else
    Msg::StatusBar(2, true, "Meshing order %d, curvilinear OFF...", order);

  double t1 = Cpu();

  // first, make sure to remove any existsing second order vertices/elements
  SetOrder1(m);    

  // m->writeMSH("BEFORE.msh");

  highOrderSmoother *displ2D = 0; 
  highOrderSmoother *displ3D = 0; 
  if(CTX::instance()->mesh.smoothInternalEdges){
    displ2D = new highOrderSmoother(2);
    displ3D = new highOrderSmoother(3);
  }

  // then create new second order vertices/elements
  edgeContainer edgeVertices;
  faceContainer faceVertices;
  
  Msg::StatusBar(2, true, "Meshing curves order %d...", order);
  for(GModel::eiter it = m->firstEdge(); it != m->lastEdge(); ++it)
    setHighOrder(*it, edgeVertices, linear, nPts, displ2D, displ3D);

  Msg::StatusBar(2, true, "Meshing surfaces order %d...", order);
  for(GModel::fiter it = m->firstFace(); it != m->lastFace(); ++it)
    setHighOrder(*it, edgeVertices, faceVertices, linear, incomplete, nPts,
                 displ2D, displ3D);

  highOrderTools hot(m);

  // now we smooth mesh the internal vertices of the faces
  // we do that model face by model face
  std::vector<MElement*> bad;
  double worst;

  // printJacobians(m, "smoothness_b.pos");
  // m->writeMSH("RAW.msh");

  if (displ2D){
    checkHighOrderTriangles("Before optimization", m, bad, worst);
    for(GModel::fiter it = m->firstFace(); it != m->lastFace(); ++it)
      displ2D->optimize(*it,edgeVertices,faceVertices);
    checkHighOrderTriangles("After optimization", m, bad, worst);
  }

  Msg::StatusBar(2, true, "Meshing volumes order %d...", order);
  for(GModel::riter it = m->firstRegion(); it != m->lastRegion(); ++it)
    setHighOrder(*it, edgeVertices, faceVertices, linear, incomplete, nPts,
                 displ2D, displ3D);

  // smooth the 3D regions
  if (displ3D){
    checkHighOrderTetrahedron("Before optimization", m, bad, worst);
    for(GModel::riter it = m->firstRegion(); it != m->lastRegion(); ++it)
      displ3D->smooth(*it);
    checkHighOrderTetrahedron("After optimization", m, bad, worst);
  }

  if(displ2D) delete displ2D;
  if(displ3D) delete displ3D;

  double t2 = Cpu();

  // printJacobians(m, "smoothness.pos");
  // m->writeMSH("SMOOTHED.msh");
  // FIXME !!
  if (!linear){
  //hot.ensureMinimumDistorsion(0.1);
    checkHighOrderTriangles("Final mesh", m, bad, worst);
    checkHighOrderTetrahedron("Final mesh", m, bad, worst);
  }

  // m->writeMSH("CORRECTED.msh");

  Msg::StatusBar(2, true, "Done meshing order %d (%g s)", order, t2 - t1);
}
