// Gmsh - Copyright (C) 1997-2009 C. Geuzaine, J.-F. Remacle
//
// See the LICENSE.txt file for license information. Please report all
// bugs and problems to <gmsh@geuz.org>.

#ifndef _GMODEL_H_
#define _GMODEL_H_

#include <algorithm>
#include <vector>
#include <set>
#include <map>
#include <string>
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
class CGNSOptions;
class gLevelset;

// A geometric model. The model is a "not yet" non-manifold B-Rep.
class GModel
{
 private:
  // the name of the model
  std::string _name;

  // the name of the file the model was read from
  std::string _fileName;

  // the visibility flag
  char _visible;

  // vertex and element caches to speed-up direct access by tag (only
  // used for post-processing I/O)
  std::vector<MVertex*> _vertexVectorCache;
  std::map<int, MVertex*> _vertexMapCache;
  std::vector<MElement*> _elementVectorCache;
  std::map<int, MElement*> _elementMapCache;

  // an octree for fast mesh element lookup
  Octree *_octree;

  // Geo (Gmsh native) model internal data
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

  // characteristic length (mesh size) fields
  FieldManager *_fields;

  // store the elements given in the map (indexed by elementary region
  // number) into the model, creating discrete geometrical entities on
  // the fly if needed
  void _storeElementsInEntities(std::map<int, std::vector<MElement*> > &map);

  // loop over all vertices connected to elements and associate
  // geometrical entity
  void _associateEntityWithMeshVertices();

  // store the vertices in the geometrical entity they are associated
  // with, and delete those that are not associated with any entity
  void _storeVerticesInEntities(std::map<int, MVertex*> &vertices);
  void _storeVerticesInEntities(std::vector<MVertex*> &vertices);

  // store the physical tags in the geometrical entities
  void _storePhysicalTagsInEntities(int dim,
                                    std::map<int, std::map<int, std::string> > &map);

  // entity that is currently being meshed (used for error reporting)
  GEntity *_currentMeshEntity;

  // index of the current model (in the static list of all loaded
  // models)
  static int _current;

 protected:
  // the sets of geometrical regions, faces, edges and vertices in the
  // model
  std::set<GRegion*, GEntityLessThan> regions;
  std::set<GFace*, GEntityLessThan> faces;
  std::set<GEdge*, GEntityLessThan> edges;
  std::set<GVertex*, GEntityLessThan> vertices;

  // map between the pair <dimension, elementary or physical number>
  // and an optional associated name
  std::map<std::pair<int, int>, std::string> physicalNames, elementaryNames;

  // the set of all used mesh partition numbers
  std::set<int> meshPartitions;
  int partitionSize[2];

 public:
  GModel(std::string name="");
  virtual ~GModel();

  // the static list of all loaded models
  static std::vector<GModel*> list;

  // return the current model, and sets the current model index if
  // index >= 0
  static GModel *current(int index=-1);

  // find a model by name
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

  // access characteristic length (mesh size) fields
  FieldManager *getFields(){ return _fields; }

  // get/set the model name
  void setName(std::string name){ _name = name; }
  std::string getName(){ return _name; }

  // get/set the model file name
  void setFileName(std::string fileName){ _fileName = fileName; }
  std::string getFileName(){ return _fileName; }

  // get/set the visibility flag
  char getVisibility(){ return _visible; }
  void setVisibility(char val){ _visible = val; }

  // get the number of entities in this model
  int getNumRegions() const { return regions.size(); }
  int getNumFaces() const { return faces.size(); }
  int getNumEdges() const { return edges.size(); }
  int getNumVertices() const { return vertices.size(); }

  // quickly check if the model is empty (i.e., if it contains no
  // entities)
  bool empty() const;

  // region, face, edge and vertex iterators
  typedef std::set<GRegion*, GEntityLessThan>::iterator riter;
  typedef std::set<GFace*, GEntityLessThan>::iterator fiter;
  typedef std::set<GEdge*, GEntityLessThan>::iterator eiter;
  typedef std::set<GVertex*, GEntityLessThan>::iterator viter;

  // get an iterator initialized to the first/last entity in this model
  riter firstRegion() { return regions.begin(); }
  fiter firstFace() { return faces.begin(); }
  eiter firstEdge() { return edges.begin(); }
  viter firstVertex() { return vertices.begin(); }
  riter lastRegion() { return regions.end(); }
  fiter lastFace() { return faces.end(); }
  eiter lastEdge() { return edges.end(); }
  viter lastVertex() { return vertices.end(); }

  // find the entity with the given tag
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

  // snap vertices on model edges by using geometry tolerance
  void snapVertices();

  // fill a vector containing all the entities in the model
  void getEntities(std::vector<GEntity*> &entities);

  // return the highest number associated with an elementary entity of
  // a given dimension
  int getMaxElementaryNumber(int dim);

  // check if there are no physical entities in the model
  bool noPhysicalGroups();

  // return all physical groups (one map per dimension: 0-D to 3-D)
  void getPhysicalGroups(std::map<int, std::vector<GEntity*> > groups[4]);

  // delete physical groups in the model
  void deletePhysicalGroups();
  void deletePhysicalGroup(int dim, int num);

  // return the highest number associated with a physical entity of a
  // given dimension
  int getMaxPhysicalNumber(int dim);

  // elementary/physical name iterator
  typedef std::map<std::pair<int, int>, std::string>::iterator piter;

  // get an iterator on the elementary/physical names
  piter firstPhysicalName() { return physicalNames.begin(); }
  piter lastPhysicalName() { return physicalNames.end(); }
  piter firstElementaryName() { return elementaryNames.begin(); }
  piter lastElementaryName() { return elementaryNames.end(); }

  // get the number of physical names
  int numPhysicalNames(){ return physicalNames.size(); }

  // associate a name with a physical entity of dimension "dim" and
  // number "num" (returns a new number id if "num"==0)
  int setPhysicalName(std::string name, int dim, int num=0);

  // get the name (if any) of a given physical group of dimension
  // "dim" and id number "num"
  std::string getPhysicalName(int dim, int num);

  // get the number of a given physical group of dimension
  // "dim" and name "name". return -1 if not found
  int getPhysicalNumber(const int &dim, const std::string & name);

  // set the selection flag on all entities
  void setSelection(int val);

  // the bounding box
  SBoundingBox3d bounds();

  // return the mesh status for the entire model
  int getMeshStatus(bool countDiscrete=true);

  // return the total number of elements in the mesh
  int getNumMeshElements();

  // get the number of each type of element in the mesh at the largest
  // dimension and return the dimension
  int getNumMeshElements(unsigned c[5]);

  // access a mesh element by coordinates (using an octree search)
  MElement *getMeshElementByCoord(SPoint3 &p);

  // access a mesh element by tag, using the element cache
  MElement *getMeshElementByTag(int n);

  // return the total number of vertices in the mesh
  int getNumMeshVertices();

  // access a mesh vertex by tag, using the vertex cache
  MVertex *getMeshVertexByTag(int n);

  // get all the mesh vertices associated with the physical group
  // of dimension "dim" and id number "num"
  void getMeshVerticesForPhysicalGroup(int dim, int num, std::vector<MVertex*> &);

  // index all the (used) mesh vertices in a continuous sequence,
  // starting at 1
  int indexMeshVertices(bool all);

  // scale the mesh by the given factor
  void scaleMesh(double factor);

  // set/get entity that is currently being meshed (for error reporting)
  void setCurrentMeshEntity(GEntity *e){ _currentMeshEntity = e; }
  GEntity *getCurrentMeshEntity(){ return _currentMeshEntity; }

  // delete all invisble mesh elements
  void removeInvisibleElements();

  // the list of partitions
  std::set<int> &getMeshPartitions() { return meshPartitions; }
  void recomputeMeshPartitions();

  // delete all the partitions
  void deleteMeshPartitions();

  // store/recall min and max partitions size
  void setMinPartitionSize(const int pSize) { partitionSize[0] = pSize; }
  void setMaxPartitionSize(const int pSize) { partitionSize[1] = pSize; }
  int getMinPartitionSize() const { return partitionSize[0]; }
  int getMaxPartitionSize() const { return partitionSize[1]; }

  // perform various coherence tests on the mesh
  void checkMeshCoherence(double tolerance);

  // remove duplicate mesh vertices
  int removeDuplicateMeshVertices(double tolerance);

  // create topology from mesh
  void createTopologyFromMesh();

  // compute distance function
  void computeDistanceFunction();

  // a container for smooth normals
  smooth_normals *normals;

  // mesh the model
  int mesh(int dimension);

  // build a new GModel by cutting the elements crossed by the levelset ls
  GModel *buildCutGModel(gLevelset *ls);

  // Gmsh native CAD format (readGEO is static, since it can create
  // multiple models)
  static int readGEO(const std::string &name);
  int writeGEO(const std::string &name, bool printLabels=true);
  int importGEOInternals();

  // Fourier model
  int readFourier();
  int readFourier(const std::string &name);
  int writeFourier(const std::string &name);

  // OCC model
  int readOCCBREP(const std::string &name);
  int readOCCSTEP(const std::string &name);
  int readOCCIGES(const std::string &name);
  int writeOCCSTEP(const std::string &name);
  int writeOCCBREP(const std::string &name);
  int importOCCShape(const void *shape);
  void addShape(std::string name, std::vector<double> &p, std::string op);

  // Gmsh mesh file format
  int readMSH(const std::string &name);
  int writeMSH(const std::string &name, double version=1.0, bool binary=false,
               bool saveAll=false, bool saveParametric=false, double scalingFactor=1.0);

  // mesh statistics (saved as a Gmsh post-processing view)
  int writePOS(const std::string &name, bool printElementary,
               bool printElementNumber, bool printGamma, bool printEta, bool printRho,
               bool printDisto, bool saveAll=false, double scalingFactor=1.0);

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
  int writeMESH(const std::string &name, int elementTagType=1,
                bool saveAll=false, double scalingFactor=1.0);

  // Nastran Bulk Data File format
  int readBDF(const std::string &name);
  int writeBDF(const std::string &name, int format=0, int elementTagType=1,
               bool saveAll=false, double scalingFactor=1.0);

  // Plot3D structured mesh format
  int readP3D(const std::string &name);
  int writeP3D(const std::string &name,
               bool saveAll=false, double scalingFactor=1.0);

  // CFD General Notation System files
  int readCGNS(const std::string &name);
  int writeCGNS(const std::string &name, int zoneDefinition,
                const CGNSOptions &options, double scalingFactor=1.0);

  // Med "Modele d'Echange de Donnees" file format (the static routine
  // is allowed to load multiple models/meshes)
  static int readMED(const std::string &name);
  int readMED(const std::string &name, int meshIndex);
  int writeMED(const std::string &name,
               bool saveAll=false, double scalingFactor=1.0);

  // VTK format
  int readVTK(const std::string &name, bool bigEndian=false);
  int writeVTK(const std::string &name, bool binary=false,
               bool saveAll=false, double scalingFactor=1.0,
               bool bigEndian=false);

  // DIFFPACK format
  int readDIFF(const std::string &name);
  int writeDIFF(const std::string &name, bool binary=false,
               bool saveAll=false, double scalingFactor=1.0);
};

#endif
