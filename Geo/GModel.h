// Gmsh - Copyright (C) 1997-2008 C. Geuzaine, J.-F. Remacle
//
// See the LICENSE.txt file for license information. Please report all
// bugs and problems to <gmsh@geuz.org>.

#ifndef _GMODEL_H_
#define _GMODEL_H_

#include <algorithm>
#include <set>
#include <map>
#include "GVertex.h"
#include "GEdge.h"
#include "GFace.h"
#include "GRegion.h"
#include "SPoint3.h"
#include "SBoundingBox3d.h"

class Octree;
class FM_Internals;
class GEO_Internals;
class OCC_Internals;
class smooth_normals;
class FieldManager;

// A geometric model. The model is a "not yet" non-manifold B-Rep.
class GModel
{
 private:
  // vertex cache to speed-up direct access by vertex number (used for
  // post-processing I/O)
  std::vector<MVertex*> _vertexVectorCache;
  std::map<int, MVertex*> _vertexMapCache;

  // an octree for fast mesh element lookup
  Octree *_octree;

  // geo model internal data
  GEO_Internals *_geo_internals;
  void _createGEOInternals();
  void _deleteGEOInternals();

  // OpenCascade model internal data
  OCC_Internals *_occ_internals;
  void _deleteOCCInternals();

  // Fourier model internal data
  FM_Internals *_fm_internals;
  void _createFMInternals();
  void _deleteFMInternals();
 
  // characteristic Lengths fields
  FieldManager *_fields;

  // store the elements given in the map (indexed by elementary region
  // number) into the model, creating discrete geometrical entities on
  // the fly if needed
  void _storeElementsInEntities(std::map<int, std::vector<MElement*> > &map);

  // loop over all vertices connected to elements and associate geo
  // entity
  void _associateEntityWithMeshVertices();

  // entity that is currently being meshed (used for error reporting)
  GEntity *_currentMeshEntity;

  // index of the current model (in the static list of all loaded
  // models)
  static int _current;

 protected:
  std::string modelName;
  std::set<GRegion*, GEntityLessThan> regions;
  std::set<GFace*, GEntityLessThan> faces;
  std::set<GEdge*, GEntityLessThan> edges;
  std::set<GVertex*, GEntityLessThan> vertices;
  std::set<int> meshPartitions;
  std::map<int, std::string> physicalNames, elementaryNames;

 public:
  GModel(std::string name="");
  virtual ~GModel();

  // the static list of all loaded models
  static std::vector<GModel*> list;

  // returns the current model, and sets the current model index if
  // index >= 0
  static GModel *current(int index=-1);

  // find the model by name
  static GModel *findByName(std::string name);

  // delete everything in a GModel
  void destroy();

  // delete all the mesh-related caches (this must be called when the
  // mesh is changed)
  void destroyMeshCaches();

  // access internal CAD representations
  GEO_Internals *getGEOInternals(){ return _geo_internals; }
  OCC_Internals *getOCCInternals(){ return _occ_internals; }
  FM_Internals *getFMInternals() { return _fm_internals; }

  // access characteristic length fields
  FieldManager *getFields(){ return _fields; }

  // get/set the model name
  void setName(std::string name){ modelName = name; }
  std::string getName(){ return modelName; }

  // get the number of regions in this model.
  int getNumRegions() const { return regions.size(); }
  int getNumFaces() const { return faces.size(); }
  int getNumEdges() const { return edges.size(); }
  int getNumVertices() const  { return vertices.size(); }

  typedef std::set<GRegion*, GEntityLessThan>::iterator riter;
  typedef std::set<GFace*, GEntityLessThan>::iterator fiter;
  typedef std::set<GEdge*, GEntityLessThan>::iterator eiter;
  typedef std::set<GVertex*, GEntityLessThan>::iterator viter;
  typedef std::map<int, std::string>::iterator piter;

  // get an iterator initialized to the first/last entity in this model.
  riter firstRegion() { return regions.begin(); }
  fiter firstFace() { return faces.begin(); }
  eiter firstEdge() { return edges.begin(); }
  viter firstVertex() { return vertices.begin(); }
  riter lastRegion() { return regions.end(); }
  fiter lastFace() { return faces.end(); }
  eiter lastEdge() { return edges.end(); }
  viter lastVertex() { return vertices.end(); }

  // find the entity with the given tag.
  GRegion *getRegionByTag(int n) const;
  GFace *getFaceByTag(int n) const;
  GEdge *getEdgeByTag(int n) const;
  GVertex *getVertexByTag(int n) const;

  // add/remove an entity in the model
  void add(GRegion *r) { regions.insert(r); }
  void add(GFace *f) { faces.insert(f); }
  void add(GEdge *e) { edges.insert(e); }
  void add(GVertex *v) { vertices.insert(v); }
  void remove(GRegion *r);
  void remove(GFace *f);
  void remove(GEdge *e);
  void remove(GVertex *v);

  // Snap vertices on model edges by using geometry tolerance
  void snapVertices();

  // Get a vector containing all the entities in the model
  std::vector<GEntity*> getEntities();

  // Checks if there are no physical entities in the model
  bool noPhysicalGroups();

  // Returns all physical groups (one map per dimension: 0-D to 3-D)
  void getPhysicalGroups(std::map<int, std::vector<GEntity*> > groups[4]);

  // Deletes physical groups in the model
  void deletePhysicalGroups();
  void deletePhysicalGroup(int dim, int num);

  // Returns the highest number associated with a physical entity
  int getMaxPhysicalNumber();

  // Get an iterator on the elementary/physical names
  piter firstPhysicalName() { return physicalNames.begin(); }
  piter lastPhysicalName() { return physicalNames.end(); }
  piter firstElementaryName() { return elementaryNames.begin(); }
  piter lastElementaryName() { return elementaryNames.end(); }

  // Get the number of physical names
  int numPhysicalNames(){ return physicalNames.size(); }

  // Associate a name with a physical number (returns new id if number==0)
  int setPhysicalName(std::string name, int number=0);

  // Get the name (if any) of a given physical group
  std::string getPhysicalName(int number);

  // The bounding box
  SBoundingBox3d bounds();

  // Returns the mesh status for the entire model
  int getMeshStatus(bool countDiscrete=true);

  // Returns the total number of elements in the mesh
  int getNumMeshElements();

  // Access a mesh element by coordinates
  MElement *getMeshElementByCoord(SPoint3 &p);

  // Returns the total number of vertices in the mesh
  int getNumMeshVertices();

  // Access a mesh vertex by tag, using the vertex cache
  MVertex *getMeshVertexByTag(int n);

  // get all the mesh vertices associated with the physical group
  // "number" of dimension "dim"
  void getMeshVertices(int number, int dim, std::vector<MVertex*> &);

  // index all the (used) mesh vertices in a continuous sequence,
  // starting at 1
  int indexMeshVertices(bool all);

  // scale the mesh by the given factor
  void scaleMesh(double factor);

  // set/get entity that is currently being meshed (for error reporting)
  void setCurrentMeshEntity(GEntity *e){ _currentMeshEntity = e; }
  GEntity *getCurrentMeshEntity(){ return _currentMeshEntity; }

  // Deletes all invisble mesh elements
  void removeInvisibleElements();

  // The list of partitions
  std::set<int> &getMeshPartitions() { return meshPartitions; }
  std::set<int> &recomputeMeshPartitions();

  // Deletes all the partitions
  void deleteMeshPartitions();

  // Performs various coherence tests on the mesh
  void checkMeshCoherence();

  // A container for smooth normals
  smooth_normals *normals;

  // Mesh the model
  int mesh(int dimension);

  // Gmsh native CAD format
  int importGEOInternals();
  int readGEO(const std::string &name);
  int writeGEO(const std::string &name, bool printLabels=true);

  // Fourier model
  int readFourier();
  int readFourier(const std::string &name);
  int writeFourier(const std::string &name);

  // OCC model
  int readOCCBREP(const std::string &name);
  int readOCCIGES(const std::string &name);
  int readOCCSTEP(const std::string &name);
  int importOCCShape(const void *shape, const void *meshConstraints=0);

  // Gmsh mesh file format
  int readMSH(const std::string &name);
  int writeMSH(const std::string &name, double version=1.0, bool binary=false,
               bool saveAll=false, double scalingFactor=1.0);

  // Mesh statistics (as Gmsh post-processing views)
  int writePOS(const std::string &name, bool printElementary,
               bool printElementNumber, bool printGamma, bool printEta, bool printRho,
               bool saveAll=false, double scalingFactor=1.0);

  // Stereo lithography format
  int readSTL(const std::string &name, double tolerance=1.e-3);
  int writeSTL(const std::string &name, bool binary=false,
               bool saveAll=false, double scalingFactor=1.0);

  // Inventor/VRML format
  int readVRML(const std::string &name);
  int writeVRML(const std::string &name,
                bool saveAll=false, double scalingFactor=1.0);

  // I-deas universal mesh format
  int readUNV(const std::string &name);
  int writeUNV(const std::string &name, bool saveAll=false,
               bool saveGroupsOfNodes=false, double scalingFactor=1.0);

  // Medit (INRIA) mesh format
  int readMESH(const std::string &name);
  int writeMESH(const std::string &name,
                bool saveAll=false, double scalingFactor=1.0);

  // Nastran Bulk Data File format
  int readBDF(const std::string &name);
  int writeBDF(const std::string &name, int format=0,
               bool saveAll=false, double scalingFactor=1.0);

  // Plot3D structured mesh format
  int readP3D(const std::string &name);
  int writeP3D(const std::string &name,
               bool saveAll=false, double scalingFactor=1.0);

  // CFD General Notation System files
  int readCGNS(const std::string &name);
  int writeCGNS(const std::string &name, double scalingFactor=1.0);

  // Med "Modele d'Echange de Donnees" file format (the static routine
  // is allowed to load multiple models/meshes)
  static int readMED(const std::string &name);
  int readMED(const std::string &name, int meshIndex);
  int writeMED(const std::string &name, 
	       bool saveAll=false, double scalingFactor=1.0);

  // VTK format
  int readVTK(const std::string &name);
  int writeVTK(const std::string &name, bool binary=false,
               bool saveAll=false, double scalingFactor=1.0);
};

#endif
