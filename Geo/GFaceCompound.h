// Gmsh - Copyright (C) 1997-2012 C. Geuzaine, J.-F. Remacle
//
// See the LICENSE.txt file for license information. Please report all
// bugs and problems to <gmsh@geuz.org>.

#ifndef _GFACE_COMPOUND_H_
#define _GFACE_COMPOUND_H_

#include <list>
#include <map>
#include "GmshConfig.h"
#include "GmshMessage.h"
#include "GFace.h"
template <class scalar> class simpleFunction;

#if defined(HAVE_SOLVER)

#include "GEdge.h"
#include "GEdgeCompound.h"
#include "meshGFaceOptimize.h"
#include "linearSystem.h"
#include "GRbf.h"

#define AR_MAX 5 //maximal geometrical aspect ratio

/*
A GFaceCompound is a model face that is the compound of model faces.

It is assumed that all the faces of the compound have been meshed
first. We use this discretization to solve elliptic problems on the
compound. Those problems enable to compute the parametric
coordinates of the mesh points. The parametrization of the compound
consist in a triangulation in the (u,v) space with parameter values at
nodes.

The compound can therefore be (re)-meshed using any surface mesh
generator of gmsh!
*/

class  GFaceCompoundTriangle {
 public:
  SPoint3 p1, p2, p3;
  SPoint2 gfp1, gfp2, gfp3;
  SPoint3 v1, v2, v3;
  GFace *gf;
  MTriangle *tri;
  GFaceCompoundTriangle() : gf(0), tri(0) {}
};

class Octree;
class GRbf;

class GFaceCompound : public GFace {
 public:
  typedef enum {ITERU=0,ITERV=1,ITERD=2} iterationStep;
  typedef enum {HARMONIC_CIRCLE=0, CONFORMAL_SPECTRAL=1, RADIAL_BASIS=2, HARMONIC_PLANE=3, 
	       CONVEX_CIRCLE=4,CONVEX_PLANE=5, HARMONIC_SQUARE=6} typeOfCompound;
  typedef enum {HARMONIC=0,CONFORMAL=1, RBF=2, CONVEX=3} typeOfMapping;
  typedef enum {UNITCIRCLE, MEANPLANE, SQUARE, ALREADYFIXED} typeOfIsomorphism;
  void computeNormals(std::map<MVertex*, SVector3> &normals) const;
 protected:
  mutable std::set<MVertex *> ov;
  mutable GRbf *_rbf;
  simpleFunction<double> *ONE;
  simpleFunction<double> *MONE;
  std::list<GFace*> _compound;
  std::list<GEdge*> _U0, _V0, _U1, _V1;
  std::list<std::list<GEdge*> > _interior_loops;
  mutable int nbT;
  mutable GFaceCompoundTriangle *_gfct;
  mutable Octree *oct;
  mutable std::set<MVertex*> allNodes;
  mutable v2t_cont adjv;
  mutable bool mapv2Tri;
  mutable std::map<MVertex*, SPoint3> coordinates;
  mutable std::map<SPoint3,SPoint3 > _coordPoints;
  mutable std::map<MVertex*, SVector3> _normals;
  mutable std::list<MTriangle*> fillTris;
  mutable std::set<MVertex*> fillNodes;
  mutable std::vector<MVertex*> _ordered;
  mutable std::vector<double> _coords;
  mutable std::map<MVertex*, int> _mapV;
  linearSystem <double> *_lsys;
  void buildOct() const ;
  void buildAllNodes() const; 
  void parametrize(iterationStep, typeOfMapping) const;
  bool parametrize_conformal(int iter, MVertex *v1, MVertex *v2) const;
  bool parametrize_conformal_spectral() const;
  void compute_distance() const;
  bool checkOrientation(int iter, bool moveBoundaries=false) const;
  bool checkOverlap(std::vector<MVertex *> &vert) const;
  void one2OneMap(bool moveBoundaries=false) const;
  void convexBoundary(double nTot) const;
  double checkAspectRatio() const;
  void computeNormals () const;
  void getBoundingEdges();
  void getUniqueEdges(std::set<GEdge*> &_unique); 
  void computeALoop(std::set<GEdge*> &_unique, std::list<GEdge*> &);
  void getTriangle(double u, double v, GFaceCompoundTriangle **lt, 
                   double &_u, double &_v) const;
  virtual double locCurvature(MTriangle *t, double u, double v) const;
  void printStuff(int iNewton=0) const;
  bool trivial() const;
  double getSizeH() const;
  double getSizeBB(const std::list<GEdge* > &elist) const;
  void fillNeumannBCS() const;
  /* double sumAngles(std::vector<MVertex*> ordered) const; */
 
 public: 
  GFaceCompound(GModel *m, int tag, std::list<GFace*> &compound,
		std::list<GEdge*> &U0, typeOfCompound typ = HARMONIC_CIRCLE,
		int allowPartition=1, 
		linearSystem<double>* lsys =0);
 GFaceCompound(GModel *m, int tag, std::list<GFace*> &compound,
	       std::list<GEdge*> &U0, std::list<GEdge*> &V0,
	       std::list<GEdge*> &U1, std::list<GEdge*> &V1,
	       typeOfCompound typ = HARMONIC_CIRCLE,
	       int allowPartition=1, 
	       linearSystem<double>* lsys =0);
  virtual ~GFaceCompound();
  Range<double> parBounds(int i) const 
  { return trivial() ? (*(_compound.begin()))->parBounds(i) : Range<double>(-1, 1); }
  virtual GPoint point(double par1, double par2) const; 
  typeOfCompound getTypeOfCompound() { return _toc;}
  SPoint2 parFromPoint(const SPoint3 &p, bool onSurface=true) const;
  virtual Pair<SVector3,SVector3> firstDer(const SPoint2 &param) const;
  virtual void secondDer(const SPoint2 &, SVector3 *, SVector3 *, SVector3 *) const; 
  virtual GEntity::GeomType geomType() const { return CompoundSurface; }
  ModelType getNativeType() const { return GmshModel; }
  void * getNativePtr() const { return 0; }
  virtual SPoint2 getCoordinates(MVertex *v) const;
  virtual double curvatureMax(const SPoint2 &param) const;
  virtual double curvatures(const SPoint2 &param, SVector3 *dirMax, SVector3 *dirMin,
   double *curvMax, double *curvMin) const;
  virtual int genusGeom () const;
  virtual bool checkTopology() const;
  bool parametrize() const ;
  void coherenceNormals();
  void coherencePatches() const;
  virtual std::list<GFace*> getCompounds() const { return _compound; }
  mutable int nbSplit;
  int getNbSplit() const { return nbSplit; }
  int allowPartition() const{ return _allowPartition; }
  void setType(typeOfIsomorphism type){ _type=type;}
  // useful for mesh generators ----------------------------------------
  GPoint intersectionWithCircle (const SVector3 &n1, const SVector3 &n2, const SVector3 &p, const double &d, double uv[2]) const;
 private:
  mutable typeOfCompound _toc;
  mutable typeOfMapping _mapping;
  mutable typeOfIsomorphism _type;
  int _allowPartition;
};

#else

//define empty class ifndef HAVE_SOLVER
template<class scalar> class linearSystem;
class GFaceCompound : public GFace {
 public:
  typedef enum {ITERU=0,ITERV=1,ITERD=2} iterationStep;
  typedef enum {HARMONIC_CIRCLE=0, CONFORMAL_SPECTRAL=1, RADIAL_BASIS=2, HARMONIC_PLANE=3, 
	       CONVEX_CIRCLE=4,CONVEX_PLANE=5, HARMONIC_SQUARE=6} typeOfCompound;
  typedef enum {HARMONIC=0,CONFORMAL=1, RBF=2, CONVEX=3} typeOfMapping;
  typedef enum {UNITCIRCLE, MEANPLANE, SQUARE, ALREADYFIXED} typeOfIsomorphism;
 GFaceCompound(GModel *m, int tag, std::list<GFace*> &compound,
	       std::list<GEdge*> &U0, typeOfMapping typ = HARMONIC, 
	       int allowPartition=1, 
	       linearSystem<double>* lsys =0)
    : GFace(m, tag)
  {
    Msg::Error("Gmsh has to be compiled with solver support to use GFaceCompounds");
  }
 GFaceCompound(GModel *m, int tag, std::list<GFace*> &compound,
	       std::list<GEdge*> &U0, std::list<GEdge*> &V0,
	       std::list<GEdge*> &U1, std::list<GEdge*> &V1,
	       typeOfCompound typ = HARMONIC_CIRCLE, 
	       int allowPartition=1, 
	       linearSystem<double>* lsys =0)
    : GFace(m, tag)
  {
    Msg::Error("Gmsh has to be compiled with solver support to use GFaceCompounds");
  }
  virtual ~GFaceCompound() {}
  GPoint point(double par1, double par2) const { return GPoint(); }
  Pair<SVector3, SVector3> firstDer(const SPoint2 &param) const
  {
    return Pair<SVector3, SVector3>(SVector3(0, 0, 0), SVector3(0, 0, 0));
  }
  void secondDer(const SPoint2 &param, 
                         SVector3 *dudu, SVector3 *dvdv, SVector3 *dudv) const{}
  virtual SPoint2 getCoordinates(MVertex *v) const { return SPoint2(); }
  void parametrize() const {}
};

#endif

#endif
