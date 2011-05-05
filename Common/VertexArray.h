// Gmsh - Copyright (C) 1997-2008 C. Geuzaine, J.-F. Remacle
//
// See the LICENSE.txt file for license information. Please report all
// bugs and problems to <gmsh@geuz.org>.

#ifndef _VERTEX_ARRAY_H_
#define _VERTEX_ARRAY_H_

#include <vector>
#include <set>
#include "SVector3.h"
#include "Context.h"

extern Context_T CTX;

class MElement;

template<int N>
class ElementData {
 private:
  float _x[N], _y[N], _z[N], _nx[N], _ny[N], _nz[N];
  unsigned int _col[N];
  MElement *_ele;
 public:
  ElementData(double *x, double *y, double *z, SVector3 *n, unsigned int *col,
              MElement *ele)
  {
    for(int i = 0; i < N; i++){
      _x[i] = x[i];
      _y[i] = y[i];
      _z[i] = z[i];
      if(n){
        _nx[i] = n[i].x();
        _ny[i] = n[i].y();
        _nz[i] = n[i].z();
      }
      else
        _nx[i] = _ny[i] = _nz[i] = 0.;
      if(col)
        _col[i] = col[i];
      else
        _col[i] = 0;
    }
    _ele = ele;
  }
  float x(int i) const { return _x[i]; }
  float y(int i) const { return _y[i]; }
  float z(int i) const { return _z[i]; }
  float nx(int i) const { return _nx[i]; }
  float ny(int i) const { return _ny[i]; }
  float nz(int i) const { return _nz[i]; }
  unsigned int col(int i) const { return _col[i]; }
  MElement *ele() const { return _ele; }
  SPoint3 barycenter() const
  {
    SPoint3 p(0., 0., 0.);
    for(int i = 0; i < N; i++){
      p[0] += _x[i];
      p[1] += _y[i];
      p[2] += _z[i];
    }
    p[0] /= (double)N;
    p[1] /= (double)N;
    p[2] /= (double)N;
    return p;
  }
};

template<int N>
class ElementDataLessThan{
 public:
  bool operator()(const ElementData<N> &e1, const ElementData<N> &e2) const
  {
    SPoint3 p1 = e1.barycenter();
    SPoint3 p2 = e2.barycenter();
    float tolerance = CTX.lc * 1.e-12;
    if(p1.x() - p2.x() >  tolerance) return true;
    if(p1.x() - p2.x() < -tolerance) return false;
    if(p1.y() - p2.y() >  tolerance) return true;
    if(p1.y() - p2.y() < -tolerance) return false;
    if(p1.z() - p2.z() >  tolerance) return true;
    return false;
  }
};

class Barycenter {
 private:
  float _x, _y, _z;
 public:
  Barycenter(double x, double y, double z) : _x(x), _y(y), _z(z){}
  float x() const { return _x; }
  float y() const { return _y; }
  float z() const { return _z; }
  void operator+=(const Barycenter &p){ _x += p.x(); _y += p.y(); _z += p.z(); }
};

class BarycenterLessThan{
 public:
  bool operator()(const Barycenter &p1, const Barycenter &p2) const
  {
    float tolerance = CTX.lc * 1.e-12;
    if(p1.x() - p2.x() >  tolerance) return true;
    if(p1.x() - p2.x() < -tolerance) return false;
    if(p1.y() - p2.y() >  tolerance) return true;
    if(p1.y() - p2.y() < -tolerance) return false;
    if(p1.z() - p2.z() >  tolerance) return true;
    return false;
  }
};

class VertexArray{
 private:
  int _numVerticesPerElement;
  std::vector<float> _vertices;
  std::vector<char> _normals;
  std::vector<unsigned char> _colors;
  std::vector<MElement*> _elements;
  std::set<ElementData<3>, ElementDataLessThan<3> > _data3;
  std::set<Barycenter, BarycenterLessThan> _barycenters;
 public:
  VertexArray(int numVerticesPerElement, int numElements);
  ~VertexArray(){}
  // returns the number of vertices in the array
  int getNumVertices() { return _vertices.size() / 3; }
  // returns the number of vertices per element
  int getNumVerticesPerElement() { return _numVerticesPerElement; }
  // returns the number of element pointers
  int getNumElementPointers() { return _elements.size(); }
  // returns a pointer to the raw vertex array
  float *getVertexArray(int i=0){ return &_vertices[i]; }
  // returns a pointer to the raw normal array
  char *getNormalArray(int i=0){ return &_normals[i]; }
  // returns a pointer to the raw color array
  unsigned char *getColorArray(int i=0){ return &_colors[i]; }
  // returns a pointer to the raw element array
  MElement **getElementPointerArray(int i=0){ return &_elements[i]; }
  // adds stuff in the arrays
  void addVertex(float x, float y, float z);
  void addNormal(float nx, float ny, float nz);
  void addColor(unsigned int col);
  void addElement(MElement *ele);
  // add element data in the arrays (if unique is set, only add the
  // element if another one with the same barycenter is not already
  // present)
  void add(double *x, double *y, double *z, SVector3 *n, unsigned int *col,
           MElement *ele=0, bool unique=true, bool boundary=false);
  // finalize the arrays
  void finalize();
  // sorts the arrays with elements back to front wrt the eye position
  void sort(double x, double y, double z);
};

#endif
