// Gmsh - Copyright (C) 1997-2009 C. Geuzaine, J.-F. Remacle
//
// See the LICENSE.txt file for license information. Please report all
// bugs and problems to <gmsh@geuz.org>.

#ifndef _MELEMENTCUT_H_
#define _MELEMENTCUT_H_

#include "GmshMessage.h"
#include "MElement.h"
#include "MTetrahedron.h"
#include "MTriangle.h"
#include "MLine.h"

class gLevelset;
class GModel;

class MPolyhedron : public MElement {
 protected:
  bool _owner;
  MElement* _orig;
  std::vector<MTetrahedron*> _parts;
  std::vector<MVertex*> _vertices;
  std::vector<MVertex*> _innerVertices;
  std::vector<MEdge> _edges;
  std::vector<MFace> _faces;
  void _init();
 public:
  MPolyhedron(std::vector<MVertex*> v, int num = 0, int part = 0,
              bool owner = false, MElement* orig = NULL)
    : MElement(num, part), _owner(owner), _orig(orig)
  {
    if(v.size() % 4){
      Msg::Error("Got %d vertices for polyhedron", (int)v.size());
      return;
    }
    for(unsigned int i = 0; i < v.size(); i += 4)
      _parts.push_back(new MTetrahedron(v[i], v[i + 1], v[i + 2], v[i + 3]));
    _init();
  }
  MPolyhedron(std::vector<MTetrahedron*> vT, int num = 0, int part = 0,
              bool owner = false, MElement* orig = NULL)
    : MElement(num, part), _owner(owner), _orig(orig)
  {
    for(unsigned int i = 0; i < vT.size(); i++)
      _parts.push_back(vT[i]);
    _init();
  }
  ~MPolyhedron() 
  {
    if(_owner)
      delete _orig;
    for(unsigned int i = 0; i < _parts.size(); i++)
      delete _parts[i];
  }
  virtual int getDim() { return 3; }
  virtual int getNumVertices() const { return _vertices.size() + _innerVertices.size(); }
  virtual int getNumVolumeVertices() const { return _innerVertices.size(); }
  virtual MVertex *getVertex(int num)
  {
    return (num < (int)_vertices.size()) ? 
      _vertices[num] : _innerVertices[num - _vertices.size()];
  }
  virtual int getNumEdges() { return _edges.size(); }
  virtual MEdge getEdge(int num) { return _edges[num]; }
  virtual int getNumEdgesRep() { return _edges.size(); }
  virtual void getEdgeRep(int num, double *x, double *y, double *z, SVector3 *n)
  {
    MEdge e(getEdge(num));
    for(unsigned int i = 0; i < _faces.size(); i++)
      for(int j = 0; j < 3; j++)
        if(_faces[i].getEdge(j) == e)
          _getEdgeRep(e.getVertex(0), e.getVertex(1), x, y, z, n, i);
  }
  virtual void getEdgeVertices(const int num, std::vector<MVertex*> &v) const
  {
    v.resize(2);
    v[0] = _edges[num].getVertex(0);
    v[1] = _edges[num].getVertex(1);
  }
  virtual int getNumFaces() { return _faces.size(); }
  virtual MFace getFace(int num) { return _faces[num]; }
  virtual int getNumFacesRep() { return _faces.size(); }
  virtual void getFaceRep(int num, double *x, double *y, double *z, SVector3 *n)
  {
    _getFaceRep(_faces[num].getVertex(0), _faces[num].getVertex(1),
                _faces[num].getVertex(2), x, y, z, n);
  }
  virtual void getFaceVertices(const int num, std::vector<MVertex*> &v) const
  {
    v.resize(3);
    v[0] = _faces[num].getVertex(0);
    v[1] = _faces[num].getVertex(1);
    v[2] = _faces[num].getVertex(2);
  }
  virtual int getType() const { return TYPE_POLYH; }
  virtual int getTypeForMSH() const { return MSH_POLYH_; }
  virtual double getVolume()
  {
    double vol = 0;
    for(unsigned int i = 0; i < _parts.size(); i++)
      vol += _parts[i]->getVolume();
    return vol;
  }
  virtual int getVolumeSign() { return (getVolume() >= 0) ? 1 : -1; }
  virtual const functionSpace* getFunctionSpace(int order=-1) const 
  {
    return _orig->getFunctionSpace(order);
  }
  virtual void getShapeFunctions(double u, double v, double w, double s[], int o)
  {
    _orig->getShapeFunctions(u, v, w, s, o);
  }
  virtual void getGradShapeFunctions(double u, double v, double w, double s[][3], int o)
  {
    _orig->getGradShapeFunctions(u, v, w, s, o);
  }
  virtual void xyz2uvw(double xyz[3], double uvw[3])
  {
    _orig->xyz2uvw(xyz,uvw);
  }
  virtual bool isInside(double u, double v, double w);
  virtual void getIntegrationPoints(int pOrder, int *npts, IntPt **pts) const;
  virtual int getNumIntegrationPointsToAllocate (int pOrder)
  {
    return _parts.size() * getNGQTetPts(pOrder);
  }
  virtual void writeMSH(FILE *fp, double version=1.0, bool binary=false, 
                        int num=0, int elementary=1, int physical=1, int parentNum=0);
  virtual MElement *getParent() const { return _orig; }
  virtual void setParent(MElement *p) { _orig = p; }
  virtual int getNumChildren() const { return _parts.size(); }
  virtual MElement *getChild(int i) const { return _parts[i]; }
  virtual bool ownsParent() const { return _owner; }
};

class MPolygon : public MElement {
 protected:
  bool _owner;
  MElement* _orig;
  std::vector<MTriangle*> _parts;
  std::vector<MVertex*> _vertices;
  std::vector<MVertex*> _innerVertices;
  std::vector<MEdge> _edges;
  void _initVertices();
 public:
  MPolygon(std::vector<MVertex*> v, int num = 0, int part = 0,
           bool owner = false, MElement* orig = NULL)
    : MElement(num, part), _owner(owner), _orig(orig)
  {
    for(unsigned int i = 0; i < v.size() / 3; i++)
      _parts.push_back(new MTriangle(v[i * 3], v[i * 3 + 1], v[i * 3 + 2]));
    _initVertices();
  }
  MPolygon(std::vector<MTriangle*> vT, int num = 0, int part = 0,
           bool owner = false, MElement* orig = NULL)
    : MElement(num, part), _owner(owner), _orig(orig)
  {
    for(unsigned int i = 0; i < vT.size(); i++){
      MTriangle *t = (MTriangle*) vT[i];
      _parts.push_back(t);
    }
    _initVertices();
  }
  ~MPolygon() 
  {
    if(_owner)
      delete _orig;
    for(unsigned int i = 0; i < _parts.size(); i++)
      delete _parts[i];
  }
  virtual int getDim(){ return 2; }
  virtual int getNumVertices() const { return _vertices.size() + _innerVertices.size(); }
  virtual int getNumFaceVertices() const { return _innerVertices.size(); }
  virtual MVertex *getVertex(int num)
  {
    return (num < (int)_vertices.size()) ? 
      _vertices[num] : _innerVertices[num - _vertices.size()];
  }
  virtual int getNumEdges() { return _edges.size(); }
  virtual MEdge getEdge(int num) { return _edges[num]; }
  virtual int getNumEdgesRep() { return getNumEdges(); }
  virtual void getEdgeRep(int num, double *x, double *y, double *z, SVector3 *n) 
  {
    MEdge e(getEdge(num));
    _getEdgeRep(e.getVertex(0), e.getVertex(1), x, y, z, n, 0);
  }
  virtual void getEdgeVertices(const int num, std::vector<MVertex*> &v) const
  {
    v.resize(2);
    v[0] = _edges[num].getVertex(0);
    v[1] = _edges[num].getVertex(1);
  }
  virtual int getNumFaces() { return 1; }
  virtual MFace getFace(int num) { return MFace(_vertices); }
  virtual int getNumFacesRep() { return _parts.size(); }
  virtual void getFaceRep(int num, double *x, double *y, double *z, SVector3 *n)
  {
    _getFaceRep(_parts[num]->getVertex(0), _parts[num]->getVertex(1),
                _parts[num]->getVertex(2), x, y, z, n);
  }
  virtual void getFaceVertices(const int num, std::vector<MVertex*> &v) const
  {
    v.resize(_vertices.size() + _innerVertices.size());
    for (unsigned int i = 0; i < _vertices.size() + _innerVertices.size(); i++)
      v[i] = (i < _vertices.size()) ? _vertices[i] : _innerVertices[i - _vertices.size()];
  }
  virtual int getType() const { return TYPE_POLYG; }
  virtual int getTypeForMSH() const { return MSH_POLYG_; }
  virtual const functionSpace* getFunctionSpace(int order=-1) const 
  {
    return _orig->getFunctionSpace(order);
  }
  virtual void getShapeFunctions(double u, double v, double w, double s[], int o)
  {
    _orig->getShapeFunctions(u, v, w, s, o);
  }
  virtual void getGradShapeFunctions(double u, double v, double w, double s[][3], int o)
  {
    _orig->getGradShapeFunctions(u, v, w, s, o);
  }
  virtual void xyz2uvw(double xyz[3], double uvw[3])
  {
    _orig->xyz2uvw(xyz,uvw);
  }
  virtual bool isInside(double u, double v, double w);
  virtual void getIntegrationPoints(int pOrder, int *npts, IntPt **pts) const;
  virtual int getNumIntegrationPointsToAllocate (int pOrder)
  {
    return _parts.size() * getNGQTPts(pOrder);
  }
  virtual void writeMSH(FILE *fp, double version=1.0, bool binary=false, 
                        int num=0, int elementary=1, int physical=1, int parentNum=0);
  virtual MElement *getParent() const { return _orig; }
  virtual void setParent(MElement *p) { _orig = p; }
  virtual int getNumChildren() const { return _parts.size(); }
  virtual MElement *getChild(int i) const { return _parts[i]; }
  virtual bool ownsParent() const { return _owner; }
};

class MTriangleBorder : public MTriangle {
 protected:
  MPolyhedron* _domains[2];
 public:
  MTriangleBorder(MVertex *v0, MVertex *v1, MVertex *v2,
                  MPolyhedron* d1, MPolyhedron* d2, int num=0, int part=0)
    : MTriangle(v0, v1, v2, num, part)
  {
    _domains[0] = d1; _domains[1] = d2;
  }
  ~MTriangleBorder() {}
  MPolyhedron* getDomain(int i) const { return _domains[i]; }
  virtual MElement *getParent() const {
    if(_domains[0]) return _domains[0]->getParent();
    if(_domains[1]) return _domains[1]->getParent();
    return NULL;
  }
  virtual const functionSpace* getFunctionSpace(int order=-1) const 
  {
    return getParent()->getFunctionSpace(order);
  }
  virtual void getShapeFunctions(double u, double v, double w, double s[], int o)
  {
    getParent()->getShapeFunctions(u, v, w, s, o);
  }
  virtual void getGradShapeFunctions(double u, double v, double w, double s[][3], int o)
  {
    getParent()->getGradShapeFunctions(u, v, w, s, o);
  }
  virtual void xyz2uvw(double xyz[3], double uvw[3])
  {
    getParent()->xyz2uvw(xyz,uvw);
  }
  virtual void getIntegrationPoints(int pOrder, int *npts, IntPt **pts) const;
  virtual int getNumIntegrationPointsToAllocate (int pOrder)
  {
    return getNGQTPts(pOrder);
  }
};

class MLineBorder : public MLine {
 protected:
  MPolygon* _domains[2];
 public:
  MLineBorder(MVertex *v0, MVertex *v1,
              MPolygon* d1, MPolygon* d2, int num=0, int part=0)
    : MLine(v0, v1, num, part)
  {
    _domains[0] = d1; _domains[1] = d2;
  }
  ~MLineBorder() {}
  MPolygon* getDomain(int i) const { return _domains[i]; }
  virtual MElement *getParent() const {
    if(_domains[0]) return _domains[0]->getParent();
    if(_domains[1]) return _domains[1]->getParent();
    return NULL;
  }
  virtual const functionSpace* getFunctionSpace(int order=-1) const 
  {
    return getParent()->getFunctionSpace(order);
  }
  virtual void getShapeFunctions(double u, double v, double w, double s[], int o)
  {
    getParent()->getShapeFunctions(u, v, w, s, o);
  }
  virtual void getGradShapeFunctions(double u, double v, double w, double s[][3], int o)
  {
    getParent()->getGradShapeFunctions(u, v, w, s, o);
  }
  virtual void xyz2uvw(double xyz[3], double uvw[3])
  {
    getParent()->xyz2uvw(xyz,uvw);
  }
  virtual void getIntegrationPoints(int pOrder, int *npts, IntPt **pts) const;
  virtual int getNumIntegrationPointsToAllocate (int pOrder)
  {
    return pOrder / 2 + 1;
  }
};

GModel *buildCutMesh(GModel *gm, gLevelset *ls,
                     std::map<int, std::vector<MElement*> > elements[10],
                     std::map<int, MVertex*> &vertexMap,
                     std::map<int, std::map<int, std::string> > physicals[4]);

#endif
