// Gmsh - Copyright (C) 1997-2009 C. Geuzaine, J.-F. Remacle
//
// See the LICENSE.txt file for license information. Please report all
// bugs and problems to <gmsh@geuz.org>.

#ifndef _MELEMENT_H_
#define _MELEMENT_H_

#include <stdio.h>
#include <algorithm>
#include <string>
#include "GmshDefines.h"
#include "GmshMessage.h"
#include "MVertex.h"
#include "MEdge.h"
#include "MFace.h"
#include "functionSpace.h"
#include "Gauss.h"

class GFace;

// A mesh element.
class MElement 
{
 private:
  // the maximum element id number in the mesh
  static int _globalNum;
  // the id number of the element (this number is unique and is
  // guaranteed never to change once a mesh has been generated)
  int _num;
  // the number of the mesh partition the element belongs to
  short _partition;
  // a visibility flag
  char _visible;
 protected:
  // the tolerance used to determine if a point is inside an element,
  // in parametric coordinates
  static double _isInsideTolerance;
  void _getEdgeRep(MVertex *v0, MVertex *v1, 
                   double *x, double *y, double *z, SVector3 *n,
                   int faceIndex=-1);
  void _getFaceRep(MVertex *v0, MVertex *v1, MVertex *v2, 
                   double *x, double *y, double *z, SVector3 *n);
 public :
  MElement(int num=0, int part=0);
  virtual ~MElement(){}

  // reset the global node number
  static int getGlobalNumber(){ return _globalNum; }
  static void resetGlobalNumber(){ _globalNum = 0; }

  // set/get the tolerance for isInside() test
  static void setTolerance (const double tol){ _isInsideTolerance = tol; }
  static double getTolerance () { return _isInsideTolerance; }

  // return the tag of the element
  virtual int getNum(){ return _num; }

  // return the geometrical dimension of the element
  virtual int getDim() = 0;

  // return the polynomial order the element
  virtual int getPolynomialOrder() const { return 1; }

  // get/set the partition to which the element belongs
  virtual int getPartition(){ return _partition; }
  virtual void setPartition(int num){ _partition = (short)num; }

  // get/set the visibility flag
  virtual char getVisibility();
  virtual void setVisibility(char val){ _visible = val; }

  // get the vertices
  virtual int getNumVertices() const = 0;
  virtual MVertex *getVertex(int num) = 0;

  // get the vertex using the I-deas UNV ordering
  virtual MVertex *getVertexUNV(int num){ return getVertex(num); }

  // get the vertex using the VTK ordering
  virtual MVertex *getVertexVTK(int num){ return getVertex(num); }

  // get the vertex using the Nastran BDF ordering
  virtual MVertex *getVertexBDF(int num){ return getVertex(num); }

  // get the vertex using MED ordering
  virtual MVertex *getVertexMED(int num){ return getVertex(num); }

  // get the vertex using DIFF ordering
  virtual MVertex *getVertexDIFF(int num){ return getVertex(num); }

  // get the number of vertices associated with edges, faces and
  // volumes (nonzero only for higher order elements, polygons or
  // polyhedra)
  virtual int getNumEdgeVertices() const { return 0; }
  virtual int getNumFaceVertices() const { return 0; }
  virtual int getNumVolumeVertices() const { return 0; }

  // get the number of primary vertices (first-order element)
  int getNumPrimaryVertices()
  {
    return getNumVertices() - getNumEdgeVertices() - getNumFaceVertices() -
      getNumVolumeVertices();
  }

  // get the edges
  virtual int getNumEdges() = 0;
  virtual MEdge getEdge(int num) = 0;

  // get an edge representation for drawing
  virtual int getNumEdgesRep() = 0;
  virtual void getEdgeRep(int num, double *x, double *y, double *z, SVector3 *n) = 0;

  // get all the vertices on an edge
  virtual void getEdgeVertices(const int num, std::vector<MVertex*> &v) const
  {
    v.resize(0);
  }

  // get the faces
  virtual int getNumFaces() = 0;
  virtual MFace getFace(int num) = 0;

  // get a face representation for drawing
  virtual int getNumFacesRep() = 0;
  virtual void getFaceRep(int num, double *x, double *y, double *z, SVector3 *n) = 0;

  // get all the vertices on a face
  virtual void getFaceVertices(const int num, std::vector<MVertex*> &v) const
  {
    v.resize(0);
  }

  // get parent and children for hierarchial grids
  virtual MElement *getParent() const { return NULL; }
  virtual void setParent(MElement *p) {}
  virtual int getNumChildren() const { return 0; }
  virtual MElement *getChild(int i) const { return NULL; }
  virtual bool ownsParent() const { return false; }

  //get the type of the element
  virtual int getType() const = 0;

  // get the max/min edge length
  virtual double maxEdge();
  virtual double minEdge();

  // get the quality measures
  virtual double rhoShapeMeasure();
  virtual double gammaShapeMeasure(){ return 0.; }
  virtual double etaShapeMeasure(){ return 0.; }
  virtual double distoShapeMeasure(){ return 1.0; }

  // compute the barycenter
  virtual SPoint3 barycenter();

  // revert the orientation of the element
  virtual void revert(){}

  // compute and change the orientation of 3D elements to get
  // positive volume
  virtual double getVolume(){ return 0.; }
  virtual int getVolumeSign(){ return 1; }
  virtual void setVolumePositive(){ if(getVolumeSign() < 0) revert(); }
  
  // return an information string for the element
  virtual std::string getInfoString();

  // get the function space for the element
  virtual const functionSpace* getFunctionSpace(int order=-1) const { return 0; }
  
  // return the interpolating nodal shape functions evaluated at point
  // (u,v,w) in parametric coordinates (if order == -1, use the
  // polynomial order of the element)
  virtual void getShapeFunctions(double u, double v, double w, double s[],
                                 int order=-1);

  // return the gradient of of the nodal shape functions evaluated at
  // point (u,v,w) in parametric coordinates (if order == -1, use the
  // polynomial order of the element)
  virtual void getGradShapeFunctions(double u, double v, double w, double s[][3],
                                     int order=-1);
  
  // return the Jacobian of the element evaluated at point (u,v,w) in
  // parametric coordinates
  double getJacobian(double u, double v, double w, double jac[3][3]);
  double getPrimaryJacobian(double u, double v, double w, double jac[3][3]);
  
  // get the point in cartesian coordinates corresponding to the point
  // (u,v,w) in parametric coordinates
  virtual void pnt(double u, double v, double w, SPoint3 &p);
  virtual void primaryPnt(double u, double v, double w, SPoint3 &p);
  
  // invert the parametrisation
  virtual void xyz2uvw(double xyz[3], double uvw[3]);

  // test if a point, given in parametric coordinates, belongs to the
  // element
  virtual bool isInside(double u, double v, double w) = 0;

  // interpolate the given nodal data (resp. its gradient, curl and
  // divergence) at point (u,v,w) in parametric coordinates
  double interpolate(double val[], double u, double v, double w, int stride=1, 
                     int order=-1);
  void interpolateGrad(double val[], double u, double v, double w, double f[3],
                       int stride=1, double invjac[3][3]=0, int order=-1);
  void interpolateCurl(double val[], double u, double v, double w, double f[3],
                       int stride=3, int order=-1);
  double interpolateDiv(double val[], double u, double v, double w, int stride=3,
                        int order=-1);

  // integration routines
  virtual int getNumIntegrationPointsToAllocate(int pOrder) const { return 0; }
  virtual void getIntegrationPoints(int pOrder, int *npts, IntPt **pts) const
  {
    Msg::Error("No integration points defined for this type of element");
  }

  // IO routines
  virtual void writeMSH(FILE *fp, double version=1.0, bool binary=false, 
                        int num=0, int elementary=1, int physical=1, int parentNum=0);
  virtual void writePOS(FILE *fp, bool printElementary, bool printElementNumber, 
                        bool printGamma, bool printEta, bool printRho, 
                        bool printDisto,double scalingFactor=1.0, int elementary=1);
  virtual void writeSTL(FILE *fp, bool binary=false, double scalingFactor=1.0);
  virtual void writeVRML(FILE *fp);
  virtual void writeUNV(FILE *fp, int num=0, int elementary=1, int physical=1);
  virtual void writeVTK(FILE *fp, bool binary=false, bool bigEndian=false);
  virtual void writeMESH(FILE *fp, int elementTagType=1, int elementary=1, 
                         int physical=0);
  virtual void writeBDF(FILE *fp, int format=0, int elementTagType=1, 
                        int elementary=1, int physical=0);
  virtual void writeDIFF(FILE *fp, int num, bool binary=false, 
                         int physical_property=1);
 
  // info for specific IO formats (returning 0 means that the element
  // is not implemented in that format)
  virtual int getTypeForMSH() const { return 0; }
  virtual int getTypeForUNV() const { return 0; }
  virtual int getTypeForVTK() const { return 0; }
  virtual const char *getStringForPOS() const { return 0; }
  virtual const char *getStringForBDF() const { return 0; }
  virtual const char *getStringForDIFF() const { return 0; }

  // return the number of vertices, as well as the element name if
  // 'name' != 0
  static int getInfoMSH(const int typeMSH, const char **const name=0);
};

class MElementLessThanLexicographic{
 public:
  static double tolerance;
  bool operator()(MElement *e1, MElement *e2) const
  {
    SPoint3 b1 = e1->barycenter();
    SPoint3 b2 = e2->barycenter();
    if(b1.x() - b2.x() >  tolerance) return true;
    if(b1.x() - b2.x() < -tolerance) return false;
    if(b1.y() - b2.y() >  tolerance) return true;
    if(b1.y() - b2.y() < -tolerance) return false;
    if(b1.z() - b2.z() >  tolerance) return true;
    return false;
  }
};

class MElementFactory{
 public:
  MElement *create(int type, std::vector<MVertex*> &v, int num=0, int part=0);
};

// Traits of various elements based on the dimension.  These generally define
// the faces of 2-D elements as MEdge and 3-D elements as MFace.

template <unsigned DIM> struct DimTr;
template <> struct DimTr<2>
{
  typedef MEdge FaceT;
  static int getNumFace(MElement *const element)
  {
    return element->getNumEdges();
  }
  static MEdge getFace(MElement *const element, const int iFace)
  {
    return element->getEdge(iFace);
  }
  static void getAllFaceVertices(MElement *const element, const int iFace,
                                 std::vector<MVertex*> &v)
  {
    element->getEdgeVertices(iFace, v);
  }
};

template <> struct DimTr<3>
{
  typedef MFace FaceT;
  static int getNumFace(MElement *const element)
  {
    return element->getNumFaces();
  }
  static MFace getFace(MElement *const element, const int iFace)
  {
    return element->getFace(iFace);
  }
  static void getAllFaceVertices(MElement *const element, const int iFace,
                                 std::vector<MVertex*> &v)
  {
    element->getFaceVertices(iFace, v);
  }
};

#endif
