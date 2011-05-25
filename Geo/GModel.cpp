// Gmsh - Copyright (C) 1997-2010 C. Geuzaine, J.-F. Remacle
//
// See the LICENSE.txt file for license information. Please report all
// bugs and problems to <gmsh@geuz.org>.

#include <stdlib.h>
#include <sstream>
#include "GmshConfig.h"
#include "GmshMessage.h"
#include "GModel.h"
#include "GModelFactory.h"
#include "MPoint.h"
#include "MLine.h"
#include "MTriangle.h"
#include "MQuadrangle.h"
#include "MTetrahedron.h"
#include "MHexahedron.h"
#include "MPrism.h"
#include "MPyramid.h"
#include "MElementCut.h"
#include "MElementOctree.h"
#include "discreteRegion.h"
#include "discreteFace.h"
#include "discreteEdge.h"
#include "discreteVertex.h"
#include "boundaryLayerFace.h"
#include "boundaryLayerEdge.h"
#include "boundaryLayerVertex.h"
#include "gmshSurface.h"
#include "Geo.h"
#include "SmoothData.h"
#include "Context.h"
#include "OS.h"
#include "GEdgeLoop.h"
#include "MVertexPositionSet.h"
#include "OpenFile.h"
#include "CreateFile.h"

#if defined(HAVE_MESH)
#include "Field.h"
#include "Generator.h"
#include "meshGFaceOptimize.h"
#endif

std::vector<GModel*> GModel::list;
int GModel::_current = -1;

static void recur_connect(MVertex *v,
                          std::multimap<MVertex*,MEdge> &v2e,
                          std::set<MEdge,Less_Edge> &group,
                          std::set<MVertex*> &touched)
{
  if (touched.find(v) != touched.end()) return;

  touched.insert(v);
  for (std::multimap <MVertex*,MEdge>::iterator it = v2e.lower_bound(v); 
       it != v2e.upper_bound(v) ; ++it){
    group.insert(it->second);
    for (int i = 0; i < it->second.getNumVertices(); ++i){
      recur_connect (it->second.getVertex(i), v2e, group, touched);
    }
  }
}

// starting form a list of elements, returns lists of lists that are
// all simply connected
static void recur_connect_e(const MEdge &e,
                            std::multimap<MEdge,MElement*, Less_Edge> &e2e,
                            std::set<MElement*> &group,
                            std::set<MEdge, Less_Edge> &touched)
{
  if (touched.find(e) != touched.end())return;
  touched.insert(e);
  for (std::multimap <MEdge,MElement*,Less_Edge>::iterator it = e2e.lower_bound(e);
         it != e2e.upper_bound(e) ; ++it){
    group.insert(it->second);
    for (int i = 0; i < it->second->getNumEdges(); ++i){
      recur_connect_e(it->second->getEdge(i), e2e, group, touched);
    }
  }
}

static int connected_bounds(std::set<MEdge, Less_Edge> &edges, 
                            std::vector<std::vector<MEdge> > &boundaries)
{
  std::multimap<MVertex*,MEdge> v2e;
  for(std::set<MEdge, Less_Edge>::iterator it = edges.begin(); it != edges.end(); it++){
    for (int j=0;j<it->getNumVertices();j++){
      v2e.insert(std::make_pair(it->getVertex(j),*it));
    }
  }

  while (!v2e.empty()){
    std::set<MEdge, Less_Edge> group;
    std::set<MVertex*> touched;
    recur_connect(v2e.begin()->first, v2e, group, touched);
    std::vector<MEdge> temp;
    temp.insert(temp.begin(), group.begin(), group.end());
    boundaries.push_back(temp);
    for (std::set<MVertex*>::iterator it = touched.begin() ; it != touched.end();++it)
      v2e.erase(*it);
  }

  return boundaries.size();
}

static int connectedRegions(std::vector<MElement*> &elements,
                            std::vector<std::vector<MElement*> > &regions)
{
  std::multimap<MEdge,MElement*,Less_Edge> e2e;
  for (unsigned int i = 0; i < elements.size(); ++i){
    for (int j = 0; j < elements[i]->getNumEdges(); j++){
      e2e.insert(std::make_pair(elements[i]->getEdge(j),elements[i]));
    }
  }
  while (!e2e.empty()){
    std::set<MElement*> group;
    std::set<MEdge,Less_Edge> touched;
    recur_connect_e (e2e.begin()->first,e2e,group,touched);
    std::vector<MElement*> temp;
    temp.insert(temp.begin(), group.begin(), group.end());
    regions.push_back(temp);
    for ( std::set<MEdge,Less_Edge>::iterator it = touched.begin() ; it != touched.end();++it)
      e2e.erase(*it);
  }

  return regions.size();
}

GModel::GModel(std::string name)
  : _name(name), _visible(1), _octree(0),
    _geo_internals(0), _occ_internals(0), _acis_internals(0), _fm_internals(0),
    _factory(0), _fields(0), _currentMeshEntity(0), normals(0)
{
  partitionSize[0] = 0; partitionSize[1] = 0;
  list.push_back(this);
  // at the moment we always create (at least an empty) GEO model
  _createGEOInternals();
#if defined(HAVE_OCC)
  _factory = new OCCFactory();
#endif
#if defined(HAVE_MESH)
  _fields = new FieldManager();
#endif
}

GModel::~GModel()
{
  std::vector<GModel*>::iterator it = std::find(list.begin(), list.end(), this);
  if(it != list.end()) list.erase(it);
  destroy();
  _deleteGEOInternals();
  _deleteOCCInternals();
#if defined(HAVE_MESH)
  delete _fields;
#endif
}

GModel *GModel::current(int index)
{
  if(list.empty()){
    Msg::Warning("No current model available: creating one");
    new GModel();
  }
  if(index >= 0) _current = index;
  if(_current < 0 || _current >= (int)list.size()) return list.back();
  return list[_current];
}

int GModel::setCurrent(GModel *m)
{
  for (unsigned int i = 0; i < list.size(); i++){
    if (list[i] == m){
      _current = i;
      break;
    }
  }
  return _current;
}

void GModel::setFactory(std::string name)
{
  if(_factory) delete _factory;
  _factory = 0;
  if(name == "Gmsh") {
    _factory = new GeoFactory();
  }
  else if(name == "OpenCASCADE"){
#if defined(HAVE_OCC)
    _factory = new OCCFactory();
#else
    Msg::Error("Missing OpenCASCADE support");
#endif
  }
}

GModel *GModel::findByName(std::string name)
{
  // return last mesh with given name
  for(int i = list.size() - 1; i >= 0; i--)
    if(list[i]->getName() == name) return list[i];
  return 0;
}

void GModel::destroy()
{
  _name.clear();

  for(riter it = firstRegion(); it != lastRegion(); ++it)
    delete *it;
  regions.clear();

  std::vector<GFace*> to_keep;
  for(fiter it = firstFace(); it != lastFace(); ++it){
    // projection faces are persistent
    if((*it)->getNativeType() == GEntity::UnknownModel &&
       (*it)->geomType() == GEntity::ProjectionFace)
      to_keep.push_back(*it);
    else
      delete *it;
  }
  faces.clear();
  faces.insert(to_keep.begin(), to_keep.end());

  for(eiter it = firstEdge(); it != lastEdge(); ++it)
    delete *it;
  edges.clear();

  for(viter it = firstVertex(); it != lastVertex(); ++it)
    delete *it;
  vertices.clear();

  destroyMeshCaches();

  MVertex::resetGlobalNumber();
  MElement::resetGlobalNumber();

  if(normals) delete normals;
  normals = 0;

#if defined(HAVE_MESH)
  _fields->reset();
#endif
  gmshSurface::reset();
}

void GModel::destroyMeshCaches()
{
  _vertexVectorCache.clear();
  _vertexMapCache.clear();
  _elementVectorCache.clear();
  _elementMapCache.clear();
  _elementIndexCache.clear();
  delete _octree;
  _octree = 0;
}

void GModel::deleteMesh()
{
  for(riter it = firstRegion(); it != lastRegion();++it)
    (*it)->deleteMesh();
  for(fiter it = firstFace(); it != lastFace();++it)
    (*it)->deleteMesh();
  for(eiter it = firstEdge(); it != lastEdge();++it)
    (*it)->deleteMesh();
  for(viter it = firstVertex(); it != lastVertex();++it)
    (*it)->deleteMesh();
  destroyMeshCaches();
}

bool GModel::empty() const
{
  return vertices.empty() && edges.empty() && faces.empty() && regions.empty();
}

std::vector<GRegion*> GModel::bindingsGetRegions()
{
  return std::vector<GRegion*> (regions.begin(), regions.end());
}

std::vector<GFace*> GModel::bindingsGetFaces()
{
  return std::vector<GFace*> (faces.begin(), faces.end());
}

std::vector<GEdge*> GModel::bindingsGetEdges()
{
  return std::vector<GEdge*> (edges.begin(), edges.end());
}

std::vector<GVertex*> GModel::bindingsGetVertices()
{
  return std::vector<GVertex*> (vertices.begin(), vertices.end());
}

GRegion *GModel::getRegionByTag(int n) const
{
  GEntity tmp((GModel*)this, n);
  std::set<GRegion*, GEntityLessThan>::const_iterator it = regions.find((GRegion*)&tmp);
  if(it != regions.end())
    return *it;
  else
    return 0;
}

GFace *GModel::getFaceByTag(int n) const
{
  GEntity tmp((GModel*)this, n);
  std::set<GFace*, GEntityLessThan>::const_iterator it = faces.find((GFace*)&tmp);
  if(it != faces.end())
    return *it;
  else
    return 0;
}

GEdge *GModel::getEdgeByTag(int n) const
{
  GEntity tmp((GModel*)this, n);
  std::set<GEdge*, GEntityLessThan>::const_iterator it = edges.find((GEdge*)&tmp);
  if(it != edges.end())
    return *it;
  else
    return 0;
}

GVertex *GModel::getVertexByTag(int n) const
{
  GEntity tmp((GModel*)this, n);
  std::set<GVertex*, GEntityLessThan>::const_iterator it = vertices.find((GVertex*)&tmp);
  if(it != vertices.end())
    return *it;
  else
    return 0;
}

void GModel::remove(GRegion *r)
{
  riter it = std::find(firstRegion(), lastRegion(), r);
  if(it != (riter)regions.end()) regions.erase(it);
}

void GModel::remove(GFace *f)
{
  fiter it = std::find(firstFace(), lastFace(), f);
  if(it != faces.end()) faces.erase(it);
}

void GModel::remove(GEdge *e)
{
  eiter it = std::find(firstEdge(), lastEdge(), e);
  if(it != edges.end()) edges.erase(it);
}

void GModel::remove(GVertex *v)
{
  viter it = std::find(firstVertex(), lastVertex(), v);
  if(it != vertices.end()) vertices.erase(it);
}

void GModel::snapVertices()
{
  viter vit = firstVertex();

  double tol = CTX::instance()->geom.tolerance;

  while (vit != lastVertex()){
    std::list<GEdge*> edges = (*vit)->edges();
    for (std::list<GEdge*>::iterator it = edges.begin(); it != edges.end(); ++it){
      Range<double> parb = (*it)->parBounds(0);
      double t;
      if ((*it)->getBeginVertex() == *vit){
        t = parb.low();
      }
      else if ((*it)->getEndVertex() == *vit){
        t = parb.high();
      }
      else{
        Msg::Error("Weird vertex: impossible to snap");
        break;
      }
      GPoint gp = (*it)->point(t);
      double d = sqrt((gp.x() - (*vit)->x()) * (gp.x() - (*vit)->x()) +
                      (gp.y() - (*vit)->y()) * (gp.y() - (*vit)->y()) +
                      (gp.z() - (*vit)->z()) * (gp.z() - (*vit)->z()));
      if (d > tol){
        (*vit)->setPosition(gp);
        Msg::Warning("Geom Vertex %d Corrupted (%12.5E)... Snap performed",
                     (*vit)->tag(), d);
      }
    }
    vit++;
  }
}

void GModel::getEntities(std::vector<GEntity*> &entities)
{
  entities.clear();
  entities.insert(entities.end(), vertices.begin(), vertices.end());
  entities.insert(entities.end(), edges.begin(), edges.end());
  entities.insert(entities.end(), faces.begin(), faces.end());
  entities.insert(entities.end(), regions.begin(), regions.end());
}

int GModel::getMaxElementaryNumber(int dim)
{
  std::vector<GEntity*> entities;
  getEntities(entities);
  int num = 0;
  for(unsigned int i = 0; i < entities.size(); i++)
    if(dim < 0 || entities[i]->dim() == dim)
      num = std::max(num, std::abs(entities[i]->tag()));
  return num;
}

bool GModel::noPhysicalGroups()
{
  std::vector<GEntity*> entities;
  getEntities(entities);
  for(unsigned int i = 0; i < entities.size(); i++)
    if(entities[i]->physicals.size()) return false;
  return true;
}

void GModel::getPhysicalGroups(std::map<int, std::vector<GEntity*> > groups[4])
{
  std::vector<GEntity*> entities;
  getEntities(entities);
  for(unsigned int i = 0; i < entities.size(); i++){
    std::map<int, std::vector<GEntity*> > &group(groups[entities[i]->dim()]);
    for(unsigned int j = 0; j < entities[i]->physicals.size(); j++){
      // physicals can be stored with negative signs when the entity
      // should be "reversed"
      int p = std::abs(entities[i]->physicals[j]);
      if(std::find(group[p].begin(), group[p].end(), entities[i]) == group[p].end())
        group[p].push_back(entities[i]);
    }
  }
}

void GModel::deletePhysicalGroups()
{
  std::vector<GEntity*> entities;
  getEntities(entities);
  for(unsigned int i = 0; i < entities.size(); i++)
    entities[i]->physicals.clear();
}

void GModel::deletePhysicalGroup(int dim, int num)
{
  std::vector<GEntity*> entities;
  getEntities(entities);
  for(unsigned int i = 0; i < entities.size(); i++){
    if(dim == entities[i]->dim()){
      std::vector<int> p;
      for(unsigned int j = 0; j < entities[i]->physicals.size(); j++)
        if(entities[i]->physicals[j] != num)
          p.push_back(entities[i]->physicals[j]);
      entities[i]->physicals = p;
    }
  }
}

int GModel::getMaxPhysicalNumber(int dim)
{
  std::vector<GEntity*> entities;
  getEntities(entities);
  int num = 0;
  for(unsigned int i = 0; i < entities.size(); i++)
    if(entities[i]->dim() == dim)
      for(unsigned int j = 0; j < entities[i]->physicals.size(); j++)
        num = std::max(num, std::abs(entities[i]->physicals[j]));
  return num;
}

int GModel::setPhysicalName(std::string name, int dim, int number)
{
  // check if the name is already used
  piter it = physicalNames.begin();
  while(it != physicalNames.end()){
    if(name == it->second && dim == it->first.first) return it->first.second;
    ++it;
  }
  // if no number is given, find the next available one
  if(!number) number = getMaxPhysicalNumber(dim) + 1;
  physicalNames[std::pair<int, int>(dim, number)] = name;
  return number;
}

std::string GModel::getPhysicalName(int dim, int number)
{
  std::map<std::pair<int, int>, std::string>::iterator it =
    physicalNames.find(std::pair<int, int>(dim, number));
  if(it != physicalNames.end()) return it->second;
  return "";
}

int GModel::getPhysicalNumber(const int &dim, const std::string &name)
{
  for(piter physIt = firstPhysicalName(); physIt != lastPhysicalName(); ++physIt)
    if(dim == physIt->first.first && name == physIt->second)
      return physIt->first.second;
  Msg::Warning("No physical group found with the name '%s'", name.c_str());
  return -1;
}

int GModel::getDim()
{
  if(getNumRegions() > 0) return 3;
  if(getNumFaces() > 0) return 2;
  if(getNumEdges() > 0) return 1;
  if(getNumVertices() > 0) return 0;
  Msg::Warning("The model is empty, dim = -1");
  return -1;
}

std::string GModel::getElementaryName(int dim, int number)
{
  std::map<std::pair<int, int>, std::string>::iterator it =
    elementaryNames.find(std::pair<int, int>(dim, number));
  if(it != elementaryNames.end()) return it->second;
  return "";
}

void GModel::setSelection(int val)
{
  std::vector<GEntity*> entities;
  getEntities(entities);

  for(unsigned int i = 0; i < entities.size(); i++){
    entities[i]->setSelection(val);
    // reset selection in elements (stored in the visibility flag to
    // save space)
    if(val == 0){
      for(unsigned int j = 0; j < entities[i]->getNumMeshElements(); j++)
        if(entities[i]->getMeshElement(j)->getVisibility() == 2)
          entities[i]->getMeshElement(j)->setVisibility(1);
    }
  }
}

SBoundingBox3d GModel::bounds()
{
  std::vector<GEntity*> entities;
  getEntities(entities);
  // using the mesh vertices for now; should use entities[i]->bounds() instead
  SBoundingBox3d bb;
  for(unsigned int i = 0; i < entities.size(); i++)
    if(entities[i]->dim() == 0)
      bb += static_cast<GVertex*>(entities[i])->xyz();
    else
      for(unsigned int j = 0; j < entities[i]->mesh_vertices.size(); j++)
        bb += entities[i]->mesh_vertices[j]->point();
  return bb;
}

int GModel::mesh(int dimension)
{
#if defined(HAVE_MESH)
  GenerateMesh(this, dimension);
  return true;
#else
  Msg::Error("Mesh module not compiled");
  return false;
#endif
}

int GModel::getMeshStatus(bool countDiscrete)
{
  for(riter it = firstRegion(); it != lastRegion(); ++it)
    if((countDiscrete || ((*it)->geomType() != GEntity::DiscreteVolume &&
                          (*it)->meshAttributes.Method != MESH_NONE)) &&
       ((*it)->tetrahedra.size() ||(*it)->hexahedra.size() ||
        (*it)->prisms.size() || (*it)->pyramids.size() ||
        (*it)->polyhedra.size())) return 3;
  for(fiter it = firstFace(); it != lastFace(); ++it)
    if((countDiscrete || ((*it)->geomType() != GEntity::DiscreteSurface &&
                          (*it)->meshAttributes.Method != MESH_NONE)) &&
       ((*it)->triangles.size() || (*it)->quadrangles.size() ||
        (*it)->polygons.size())) return 2;
  for(eiter it = firstEdge(); it != lastEdge(); ++it)
    if((countDiscrete || ((*it)->geomType() != GEntity::DiscreteCurve &&
                          (*it)->meshAttributes.Method != MESH_NONE)) &&
       (*it)->lines.size()) return 1;
  for(viter it = firstVertex(); it != lastVertex(); ++it)
    if((*it)->mesh_vertices.size()) return 0;
  return -1;
}

int GModel::getNumMeshVertices()
{
  std::vector<GEntity*> entities;
  getEntities(entities);
  unsigned int n = 0;
  for(unsigned int i = 0; i < entities.size(); i++)
    n += entities[i]->mesh_vertices.size();
  return n;
}

int GModel::getNumMeshElements()
{
  std::vector<GEntity*> entities;
  getEntities(entities);
  unsigned int n = 0;
  for(unsigned int i = 0; i < entities.size(); i++)
    n += entities[i]->getNumMeshElements();
  return n;
}

int GModel::getNumMeshElements(unsigned c[5])
{
  c[0] = 0; c[1] = 0; c[2] = 0; c[3] = 0; c[4] = 0;
  for(riter it = firstRegion(); it != lastRegion(); ++it)
    (*it)->getNumMeshElements(c);
  if(c[0] + c[1] + c[2] + c[3] + c[4]) return 3;
  for(fiter it = firstFace(); it != lastFace(); ++it)
    (*it)->getNumMeshElements(c);
  if(c[0] + c[1] + c[2]) return 2;
  for(eiter it = firstEdge(); it != lastEdge(); ++it)
    (*it)->getNumMeshElements(c);
  if(c[0]) return 1;
  return 0;
}

MElement *GModel::getMeshElementByCoord(SPoint3 &p)
{
  if(!_octree){
    Msg::Debug("Rebuilding mesh element octree");
    _octree = new MElementOctree(this);
  }
  return _octree->find(p.x(), p.y(), p.z());
}

MVertex *GModel::getMeshVertexByTag(int n)
{
  if(_vertexVectorCache.empty() && _vertexMapCache.empty()){
    Msg::Debug("Rebuilding mesh vertex cache");
    _vertexVectorCache.clear();
    _vertexMapCache.clear();
    bool dense = (getNumMeshVertices() == MVertex::getGlobalNumber());
    std::vector<GEntity*> entities;
    getEntities(entities);
    if(dense){
      Msg::Debug("Good: we have a dense vertex numbering in the cache");
      // numbering starts at 1
      _vertexVectorCache.resize(MVertex::getGlobalNumber() + 1);
      for(unsigned int i = 0; i < entities.size(); i++)
        for(unsigned int j = 0; j < entities[i]->mesh_vertices.size(); j++)
          _vertexVectorCache[entities[i]->mesh_vertices[j]->getNum()] =
            entities[i]->mesh_vertices[j];
    }
    else{
      for(unsigned int i = 0; i < entities.size(); i++)
        for(unsigned int j = 0; j < entities[i]->mesh_vertices.size(); j++)
          _vertexMapCache[entities[i]->mesh_vertices[j]->getNum()] =
            entities[i]->mesh_vertices[j];
    }
  }

  if(n < (int)_vertexVectorCache.size())
    return _vertexVectorCache[n];
  else
    return _vertexMapCache[n];
}

void GModel::getMeshVerticesForPhysicalGroup(int dim, int num, std::vector<MVertex*> &v)
{
  v.clear();
  std::map<int, std::vector<GEntity*> > groups[4];
  getPhysicalGroups(groups);
  std::map<int, std::vector<GEntity*> >::const_iterator it = groups[dim].find(num);
  if(it == groups[dim].end()) return;
  const std::vector<GEntity *> &entities = it->second;
  std::set<MVertex*> sv;
  for(unsigned int i = 0; i < entities.size(); i++){
    if(dim == 0){
      GVertex *g = (GVertex*)entities[i];
      sv.insert(g->mesh_vertices[0]);
    }
    else{
      for(unsigned int j = 0; j < entities[i]->getNumMeshElements(); j++){
        MElement *e = entities[i]->getMeshElement(j);
        for(int k = 0; k < e->getNumVertices(); k++)
          sv.insert(e->getVertex(k));
      }
    }
  }
  v.insert(v.begin(), sv.begin(), sv.end());
}

MElement *GModel::getMeshElementByTag(int n)
{
  if(_elementVectorCache.empty() && _elementMapCache.empty()){
    Msg::Debug("Rebuilding mesh element cache");
    _elementVectorCache.clear();
    _elementMapCache.clear();
    bool dense = (getNumMeshElements() == MElement::getGlobalNumber());
    std::vector<GEntity*> entities;
    getEntities(entities);
    if(dense){
      Msg::Debug("Good: we have a dense element numbering in the cache");
      // numbering starts at 1
      _elementVectorCache.resize(MElement::getGlobalNumber() + 1);
      for(unsigned int i = 0; i < entities.size(); i++)
        for(unsigned int j = 0; j < entities[i]->getNumMeshElements(); j++){
          MElement *e = entities[i]->getMeshElement(j);
          _elementVectorCache[e->getNum()] = e;
        }
    }
    else{
      for(unsigned int i = 0; i < entities.size(); i++)
        for(unsigned int j = 0; j < entities[i]->getNumMeshElements(); j++){
          MElement *e = entities[i]->getMeshElement(j);
          _elementMapCache[e->getNum()] = e;
        }
    }
  }

  if(n < (int)_elementVectorCache.size())
    return _elementVectorCache[n];
  else
    return _elementMapCache[n];
}

int GModel::getMeshElementIndex(MElement *e)
{
  if(!e) return 0;
  std::map<int, int>::iterator it = _elementIndexCache.find(e->getNum());
  if(it != _elementIndexCache.end()) return it->second;
  return e->getNum();
}

void GModel::setMeshElementIndex(MElement *e, int index)
{
  _elementIndexCache[e->getNum()] = index; 
}

template <class T>
static void removeInvisible(std::vector<T*> &elements, bool all)
{
  std::vector<T*> tmp;
  for(unsigned int i = 0; i < elements.size(); i++){
    if(all || !elements[i]->getVisibility())
      delete elements[i];
    else
      tmp.push_back(elements[i]);
  }
  elements.clear();
  elements = tmp;
}

void GModel::removeInvisibleElements()
{
  for(riter it = firstRegion(); it != lastRegion(); ++it){
    bool all = !(*it)->getVisibility();
    removeInvisible((*it)->tetrahedra, all);
    removeInvisible((*it)->hexahedra, all);
    removeInvisible((*it)->prisms, all);
    removeInvisible((*it)->pyramids, all);
    removeInvisible((*it)->polyhedra, all);
    (*it)->deleteVertexArrays();
  }
  for(fiter it = firstFace(); it != lastFace(); ++it){
    bool all = !(*it)->getVisibility();
    removeInvisible((*it)->triangles, all);
    removeInvisible((*it)->quadrangles, all);
    removeInvisible((*it)->polygons, all);
    (*it)->deleteVertexArrays();
  }
  for(eiter it = firstEdge(); it != lastEdge(); ++it){
    bool all = !(*it)->getVisibility();
    removeInvisible((*it)->lines, all);
    (*it)->deleteVertexArrays();
  }
  destroyMeshCaches();
}

int GModel::indexMeshVertices(bool all, int singlePartition)
{
  std::vector<GEntity*> entities;
  getEntities(entities);

  // tag all mesh vertices with -1 (negative vertices will not be
  // saved)
  for(unsigned int i = 0; i < entities.size(); i++)
    for(unsigned int j = 0; j < entities[i]->mesh_vertices.size(); j++)
      entities[i]->mesh_vertices[j]->setIndex(-1);

  // tag all mesh vertices belonging to elements that need to be saved
  // with 0, or with -2 if they need to be taken into account in the
  // numbering but need not to be saved (because we save a single
  // partition and they are not used in that partition)
  for(unsigned int i = 0; i < entities.size(); i++){
    if(all || entities[i]->physicals.size()){
      for(unsigned int j = 0; j < entities[i]->getNumMeshElements(); j++){
        MElement *e = entities[i]->getMeshElement(j);
        for(int k = 0; k < e->getNumVertices(); k++){
          if(!singlePartition || e->getPartition() == singlePartition)
            e->getVertex(k)->setIndex(0);
          else if(e->getVertex(k)->getIndex() == -1)
            e->getVertex(k)->setIndex(-2);
        }
      }
    }
  }

  // renumber all the mesh vertices tagged with 0
  int numVertices = 0, index = 0;
  for(unsigned int i = 0; i < entities.size(); i++)
    for(unsigned int j = 0; j < entities[i]->mesh_vertices.size(); j++)
      if(!entities[i]->mesh_vertices[j]->getIndex()){
        index++;
        numVertices++;
        entities[i]->mesh_vertices[j]->setIndex(index);
      }
      else if(entities[i]->mesh_vertices[j]->getIndex() == -2)
        index++;
  
  return numVertices;
}

void GModel::scaleMesh(double factor)
{
  std::vector<GEntity*> entities;
  getEntities(entities);
  for(unsigned int i = 0; i < entities.size(); i++)
    for(unsigned int j = 0; j < entities[i]->mesh_vertices.size(); j++){
      MVertex *v = entities[i]->mesh_vertices[j];
      v->x() *= factor;
      v->y() *= factor;
      v->z() *= factor;
    }
}

void GModel::recomputeMeshPartitions()
{
  meshPartitions.clear();
  std::vector<GEntity*> entities;
  getEntities(entities);
  for(unsigned int i = 0; i < entities.size(); i++){
    for(unsigned int j = 0; j < entities[i]->getNumMeshElements(); j++){
      int part = entities[i]->getMeshElement(j)->getPartition();
      if(part)  meshPartitions.insert(part);
    }
  }
}

void GModel::deleteMeshPartitions()
{
  std::vector<GEntity*> entities;
  getEntities(entities);
  for(unsigned int i = 0; i < entities.size(); i++)
    for(unsigned int j = 0; j < entities[i]->getNumMeshElements(); j++)
      entities[i]->getMeshElement(j)->setPartition(0);
  meshPartitions.clear();
}

template<class T>
static void _addElements(std::vector<T*> &dst, const std::vector<MElement*> &src)
{
  for(unsigned int i = 0; i < src.size(); i++) dst.push_back((T*)src[i]);
}

void GModel::_storeElementsInEntities(std::map<int, std::vector<MElement*> > &map)
{
  std::map<int, std::vector<MElement*> >::const_iterator it = map.begin();
  for(; it != map.end(); ++it){
    if(!it->second.size()) continue;
    int type = it->second[0]->getType();
    switch(type){
    case TYPE_PNT:
      {
        GVertex *v = getVertexByTag(it->first);
        if(!v){
          v = new discreteVertex(this, it->first);
          add(v);
        }
        if(!v->points.empty()) { // CAD points already have one by default
          v->points.clear();
          v->mesh_vertices.clear();
        }  
        _addElements(v->points, it->second);
      }
      break;
    case TYPE_LIN:
      {
        GEdge *e = getEdgeByTag(it->first);
        if(!e){
          e = new discreteEdge(this, it->first, 0, 0);
          add(e);
        }
        _addElements(e->lines, it->second);
      }
      break;
    case TYPE_TRI: case TYPE_QUA: case TYPE_POLYG:
      {
        GFace *f = getFaceByTag(it->first);
        if(!f){
          f = new discreteFace(this, it->first);
          add(f);
        }
        if(type == TYPE_TRI) _addElements(f->triangles, it->second);
        else if(type == TYPE_QUA) _addElements(f->quadrangles, it->second);
        else _addElements(f->polygons, it->second);
      }
      break;
    case TYPE_TET: case TYPE_HEX: case TYPE_PYR: case TYPE_PRI: case TYPE_POLYH:
      {
        GRegion *r = getRegionByTag(it->first);
        if(!r){
          r = new discreteRegion(this, it->first);
          add(r);
        }
        if(type == TYPE_TET) _addElements(r->tetrahedra, it->second);
        else if(type == TYPE_HEX) _addElements(r->hexahedra, it->second);
        else if(type == TYPE_PRI) _addElements(r->prisms, it->second);
        else if(type == TYPE_PYR) _addElements(r->pyramids, it->second);
        else _addElements(r->polyhedra, it->second);
      }
      break;
    }
  }
}

template<class T>
static void _associateEntityWithElementVertices(GEntity *ge, std::vector<T*> &elements)
{
  for(unsigned int i = 0; i < elements.size(); i++){
    for(int j = 0; j < elements[i]->getNumVertices(); j++){
      if (!elements[i]->getVertex(j)->onWhat() ||
	  elements[i]->getVertex(j)->onWhat()->dim() > ge->dim())
	elements[i]->getVertex(j)->setEntity(ge);
    }
  }
}

void GModel::_associateEntityWithMeshVertices()
{
  // loop on regions, then on faces, edges and vertices and store the
  // entity pointer in the the elements' vertices (this way we
  // associate the entity of lowest geometrical degree with each
  // vertex)
  for(riter it = firstRegion(); it != lastRegion(); ++it){
    _associateEntityWithElementVertices(*it, (*it)->tetrahedra);
    _associateEntityWithElementVertices(*it, (*it)->hexahedra);
    _associateEntityWithElementVertices(*it, (*it)->prisms);
    _associateEntityWithElementVertices(*it, (*it)->pyramids);
    _associateEntityWithElementVertices(*it, (*it)->polyhedra);
  }
  for(fiter it = firstFace(); it != lastFace(); ++it){
    _associateEntityWithElementVertices(*it, (*it)->triangles);
    _associateEntityWithElementVertices(*it, (*it)->quadrangles);
    _associateEntityWithElementVertices(*it, (*it)->polygons);
  }
  for(eiter it = firstEdge(); it != lastEdge(); ++it){
    _associateEntityWithElementVertices(*it, (*it)->lines);
  }
  for(viter it = firstVertex(); it != lastVertex(); ++it){
    _associateEntityWithElementVertices(*it, (*it)->points);
  }
}

void GModel::_storeVerticesInEntities(std::map<int, MVertex*> &vertices)
{
  std::map<int, MVertex*>::iterator it = vertices.begin();
  for(; it != vertices.end(); ++it){
    MVertex *v = it->second;
    GEntity *ge = v->onWhat();
    if(ge) ge->mesh_vertices.push_back(v);
    else{
      delete v; // we delete all unused vertices
      it->second = 0;
    }
  }
}

void GModel::_storeVerticesInEntities(std::vector<MVertex*> &vertices)
{
  for(unsigned int i = 0; i < vertices.size(); i++){
    MVertex *v = vertices[i];
    if(v){ // the vector is allowed to have null entries
      GEntity *ge = v->onWhat();
      if(ge) ge->mesh_vertices.push_back(v);
      else{
        delete v; // we delete all unused vertices
        vertices[i] = 0;
      }
    }
  }
}

void GModel::checkMeshCoherence(double tolerance)
{
  int numEle = getNumMeshElements();
  if(!numEle) return;

  Msg::StatusBar(2, true, "Checking mesh coherence (%d elements)...", numEle);

  SBoundingBox3d bbox = bounds();
  double lc = bbox.empty() ? 1. : norm(SVector3(bbox.max(), bbox.min()));
  double eps = lc * tolerance;

  std::vector<GEntity*> entities;
  getEntities(entities);

  // check for duplicate mesh vertices
  {
    Msg::Info("Checking for duplicate vertices...");
    std::vector<MVertex*> vertices;
    for(unsigned int i = 0; i < entities.size(); i++)
      vertices.insert(vertices.end(), entities[i]->mesh_vertices.begin(), 
                      entities[i]->mesh_vertices.end());
    MVertexPositionSet pos(vertices);
    for(unsigned int i = 0; i < vertices.size(); i++)
      pos.find(vertices[i]->x(), vertices[i]->y(), vertices[i]->z(), eps);
    int num = 0;
    for(unsigned int i = 0; i < vertices.size(); i++)
      if(!vertices[i]->getIndex()){
        Msg::Info("Duplicate vertex at (%.16g,%.16g,%.16g)",
                  vertices[i]->x(), vertices[i]->y(), vertices[i]->z());
        num++;
      }
    if(num) Msg::Error("%d duplicate vert%s", num, num > 1 ? "ices" : "ex");
  }

  // check for duplicate elements
  {
    Msg::Info("Checking for duplicate elements...");
    std::vector<MVertex*> vertices;
    for(unsigned int i = 0; i < entities.size(); i++)
      for(unsigned int j = 0; j < entities[i]->getNumMeshElements(); j++){
        SPoint3 p = entities[i]->getMeshElement(j)->barycenter();
        vertices.push_back(new MVertex(p.x(), p.y(), p.z()));
      }
    MVertexPositionSet pos(vertices);
    for(unsigned int i = 0; i < vertices.size(); i++)
      pos.find(vertices[i]->x(), vertices[i]->y(), vertices[i]->z(), eps);
    int num = 0;
    for(unsigned int i = 0; i < vertices.size(); i++){
      if(!vertices[i]->getIndex()){
        Msg::Info("Duplicate element with barycenter (%.16g,%.16g,%.16g)", 
                  vertices[i]->x(), vertices[i]->y(), vertices[i]->z());
        num++;
      }
      delete vertices[i];
    }
    if(num) Msg::Error("%d duplicate element%s", num, num > 1 ? "s" : "");
  }

  Msg::StatusBar(2, true, "Done checking mesh coherence");
}

int GModel::removeDuplicateMeshVertices(double tolerance)
{
  Msg::StatusBar(2, true, "Removing duplicate mesh vertices...");

  SBoundingBox3d bbox = bounds();
  double lc = bbox.empty() ? 1. : norm(SVector3(bbox.max(), bbox.min()));
  double eps = lc * tolerance;

  std::vector<GEntity*> entities;
  getEntities(entities);

  std::vector<MVertex*> vertices;
  for(unsigned int i = 0; i < entities.size(); i++)
    for(unsigned int j = 0; j < entities[i]->mesh_vertices.size(); j++){
      MVertex *v = entities[i]->mesh_vertices[j];
      vertices.push_back(new MVertex(v->x(), v->y(), v->z()));
    }
  MVertexPositionSet pos(vertices);
  for(unsigned int i = 0; i < vertices.size(); i++)
    pos.find(vertices[i]->x(), vertices[i]->y(), vertices[i]->z(), eps);
  int num = 0;
  for(unsigned int i = 0; i < vertices.size(); i++)
    if(!vertices[i]->getIndex()) num++;

  if(!num){
    for(unsigned int i = 0; i < vertices.size(); i++)
      delete vertices[i];
    Msg::Info("No duplicate vertices found");
    return 0;
  }

  std::map<int, std::vector<MElement*> > elements[10];
  for(unsigned int i = 0; i < entities.size(); i++){
    for(unsigned int j = 0; j < entities[i]->getNumMeshElements(); j++){
      MElement *e = entities[i]->getMeshElement(j);
      std::vector<MVertex*> verts;
      for(int k = 0; k < e->getNumVertices(); k++){
        MVertex *v = e->getVertex(k);
        MVertex *v2 = pos.find(v->x(), v->y(), v->z(), eps);
        if(v2) verts.push_back(v2);
      }
      if((int)verts.size() == e->getNumVertices()){
        MElementFactory factory;
        MElement *e2 = factory.create(e->getTypeForMSH(), verts, e->getNum(),
                                      e->getPartition());
        switch(e2->getType()){
        case TYPE_PNT: elements[0][entities[i]->tag()].push_back(e2); break;
        case TYPE_LIN: elements[1][entities[i]->tag()].push_back(e2); break;
        case TYPE_TRI: elements[2][entities[i]->tag()].push_back(e2); break;
        case TYPE_QUA: elements[3][entities[i]->tag()].push_back(e2); break;
        case TYPE_TET: elements[4][entities[i]->tag()].push_back(e2); break;
        case TYPE_HEX: elements[5][entities[i]->tag()].push_back(e2); break;
        case TYPE_PRI: elements[6][entities[i]->tag()].push_back(e2); break;
        case TYPE_PYR: elements[7][entities[i]->tag()].push_back(e2); break;
        case TYPE_POLYG: elements[8][entities[i]->tag()].push_back(e2); break;
        case TYPE_POLYH: elements[9][entities[i]->tag()].push_back(e2); break;
        }
      }
      else
        Msg::Error("Could not recreate element %d", e->getNum());
    }
  }

  for(unsigned int i = 0; i < entities.size(); i++)
    entities[i]->deleteMesh();

  for(int i = 0; i < (int)(sizeof(elements) / sizeof(elements[0])); i++)
    _storeElementsInEntities(elements[i]);
  _associateEntityWithMeshVertices();
  _storeVerticesInEntities(vertices);

  if(num)
    Msg::Info("Removed %d duplicate mesh %s", num, num > 1 ? "vertices" : "vertex");

  Msg::StatusBar(2, true, "Done removing duplicate mesh vertices");
  return num;
}

void GModel::createTopologyFromMesh()
{
  
  Msg::StatusBar(2, true, "Creating topology from mesh...");
  double t1 = Cpu();

  removeDuplicateMeshVertices(CTX::instance()->geom.tolerance);

  // for each discreteRegion, create topology
  std::vector<discreteRegion*> discRegions;
  for(riter it = firstRegion(); it != lastRegion(); it++)
    if((*it)->geomType() == GEntity::DiscreteVolume)
      discRegions.push_back((discreteRegion*) *it);

  //add mesh vertices
  for (std::vector<discreteRegion*>::iterator it = discRegions.begin();
       it != discRegions.end(); it++){
    for( std::vector<MVertex*>::const_iterator itv = (*it)->mesh_vertices.begin();  
	 itv != (*it)->mesh_vertices.end(); itv++)
      (*itv)->setEntity(*it);  
  }
  
  //for each discreteFace, createTopology
  std::vector<discreteFace*> discFaces;
  for(fiter it = firstFace(); it != lastFace(); it++)
    if((*it)->geomType() == GEntity::DiscreteSurface)
      discFaces.push_back((discreteFace*) *it);
  
  //--------
  //for each discrete face, compute if it is made of connected faces
  std::vector<discreteFace*> newDiscFaces;
  for (std::vector<discreteFace*>::iterator itF = discFaces.begin(); 
         itF != discFaces.end(); itF++){

    std::vector<MElement*> allElements;
    for (unsigned int i = 0; i < (*itF)->getNumMeshElements(); i++)
      allElements.push_back((*itF)->getMeshElement(i));

    std::vector<std::vector<MElement*> >  conRegions;
    int nbFaces = connectedRegions (allElements, conRegions);
    
    remove(*itF);
    for (int ifa  = 0; ifa < nbFaces; ifa++){
      std::vector<MElement*> myElements = conRegions[ifa];

      int numF;
      if (nbFaces == 1) numF = (*itF)->tag(); 
      else numF = getMaxElementaryNumber(2) + 1;
      discreteFace *f = new discreteFace(this, numF);
      //printf("*** Created discreteFace %d \n", numF);
      add(f);
      newDiscFaces.push_back(f);

      //fill new face with mesh vertices
      std::set<MVertex*> allV;
      for(unsigned int i = 0; i < myElements.size(); i++) {
	MVertex *v0 = myElements[i]->getVertex(0);
	MVertex *v1 = myElements[i]->getVertex(1);
	MVertex *v2 = myElements[i]->getVertex(2);
	f->triangles.push_back(new MTriangle( v0, v1, v2));
	allV.insert(v0);  allV.insert(v1);  allV.insert(v2);
	v0->setEntity(f); v1->setEntity(f); v2->setEntity(f);
      }
      f->mesh_vertices.insert(f->mesh_vertices.begin(), allV.begin(),allV.end());
      
      //delete new mesh vertices of face from adjacent regions
      for (std::vector<discreteRegion*>::iterator itR = discRegions.begin();
	   itR != discRegions.end(); itR++){
	for (std::set<MVertex*>::iterator itv = allV.begin();itv != allV.end(); itv++) {
	  std::vector<MVertex*>::iterator itve = 
	    std::find((*itR)->mesh_vertices.begin(), (*itR)->mesh_vertices.end(), *itv);
	  if (itve != (*itR)->mesh_vertices.end()) (*itR)->mesh_vertices.erase(itve);
	}
      }
      
    }

  }
  discFaces = newDiscFaces;
  //------

  //set boundary faces for each volume

  // find boundary faces of each region and put them in a map_faces that
  // associates the MElements with the tags of the adjacent regions
  std::map<MFace, std::vector<int>, Less_Face > map_faces;
  for (std::vector<discreteRegion*>::iterator it = discRegions.begin(); 
       it != discRegions.end(); it++){
    (*it)->findFaces(map_faces);
  }

 // create reverse map, for each region find set of MFaces that are
  // candidate for new discrete Face
  std::map<int, std::set<int> > region2Faces;

  for (std::vector<discreteFace*>::iterator itF = discFaces.begin(); 
       itF != discFaces.end(); itF++){
    for (unsigned int i = 0; i < (*itF)->getNumMeshElements(); i++){
      MFace mf = (*itF)->getMeshElement(i)->getFace(0);
      std::map<MFace, std::vector<int>, Less_Face >::iterator itset = map_faces.find(mf);
      if (itset !=  map_faces.end()){
	std::vector<int> tagRegions = itset->second;
	for (std::vector<int>::iterator itR =tagRegions.begin(); itR != tagRegions.end(); itR++){
	  std::map<int, std::set<int> >::iterator itmap = region2Faces.find(*itR);
	  if (itmap == region2Faces.end()){
	    std::set<int>  oneFace; oneFace.insert((*itF)->tag());
	    region2Faces.insert(std::make_pair(*itR, oneFace));
	  } 
	  else{
	     std::set<int>  allFaces = itmap->second;
	     allFaces.insert(allFaces.begin(), (*itF)->tag());
	     itmap->second = allFaces;
	  }
	}
      }
    }
  }

  for (std::vector<discreteRegion*>::iterator it = discRegions.begin();  it != discRegions.end(); it++){
    std::map<int, std::set<int> >::iterator itr = region2Faces.find((*it)->tag());
    if (itr != region2Faces.end()){
      std::set<int> bcFaces = itr->second;
      (*it)->setBoundFaces(bcFaces);
    }
  }

  //create Topo for faces
  createTopologyFromFaces(discFaces);
  
  double t2 = Cpu();
  Msg::StatusBar(2, true, "Done creating topology from mesh (%g s)", t2-t1);
  
  //create old format (necessary for boundary layers)
  exportDiscreteGEOInternals();
}

void GModel::createTopologyFromFaces(std::vector<discreteFace*> &discFaces)
{
  std::vector<discreteEdge*> discEdges;
  for(eiter it = firstEdge(); it != lastEdge(); it++){
    if((*it)->geomType() == GEntity::DiscreteCurve)
      discEdges.push_back((discreteEdge*) *it);
  }

  // find boundary edges of each face and put them in a map_edges that
  // associates the MEdges with the tags of the adjacent faces
  std::map<MEdge, std::vector<int>, Less_Edge > map_edges;
  for (std::vector<discreteFace*>::iterator it = discFaces.begin(); 
       it != discFaces.end(); it++){
    (*it)->findEdges(map_edges);
  }
  
  //return if no boundary edges (torus, sphere, ...)
  if (map_edges.empty()) return;

  // create reverse map, for each face find set of MEdges that are
  // candidate for new discrete Edges
  std::map<int, std::vector<int> > face2Edges;

  while (!map_edges.empty()){
    std::set<MEdge, Less_Edge> myEdges;
    myEdges.clear();
    std::vector<int> tagFaces = map_edges.begin()->second;
    myEdges.insert(map_edges.begin()->first);
    map_edges.erase(map_edges.begin());

    std::map<MEdge, std::vector<int>, Less_Edge >::iterator itmap = map_edges.begin();
    while (itmap != map_edges.end()){
      std::vector<int> tagFaces2 = itmap->second;
      if (tagFaces2 == tagFaces){
        myEdges.insert(itmap->first);
        map_edges.erase(itmap++);
      }
      else
        itmap++;
    }

    // if the loaded mesh already contains discrete Edges, check if
    // the candidate discrete Edge does contain any of those; if not,
    // create discreteEdges and create a map face2Edges that associate
    // for each face the boundary discrete Edges

    for (std::vector<discreteEdge*>::iterator itE = discEdges.begin(); 
         itE != discEdges.end(); itE++){

      bool candidate = true;
      for (unsigned int i = 0; i < (*itE)->getNumMeshElements(); i++){
        MEdge me = (*itE)->getMeshElement(i)->getEdge(0);
        std::set<MEdge, Less_Edge >::iterator itset = myEdges.find(me);
        if (itset ==  myEdges.end()){
          candidate = false;
          break;
        }
      }

      if (candidate){
        std::vector<int> tagEdges;
        tagEdges.push_back((*itE)->tag());
        for (unsigned int i = 0; i < (*itE)->getNumMeshElements(); i++){
          MEdge me = (*itE)->getMeshElement(i)->getEdge(0);
          std::set<MEdge, Less_Edge >::iterator itset = myEdges.find(me);
          myEdges.erase(itset);
        }

        for (std::vector<int>::iterator itFace = tagFaces.begin(); 
             itFace != tagFaces.end(); itFace++) {
          std::map<int, std::vector<int> >::iterator it = face2Edges.find(*itFace);
          if (it == face2Edges.end()) face2Edges.insert(std::make_pair(*itFace, tagEdges));
          else{
            std::vector<int> allEdges = it->second;
            allEdges.insert(allEdges.begin(), tagEdges.begin(), tagEdges.end());
            it->second = allEdges;
          }
	  
	  //delete new mesh vertices of edge from adjacent faces
	  GFace *gf = getFaceByTag(*itFace);
	  for (std::vector<int>::iterator itEdge = tagEdges.begin(); 
	       itEdge != tagEdges.end(); itEdge++) {
	    int count  = 0;
	    GEdge *ge = getEdgeByTag(*itEdge);
	    for( std::vector<MVertex*>::const_iterator itv = ge->mesh_vertices.begin(); 
		 itv != ge->mesh_vertices.end(); itv++){
	      std::vector<MVertex*>::iterator itve = std::find
		(gf->mesh_vertices.begin(), gf->mesh_vertices.end(), *itv);
	      if (itve != gf->mesh_vertices.end()){ count++; gf->mesh_vertices.erase(itve);}
	    }
	  }  
        }

      }
    }

    std::vector<std::vector<MEdge> > boundaries;
    int nbBounds = connected_bounds(myEdges, boundaries);   

    for (int ib  = 0; ib < nbBounds; ib++){
      std::vector<MEdge> myLines = boundaries[ib];

      int numE = getMaxElementaryNumber(1) + 1;
      discreteEdge *e = new discreteEdge(this, numE, 0, 0);
      //printf("*** Created discreteEdge %d \n", numE);

      add(e);
      discEdges.push_back(e);

      //fill new edge with mesh vertices
      std::set<MVertex*> allV;
      for(unsigned int i = 0; i < myLines.size(); i++) {
        MVertex *v0 = myLines[i].getVertex(0);
        MVertex *v1 = myLines[i].getVertex(1);
        e->lines.push_back(new MLine( v0, v1));
	allV.insert(v0);//before it was std::vector with find ??
	allV.insert(v1);
        v0->setEntity(e);
        v1->setEntity(e);
      }
      e->mesh_vertices.insert(e->mesh_vertices.begin(), allV.begin(),allV.end());

      for (std::vector<int>::iterator itFace = tagFaces.begin(); 
           itFace != tagFaces.end(); itFace++) {

        //delete new mesh vertices of edge from adjacent faces
        GFace *dFace = getFaceByTag(*itFace);
        for (std::set<MVertex*>::iterator itv = allV.begin();itv != allV.end(); itv++) {
          std::vector<MVertex*>::iterator itve = 
            std::find(dFace->mesh_vertices.begin(), dFace->mesh_vertices.end(), *itv);
          if (itve != dFace->mesh_vertices.end()) dFace->mesh_vertices.erase(itve);
        }
    
        //fill face2Edges with the new edge
        std::map<int, std::vector<int> >::iterator f2e = face2Edges.find(*itFace);
        if (f2e == face2Edges.end()){
          std::vector<int> tagEdges;
          tagEdges.push_back(numE);
          face2Edges.insert(std::make_pair(*itFace,tagEdges));
        }
        else{
          std::vector<int> tagEdges = f2e->second;
          tagEdges.push_back(numE);
          f2e->second = tagEdges;
        }

      }
     
    }
    if (map_edges.empty()) break;
  }

  // set boundary edges for each face
  for (std::vector<discreteFace*>::iterator it = discFaces.begin();
       it != discFaces.end(); it++){
    std::map<int, std::vector<int> >::iterator ite = face2Edges.find((*it)->tag());
    if (ite != face2Edges.end()){
      std::vector<int> bcEdges = ite->second;
      (*it)->setBoundEdges(bcEdges);
    }
  }

  // for each discreteEdge, create Topology
  std::map<GFace*, std::map<MVertex*, MVertex*, std::less<MVertex*> > > face2Vert;
  std::map<GRegion*, std::map<MVertex*, MVertex*, std::less<MVertex*> > > region2Vert;
  face2Vert.clear();
  region2Vert.clear();
  for (std::vector<discreteEdge*>::iterator it = discEdges.begin(); it != discEdges.end(); it++){
    (*it)->createTopo();
    (*it)->parametrize(face2Vert, region2Vert);
  }

  //fill edgeLoops of Faces or correct sign of l_edges
  // for (std::vector<discreteFace*>::iterator itF = discFaces.begin();
  //      itF != discFaces.end(); itF++){
  //   //EMI, TODO
  //   std::list<GEdgeLoop> edgeLoops = (*itF)->edgeLoops; 
  //   edgeLoops.clear();
  //   GEdgeLoop el((*itF)->edges());
  //   edgeLoops.push_back(el);
  // }


 //we need to recreate lines, triangles and tets
  //that contain those new MEdgeVertices
  for (std::map<GFace*, std::map<MVertex*, MVertex*, std::less<MVertex*> > >::iterator
         iFace= face2Vert.begin(); iFace != face2Vert.end(); iFace++){
    std::map<MVertex*, MVertex*, std::less<MVertex*> > old2new = iFace->second;
    GFace *gf = iFace->first;
    std::vector<MTriangle*> newTriangles;
    std::vector<MQuadrangle*> newQuadrangles;
    for (unsigned int i = 0; i < gf->getNumMeshElements(); ++i){
      MElement *e = gf->getMeshElement(i);
      int N = e->getNumVertices();
      std::vector<MVertex *> v(N);
      for(int j = 0; j < N; j++) v[j] = e->getVertex(j);
      for (int j = 0; j < N; j++){
        std::map<MVertex*, MVertex*, std::less<MVertex*> >::iterator itmap = old2new.find(v[j]);
        MVertex *vNEW;
        if (itmap != old2new.end())  {
          vNEW = itmap->second;
          v[j]=vNEW;
        }
      }
      if (N == 3) newTriangles.push_back(new  MTriangle(v[0], v[1], v[2]));
      else if ( N == 4)  newQuadrangles.push_back(new  MQuadrangle(v[0], v[1], v[2], v[3]));
      
    }
    gf->deleteVertexArrays();
    gf->triangles.clear();
    gf->triangles = newTriangles;
    gf->quadrangles.clear();
    gf->quadrangles = newQuadrangles;
  }

  for (std::map<GRegion*, std::map<MVertex*, MVertex*, std::less<MVertex*> > >::iterator
         iRegion= region2Vert.begin(); iRegion != region2Vert.end(); iRegion++){
    std::map<MVertex*, MVertex*, std::less<MVertex*> > old2new = iRegion->second;
    GRegion *gr = iRegion->first;
     std::vector<MTetrahedron*> newTetrahedra;
     std::vector<MHexahedron*> newHexahedra;
     std::vector<MPrism*> newPrisms;
     std::vector<MPyramid*> newPyramids;
     for (unsigned int i = 0; i < gr->getNumMeshElements(); ++i){
       MElement *e = gr->getMeshElement(i);
       int N = e->getNumVertices();
       std::vector<MVertex *> v(N);
       for(int j = 0; j < N; j++) v[j] = e->getVertex(j);
       for (int j = 0; j < N; j++){
         std::map<MVertex*, MVertex*, std::less<MVertex*> >::iterator itmap = old2new.find(v[j]);
         MVertex *vNEW;
         if (itmap != old2new.end())  {
           vNEW = itmap->second;
           v[j]=vNEW;
         }
       }
       if (N == 4) newTetrahedra.push_back(new MTetrahedron(v[0], v[1], v[2], v[3]));
       else if (N == 5) newPyramids.push_back(new MPyramid(v[0], v[1], v[2], v[3], v[4]));
       else if (N == 6) newPrisms.push_back(new MPrism(v[0], v[1], v[2], v[3], v[4], v[5]));
       else if (N == 8) newHexahedra.push_back(new MHexahedron(v[0], v[1], v[2], v[3],
                                                               v[4], v[5], v[6], v[7]));
     }
     gr->deleteVertexArrays();
     gr->tetrahedra.clear();
     gr->tetrahedra = newTetrahedra;
     gr->pyramids.clear();
     gr->pyramids = newPyramids;
     gr->prisms.clear();
     gr->prisms = newPrisms;
     gr->hexahedra.clear();
     gr->hexahedra = newHexahedra;
   }

}

GModel *GModel::buildCutGModel(gLevelset *ls, bool cutElem)
{
  std::map<int, std::vector<MElement*> > elements[10];
  std::map<int, std::map<int, std::string> > physicals[4];
  std::map<int, MVertex*> vertexMap;

  Msg::Info("Cutting mesh...");
  double t1 = Cpu();

  GModel *cutGM = buildCutMesh(this, ls, elements, vertexMap, physicals, cutElem);

  for(int i = 0; i < (int)(sizeof(elements) / sizeof(elements[0])); i++)
    cutGM->_storeElementsInEntities(elements[i]);

  cutGM->_associateEntityWithMeshVertices();

  cutGM->_storeVerticesInEntities(vertexMap);

  for(int i = 0; i < 4; i++)
    cutGM->_storePhysicalTagsInEntities(i, physicals[i]);

  double t2 = Cpu();

  Msg::Info("Mesh cutting complete (%g s)", t2 - t1);
  return cutGM;
}

void GModel::load(std::string fileName)
{
  GModel *temp = GModel::current();
  GModel::setCurrent(this);
  MergeFile(fileName, true);
  GModel::setCurrent(temp);
}

void GModel::save(std::string fileName)
{
  GModel *temp = GModel::current();
  GModel::setCurrent(this);
  int guess = GuessFileFormatFromFileName(fileName);
  CreateOutputFile(fileName, guess);
  GModel::setCurrent(temp);
}

GVertex *GModel::addVertex(double x, double y, double z, double lc)
{
  if(_factory) return _factory->addVertex(this, x, y, z, lc);
  return 0;
}

GEdge *GModel::addLine(GVertex *v1, GVertex *v2)
{
  if(_factory) return _factory->addLine(this, v1, v2);
  return 0;
}

GEdge *GModel::addCircleArcCenter(double x, double y, double z, GVertex *start, 
                                  GVertex *end)
{
  if(_factory)
    return _factory->addCircleArc(this, GModelFactory::CENTER_START_END,
                                  start, end, SPoint3(x, y, z));
  return 0;
}

GEdge *GModel::addCircleArc3Points(double x, double y, double z, GVertex *start,
                                   GVertex *end)
{
  if(_factory) 
    return _factory->addCircleArc(this, GModelFactory::THREE_POINTS, 
                                  start, end, SPoint3(x, y, z));
  return 0;
}

GEdge *GModel::addBezier(GVertex *start, GVertex *end, 
			 std::vector<std::vector<double> > points) 
{
  if(_factory) 
    return _factory->addSpline(this, GModelFactory::BEZIER, start, end, 
                               points);
  return 0;
}

GEdge *GModel::addNURBS(GVertex *start, GVertex *end,
			std::vector<std::vector<double> > points, 
			std::vector<double> knots,
			std::vector<double> weights, 
			std::vector<int> mult){  
  if(_factory)
    return _factory->addNURBS(this, start,end,points,knots,weights, mult);
  return 0;
}

void GModel::addRuledFaces (std::vector<std::vector<GEdge *> > edges){
  if(_factory)
    _factory->addRuledFaces(this, edges);
}

GFace* GModel::addFace (std::vector<GEdge *> edges, std::vector< std::vector<double > > points){
  if(_factory)
    return _factory->addFace(this, edges, points);
  return 0;
}


GFace* GModel::addPlanarFace (std::vector<std::vector<GEdge *> > edges){
  if(_factory)
    return _factory->addPlanarFace(this, edges);
  return 0;
}

GRegion* GModel::addVolume (std::vector<std::vector<GFace *> > faces){
  if(_factory)
    return _factory->addVolume(this, faces);
  return 0;
}

GEntity *GModel::revolve(GEntity *e, std::vector<double> p1, std::vector<double> p2,
                         double angle)
{
  if(_factory)
    return _factory->revolve(this, e, p1, p2, angle);
  return 0;
}

GEntity *GModel::extrude(GEntity *e, std::vector<double> p1, std::vector<double> p2)
{
  if(_factory) 
    return _factory->extrude(this, e, p1, p2);
  return 0;
}

void GModel::createBoundaryLayer(std::vector<GEntity *> e, double h)
{
#if defined(HAVE_MESH)
  // FIXME remove this!
  Field *f = _fields->newField(0, "MathEval");
  char s[128];
  sprintf(s, "%g", h);
  f->options["F"]->string() = s;

  GModel *gm = this;
  for(unsigned int i = 0; i < e.size(); i++){
    if(e[i]->dim() == 1){
      GEdge *ge = static_cast<GEdge*>(e[i]);
      GVertex *beg = ge->getBeginVertex();
      GVertex *end = ge->getEndVertex();
      GVertex *v0 = new boundaryLayerVertex(gm, gm->getMaxElementaryNumber(0) + 1, beg);
      gm->add(v0);
      GVertex *v1 = new boundaryLayerVertex(gm, gm->getMaxElementaryNumber(0) + 1, end);
      gm->add(v1);
      GEdge *e0 = new boundaryLayerEdge(gm, gm->getMaxElementaryNumber(1) + 1, v0, v1, ge);
      gm->add(e0);
      GEdge *e1 = new boundaryLayerEdge(gm, gm->getMaxElementaryNumber(1) + 1, beg, v0, beg);
      gm->add(e1);
      GEdge *e2 = new boundaryLayerEdge(gm, gm->getMaxElementaryNumber(1) + 1, end, v1, end);
      gm->add(e2);
      boundaryLayerFace *f0 = new boundaryLayerFace(gm, gm->getMaxElementaryNumber(2) + 1, e, f, ge);
      std::vector<int> boundEdges;
      boundEdges.push_back(ge->tag());
      boundEdges.push_back(e0->tag());
      boundEdges.push_back(e1->tag());
      boundEdges.push_back(e2->tag());
      f0->setBoundEdges(boundEdges);
      gm->add(f0);
    }
    else{
      Msg::Error("FIXME: GModel boundary layer creation only for edges for now");
    }
  }

  glue(CTX::instance()->geom.tolerance);
#endif
}

GEntity *GModel::addPipe(GEntity *e, std::vector<GEdge *>  edges){
  if(_factory) 
    return _factory->addPipe(this,e,edges);
  return 0;
}

GEntity *GModel::addSphere(double cx, double cy, double cz, double radius)
{
  if(_factory) return _factory->addSphere(this, cx, cy, cz, radius);
  return 0;
}

GEntity *GModel::addCylinder(std::vector<double> p1, std::vector<double> p2, 
                             double radius)
{
  if(_factory) return _factory->addCylinder(this, p1, p2, radius);
  return 0;
}

GEntity *GModel::addTorus(std::vector<double> p1, std::vector<double> p2, 
                          double radius1, double radius2)
{
  if(_factory) return _factory->addTorus(this, p1, p2, radius1, radius2);
  return 0;
}

GEntity *GModel::addBlock(std::vector<double> p1, std::vector<double> p2)
{
  if(_factory) return _factory->addBlock(this, p1, p2);
  return 0;
}

GEntity *GModel::addCone(std::vector<double> p1, std::vector<double> p2,
                         double radius1, double radius2)
{
  if(_factory) return _factory->addCone(this, p1, p2,radius1, radius2);
  return 0;
}

GModel *GModel::computeBooleanUnion(GModel *tool, int createNewModel)
{
  if(_factory)
    return _factory->computeBooleanUnion(this, tool, createNewModel);
  return 0;
}

GModel *GModel::computeBooleanIntersection(GModel *tool, int createNewModel)
{
  if(_factory)
    return _factory->computeBooleanIntersection(this, tool, createNewModel);
  return 0;
}

GModel *GModel::computeBooleanDifference(GModel *tool, int createNewModel)
{
  if(_factory)
    return _factory->computeBooleanDifference(this, tool, createNewModel);
  return 0;
}

static void computeDuplicates(GModel *model, 
                              std::multimap<GVertex*, GVertex*> &Unique2Duplicates,
                              std::map<GVertex*, GVertex*> &Duplicates2Unique,
                              const double &eps)
{
  // FIXME: currently we use a greedy algorithm in n^2 (using a
  // kd-tree: cf. MVertexPositionSet)
  // FIXME: add option to remove orphaned entities after duplicate check
  std::list<GVertex*> v;
  v.insert(v.begin(), model->firstVertex(), model->lastVertex());

  while(!v.empty()){
    GVertex *pv = *v.begin();
    v.erase(v.begin());
    bool found = false;
    for (std::multimap<GVertex*,GVertex*>::iterator it = Unique2Duplicates.begin(); 
         it != Unique2Duplicates.end(); ++it){
      GVertex *unique = it->first;
      const double d = sqrt((unique->x() - pv->x()) * (unique->x() - pv->x()) +
                            (unique->y() - pv->y()) * (unique->y() - pv->y()) +
                            (unique->z() - pv->z()) * (unique->z() - pv->z()));
      if (d <= eps && pv->geomType() == unique->geomType()) {
	found = true;
	Unique2Duplicates.insert(std::make_pair(unique, pv));
	Duplicates2Unique[pv] = unique;
	break;
      }
    }
    if (!found) {
      Unique2Duplicates.insert(std::make_pair(pv, pv));
      Duplicates2Unique[pv] = pv;
    }
  }  
}

static void glueVerticesInEdges(GModel *model, 
                                std::multimap<GVertex*, GVertex*> &Unique2Duplicates,
				std::map<GVertex*, GVertex*> &Duplicates2Unique)
{
  Msg::Debug("Gluing Edges");
  for (GModel::eiter it = model->firstEdge(); it != model->lastEdge(); ++it){
    GEdge *e = *it;
    GVertex *v1 = e->getBeginVertex();
    GVertex *v2 = e->getEndVertex();
    GVertex *replacementOfv1 = Duplicates2Unique[v1];
    GVertex *replacementOfv2 = Duplicates2Unique[v2];
    if ((v1 != replacementOfv1) || (v2 != replacementOfv2)){
      Msg::Debug("Model Edge %d is re-build", e->tag());
      e->replaceEndingPoints (replacementOfv1, replacementOfv2);
    }
  }
}

static void computeDuplicates(GModel *model,
                              std::multimap<GEdge*, GEdge*> &Unique2Duplicates,
                              std::map<GEdge*,GEdge*> &Duplicates2Unique,
                              const double &eps)
{
  std::list<GEdge*> e;
  e.insert(e.begin(), model->firstEdge(), model->lastEdge());

  while(!e.empty()){
    GEdge *pe = *e.begin();
    e.erase(e.begin());
    bool found = false;
    for (std::multimap<GEdge*,GEdge*>::iterator it = Unique2Duplicates.begin(); 
         it != Unique2Duplicates.end(); ++it ){
      GEdge *unique = it->first;
      // first check edges that have same endpoints
      if (((unique->getBeginVertex() == pe->getBeginVertex() && 
            unique->getEndVertex() == pe->getEndVertex()) ||
           (unique->getEndVertex() == pe->getBeginVertex() && 
            unique->getBeginVertex() == pe->getEndVertex())) &&
          unique->geomType() == pe->geomType()){
	if ((unique->geomType() == GEntity::Line && pe->geomType() == GEntity::Line) ||
            unique->geomType() == GEntity::DiscreteCurve || 
            pe->geomType() == GEntity::DiscreteCurve ||
            unique->geomType() == GEntity::BoundaryLayerCurve || 
            pe->geomType() == GEntity::BoundaryLayerCurve){
	  found = true;
	  Unique2Duplicates.insert(std::make_pair(unique,pe));
	  Duplicates2Unique[pe] = unique;
	  break;	  
	} 
        // compute a point
        Range<double> r = pe->parBounds(0);
        GPoint gp = pe->point(0.5 * (r.low() + r.high()));
	double t;
	GPoint gp2 = pe->closestPoint(SPoint3(gp.x(),gp.y(),gp.z()),t);
	const double d = sqrt((gp.x() - gp2.x()) * (gp.x() - gp2.x()) +
			      (gp.y() - gp2.y()) * (gp.y() - gp2.y()) +
			      (gp.z() - gp2.z()) * (gp.z() - gp2.z()));
	if (t >= r.low() && t <= r.high() && d <= eps) {
	  found = true;
	  Unique2Duplicates.insert(std::make_pair(unique,pe));
	  Duplicates2Unique[pe] = unique;
	  break;
	}
      }
    }
    if (!found) {
      Unique2Duplicates.insert(std::make_pair(pe,pe));
      Duplicates2Unique[pe] = pe;
    }
  }
}

static void glueEdgesInFaces(GModel *model, 
                             std::multimap<GEdge*, GEdge*> &Unique2Duplicates,
			     std::map<GEdge*, GEdge*> &Duplicates2Unique)
{
  Msg::Debug("Gluing Model Faces");
  for (GModel::fiter it = model->firstFace(); it != model->lastFace(); ++it){
    GFace *f = *it;
    bool aDifferenceExists = false;
    std::list<GEdge*> old = f->edges(), enew;
    for (std::list<GEdge*>::iterator eit = old.begin(); eit !=old.end(); eit++){
      GEdge *temp = Duplicates2Unique[*eit];
      enew.push_back(temp);
      if (temp != *eit){
	aDifferenceExists = true;
      }
    }
    if (aDifferenceExists){
      Msg::Debug("Model Face %d is re-build", f->tag());
      f->replaceEdges (enew);
    }
  }
}

static void computeDuplicates(GModel *model, 
                              std::multimap<GFace*, GFace*> &Unique2Duplicates,
                              std::map<GFace*,GFace*> &Duplicates2Unique, 
                              const double &eps)
{
  std::list<GFace*> f;
  f.insert(f.begin(),model->firstFace(),model->lastFace());

  while(!f.empty()){
    GFace *pf = *f.begin();
    Range<double> r = pf->parBounds(0);
    Range<double> s = pf->parBounds(1);
    f.erase(f.begin());
    std::list<GEdge*> pf_edges = pf->edges();
    pf_edges.sort();
    bool found = false;
    for (std::multimap<GFace*,GFace*>::iterator it = Unique2Duplicates.begin();
         it != Unique2Duplicates.end(); ++it){
      GFace *unique = it->first;      
      std::list<GEdge*> unique_edges = unique->edges();
      if (pf->geomType() == unique->geomType() && 
          unique_edges.size() == pf_edges.size()){
	unique_edges.sort();
	std::list<GEdge*>::iterator it_pf = pf_edges.begin();
	std::list<GEdge*>::iterator it_unique = unique_edges.begin();
	bool all_similar = true;
	// first check faces that have same edges
	for (; it_pf !=  pf_edges.end() ;  ++it_pf,it_unique++){
	  if (*it_pf != *it_unique) all_similar = false;
	}
	if (all_similar){
	  if (unique->geomType() == GEntity::Plane && pf->geomType() == GEntity::Plane){
	    found = true;
	    Unique2Duplicates.insert(std::make_pair(unique,pf));
	    Duplicates2Unique[pf] = unique;
	    break;	  
	  } 
	  double t[2]={0,0};
          // FIXME: evaluate a point on the surface (use e.g. buildRepresentationCross)
	  const double d = 1.0; 
	  if (t[0] >= r.low() && t[0] <= r.high() && 
	      t[1] >= s.low() && t[1] <= s.high() && d <= eps) {
	    found = true;
	    Unique2Duplicates.insert(std::make_pair(unique,pf));
	    Duplicates2Unique[pf] = unique;
	    break;
	  }
	}
      }
    }
    if (!found) {
      Unique2Duplicates.insert(std::make_pair(pf,pf));
      Duplicates2Unique[pf] = pf;
    }
  }
}

static void glueFacesInRegions(GModel *model,
                               std::multimap<GFace*, GFace*> &Unique2Duplicates,
			       std::map<GFace*, GFace*> &Duplicates2Unique)
{
  Msg::Debug("Gluing Regions");
  for (GModel::riter it = model->firstRegion(); it != model->lastRegion();++it){
    GRegion *r = *it;
    bool aDifferenceExists = false;
    std::list<GFace*> old = r->faces(), fnew;
    for (std::list<GFace*>::iterator fit = old.begin(); fit != old.end(); fit++){
      std::map<GFace*, GFace*>::iterator itR = Duplicates2Unique.find(*fit);
      if (itR == Duplicates2Unique.end()){
        Msg::Fatal("Error in the gluing process");
      }
      GFace *temp = itR->second;;
      fnew.push_back(temp);
      if (temp != *fit){
	aDifferenceExists = true;
      }
    }
    if (aDifferenceExists){
      Msg::Debug("Model Region %d is re-build", r->tag());
      r->replaceFaces (fnew);
    }
  }
}

void GModel::glue(double eps)
{
  {
    std::multimap<GVertex*,GVertex*> Unique2Duplicates;
    std::map<GVertex*,GVertex*> Duplicates2Unique;
    computeDuplicates(this, Unique2Duplicates, Duplicates2Unique, eps);
    glueVerticesInEdges(this, Unique2Duplicates, Duplicates2Unique);
  }
  {
    std::multimap<GEdge*,GEdge*> Unique2Duplicates;
    std::map<GEdge*,GEdge*> Duplicates2Unique;
    computeDuplicates(this, Unique2Duplicates, Duplicates2Unique, eps);
    glueEdgesInFaces(this, Unique2Duplicates, Duplicates2Unique);
  }    
  {
    std::multimap<GFace*,GFace*> Unique2Duplicates;
    std::map<GFace*,GFace*> Duplicates2Unique;
    computeDuplicates(this, Unique2Duplicates, Duplicates2Unique, eps);
    glueFacesInRegions(this, Unique2Duplicates, Duplicates2Unique);
  }    
}

void GModel::insertRegion(GRegion *r)
{
  regions.insert(r);
}

GEdge *getNewModelEdge(GFace *gf1, GFace *gf2, 
                       std::map<std::pair<int, int>, GEdge*> &newEdges)
{
  int t1 = gf1 ? gf1->tag() : -1;
  int t2 = gf2 ? gf2->tag() : -1;
  int i1 = std::min(t1, t2);
  int i2 = std::max(t1, t2);

  if(i1 == i2) return 0;

  std::map<std::pair<int, int>, GEdge*>::iterator it = 
    newEdges.find(std::make_pair<int, int>(i1, i2));
  if(it == newEdges.end()){
    discreteEdge *ge = new discreteEdge
      (GModel::current(), GModel::current()->getMaxElementaryNumber(1) + 1, 0, 0);
    GModel::current()->add(ge);
    newEdges[std::make_pair<int, int>(i1, i2)] = ge;
    return ge;
  }
  else
    return it->second;  
}

#if defined(HAVE_MESH)

void recurClassifyEdges(MTri3 *t, std::map<MTriangle*, GFace*> &reverse,
                        std::map<MLine*, GEdge*, compareMLinePtr> &lines,
                        std::set<MLine*> &touched, std::set<MTri3*> &trisTouched,
                        std::map<std::pair<int, int>, GEdge*> &newEdges)
{
  if(!t->isDeleted()){
    trisTouched.erase(t);
    t->setDeleted(true);
    GFace *gf1 = reverse[t->tri()];
    for(int i = 0; i < 3; i++){
      GFace *gf2 = 0;
      MTri3 *tn = t->getNeigh(i);
      if(tn)
        gf2 = reverse[tn->tri()];
      edgeXface exf(t, i);
      MLine ml(exf.v[0], exf.v[1]);
      std::map<MLine*, GEdge*, compareMLinePtr>::iterator it = lines.find(&ml);
      if(it != lines.end()){
        if(touched.find(it->first) == touched.end()){
          GEdge *ge =  getNewModelEdge(gf1, gf2, newEdges);
          if(ge) ge->lines.push_back(it->first);
          touched.insert(it->first);
        }
      }
      if(tn)
        recurClassifyEdges(tn, reverse, lines, touched, trisTouched,newEdges);
    }
  }
}

void recurClassify(MTri3 *t, GFace *gf,
                   std::map<MLine*, GEdge*, compareMLinePtr> &lines,
                   std::map<MTriangle*, GFace*> &reverse)
{
  if(!t->isDeleted()){
    gf->triangles.push_back(t->tri());
    reverse[t->tri()] = gf;
    t->setDeleted(true);
    for(int i = 0; i < 3; i++){
      MTri3 *tn = t->getNeigh(i);
      if(tn){
        edgeXface exf(t, i);
        MLine ml(exf.v[0], exf.v[1]);       
        std::map<MLine*, GEdge*, compareMLinePtr>::iterator it = lines.find(&ml);
        if(it == lines.end())
          recurClassify(tn, gf, lines, reverse);
      }
    }  
  }
}

#endif

void GModel::detectEdges(double _tresholdAngle)
{
#if defined(HAVE_MESH)
  e2t_cont adj;
  std::vector<MTriangle*> elements;
  std::vector<edge_angle> edges_detected, edges_lonly;
  for(GModel::fiter it = GModel::current()->firstFace(); 
      it != GModel::current()->lastFace(); ++it)
    elements.insert(elements.end(), (*it)->triangles.begin(), 
		    (*it)->triangles.end());  
  buildEdgeToTriangle(elements, adj);
  buildListOfEdgeAngle(adj, edges_detected, edges_lonly);
  GEdge *selected = new discreteEdge
    (this, getMaxElementaryNumber(1) + 1, 0, 0);
  add(selected);

  for(unsigned int i = 0; i < edges_detected.size(); i++){
    edge_angle ea = edges_detected[i];
    if(ea.angle <= _tresholdAngle) break;
    selected->lines.push_back(new MLine(ea.v1, ea.v2));
  } 
	
  for(unsigned int i = 0 ; i < edges_lonly.size(); i++){
    edge_angle ea = edges_lonly[i];
    selected->lines.push_back(new MLine(ea.v1, ea.v2));
  } 
  std::set<GFace*> _temp;
  _temp.insert(faces.begin(),faces.end());
  classifyFaces(_temp);
  remove(selected);
  //  delete selected;
#endif
}

void GModel::classifyFaces(std::set<GFace*> &_faces) 
{
#if defined(HAVE_MESH)
  std::map<MLine*, GEdge*, compareMLinePtr> lines;

  for(GModel::eiter it = GModel::current()->firstEdge(); 
      it != GModel::current()->lastEdge(); ++it){
    for(unsigned int i = 0; i < (*it)->lines.size();i++) 
      lines[(*it)->lines[i]] = *it;
  }

  std::map<MTriangle*, GFace*> reverse_old;
  std::list<MTri3*> tris;
  {
    std::set<GFace*>::iterator it = _faces.begin();
    while(it != _faces.end()){
      GFace *gf = *it;
      for(unsigned int i = 0; i < gf->triangles.size(); i++){
        tris.push_back(new MTri3(gf->triangles[i], 0));
	reverse_old[gf->triangles[i]] = gf;
      }
      gf->triangles.clear();
      gf->mesh_vertices.clear();
      ++it;
    }
  }
  if(tris.empty()) return;

  connectTriangles(tris);

  std::map<MTriangle*, GFace*> reverse;
  std::multimap<GFace*, GFace*> replacedBy;
  // color all triangles
  std::list<MTri3*> ::iterator it = tris.begin();
  std::list<GFace*> newf;
  while(it != tris.end()){
    if(!(*it)->isDeleted()){
      discreteFace *gf = new discreteFace
        (GModel::current(), GModel::current()->getMaxElementaryNumber(2) + 1);
      recurClassify(*it, gf, lines, reverse);
      GModel::current()->add(gf);
      newf.push_back(gf);
      
      for (unsigned int i = 0; i < gf->triangles.size(); i++){
	replacedBy.insert(std::make_pair(reverse_old[gf->triangles[i]],gf));
      }
    }
    ++it;
  }

  // now we have all faces coloured. If some regions were existing, replace
  // their faces by the new ones

  for (riter rit = firstRegion(); rit != lastRegion(); ++rit){
    std::list<GFace *> _xfaces = (*rit)->faces();
    std::set<GFace *> _newFaces;
    for (std::list<GFace *>::iterator itf = _xfaces.begin(); itf != _xfaces.end(); ++itf){
      std::multimap<GFace*, GFace*>::iterator itLow = replacedBy.lower_bound(*itf);
      std::multimap<GFace*, GFace*>::iterator itUp = replacedBy.upper_bound(*itf);
      for (; itLow != itUp; ++itLow)
	_newFaces.insert(itLow->second);
    }
    std::list<GFace *> _temp;
    _temp.insert(_temp.begin(),_newFaces.begin(),_newFaces.end());
    (*rit)->set(_temp);
  }

  // color some lines
  it = tris.begin();
  while(it != tris.end()){
    (*it)->setDeleted(false);
    ++it;
  }
  
  // classify edges that are bound by different GFaces
  std::map<std::pair<int, int>, GEdge*> newEdges;
  std::set<MLine*> touched;
  std::set<MTri3*> trisTouched;
  // bug fix : multiply connected domains
  
  trisTouched.insert(tris.begin(),tris.end());
  while(!trisTouched.empty())
    recurClassifyEdges(*trisTouched.begin(), reverse, lines, touched, trisTouched,newEdges);

  std::map<discreteFace*,std::vector<int> > newFaceTopology;
  
  // check if new edges should not be splitted 
  // splitted if composed of several open or closed edges

  std::map<MVertex*,GVertex*> modelVertices;

  for (std::map<std::pair<int, int>, GEdge*>::iterator ite = newEdges.begin();
       ite != newEdges.end() ; ++ite){
    std::list<MLine*> allSegments;
    for(unsigned int i = 0; i < ite->second->lines.size(); i++)
      allSegments.push_back(ite->second->lines[i]);

    while (!allSegments.empty()) {
      std::list<MLine*> segmentsForThisDiscreteEdge;
      MVertex *vB = (*allSegments.begin())->getVertex(0);
      MVertex *vE = (*allSegments.begin())->getVertex(1);
      segmentsForThisDiscreteEdge.push_back(*allSegments.begin());
      allSegments.erase(allSegments.begin());
      while(1){
	bool found = false;
	for (std::list<MLine*>::iterator it = allSegments.begin();
             it != allSegments.end(); ++it){ 
	  MVertex *v1 = (*it)->getVertex(0);
	  MVertex *v2 = (*it)->getVertex(1);
	  if (v1 == vE || v2 == vE){
	    segmentsForThisDiscreteEdge.push_back(*it);
	    if (v2 == vE)(*it)->revert();
	    vE = (v1 == vE) ? v2 : v1;
	    found = true;
	    allSegments.erase(it);
	    break;	  
	  }
	  if (v1 == vB || v2 == vB){
	    segmentsForThisDiscreteEdge.push_front(*it);
	    if (v1 == vB)(*it)->revert();
	    vB = (v1 == vB) ? v2 : v1;
	    found = true;
	    allSegments.erase(it);
	    break;	  
	  }
	}
	if (vE == vB)break;
	if (!found)break;
      }

      std::map<MVertex*,GVertex*>::iterator itMV = modelVertices.find(vB);
      if (itMV == modelVertices.end()){
	GVertex *newGv = new discreteVertex
          (GModel::current(), GModel::current()->getMaxElementaryNumber(0) + 1);
	newGv->mesh_vertices.push_back(vB);
	vB->setEntity(newGv);	
	newGv->points.push_back(new MPoint(vB));
	GModel::current()->add(newGv);
	modelVertices[vB] = newGv;
      }
      itMV = modelVertices.find(vE);
      if (itMV == modelVertices.end()){
	GVertex *newGv = new discreteVertex
          (GModel::current(), GModel::current()->getMaxElementaryNumber(0) + 1);
	newGv->mesh_vertices.push_back(vE);
	newGv->points.push_back(new MPoint(vE));
	vE->setEntity(newGv);	
	GModel::current()->add(newGv);
	modelVertices[vE] = newGv;
      }

      GEdge *newGe = new discreteEdge
        (GModel::current(), GModel::current()->getMaxElementaryNumber(1) + 1,
         modelVertices[vB], modelVertices[vE]);
      newGe->lines.insert(newGe->lines.end(), segmentsForThisDiscreteEdge.begin(),
                          segmentsForThisDiscreteEdge.end());

      for (std::list<MLine*>::iterator itL =  segmentsForThisDiscreteEdge.begin();
	   itL !=  segmentsForThisDiscreteEdge.end(); ++itL){
	if((*itL)->getVertex(0)->onWhat()->dim() != 0){
	  newGe->mesh_vertices.push_back((*itL)->getVertex(0));
	  (*itL)->getVertex(0)->setEntity(newGe);
	}
      }

      GModel::current()->add(newGe);
      discreteFace *gf1 = dynamic_cast<discreteFace*>
        (GModel::current()->getFaceByTag(ite->first.first));
      discreteFace *gf2 = dynamic_cast<discreteFace*> 
        (GModel::current()->getFaceByTag(ite->first.second));
      if (gf1)newFaceTopology[gf1].push_back(newGe->tag());
      if (gf2)newFaceTopology[gf2].push_back(newGe->tag());
    }
  }

  std::map<discreteFace*,std::vector<int> >::iterator itFT =  newFaceTopology.begin();
  for (;itFT != newFaceTopology.end();++itFT){
    itFT->first->setBoundEdges(itFT->second);
  }

  for (std::map<std::pair<int, int>, GEdge*>::iterator it = newEdges.begin();
       it != newEdges.end(); ++it){
    GEdge *ge = it->second;
    GModel::current()->remove(ge);
    //    delete ge;
  }
  
  it = tris.begin();
  while(it != tris.end()){
    delete *it;
    ++it;
  }

  // delete empty mesh faces and reclasssify
  std::set<GFace*, GEntityLessThan> fac = faces;
  for (fiter fit = fac.begin() ; fit !=fac.end() ; ++fit){
    std::set<MVertex *> _verts;
    (*fit)->mesh_vertices.clear();
    for (unsigned int i = 0; i < (*fit)->triangles.size(); i++){
      for (int j = 0; j < 3; j++){
	if ((*fit)->triangles[i]->getVertex(j)->onWhat()->dim() > 1){
	  (*fit)->triangles[i]->getVertex(j)->setEntity(*fit);
	  _verts.insert((*fit)->triangles[i]->getVertex(j));
	}
      }      
    }
    if ((*fit)->triangles.size())
      (*fit)->mesh_vertices.insert((*fit)->mesh_vertices.begin(),
                                   _verts.begin(), _verts.end());
    else
      remove(*fit);
  }
#endif
}


#include "Bindings.h"

void GModel::registerBindings(binding *b)
{
  classBinding *cb = b->addClass<GModel>("GModel");
  cb->setDescription("A GModel contains a geometry and its mesh.");

  methodBinding *cm;
  cm = cb->addMethod("mesh", &GModel::mesh);
  cm->setArgNames("dim", NULL);
  cm->setDescription("Generate a mesh of this model in dimension 'dim'.");
  cm = cb->addMethod("load", &GModel::load);
  cm->setDescription("Merge the file 'filename' in this model, the file can be "
                     "in any format (guessed from the extension) known by gmsh.");
  cm->setArgNames("filename", NULL);
  cm = cb->addMethod("setFactory", &GModel::setFactory);
  cm->setDescription("Set the GModel factory: choose between 'Gmsh' or 'OpenCASCADE'.");
  cm->setArgNames("name", NULL);
  cm = cb->addMethod("save", &GModel::save);
  cm->setDescription("Save this model in the file 'filename'. The content of the "
                     "file depends on the format (guessed from the extension).");
  cm->setArgNames("filename", NULL);
  cm = cb->addMethod("getNumMeshElements", (int (GModel::*)())&GModel::getNumMeshElements);
  cm->setDescription("return the number of mesh elemnts in the model");
  cm = cb->addMethod("getMeshElementByTag", &GModel::getMeshElementByTag);
  cm->setArgNames("tag", NULL);
  cm->setDescription("access a mesh element by tag, using the element cache");
  cm = cb->addMethod("getNumMeshVertices", &GModel::getNumMeshVertices);
  cm->setDescription("return the total number of vertices in the mesh");
  cm = cb->addMethod("getMeshVertexByTag", &GModel::getMeshVertexByTag);
  cm->setDescription("access a mesh vertex by tag, using the vertex cache");
  cm->setArgNames("tag", NULL);
  cm = cb->addMethod("getNumRegions", &GModel::getNumRegions);
  cm->setDescription("return the number of rgions (3D geometrical entities)");
  cm = cb->addMethod("getNumFaces", &GModel::getNumFaces);
  cm->setDescription("return the number of faces (2D geometrical entities)");
  cm = cb->addMethod("getNumEdges", &GModel::getNumEdges);
  cm->setDescription("return the number of edges (1D geometrical entities)");
  cm = cb->addMethod("getNumVertices", &GModel::getNumVertices);
  cm->setDescription("return the number of vertices (0D geometrical entities)");
  cm = cb->addMethod("getFaceByTag", &GModel::getFaceByTag);
  cm->setDescription("access a geometrical face by tag");
  cm->setArgNames("tag", NULL);
  cm = cb->addMethod("getEdgeByTag", &GModel::getEdgeByTag);
  cm->setDescription("access a geometrical edge by tag");
  cm->setArgNames("tag", NULL);
  cm = cb->addMethod("getVertexByTag", &GModel::getVertexByTag);
  cm->setDescription("access a geometrical vertex by tag");
  cm->setArgNames("tag", NULL);
  cm = cb->addMethod("getRegionByTag", &GModel::getRegionByTag);
  cm->setDescription("access a geometrical region by tag");
  cm->setArgNames("tag", NULL);

  cm = cb->addMethod("insertRegion", &GModel::insertRegion);
  cm->setDescription("insert an existing region to the model list");
  cm->setArgNames("region", NULL);
  cm = cb->addMethod("getRegions", &GModel::bindingsGetRegions);
  cm->setDescription("return a vector of the regions");
  cm = cb->addMethod("getFaces", &GModel::bindingsGetFaces);
  cm->setDescription("return a vector of the faces");
  cm = cb->addMethod("getEdges", &GModel::bindingsGetEdges);
  cm->setDescription("return a vector of the edges");
  cm = cb->addMethod("getVertices", &GModel::bindingsGetVertices);
  cm->setDescription("return a vector of the vertices");

  cm = cb->addMethod("addVertex", &GModel::addVertex);
  cm->setDescription("create a new vertex at position (x, y, z), with target "
                     "mesh size lc");
  cm->setArgNames("x", "y", "z", "lc", NULL);
  cm = cb->addMethod("addLine", &GModel::addLine);
  cm->setDescription("create a straight line going from v1 to v2");
  cm->setArgNames("v1", "v2", NULL);
  cm = cb->addMethod("addBezier", &GModel::addBezier);
  cm->setDescription("create a spline going from v1 to v2 and with some control "
                     "points listed in a fullMatrix(N,3)");
  cm->setArgNames("v1", "v2", "controlPoints", NULL);
  cm = cb->addMethod("addNURBS", &GModel::addNURBS);
  cm->setDescription("creates a NURBS curve from v1 to v2 with control Points, "
                     "knots, weights and multiplicities");
  cm->setArgNames("v1", "v2", "{{poles}}","{knots}","{weights}","{mult}",NULL);
  cm = cb->addMethod("addFace", &GModel::addFace);
  cm->setDescription("creates a face that is constraint by edges and points");
  cm->setArgNames("{list of edges}","{{x,y,z},{x,y,z},...}",NULL);
  cm = cb->addMethod("addRuledFaces", &GModel::addRuledFaces);
  cm->setDescription("create ruled faces that contains a list of wires");
  cm->setArgNames("{{list of edges},{list of edges},...}",NULL);
  cm = cb->addMethod("addPlanarFace", &GModel::addPlanarFace);
  cm->setDescription("creates a planar face that contains a list of wires");
  cm->setArgNames("{{list of edges},{list of edges},...}",NULL);
  cm = cb->addMethod("addVolume", &GModel::addVolume);
  cm->setDescription("creates a Volume bounded by a list of faces");
  cm->setArgNames("{{list of faces},{list of faces},...}",NULL);
  cm = cb->addMethod("addPipe", &GModel::addPipe);
  cm->setDescription("creates a pipe with a base and a wire for the direction");
  cm->setArgNames("ge","{list of edges}",NULL);
  cm = cb->addMethod("addCircleArcCenter", &GModel::addCircleArcCenter);
  cm->setDescription("create a circle arc going from v1 to v2 with its center "
                     "at (x,y,z)");
  cm->setArgNames("x", "y", "z", "v1", "v2", NULL);
  cm = cb->addMethod("addCircleArc3Points", &GModel::addCircleArc3Points);
  cm->setDescription("create a circle arc going from v1 to v2 with an "
                     "intermediary point Xi(x,y,z)");
  cm->setArgNames("x", "y", "z", "v1", "v2", NULL);
  cm = cb->addMethod("revolve", &GModel::revolve);
  cm->setDescription("revolve an entity of a given angle. Axis is defined by 2 "
                     "points");
  cm->setArgNames("entity", "{x1,y1,z1}", "{x2,y2,z2}", "angle", NULL);
  cm = cb->addMethod("extrude", &GModel::extrude);
  cm->setDescription("extrudes an entity. Axis is defined by 2 points");
  cm->setArgNames("entity", "{x1,y1,z1}", "{x2,y2,z2}", NULL);
  cm = cb->addMethod("addSphere", &GModel::addSphere);
  cm->setDescription("add a sphere");
  cm->setArgNames("xc", "yc", "zc", "radius", NULL);
  cm = cb->addMethod("addBlock", &GModel::addBlock);
  cm->setDescription("add a block");
  cm->setArgNames("{x1,y1,z1}", "{x2,y2,z2}", NULL);
  cm = cb->addMethod("addCone", &GModel::addCone);
  cm->setDescription("add a cone");
  cm->setArgNames("{x1,y1,z1}","{x2,y2,z2}","R1","R2",NULL);
  cm = cb->addMethod("addCylinder", &GModel::addCylinder);
  cm->setDescription("add a cylinder");
  cm->setArgNames("{x1,y1,z1}","{x2,y2,z2}", "R",NULL);
  cm = cb->addMethod("addTorus", &GModel::addTorus);
  cm->setDescription("add a torus");
  cm->setArgNames("{x1,y1,z1}","{x2,y2,z2}","R1","R2",NULL);
  cm = cb->addMethod("computeUnion", &GModel::computeBooleanUnion);
  cm->setDescription("compute the boolean union of the model with another one "
                     "(tool). The third parameter tells if a new model has to "
                     "be created");
  cm->setArgNames("tool", "createNewGModel",NULL);
  cm = cb->addMethod("computeIntersection", &GModel::computeBooleanIntersection);
  cm->setDescription("compute the boolean intersection of the model with another "
                     "one. The third parameter tells if a new model has to be created");
  cm->setArgNames("tool","createNewGModel",NULL);
  cm = cb->addMethod("computeDifference", &GModel::computeBooleanDifference);
  cm->setDescription("compute the boolean difference of the model with another "
                     "one (tool). The third parameter tells if a new model has to "
                     "be created");
  cm->setArgNames("tool","createNewGModel",NULL);
  cm = cb->addMethod("glue", &GModel::glue);
  cm->setDescription("glue the geometric model using geometric tolerance eps");
  cm->setArgNames("eps",NULL);
  cm = cb->addMethod("setAsCurrent", &GModel::setAsCurrent);
  cm->setDescription("set the model as the current (active) one");
  cm = cb->setConstructor<GModel>();
  cm->setDescription("Create an empty GModel");

  cm = cb->addMethod("getPhysicalName", &GModel::getPhysicalName);
  cm->setDescription("get the name of an physical group, identified by its "
                     "dimension and number. Returns empty string if physical "
                     "name is not assigned");
  cm->setArgNames("dim","number",NULL);
  cm = cb->addMethod("setPhysicalName", &GModel::setPhysicalName);
  cm->setDescription("set the name of an physical group, identified by its "
                     "dimension and number. If number=0, the first free number "
                     "is chosen. Returns the number.");
  cm->setArgNames("physicalName","dim","number",NULL);

  cm = cb->addMethod("createTopology", &GModel::createTopologyFromMesh);
  cm->setDescription("build the topology for a given mesh.");
  cm->setArgNames(NULL);

  cm = cb->addMethod("detectEdges", &GModel::detectEdges);
  cm->setDescription(" use an angle threshold to tag edges.");
  cm->setArgNames("angle",NULL);

  cm = cb->addMethod("createBoundaryLayer", &GModel::createBoundaryLayer);
  cm->setDescription("create a boundary layer using a given field for the "
                     "extrusion height.");
  cm->setArgNames("{list of entities}","height",NULL);
}