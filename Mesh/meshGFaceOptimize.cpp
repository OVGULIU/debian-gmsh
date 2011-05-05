// Gmsh - Copyright (C) 1997-2008 C. Geuzaine, J.-F. Remacle
//
// See the LICENSE.txt file for license information. Please report all
// bugs and problems to <gmsh@geuz.org>.

#include "meshGFaceOptimize.h"
#include "qualityMeasures.h"
#include "GFace.h"
#include "GEdge.h"
#include "GVertex.h"
#include "MVertex.h"
#include "MElement.h"
#include "BackgroundMesh.h"
#include "Message.h"

static void setLcsInit(MTriangle *t, std::map<MVertex*, double> &vSizes)
{
  for (int i = 0; i < 3; i++){
    for (int j = i + 1; j < 3; j++){
      MVertex *vi = t->getVertex(i);
      MVertex *vj = t->getVertex(j);
      vSizes[vi] = -1;
      vSizes[vj] = -1;
    }
  }
}

static void setLcs(MTriangle *t, std::map<MVertex*,double> &vSizes)
{
  for (int i = 0; i < 3; i++){
    for (int j = i + 1; j < 3; j++){
      MVertex *vi = t->getVertex(i);
      MVertex *vj = t->getVertex(j);
      double dx = vi->x()-vj->x();
      double dy = vi->y()-vj->y();
      double dz = vi->z()-vj->z();
      double l = sqrt(dx * dx + dy * dy + dz * dz);
      std::map<MVertex*,double>::iterator iti = vSizes.find(vi);	  
      std::map<MVertex*,double>::iterator itj = vSizes.find(vj);	  
      if (iti->second < 0 || iti->second > l) iti->second = l;
      if (itj->second < 0 || itj->second > l) itj->second = l;
    }
  }
}

static void setLcs(MLine *t, std::map<MVertex*,double> &vSizes)
{
  MVertex *vi = t->getVertex(0);
  MVertex *vj = t->getVertex(1);
  double dx = vi->x()-vj->x();
  double dy = vi->y()-vj->y();
  double dz = vi->z()-vj->z();
  double l = sqrt(dx * dx + dy * dy + dz * dz);
  std::map<MVertex*,double>::iterator iti = vSizes.find(vi);	  
  std::map<MVertex*,double>::iterator itj = vSizes.find(vj);	  
  if (iti->second < 0 || iti->second < l) iti->second = l;
  if (itj->second < 0 || itj->second < l) itj->second = l;
}

void buidMeshGenerationDataStructures(GFace *gf, std::set<MTri3*, compareTri3Ptr> &AllTris,
                                      std::vector<double> &vSizes,
                                      std::vector<double> &vSizesBGM,
                                      std::vector<double> &Us,
                                      std::vector<double> &Vs)
{
  std::map<MVertex*, double> vSizesMap;
  std::list<GEdge*> edges = gf->edges();

  for (unsigned int i = 0;i < gf->triangles.size(); i++)
    setLcsInit(gf->triangles[i], vSizesMap);

//   std::list<GEdge*>::iterator it = edges.begin();
//   while (it != edges.end()){
//     GEdge *ge = *it ;
//     for (int i=0;i<ge->lines.size();i++){
//       setLcs(ge->lines[i], vSizesMap);      
//     }
//     ++it;
//   }

  for (unsigned int i = 0;i < gf->triangles.size(); i++)
    setLcs(gf->triangles[i], vSizesMap);

  int NUM = 0;
  for (std::map<MVertex*, double>::iterator it = vSizesMap.begin();
       it != vSizesMap.end(); ++it){
    it->first->setNum(NUM++);
    vSizes.push_back(it->second);
    vSizesBGM.push_back(it->second);
    double u0, v0;
    parametricCoordinates(it->first, gf, u0, v0);
    Us.push_back(u0);
    Vs.push_back(v0);
  }
  for(unsigned int i = 0; i < gf->triangles.size(); i++){
    double lc = 0.3333333333 * (vSizes [gf->triangles[i]->getVertex(0)->getNum()] +
                                vSizes [gf->triangles[i]->getVertex(1)->getNum()] +
                                vSizes [gf->triangles[i]->getVertex(2)->getNum()]);
    AllTris.insert(new MTri3(gf->triangles[i], lc));
  }
  gf->triangles.clear();
  connectTriangles(AllTris );
}

void transferDataStructure(GFace *gf, std::set<MTri3*, compareTri3Ptr> &AllTris)
{
  while (1) {
    if (AllTris.begin() == AllTris.end() ) break;
    MTri3 *worst = *AllTris.begin();
    if (worst->isDeleted())
      delete worst->tri();
    else
      gf->triangles.push_back(worst->tri());
    delete worst;
    AllTris.erase(AllTris.begin());      
  }
}

void buildVertexToTriangle(std::vector<MTriangle*> &triangles, v2t_cont &adj)
{
  adj.clear();
  for (unsigned int i = 0; i < triangles.size(); i++){
    MTriangle *t = triangles[i];
    for (unsigned int j = 0; j < 3; j++){
      MVertex *v = t->getVertex(j);
      v2t_cont :: iterator it = adj.find(v);
      if (it == adj.end()){
        std::vector<MTriangle*> one;
        one.push_back(t);
        adj[v] = one;
      }
      else{
        it->second.push_back(t);
      }
    }
  }
}

void buildEdgeToTriangle(GFace *gf, e2t_cont &adj)
{
  buildEdgeToTriangle(gf->triangles, adj);
}

void buildEdgeToTriangle(std::vector<MTriangle*> &triangles, e2t_cont &adj)
{
  adj.clear();
  for (unsigned int i = 0; i < triangles.size(); i++){
    MTriangle *t = triangles[i];
    for (unsigned int j = 0; j < 3; j++){
      MVertex *v1 = t->getVertex(j);
      MVertex *v2 = t->getVertex((j + 1) % 3);
      MEdge e(v1, v2);
      e2t_cont::iterator it = adj.find(e);
      if (it == adj.end()){
        std::pair<MTriangle*, MTriangle*> one = std::make_pair(t, (MTriangle*)0);
        adj[e] = one;
      }
      else
        {
          it->second.second = t;
        }
    }
  }
}

void parametricCoordinates(MTriangle *t, GFace *gf, double u[3], double v[3])
{
  for (unsigned int j = 0; j < 3; j++){
    MVertex *ver = t->getVertex(j);
    parametricCoordinates(ver, gf, u[j], v[j]);
  }
}

void laplaceSmoothing(GFace *gf)
{
  v2t_cont adj;
  buildVertexToTriangle(gf->triangles, adj);

  for (int i = 0; i < 5; i++){
    v2t_cont :: iterator it = adj.begin();
    while (it != adj.end()){
      MVertex *ver= it->first;
      GEntity *ge = ver->onWhat();
      // this vertex in internal to the face
      if (ge->dim() == 2){
        double initu,initv;
        ver->getParameter(0, initu);
        ver->getParameter(1, initv);
        const std::vector<MTriangle*> &lt = it->second;
        double fact = lt.size() ? 1. / (3. * lt.size()) : 0;
        double cu = 0, cv = 0;
        double pu[3], pv[3];
        for (unsigned int i = 0; i < lt.size(); i++){
          parametricCoordinates(lt[i], gf, pu, pv);
          cu += fact * (pu[0] + pu[1] + pu[2]);
          cv += fact * (pv[0] + pv[1] + pv[2]);
          // have to test validity !
        }
        ver->setParameter(0, cu);
        ver->setParameter(1, cv);
        GPoint pt = gf->point(SPoint2(cu, cv));
        ver->x() = pt.x();
        ver->y() = pt.y();
        ver->z() = pt.z();
      }
      ++it;
    }  
  }
}

extern void fourthPoint(double *p1, double *p2, double *p3, double *p4);

double surfaceTriangleUV(MVertex *v1, MVertex *v2, MVertex *v3,           
                         const std::vector<double> &Us,
                         const std::vector<double> &Vs)
{
  const double v12[2] = {Us[v2->getNum()] - Us[v1->getNum()],
                         Vs[v2->getNum()] - Vs[v1->getNum()]};
  const double v13[2] = {Us[v3->getNum()] - Us[v1->getNum()],
                         Vs[v3->getNum()] - Vs[v1->getNum()]};
  return 0.5 * fabs (v12[0] * v13[1] - v12[1] * v13[0]);
}

bool gmshEdgeSwap(std::set<swapquad> &configs, MTri3 *t1, GFace *gf, int iLocalEdge,
                  std::vector<MTri3*> &newTris, const gmshSwapCriterion &cr,               
                  const std::vector<double> &Us, const std::vector<double> &Vs,
                  const std::vector<double> &vSizes, const std::vector<double> &vSizesBGM)
{
  MTri3 *t2 = t1->getNeigh(iLocalEdge);
  if (!t2) return false;

  MVertex *v1 = t1->tri()->getVertex(iLocalEdge == 0 ? 2 : iLocalEdge - 1);
  MVertex *v2 = t1->tri()->getVertex((iLocalEdge) % 3);
  MVertex *v3 = t1->tri()->getVertex((iLocalEdge + 1) % 3);
  MVertex *v4 = 0;
  for (int i = 0; i < 3; i++)
    if (t2->tri()->getVertex(i) != v1 && t2->tri()->getVertex(i) != v2)
      v4 = t2->tri()->getVertex(i);
  
  swapquad sq (v1, v2, v3, v4);
  if (configs.find(sq) != configs.end()) return false;
  configs.insert(sq);

  const double volumeRef = surfaceTriangleUV(v1, v2, v3, Us, Vs) + 
    surfaceTriangleUV(v1, v2, v4, Us, Vs);

  MTriangle *t1b = new MTriangle(v2, v3, v4);  
  MTriangle *t2b = new MTriangle(v4, v3, v1); 
  const double v1b = surfaceTriangleUV(v2, v3, v4, Us, Vs);
  const double v2b = surfaceTriangleUV(v4, v3, v1, Us, Vs);
  const double volume = v1b + v2b;
  if (fabs(volume - volumeRef) > 1.e-10 * (volume + volumeRef) || 
      v1b < 1.e-8 * (volume + volumeRef) ||
      v2b < 1.e-8 * (volume + volumeRef)){
    delete t1b;
    delete t2b;
    return false;
  }
  
  switch(cr){
  case SWCR_QUAL:
    {
      const double triQualityRef = std::min(qmTriangle(t1->tri(), QMTRI_RHO),
                                            qmTriangle(t2->tri(), QMTRI_RHO));
      const double triQuality = std::min(qmTriangle(t1b, QMTRI_RHO),
                                         qmTriangle(t2b, QMTRI_RHO));
      if (triQuality < triQualityRef){
        delete t1b;
        delete t2b;
        return false;
      }
      break;
    }
  case SWCR_DEL:
    {
      double edgeCenter[2] ={(Us[v1->getNum()] + Us[v2->getNum()] + Us[v3->getNum()] + 
                              Us[v4->getNum()]) * .25,
                             (Vs[v1->getNum()] + Vs[v2->getNum()] + Vs[v3->getNum()] + 
                              Vs[v4->getNum()]) * .25};
      double uv4[2] ={Us[v4->getNum()], Vs[v4->getNum()]};
      double metric[3];
      buildMetric(gf, edgeCenter, metric);
      if (!inCircumCircleAniso(gf, t1->tri(), uv4, metric, Us, Vs)){
        delete t1b;
        delete t2b;
        return false;
      }      
    }
    break;
  case SWCR_CLOSE:
    {
      double avg1[3] = {(v1->x() + v2->x()) *.5,(v1->y() + v2->y()) *.5,
                        (v1->z() + v2->z()) *.5};
      double avg2[3] = {(v3->x() + v4->x()) *.5,(v3->y() + v4->y()) *.5,
                        (v3->z() + v4->z()) *.5};
      
      GPoint gp1 = gf->point(SPoint2((Us[v1->getNum()] + Us[v2->getNum()]) * .5,
                                     (Vs[v1->getNum()] + Vs[v2->getNum()]) * .5));
      GPoint gp2 = gf->point(SPoint2((Us[v3->getNum()] + Us[v4->getNum()]) * .5,
                                     (Vs[v3->getNum()] + Vs[v4->getNum()]) * .5));
      double d1 = (avg1[0] - gp1.x()) * (avg1[0] - gp1.x()) + (avg1[1] - gp1.y()) * 
        (avg1[1]-gp1.y()) + (avg1[2] - gp1.z()) * (avg1[2] - gp1.z());
      double d2 = (avg2[0] - gp2.x()) * (avg2[0] - gp2.x()) + (avg2[1] - gp2.y()) *
        (avg2[1] - gp2.y()) + (avg2[2] - gp2.z()) * (avg2[2] - gp2.z());
      if (d1 < d2){
        delete t1b;
        delete t2b;
        return false;
      }      
    }
    break;
  default :
    Msg::Error("Unknown swapping criterion");
    return false;
  }

  std::list<MTri3*> cavity;
  for(int i = 0; i < 3; i++){    
    if (t1->getNeigh(i) && t1->getNeigh(i) != t2){      
      bool found = false;
      for (std::list<MTri3*>::iterator it = cavity.begin(); it != cavity.end(); it++){
        if (*it == t1->getNeigh(i)) found = true;
      }
      if (!found)cavity.push_back(t1->getNeigh(i));
    }
  }
  for(int i = 0; i < 3; i++){    
    if (t2->getNeigh(i) && t2->getNeigh(i) != t1){      
      bool found = false;
      for (std::list<MTri3*>::iterator it = cavity.begin(); it != cavity.end(); it++){
        if (*it == t2->getNeigh(i)) found = true;
      }
      if (!found)cavity.push_back(t2->getNeigh(i));
    }
  }
  double lc1 = 0.3333333333 * (vSizes[t1b->getVertex(0)->getNum()] +
                               vSizes[t1b->getVertex(1)->getNum()] +
                               vSizes[t1b->getVertex(2)->getNum()]);
  double lcBGM1 = 0.3333333333 * (vSizesBGM[t1b->getVertex(0)->getNum()] +
                                  vSizesBGM[t1b->getVertex(1)->getNum()] +
                                  vSizesBGM[t1b->getVertex(2)->getNum()]);
  double lc2 = 0.3333333333 * (vSizes[t2b->getVertex(0)->getNum()] +
                               vSizes[t2b->getVertex(1)->getNum()] +
                               vSizes[t2b->getVertex(2)->getNum()]);
  double lcBGM2 = 0.3333333333 * (vSizesBGM[t2b->getVertex(0)->getNum()] +
                                  vSizesBGM[t2b->getVertex(1)->getNum()] +
                                  vSizesBGM[t2b->getVertex(2)->getNum()]);
  MTri3 *t1b3 = new MTri3(t1b, Extend1dMeshIn2dSurfaces() ? 
                          std::min(lc1, lcBGM1) : lcBGM1);
  MTri3 *t2b3 = new MTri3(t2b, Extend1dMeshIn2dSurfaces() ?
                          std::min(lc2, lcBGM2) : lcBGM2);

  cavity.push_back(t1b3);
  cavity.push_back(t2b3);
  t1->setDeleted(true);
  t2->setDeleted(true);
  connectTriangles(cavity);
  newTris.push_back(t1b3);
  newTris.push_back(t2b3);

  return true;
}

inline double computeEdgeAdimLength(MVertex *v1, MVertex *v2, GFace *f,
                                    const std::vector<double> &Us,
                                    const std::vector<double> &Vs,
                                    const std::vector<double> &vSizes ,
                                    const std::vector<double> &vSizesBGM)
{  
  const double edgeCenter[2] ={(Us[v1->getNum()] + Us[v2->getNum()]) * .5,
                               (Vs[v1->getNum()] + Vs[v2->getNum()]) * .5};
  GPoint GP = f->point (edgeCenter[0], edgeCenter[1]);
  
  const double dx1 = v1->x() - GP.x();
  const double dy1 = v1->y() - GP.y();
  const double dz1 = v1->z() - GP.z();
  const double l1 = sqrt(dx1 * dx1 + dy1 * dy1 + dz1 * dz1);
  const double dx2 = v2->x() - GP.x();
  const double dy2 = v2->y() - GP.y();
  const double dz2 = v2->z() - GP.z();
  const double l2 = sqrt(dx2 * dx2 + dy2 * dy2 + dz2 * dz2);
  if (Extend1dMeshIn2dSurfaces())
    return 2 * (l1 + l2) / (std::min(vSizes[v1->getNum()], vSizesBGM[v1->getNum()]) +
                            std::min(vSizes[v2->getNum()], vSizesBGM[v2->getNum()]));
  return 2 * (l1 + l2) / (vSizesBGM[v1->getNum()] + vSizesBGM[v2->getNum()]);
}

bool gmshEdgeSplit(const double lMax, MTri3 *t1, GFace *gf, int iLocalEdge,
                   std::vector<MTri3*> &newTris, const gmshSplitCriterion &cr,             
                   std::vector<double> &Us, std::vector<double> &Vs,
                   std::vector<double> &vSizes, std::vector<double> &vSizesBGM)
{
  MTri3 *t2 = t1->getNeigh(iLocalEdge);
  if (!t2) return false;

  MVertex *v1 = t1->tri()->getVertex(iLocalEdge == 0 ? 2 : iLocalEdge - 1);
  MVertex *v2 = t1->tri()->getVertex(iLocalEdge % 3);
  double edgeCenter[2] ={(Us[v1->getNum()] + Us[v2->getNum()]) * .5,
                         (Vs[v1->getNum()] + Vs[v2->getNum()]) * .5};
 
  double al = computeEdgeAdimLength(v1, v2, gf,Us, Vs, vSizes, vSizesBGM);
  if (al <= lMax) return false;

  MVertex *v3 = t1->tri()->getVertex((iLocalEdge + 1) % 3);
  MVertex *v4 = 0 ;
  for (int i = 0; i < 3; i++)
    if (t2->tri()->getVertex(i) != v1 && t2->tri()->getVertex(i) != v2)
      v4 = t2->tri()->getVertex(i);

  GPoint p = gf->point(edgeCenter[0], edgeCenter[1]);
  MVertex *vnew = new MFaceVertex(p.x(), p.y(), p.z(), gf, 
                                  edgeCenter[0], edgeCenter[1]);

  MTriangle *t1b = new MTriangle(v1, vnew, v3);  
  MTriangle *t2b = new MTriangle(vnew, v2, v3); 
  MTriangle *t3b = new MTriangle(v1, v4, vnew); 
  MTriangle *t4b = new MTriangle(v4, v2, vnew); 
  
  switch(cr){
  case SPCR_QUAL:
    {
      const double triQualityRef = std::min(qmTriangle(t1->tri(), QMTRI_RHO),
                                            qmTriangle(t2->tri(), QMTRI_RHO));
      const double triQuality = 
        std::min(std::min(std::min(qmTriangle(t1b, QMTRI_RHO),
                                   qmTriangle(t2b, QMTRI_RHO)),
                          qmTriangle(t3b, QMTRI_RHO)), 
                 qmTriangle(t4b, QMTRI_RHO));
      if (triQuality < triQualityRef){
        delete t1b;
        delete t2b;
        delete t3b;
        delete t4b;
        delete vnew;
        return false;
      }
      break;
    }
  case SPCR_ALLWAYS:
    break;
  default :
    Msg::Error("Unknown swapping criterion");
    return false;
  }

  gf->mesh_vertices.push_back(vnew);
  vnew->setNum(Us.size());
  double lcN = 0.5 * (vSizes [v1->getNum()] + vSizes [v2->getNum()] );
  double lcBGM =  BGM_MeshSize(gf, edgeCenter[0], edgeCenter[1], p.x(), p.y(), p.z());
  
  vSizesBGM.push_back(lcBGM);
  vSizes.push_back(lcN);
  Us.push_back(edgeCenter[0]);
  Vs.push_back(edgeCenter[1]);

  std::list<MTri3*> cavity;
  for(int i = 0; i < 3; i++){    
    if (t1->getNeigh(i) && t1->getNeigh(i) != t2){      
      bool found = false;
      for (std::list<MTri3*>::iterator it = cavity.begin();it != cavity.end(); it++){
        if (*it == t1->getNeigh(i)) found = true;
      }
      if (!found) cavity.push_back(t1->getNeigh(i));
    }
  }
  for(int i = 0; i < 3; i++){
    if (t2->getNeigh(i) && t2->getNeigh(i) != t1){      
      bool found = false;
      for (std::list<MTri3*>::iterator it = cavity.begin(); it != cavity.end(); it++){
        if (*it == t2->getNeigh(i))found = true;
      }
      if (!found) cavity.push_back(t2->getNeigh(i));
    }
  }
  double lc1 = 0.3333333333 * (vSizes[t1b->getVertex(0)->getNum()] +
                               vSizes[t1b->getVertex(1)->getNum()] +
                               vSizes[t1b->getVertex(2)->getNum()]);
  double lcBGM1 = 0.3333333333 * (vSizesBGM[t1b->getVertex(0)->getNum()] +
                                  vSizesBGM[t1b->getVertex(1)->getNum()] +
                                  vSizesBGM[t1b->getVertex(2)->getNum()]);
  double lc2 = 0.3333333333 * (vSizes[t2b->getVertex(0)->getNum()] +
                               vSizes[t2b->getVertex(1)->getNum()] +
                               vSizes[t2b->getVertex(2)->getNum()]);
  double lcBGM2 = 0.3333333333 * (vSizesBGM[t2b->getVertex(0)->getNum()] +
                                  vSizesBGM[t2b->getVertex(1)->getNum()] +
                                  vSizesBGM[t2b->getVertex(2)->getNum()]);
  double lc3 = 0.3333333333 * (vSizes[t3b->getVertex(0)->getNum()] +
                               vSizes[t3b->getVertex(1)->getNum()] +
                               vSizes[t3b->getVertex(2)->getNum()]);
  double lcBGM3 = 0.3333333333 * (vSizesBGM[t3b->getVertex(0)->getNum()] +
                                  vSizesBGM[t3b->getVertex(1)->getNum()] +
                                  vSizesBGM[t3b->getVertex(2)->getNum()]);
  double lc4 = 0.3333333333 * (vSizes[t4b->getVertex(0)->getNum()] +
                               vSizes[t4b->getVertex(1)->getNum()] +
                               vSizes[t4b->getVertex(2)->getNum()]);
  double lcBGM4 = 0.3333333333 * (vSizesBGM[t4b->getVertex(0)->getNum()] +
                                  vSizesBGM[t4b->getVertex(1)->getNum()] +
                                  vSizesBGM[t4b->getVertex(2)->getNum()]);
  MTri3 *t1b3 = new MTri3(t1b, Extend1dMeshIn2dSurfaces() ? std::min(lc1, lcBGM1) : lcBGM1);
  MTri3 *t2b3 = new MTri3(t2b, Extend1dMeshIn2dSurfaces() ? std::min(lc2, lcBGM2) : lcBGM2);
  MTri3 *t3b3 = new MTri3(t3b, Extend1dMeshIn2dSurfaces() ? std::min(lc3, lcBGM3) : lcBGM3);
  MTri3 *t4b3 = new MTri3(t4b, Extend1dMeshIn2dSurfaces() ? std::min(lc4, lcBGM4) : lcBGM4);

  cavity.push_back(t1b3);
  cavity.push_back(t2b3);
  cavity.push_back(t3b3);
  cavity.push_back(t4b3);
  t1->setDeleted(true);
  t2->setDeleted(true);
  connectTriangles(cavity);
  newTris.push_back(t1b3);
  newTris.push_back(t2b3);
  newTris.push_back(t3b3);
  newTris.push_back(t4b3);

  return true;
}

void computeNeighboringTrisOfACavity(const std::vector<MTri3*> &cavity,
                                     std::vector<MTri3*> &outside)
{
  outside.clear();
  for (unsigned int i = 0; i < cavity.size(); i++){
    for (int j = 0; j < 3; j++){
      MTri3 * neigh = cavity[i]->getNeigh(j);
      if(neigh){
        bool found = false;
        for(unsigned int k = 0; k < outside.size(); k++){
          if(outside[k] == neigh){
            found = true;
            break;
          }
        }
        if(!found){
          for (unsigned int k = 0; k < cavity.size(); k++){
            if(cavity[k] == neigh){
              found = true;
            }
          }
        }
        if(!found) outside.push_back(neigh);
      }
    }
  }
}
        
bool gmshBuildVertexCavity(MTri3 *t, int iLocalVertex, MVertex **v1,
                           std::vector<MTri3*> &cavity, std::vector<MTri3*> &outside,
                           std::vector<MVertex*> &ring)
{
  cavity.clear();
  ring.clear();

  *v1 = t->tri()->getVertex(iLocalVertex);

  MVertex *lastinring = t->tri()->getVertex((iLocalVertex + 1) % 3);
  ring.push_back(lastinring);
  cavity.push_back(t);

  while (1){
    int iEdge = -1;
    for (int i = 0; i < 3; i++){
      MVertex *v2  = t->tri()->getVertex((i + 2) % 3);
      MVertex *v3  = t->tri()->getVertex(i);
      if ((v2 == *v1 && v3 == lastinring) ||
          (v2 == lastinring && v3 == *v1)){
        iEdge = i;
        t = t->getNeigh(i);
        if (t == cavity[0]) {
          computeNeighboringTrisOfACavity(cavity, outside);
          return true;
        }
        if (!t) return false;
        if (t->isDeleted()){ 
	  Msg::Error("Impossible to build vertex cavity");
	  return false;
	}  
        cavity.push_back(t);
        for (int j = 0; j < 3; j++){
          if (t->tri()->getVertex(j) !=lastinring && t->tri()->getVertex(j) != *v1){
            lastinring = t->tri()->getVertex(j);
            ring.push_back(lastinring);
            j = 100;
          }
        }
        break;
      }
    }
    if (iEdge == -1) {
      Msg::Error("Impossible to build vertex cavity");
      return false;
    }
  }
}

bool gmshVertexCollapse(const double lMin, MTri3 *t1, GFace *gf,
                        int iLocalVertex, std::vector<MTri3*> &newTris,
                        std::vector<double> &Us, std::vector<double> &Vs,
                        std::vector<double> &vSizes, std::vector<double> &vSizesBGM)
{
  MVertex *v;
  std::vector<MTri3*> cavity;
  std::vector<MTri3*> outside;
  std::vector<MVertex*> ring ;

  if (!gmshBuildVertexCavity(t1, iLocalVertex, &v, cavity, outside, ring)) return false;
  
  double l_min = lMin;
  int iMin = -1;
  for (unsigned int i = 0; i < ring.size(); i++){
    double l = computeEdgeAdimLength(v, ring[i], gf, Us, Vs, vSizes, vSizesBGM);
    if (l < l_min){
      iMin = i;
    }
  }
  if(iMin == -1) return false;

  double surfBefore = 0.0;
  for(unsigned int i = 0; i < ring.size(); i++){
    MVertex *v1 = ring[i];
    MVertex *v2 = ring[(i + 1) % ring.size()];
    surfBefore += surfaceTriangleUV(v1, v2, v, Us, Vs);
  }

  double surfAfter = 0.0;
  for(unsigned int i = 0; i < ring.size() - 2; i++){
    MVertex *v1 = ring[(iMin + 1 + i) % ring.size()];
    MVertex *v2 = ring[(iMin + 1 + i + 1) % ring.size()];
    double sAfter = surfaceTriangleUV(v1, v2, ring[iMin], Us, Vs);
    double sBefore = surfaceTriangleUV(v1, v2, v, Us, Vs);
    if(sAfter < 0.1 * sBefore) return false;
    surfAfter += sAfter;
  }

  //  printf("%12.5E %12.5E %d\n",surfBefore,surfAfter,iMin);
  if(fabs(surfBefore - surfAfter) > 1.e-10*(surfBefore + surfAfter)) return false;

  for(unsigned int i = 0; i < ring.size() - 2; i++){
    MVertex *v1 = ring[(iMin + 1 + i) % ring.size()];
    MVertex *v2 = ring[(iMin + 1 + i + 1) % ring.size()];
    MTriangle *t = new MTriangle(v1,v2,ring[iMin]);
    double lc = 0.3333333333 * (vSizes[t->getVertex(0)->getNum()] +
                                vSizes[t->getVertex(1)->getNum()] +
                                vSizes[t->getVertex(2)->getNum()]);
    double lcBGM = 0.3333333333 * (vSizesBGM[t->getVertex(0)->getNum()] +
                                   vSizesBGM[t->getVertex(1)->getNum()] +
                                   vSizesBGM[t->getVertex(2)->getNum()]);
    MTri3 *t3 = new MTri3(t, Extend1dMeshIn2dSurfaces() ? std::min(lc, lcBGM) : lcBGM); 
    // printf("Creation %p = %d %d %d\n",t3,v1->getNum(),v2->getNum(),ring[iMin]->getNum());
    outside.push_back(t3);
    newTris.push_back(t3);
  }
  for(unsigned int i = 0; i < cavity.size(); i++)
    cavity[i]->setDeleted(true);
  connectTriangles(outside);
  return true;
}

int edgeSwapPass (GFace *gf, std::set<MTri3*, compareTri3Ptr> &allTris,
                  const gmshSwapCriterion &cr,
                  const std::vector<double> &Us, const std::vector<double> &Vs,
                  const std::vector<double> &vSizes, const std::vector<double> &vSizesBGM)
{
  typedef std::set<MTri3*, compareTri3Ptr> CONTAINER ;

  int nbSwapTot = 0;
  std::set<swapquad> configs;
  for (int iter = 0; iter < 1200; iter++){
    int nbSwap = 0;
    std::vector<MTri3*> newTris;
    for (CONTAINER::iterator it = allTris.begin(); it != allTris.end(); ++it){
      if (!(*it)->isDeleted()){
        for (int i = 0; i < 3; i++){
          if (gmshEdgeSwap(configs, *it, gf, i, newTris, cr, Us, Vs, vSizes, vSizesBGM)){
            nbSwap++;
            break;
          }
        }
      }
      else{
        delete *it;
        CONTAINER::iterator itb = it;
        ++it;
        allTris.erase(itb);
        if (it == allTris.end()) break;
      }
    }
    allTris.insert(newTris.begin(), newTris.end());
    nbSwapTot += nbSwap;
    if (nbSwap == 0) break;
  }  
  return nbSwapTot;
}

int edgeSplitPass(double maxLC, GFace *gf, std::set<MTri3*,compareTri3Ptr> &allTris,
                  const gmshSplitCriterion &cr,
                  std::vector<double> &Us, std::vector<double> &Vs,
                  std::vector<double> &vSizes, std::vector<double> &vSizesBGM)
{
  typedef std::set<MTri3*, compareTri3Ptr> CONTAINER ;
  std::vector<MTri3*> newTris;

  int nbSplit = 0;
  
  for (CONTAINER::iterator it = allTris.begin(); it != allTris.end(); ++it){
    if (!(*it)->isDeleted()){
      for (int i = 0; i < 3; i++){
        if (gmshEdgeSplit(maxLC, *it, gf, i, newTris, cr, Us, Vs, vSizes, vSizesBGM)) {
          nbSplit++;
          break;
        }
      }
    }
     else{
       CONTAINER::iterator itb = it;
       delete *it;
       ++it;
       allTris.erase(itb);
       if (it == allTris.end()) break;
     }
  }  
  printf("B %d %d tris ", (int)allTris.size(), (int)newTris.size());
  allTris.insert(newTris.begin(),newTris.end());
  printf("A %d %d tris\n", (int)allTris.size(), (int)newTris.size());
  return nbSplit;
}

int edgeCollapsePass(double minLC, GFace *gf, std::set<MTri3*,compareTri3Ptr> &allTris,
                     std::vector<double> &Us, std::vector<double> &Vs,
                     std::vector<double> &vSizes, std::vector<double> &vSizesBGM)
{
  typedef std::set<MTri3*,compareTri3Ptr> CONTAINER ;
  std::vector<MTri3*> newTris;

  int nbCollapse = 0;
  
  for (CONTAINER::reverse_iterator it = allTris.rbegin(); it != allTris.rend(); ++it){
    if (!(*it)->isDeleted()){
      for (int i = 0; i < 3; i++){
        if (gmshVertexCollapse(minLC, *it, gf, i, newTris, Us, Vs, vSizes, vSizesBGM)) {
          nbCollapse++;
          break;
        }
      }
    }
//      else{
//        CONTAINER::reverse_iterator itb = it;
//        delete *it;
//        ++it;
//        allTris.rerase(itb);
//         if (it == allTris.rend())break;
//      }
//     if (nbCollapse == 114)break;
  }  
  printf("B %d %d tris ", (int)allTris.size(), (int)newTris.size());
  allTris.insert(newTris.begin(),newTris.end());
  printf("A %d %d tris\n", (int)allTris.size(), (int)newTris.size());
  return nbCollapse;
}
