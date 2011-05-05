// Gmsh - Copyright (C) 1997-2008 C. Geuzaine, J.-F. Remacle
//
// See the LICENSE.txt file for license information. Please report all
// bugs and problems to <gmsh@geuz.org>.

#include <stdlib.h>
#include <string.h>
#include <map>
#include <string>
#include "GModel.h"
#include "GmshDefines.h"
#include "MElement.h"
#include "SBoundingBox3d.h"
#include "discreteRegion.h"
#include "discreteFace.h"
#include "discreteEdge.h"
#include "discreteVertex.h"
#include "StringUtils.h"

#if defined(HAVE_GMSH_EMBEDDED)
#  include "GmshEmbedded.h"
#else
#  include "Message.h"
#endif

static void storeVerticesInEntities(std::map<int, MVertex*> &vertices)
{
  std::map<int, MVertex*>::const_iterator it = vertices.begin();
  for(; it != vertices.end(); ++it){
    MVertex *v = it->second;
    GEntity *ge = v->onWhat();
    if(ge) 
      ge->mesh_vertices.push_back(v);
    else
      delete v; // we delete all unused vertices
  }
}

static void storeVerticesInEntities(std::vector<MVertex*> &vertices)
{
  for(unsigned int i = 0; i < vertices.size(); i++){
    MVertex *v = vertices[i];
    if(v){ // the vector can have null entries (first or last element)
      GEntity *ge = v->onWhat();
      if(ge) 
        ge->mesh_vertices.push_back(v);
      else
        delete v; // we delete all unused vertices
    }
  }
}

static void storePhysicalTagsInEntities(GModel *m, int dim,
                                        std::map<int, std::map<int, std::string> > &map)
{
  std::map<int, std::map<int, std::string> >::const_iterator it = map.begin();
  for(; it != map.end(); ++it){
    GEntity *ge = 0;
    switch(dim){
    case 0: ge = m->getVertexByTag(it->first); break;
    case 1: ge = m->getEdgeByTag(it->first); break;
    case 2: ge = m->getFaceByTag(it->first); break;
    case 3: ge = m->getRegionByTag(it->first); break;
    }
    if(ge){
      std::map<int, std::string>::const_iterator it2 = it->second.begin();
      for(; it2 != it->second.end(); ++it2){
        if(std::find(ge->physicals.begin(), ge->physicals.end(), it2->first) == 
           ge->physicals.end())
          ge->physicals.push_back(it2->first);
      }
    }
  }
}

static bool getVertices(int num, int *indices, std::map<int, MVertex*> &map, 
                        std::vector<MVertex*> &vertices)
{
  for(int i = 0; i < num; i++){
    if(!map.count(indices[i])){
      Msg::Error("Wrong vertex index %d", indices[i]);
      return false;
    }
    else
      vertices.push_back(map[indices[i]]);
  }
  return true;
}

static bool getVertices(int num, int *indices, std::vector<MVertex*> &vec,
                        std::vector<MVertex*> &vertices)
{
  for(int i = 0; i < num; i++){
    if(indices[i] < 0 || indices[i] > (int)(vec.size() - 1)){
      Msg::Error("Wrong vertex index %d", indices[i]);
      return false;
    }
    else
      vertices.push_back(vec[indices[i]]);
  }
  return true;
}

static int getNumVerticesForElementTypeMSH(int type)
{
  switch (type) {
  case MSH_PNT    : return 1;
  case MSH_LIN_2  : return 2;
  case MSH_LIN_3  : return 2 + 1;
  case MSH_LIN_4  : return 2 + 2;
  case MSH_LIN_5  : return 2 + 3;
  case MSH_LIN_6  : return 2 + 4;
  case MSH_TRI_3  : return 3;
  case MSH_TRI_6  : return 3 + 3;
  case MSH_TRI_9  : return 3 + 6;
  case MSH_TRI_10 : return 3 + 6 + 1;
  case MSH_TRI_12 : return 3 + 9;
  case MSH_TRI_15 : return 3 + 9 + 3;
  case MSH_TRI_15I: return 3 + 12;
  case MSH_TRI_21 : return 3 + 12 + 6;
  case MSH_QUA_4  : return 4;
  case MSH_QUA_8  : return 4 + 4;
  case MSH_QUA_9  : return 4 + 4 + 1;
  case MSH_TET_4  : return 4;
  case MSH_TET_10 : return 4 + 6;
  case MSH_TET_20 : return 4 + 12 + 4;
  case MSH_TET_35 : return 4 + 18 + 12 + 1;
  case MSH_TET_52 : return 4 + 24 + 24 + 0;
  case MSH_TET_56 : return 4 + 24 + 24 + 4;
  case MSH_HEX_8  : return 8;
  case MSH_HEX_20 : return 8 + 12;
  case MSH_HEX_27 : return 8 + 12 + 6 + 1;
  case MSH_PRI_6  : return 6;
  case MSH_PRI_15 : return 6 + 9;
  case MSH_PRI_18 : return 6 + 9 + 3;
  case MSH_PYR_5  : return 5;
  case MSH_PYR_13 : return 5 + 8;
  case MSH_PYR_14 : return 5 + 8 + 1;
  default: 
    Msg::Error("Unknown type of element %d", type);
    return 0;
  }
}

static void createElementMSH(GModel *m, int num, int type, int physical, 
                             int reg, int part, std::vector<MVertex*> &v, 
                             std::map<int, std::vector<MVertex*> > &points,
                             std::map<int, std::vector<MElement*> > elem[7],
                             std::map<int, std::map<int, std::string> > physicals[4])
{
  int dim;
  if(type == MSH_PNT){
    dim = 0;
    points[reg].push_back(v[0]);
  }
  else{
    MElementFactory factory;
    MElement *e = factory.create(type, v, num, part);

    if(!e){
      Msg::Error("Unknown type of element %d", type);
      return;
    }
    dim = e->getDim();
    int idx;
    switch(e->getNumEdges()){
    case 1 : idx = 0; break;
    case 3 : idx = 1; break;
    case 4 : idx = 2; break;
    case 6 : idx = 3; break;
    case 12 : idx = 4; break;
    case 9 : idx = 5; break;
    case 8 : idx = 6; break;
    default : Msg::Error("Wrong number of edges in element"); return;
    }
    elem[idx][reg].push_back(e);
  }
  
  if(physical && (!physicals[dim].count(reg) || !physicals[dim][reg].count(physical)))
    physicals[dim][reg][physical] = "unnamed";
  
  if(part) m->getMeshPartitions().insert(part);
}

int GModel::readMSH(const std::string &name)
{
  FILE *fp = fopen(name.c_str(), "rb");
  if(!fp){
    Msg::Error("Unable to open file '%s'", name.c_str());
    return 0;
  }

  double version = 1.0;
  bool binary = false, swap = false;
  char str[256] = "XXX";
  std::map<int, std::vector<MVertex*> > points;
  std::map<int, std::vector<MElement*> > elements[7];
  std::map<int, std::map<int, std::string> > physicals[4];
  bool postpro = false;

  // we might want to cache those for post-processing lookups
  std::map<int, MVertex*> vertexMap;
  std::vector<MVertex*> vertexVector;
 
  while(1) {

    while(str[0] != '$'){
      if(!fgets(str, sizeof(str), fp) || feof(fp))
        break;
    }
    
    if(feof(fp))
      break;

    if(!strncmp(&str[1], "MeshFormat", 10)) {

      if(!fgets(str, sizeof(str), fp)) return 0;
      int format, size;
      if(sscanf(str, "%lf %d %d", &version, &format, &size) != 3) return 0;
      if(format){
        binary = true;
        Msg::Info("Mesh is in binary format");
        int one;
        if(fread(&one, sizeof(int), 1, fp) != 1) return 0;
        if(one != 1){
          swap = true;
          Msg::Info("Swapping bytes from binary file");
        }
      }

    }
    else if(!strncmp(&str[1], "PhysicalNames", 13)) {

      if(!fgets(str, sizeof(str), fp)) return 0;
      int numNames;
      if(sscanf(str, "%d", &numNames) != 1) return 0;
      for(int i = 0; i < numNames; i++) {
        int num;
        if(fscanf(fp, "%d", &num) != 1) return 0;
        if(!fgets(str, sizeof(str), fp)) return 0;
        std::string name = ExtractDoubleQuotedString(str, 256);
        if(name.size()) setPhysicalName(name, num);
      }

    }
    else if(!strncmp(&str[1], "NO", 2) || !strncmp(&str[1], "Nodes", 5)) {

      if(!fgets(str, sizeof(str), fp)) return 0;
      int numVertices;
      if(sscanf(str, "%d", &numVertices) != 1) return 0;
      Msg::Info("%d vertices", numVertices);
      Msg::ResetProgressMeter();
      vertexVector.clear();
      vertexMap.clear();
          int minVertex = numVertices + 1, maxVertex = -1;
      for(int i = 0; i < numVertices; i++) {
        int num;
        double xyz[3];
        if(!binary){
          if(fscanf(fp, "%d %lf %lf %lf", &num, &xyz[0], &xyz[1], &xyz[2]) != 4) return 0;
        }
        else{
          if(fread(&num, sizeof(int), 1, fp) != 1) return 0;
          if(swap) SwapBytes((char*)&num, sizeof(int), 1);
          if(fread(xyz, sizeof(double), 3, fp) != 3) return 0;
          if(swap) SwapBytes((char*)xyz, sizeof(double), 3);
        }
        minVertex = std::min(minVertex, num);
        maxVertex = std::max(maxVertex, num);
        if(vertexMap.count(num))
          Msg::Warning("Skipping duplicate vertex %d", num);
        else
          vertexMap[num] = new MVertex(xyz[0], xyz[1], xyz[2], 0, num);
	if(numVertices > 100000) 
	  Msg::ProgressMeter(i + 1, numVertices, "Reading nodes");
      }
      // If the vertex numbering is dense, tranfer the map into a
      // vector to speed up element creation
      if((int)vertexMap.size() == numVertices && 
         ((minVertex == 1 && maxVertex == numVertices) ||
          (minVertex == 0 && maxVertex == numVertices - 1))){
        Msg::Info("Vertex numbering is dense");
        vertexVector.resize(vertexMap.size() + 1);
        if(minVertex == 1) 
          vertexVector[0] = 0;
        else
          vertexVector[numVertices] = 0;
        std::map<int, MVertex*>::const_iterator it = vertexMap.begin();
        for(; it != vertexMap.end(); ++it)
          vertexVector[it->first] = it->second;
        vertexMap.clear();
      }

    }
    else if(!strncmp(&str[1], "ELM", 3) || !strncmp(&str[1], "Elements", 8)) {

      if(!fgets(str, sizeof(str), fp)) return 0;
      int numElements;
      sscanf(str, "%d", &numElements);
      Msg::Info("%d elements", numElements);
      Msg::ResetProgressMeter();
      if(!binary){
        for(int i = 0; i < numElements; i++) {
          int num, type, physical = 0, elementary = 0, partition = 0, numVertices;
          if(version <= 1.0){
            fscanf(fp, "%d %d %d %d %d", &num, &type, &physical, &elementary, &numVertices);
            if(numVertices != getNumVerticesForElementTypeMSH(type)) return 0;
          }
          else{
            int numTags;
            fscanf(fp, "%d %d %d", &num, &type, &numTags);
            for(int j = 0; j < numTags; j++){
              int tag;
              fscanf(fp, "%d", &tag);       
              if(j == 0)      physical = tag;
              else if(j == 1) elementary = tag;
              else if(j == 2) partition = tag;
              // ignore any other tags for now
            }
            if(!(numVertices = getNumVerticesForElementTypeMSH(type))) return 0;
          }
          int indices[60];
          for(int j = 0; j < numVertices; j++) fscanf(fp, "%d", &indices[j]);
          std::vector<MVertex*> vertices;
          if(vertexVector.size()){
            if(!getVertices(numVertices, indices, vertexVector, vertices)) return 0;
          }
          else{
            if(!getVertices(numVertices, indices, vertexMap, vertices)) return 0;
          }
          createElementMSH(this, num, type, physical, elementary, partition, 
                           vertices, points, elements, physicals);
	  if(numElements > 100000) 
	    Msg::ProgressMeter(i + 1, numElements, "Reading elements");
        }
      }
      else{
        int numElementsPartial = 0;
        while(numElementsPartial < numElements){
          int header[3];
          if(fread(header, sizeof(int), 3, fp) != 3) return 0;
          if(swap) SwapBytes((char*)header, sizeof(int), 3);
          int type = header[0];
          int numElms = header[1];
          int numTags = header[2];
          int numVertices = getNumVerticesForElementTypeMSH(type);
          unsigned int n = 1 + numTags + numVertices;
          int *data = new int[n];
          for(int i = 0; i < numElms; i++) {
            if(fread(data, sizeof(int), n, fp) != n) return 0;
            if(swap) SwapBytes((char*)data, sizeof(int), n);
            int num = data[0];
            int physical = (numTags > 0) ? data[4 - numTags] : 0;
            int elementary = (numTags > 1) ? data[4 - numTags + 1] : 0;
            int partition = (numTags > 2) ? data[4 - numTags + 2] : 0;
            int *indices = &data[numTags + 1];
            std::vector<MVertex*> vertices;
            if(vertexVector.size()){
              if(!getVertices(numVertices, indices, vertexVector, vertices)) return 0;
            }
            else{
              if(!getVertices(numVertices, indices, vertexMap, vertices)) return 0;
            }
            createElementMSH(this, num, type, physical, elementary, partition, 
                             vertices, points, elements, physicals);
	    if(numElements > 100000) 
	      Msg::ProgressMeter(numElementsPartial + i + 1, numElements, 
				 "Reading elements");
          }
          delete [] data;
          numElementsPartial += numElms;
        }
      }

    }
    else if(!strncmp(&str[1], "NodeData", 8)) {

      // there's some nodal post-processing data to read later on, so
      // cache the vertex indexing data
      if(vertexVector.size())
        _vertexVectorCache = vertexVector;
      else
        _vertexMapCache = vertexMap;
      postpro = true;
      break;

    }
    else if(!strncmp(&str[1], "ElementData", 11) ||
	    !strncmp(&str[1], "ElementNodeData", 15)) {

      // there's some element post-processing data to read later on
      postpro = true;
      break;

    }

    do {
      if(!fgets(str, sizeof(str), fp) || feof(fp))
        break;
    } while(str[0] != '$');
  }

  // store the elements in their associated elementary entity. If the
  // entity does not exist, create a new one.
  for(int i = 0; i < (int)(sizeof(elements) / sizeof(elements[0])); i++)
    _storeElementsInEntities(elements[i]);

  // treat points separately
  for(std::map<int, std::vector<MVertex*> >::iterator it = points.begin(); 
      it != points.end(); ++it){
    GVertex *v = getVertexByTag(it->first);
    if(!v){
      v = new discreteVertex(this, it->first);
      add(v);
    }
    for(unsigned int i = 0; i < it->second.size(); i++) 
      v->mesh_vertices.push_back(it->second[i]);
  }

  // associate the correct geometrical entity with each mesh vertex
  _associateEntityWithMeshVertices();

  // special case for geometry vertices: now that the correct
  // geometrical entity has been associated with the vertices, we
  // reset mesh_vertices so that it can be filled again below
  for(viter it = firstVertex(); it != lastVertex(); ++it)
    (*it)->mesh_vertices.clear();

  // store the vertices in their associated geometrical entity
  if(vertexVector.size())
    storeVerticesInEntities(vertexVector);
  else
    storeVerticesInEntities(vertexMap);

  // store the physical tags
  for(int i = 0; i < 4; i++)
    storePhysicalTagsInEntities(this, i, physicals[i]);

  fclose(fp);
  return postpro ? 2 : 1;
}

static void writeElementHeaderMSH(bool binary, FILE *fp, std::map<int,int> &elements,
                                  int t1, int t2=0, int t3=0, int t4=0, 
                                  int t5=0, int t6=0, int t7=0, int t8=0)
{
  if(!binary) return;

  int numTags = 3;
  int data[3];
  if(elements.count(t1)){
    data[0] = t1;  data[1] = elements[t1];  data[2] = numTags;
    fwrite(data, sizeof(int), 3, fp);
  }
  else if(t2 && elements.count(t2)){
    data[0] = t2;  data[1] = elements[t2];  data[2] = numTags;
    fwrite(data, sizeof(int), 3, fp);
  }
  else if(t3 && elements.count(t3)){
    data[0] = t3;  data[1] = elements[t3];  data[2] = numTags;
    fwrite(data, sizeof(int), 3, fp);
  }
  else if(t4 && elements.count(t4)){
    data[0] = t4;  data[1] = elements[t4];  data[2] = numTags;
    fwrite(data, sizeof(int), 3, fp);
  }
  else if(t5 && elements.count(t5)){
    data[0] = t5;  data[1] = elements[t5];  data[2] = numTags;
    fwrite(data, sizeof(int), 3, fp);
  }
  else if(t6 && elements.count(t6)){
    data[0] = t6;  data[1] = elements[t6];  data[2] = numTags;
    fwrite(data, sizeof(int), 3, fp);
  }
  else if(t7 && elements.count(t7)){
    data[0] = t7;  data[1] = elements[t7];  data[2] = numTags;
    fwrite(data, sizeof(int), 3, fp);
  }
  else if(t8 && elements.count(t8)){
    data[0] = t8;  data[1] = elements[t8];  data[2] = numTags;
    fwrite(data, sizeof(int), 3, fp);
  }
}

template<class T>
static void writeElementsMSH(FILE *fp, const std::vector<T*> &ele, int saveAll, 
                             double version, bool binary, int &num, int elementary, 
                             std::vector<int> &physicals)
{
  for(unsigned int i = 0; i < ele.size(); i++)
    if(saveAll)
      ele[i]->writeMSH(fp, version, binary, ++num, elementary, 0);
    else
      for(unsigned int j = 0; j < physicals.size(); j++)
        ele[i]->writeMSH(fp, version, binary, ++num, elementary, physicals[j]);
}

int GModel::writeMSH(const std::string &name, double version, bool binary, 
                     bool saveAll, double scalingFactor)
{
  FILE *fp = fopen(name.c_str(), binary ? "wb" : "w");
  if(!fp){
    Msg::Error("Unable to open file '%s'", name.c_str());
    return 0;
  }

  // if there are no physicals we save all the elements
  if(noPhysicalGroups()) saveAll = true;

  // get the number of vertices and index the vertices in a continuous
  // sequence
  int numVertices = indexMeshVertices(saveAll);
  
  // binary format exists only in version 2
  if(binary) version = 2.0;

  // get the number of elements (we assume that all the elements in a
  // list have the same type, i.e., they are all of the same
  // polynomial order)
  std::map<int,int> elements;
  for(viter it = firstVertex(); it != lastVertex(); ++it){
    int p = (saveAll ? 1 : (*it)->physicals.size());
    int n = p * (*it)->mesh_vertices.size();
    if(n) elements[MSH_PNT] += n;
  }
  for(eiter it = firstEdge(); it != lastEdge(); ++it){
    int p = (saveAll ? 1 : (*it)->physicals.size());
    int n = p * (*it)->lines.size();
    if(n) elements[(*it)->lines[0]->getTypeForMSH()] += n;
  }
  for(fiter it = firstFace(); it != lastFace(); ++it){
    int p = (saveAll ? 1 : (*it)->physicals.size());
    int n = p * (*it)->triangles.size();
    if(n) elements[(*it)->triangles[0]->getTypeForMSH()] += n;
    n = p * (*it)->quadrangles.size();
    if(n) elements[(*it)->quadrangles[0]->getTypeForMSH()] += n;
  }
  for(riter it = firstRegion(); it != lastRegion(); ++it){
    int p = (saveAll ? 1 : (*it)->physicals.size());
    int n = p * (*it)->tetrahedra.size();
    if(n) elements[(*it)->tetrahedra[0]->getTypeForMSH()] += n;
    n = p * (*it)->hexahedra.size();
    if(n) elements[(*it)->hexahedra[0]->getTypeForMSH()] += n;
    n = p * (*it)->prisms.size();
    if(n) elements[(*it)->prisms[0]->getTypeForMSH()] += n;
    n = p * (*it)->pyramids.size();
    if(n) elements[(*it)->pyramids[0]->getTypeForMSH()] += n;
  }

  int numElements = 0;
  std::map<int,int>::const_iterator it = elements.begin();
  for(; it != elements.end(); ++it)
    numElements += it->second;

  if(version >= 2.0){
    fprintf(fp, "$MeshFormat\n");
    fprintf(fp, "%g %d %d\n", version, binary ? 1 : 0, (int)sizeof(double));
    if(binary){
      int one = 1;
      fwrite(&one, sizeof(int), 1, fp);
      fprintf(fp, "\n");
    }
    fprintf(fp, "$EndMeshFormat\n");

    if(numPhysicalNames()){
      fprintf(fp, "$PhysicalNames\n");
      fprintf(fp, "%d\n", numPhysicalNames());
      for(piter it = firstPhysicalName(); it != lastPhysicalName(); it++)
        fprintf(fp, "%d \"%s\"\n", it->first, it->second.c_str());
      fprintf(fp, "$EndPhysicalNames\n");
    }

    fprintf(fp, "$Nodes\n");
  }
  else
    fprintf(fp, "$NOD\n");

  fprintf(fp, "%d\n", numVertices);
  for(viter it = firstVertex(); it != lastVertex(); ++it)
    for(unsigned int i = 0; i < (*it)->mesh_vertices.size(); i++) 
      (*it)->mesh_vertices[i]->writeMSH(fp, binary, scalingFactor);
  for(eiter it = firstEdge(); it != lastEdge(); ++it)
    for(unsigned int i = 0; i < (*it)->mesh_vertices.size(); i++)
      (*it)->mesh_vertices[i]->writeMSH(fp, binary, scalingFactor);
  for(fiter it = firstFace(); it != lastFace(); ++it)
    for(unsigned int i = 0; i < (*it)->mesh_vertices.size(); i++) 
      (*it)->mesh_vertices[i]->writeMSH(fp, binary, scalingFactor);
  for(riter it = firstRegion(); it != lastRegion(); ++it)
    for(unsigned int i = 0; i < (*it)->mesh_vertices.size(); i++) 
      (*it)->mesh_vertices[i]->writeMSH(fp, binary, scalingFactor);

  if(binary) fprintf(fp, "\n");

  if(version >= 2.0){
    fprintf(fp, "$EndNodes\n");
    fprintf(fp, "$Elements\n");
  }
  else{
    fprintf(fp, "$ENDNOD\n");
    fprintf(fp, "$ELM\n");
  }

  fprintf(fp, "%d\n", numElements);
  int num = 0;

  writeElementHeaderMSH(binary, fp, elements, MSH_PNT);
  for(viter it = firstVertex(); it != lastVertex(); ++it)
    writeElementsMSH(fp, (*it)->mesh_vertices, saveAll, version, binary, num,
                     (*it)->tag(), (*it)->physicals);
  writeElementHeaderMSH(binary, fp, elements, MSH_LIN_2, MSH_LIN_3, MSH_LIN_4, MSH_LIN_5);
  for(eiter it = firstEdge(); it != lastEdge(); ++it)
    writeElementsMSH(fp, (*it)->lines, saveAll, version, binary, num,
                     (*it)->tag(), (*it)->physicals);
  writeElementHeaderMSH(binary, fp, elements, MSH_TRI_3, MSH_TRI_6, MSH_TRI_9, 
                        MSH_TRI_10, MSH_TRI_12, MSH_TRI_15, MSH_TRI_15I, MSH_TRI_21);
  for(fiter it = firstFace(); it != lastFace(); ++it)
    writeElementsMSH(fp, (*it)->triangles, saveAll, version, binary, num,
                     (*it)->tag(), (*it)->physicals);
  writeElementHeaderMSH(binary, fp, elements, MSH_QUA_4, MSH_QUA_9, MSH_QUA_8);
  for(fiter it = firstFace(); it != lastFace(); ++it)
    writeElementsMSH(fp, (*it)->quadrangles, saveAll, version, binary, num,
                     (*it)->tag(), (*it)->physicals);
  writeElementHeaderMSH(binary, fp, elements, MSH_TET_4, MSH_TET_10, MSH_TET_20, MSH_TET_35, MSH_TET_56, MSH_TET_52);
  for(riter it = firstRegion(); it != lastRegion(); ++it)
    writeElementsMSH(fp, (*it)->tetrahedra, saveAll, version, binary, num,
                     (*it)->tag(), (*it)->physicals);
  writeElementHeaderMSH(binary, fp, elements, MSH_HEX_8, MSH_HEX_27, MSH_HEX_20);
  for(riter it = firstRegion(); it != lastRegion(); ++it)
    writeElementsMSH(fp, (*it)->hexahedra, saveAll, version, binary, num,
                     (*it)->tag(), (*it)->physicals);
  writeElementHeaderMSH(binary, fp, elements, MSH_PRI_6, MSH_PRI_18, MSH_PRI_15);
  for(riter it = firstRegion(); it != lastRegion(); ++it)
    writeElementsMSH(fp, (*it)->prisms, saveAll, version, binary, num,
                     (*it)->tag(), (*it)->physicals);
  writeElementHeaderMSH(binary, fp, elements, MSH_PYR_5, MSH_PYR_14, MSH_PYR_13);
  for(riter it = firstRegion(); it != lastRegion(); ++it)
    writeElementsMSH(fp, (*it)->pyramids, saveAll, version, binary, num,
                     (*it)->tag(), (*it)->physicals);
  
  if(binary) fprintf(fp, "\n");

  if(version >= 2.0){
    fprintf(fp, "$EndElements\n");
  }
  else{
    fprintf(fp, "$ENDELM\n");
  }

  fclose(fp);
  return 1;
}

int GModel::writePOS(const std::string &name, bool printElementary, 
                     bool printElementNumber, bool printGamma, bool printEta, 
                     bool printRho, bool saveAll, double scalingFactor)
{
  FILE *fp = fopen(name.c_str(), "w");
  if(!fp){
    Msg::Error("Unable to open file '%s'", name.c_str());
    return 0;
  }

  bool f[5] = {printElementary, printElementNumber, printGamma, printEta, printRho};

  bool first = true;  
  std::string names;
  if(f[0]){
    if(first) first = false; else names += ",";
    names += "\"Elementary Entity\"";
  }
  if(f[1]){
    if(first) first = false; else names += ",";
    names += "\"Element Number\"";
  }
  if(f[2]){
    if(first) first = false; else names += ",";
    names += "\"Gamma\"";
  }
  if(f[3]){
    if(first) first = false; else names += ",";
    names += "\"Eta\"";
  }
  if(f[4]){
    if(first) first = false; else names += ",";
    names += "\"Rho\"";
  }

  if(names.empty()) return 0;

  if(noPhysicalGroups()) saveAll = true;

  fprintf(fp, "View \"Statistics\" {\n");
  fprintf(fp, "T2(1.e5,30,%d){%s};\n", (1<<16)|(4<<8), names.c_str());

  for(eiter it = firstEdge(); it != lastEdge(); ++it) {
    if(saveAll || (*it)->physicals.size()){
      for(unsigned int i = 0; i < (*it)->lines.size(); i++)
        (*it)->lines[i]->writePOS(fp, f[0], f[1], f[2], f[3], f[4], 
                                  scalingFactor, (*it)->tag());
    }
  }

  for(fiter it = firstFace(); it != lastFace(); ++it) {
    if(saveAll || (*it)->physicals.size()){
      for(unsigned int i = 0; i < (*it)->triangles.size(); i++)
        (*it)->triangles[i]->writePOS(fp, f[0], f[1], f[2], f[3], f[4], 
                                      scalingFactor, (*it)->tag());
      for(unsigned int i = 0; i < (*it)->quadrangles.size(); i++)
        (*it)->quadrangles[i]->writePOS(fp, f[0], f[1], f[2], f[3], f[4], 
                                        scalingFactor, (*it)->tag());
    }
  }

  for(riter it = firstRegion(); it != lastRegion(); ++it) {
    if(saveAll || (*it)->physicals.size()){
      for(unsigned int i = 0; i < (*it)->tetrahedra.size(); i++)
        (*it)->tetrahedra[i]->writePOS(fp, f[0], f[1], f[2], f[3], f[4],
                                       scalingFactor, (*it)->tag());
      for(unsigned int i = 0; i < (*it)->hexahedra.size(); i++)
        (*it)->hexahedra[i]->writePOS(fp, f[0], f[1], f[2], f[3], f[4], 
                                      scalingFactor, (*it)->tag());
      for(unsigned int i = 0; i < (*it)->prisms.size(); i++)
        (*it)->prisms[i]->writePOS(fp, f[0], f[1], f[2], f[3], f[4], 
                                   scalingFactor, (*it)->tag());
      for(unsigned int i = 0; i < (*it)->pyramids.size(); i++)
        (*it)->pyramids[i]->writePOS(fp, f[0], f[1], f[2], f[3], f[4], 
                                     scalingFactor, (*it)->tag());
    }
  }

  fprintf(fp, "};\n");

  fclose(fp);
  return 1;
}

int GModel::readSTL(const std::string &name, double tolerance)
{
  FILE *fp = fopen(name.c_str(), "rb");
  if(!fp){
    Msg::Error("Unable to open file '%s'", name.c_str());
    return 0;
  }

  // read all facets and store triplets of points
  std::vector<SPoint3> points;
  SBoundingBox3d bbox;

  // "solid", or binary data header
  char buffer[256];
  fgets(buffer, sizeof(buffer), fp);

  if(!strncmp(buffer, "solid", 5)) { 
    // ASCII STL
    while(!feof(fp)) {
      // "facet normal x y z", or "endsolid"
      if(!fgets(buffer, sizeof(buffer), fp)) break;
      if(!strncmp(buffer, "endsolid", 8)) break;
      char s1[256], s2[256];
      float x, y, z;
      sscanf(buffer, "%s %s %f %f %f", s1, s2, &x, &y, &z);
      // "outer loop"
      if(!fgets(buffer, sizeof(buffer), fp)) break;
      // "vertex x y z"
      for(int i = 0; i < 3; i++){
        if(!fgets(buffer, sizeof(buffer), fp)) break;
        sscanf(buffer, "%s %f %f %f", s1, &x, &y, &z);
        SPoint3 p(x, y, z);
        points.push_back(p);
        bbox += p;
      }
      // "endloop"
      if(!fgets(buffer, sizeof(buffer), fp)) break;
      // "endfacet"
      if(!fgets(buffer, sizeof(buffer), fp)) break;
    }
  }
  else{
    // Binary STL
    Msg::Info("Mesh is in binary format");
    rewind(fp);
    char header[80];
    if(fread(header, sizeof(char), 80, fp)){
      unsigned int nfacets = 0;
      size_t ret = fread(&nfacets, sizeof(unsigned int), 1, fp);
      bool swap = false;
      if(nfacets > 10000000){
        Msg::Info("Swapping bytes from binary file");
        swap = true;
        SwapBytes((char*)&nfacets, sizeof(unsigned int), 1);
      }
      if(ret && nfacets){
        char *data = new char[nfacets * 50 * sizeof(char)];
        ret = fread(data, sizeof(char), nfacets * 50, fp);
        if(ret == nfacets * 50){
          for(unsigned int i = 0; i < nfacets; i++) {
            float *xyz = (float *)&data[i * 50 * sizeof(char)];
            if(swap) SwapBytes((char*)xyz, sizeof(float), 12);
            for(int j = 0; j < 3; j++){
              SPoint3 p(xyz[3 + 3 * j], xyz[3 + 3 * j + 1], xyz[3 + 3 * j + 2]);
              points.push_back(p);
              bbox += p;
            }
          }
        }
        delete data;
      }
    }
  }

  if(!points.size()){
    Msg::Error("No facets found in STL file");
    return 0;
  }
  
  if(points.size() % 3){
    Msg::Error("Wrong number of points in STL file");
    return 0;
  }

  Msg::Info("%d facets", points.size() / 3);

  // create face
  GFace *face = new discreteFace(this, getNumFaces() + 1);
  add(face);

  // create (unique) vertices and triangles
  double lc = norm(SVector3(bbox.max(), bbox.min()));
  MVertexLessThanLexicographic::tolerance = lc * tolerance;
  std::set<MVertex*, MVertexLessThanLexicographic> vertices;
  for(unsigned int i = 0; i < points.size(); i += 3){
    MVertex *v[3];
    for(int j = 0; j < 3; j++){
      double x = points[i + j].x(), y = points[i + j].y(), z = points[i + j].z();
      MVertex w(x, y, z);
      std::set<MVertex*, MVertexLessThanLexicographic>::iterator it = vertices.find(&w);
      if(it != vertices.end()) {
        v[j] = *it;
      }
      else {
        v[j] = new MVertex(x, y, z, face);
        vertices.insert(v[j]);
        face->mesh_vertices.push_back(v[j]);
      }
    }
    face->triangles.push_back(new MTriangle(v[0], v[1], v[2]));
  }

  fclose(fp);
  return 1;
}

int GModel::writeSTL(const std::string &name, bool binary, bool saveAll,
                     double scalingFactor)
{
  FILE *fp = fopen(name.c_str(), binary ? "wb" : "w");
  if(!fp){
    Msg::Error("Unable to open file '%s'", name.c_str());
    return 0;
  }

  if(noPhysicalGroups()) saveAll = true;

  if(!binary){
    fprintf(fp, "solid Created by Gmsh\n");
  }
  else{
    char header[80];
    strncpy(header, "Created by Gmsh", 80);
    fwrite(header, sizeof(char), 80, fp);
    unsigned int nfacets = 0;
    for(fiter it = firstFace(); it != lastFace(); ++it){
      if(saveAll || (*it)->physicals.size()){
        nfacets += (*it)->triangles.size() + 2 * (*it)->quadrangles.size();
      }
    }
    fwrite(&nfacets, sizeof(unsigned int), 1, fp);
  }
    
  for(fiter it = firstFace(); it != lastFace(); ++it) {
    if(saveAll || (*it)->physicals.size()){
      for(unsigned int i = 0; i < (*it)->triangles.size(); i++)
        (*it)->triangles[i]->writeSTL(fp, binary, scalingFactor);
      for(unsigned int i = 0; i < (*it)->quadrangles.size(); i++)
        (*it)->quadrangles[i]->writeSTL(fp, binary, scalingFactor);
    }
  }

  if(!binary)
    fprintf(fp, "endsolid Created by Gmsh\n");
  
  fclose(fp);
  return 1;
}

static int skipUntil(FILE *fp, const char *key)
{
  char str[256], key_bracket[256];
  strcpy(key_bracket, key);
  strcat(key_bracket, "[");
  while(fscanf(fp, "%s", str)){
    if(!strcmp(str, key)){ 
      while(!feof(fp) && fgetc(fp) != '['){}
      return 1; 
    }
    if(!strcmp(str, key_bracket)){
      return 1;
    }
  }
  return 0;
}

static int readVerticesVRML(FILE *fp, std::vector<MVertex*> &vertexVector,
                            std::vector<MVertex*> &allVertexVector)
{
  double x, y, z;
  if(fscanf(fp, "%lf %lf %lf", &x, &y, &z) != 3) return 0;
  vertexVector.push_back(new MVertex(x, y, z));
  while(fscanf(fp, " , %lf %lf %lf", &x, &y, &z) == 3)
    vertexVector.push_back(new MVertex(x, y, z));
  for(unsigned int i = 0; i < vertexVector.size(); i++)
    allVertexVector.push_back(vertexVector[i]);
  Msg::Info("%d vertices", vertexVector.size());
  return 1;
}

static int readElementsVRML(FILE *fp, std::vector<MVertex*> &vertexVector, int region,
                            std::map<int, std::vector<MElement*> > elements[3], 
                            bool strips=false)
{
  int i;
  std::vector<int> idx;
  if(fscanf(fp, "%d", &i) != 1) return 0;
  idx.push_back(i);

  // check if vertex indices are separated by commas
  char tmp[256];
  const char *format;
  fpos_t position;
  fgetpos(fp, &position);
  fgets(tmp, sizeof(tmp), fp);
  fsetpos(fp, &position);
  if(strstr(tmp, ","))
    format = " , %d";
  else
    format = " %d";

  while(fscanf(fp, format, &i) == 1){
    if(i != -1){
      idx.push_back(i);
    }
    else{
      std::vector<MVertex*> vertices;
      if(!getVertices(idx.size(), &idx[0], vertexVector, vertices)) return 0;
      idx.clear();
      if(vertices.size() < 2){
        Msg::Info("Skipping %d-vertex element", (int)vertices.size());
      }
      else if(vertices.size() == 2){
        elements[0][region].push_back(new MLine(vertices));
      }
      else if(vertices.size() == 3){
        elements[1][region].push_back(new MTriangle(vertices));
      }
      else if(!strips && vertices.size() == 4){
        elements[2][region].push_back(new MQuadrangle(vertices));
      }
      else if(strips){ // triangle strip
        for(unsigned int j = 2; j < vertices.size(); j++){
          if(j % 2)
            elements[1][region].push_back
              (new MTriangle(vertices[j], vertices[j - 1], vertices[j - 2]));
          else
            elements[1][region].push_back
              (new MTriangle(vertices[j - 2], vertices[j - 1], vertices[j]));
        }
      }
      else{ // import polygon as triangle fan
        for(unsigned int j = 2; j < vertices.size(); j++){
          elements[1][region].push_back
            (new MTriangle(vertices[0], vertices[j-1], vertices[j]));
        }
      }
    }
  }
  if(idx.size()){
    Msg::Error("Prematured end of VRML file");
    return 0;
  }
  Msg::Info("%d elements", elements[0][region].size() + 
      elements[1][region].size() + elements[2][region].size());
  return 1;
}

int GModel::readVRML(const std::string &name)
{
  FILE *fp = fopen(name.c_str(), "r");
  if(!fp){
    Msg::Error("Unable to open file '%s'", name.c_str());
    return 0;
  }

  // This is by NO means a complete VRML/Inventor parser (but it's
  // sufficient for reading simple Inventor files... which is all I
  // need)
  std::vector<MVertex*> vertexVector, allVertexVector;
  std::map<int, std::vector<MElement*> > elements[3];
  int region = 0;
  char buffer[256], str[256];
  while(!feof(fp)) {
    if(!fgets(buffer, sizeof(buffer), fp)) break;
    if(buffer[0] != '#'){ // skip comments
      sscanf(buffer, "%s", str);
      if(!strcmp(str, "Coordinate3")){
        vertexVector.clear();
        if(!skipUntil(fp, "point")) break;
        if(!readVerticesVRML(fp, vertexVector, allVertexVector)) break;
      }
      else if(!strcmp(str, "coord")){
        vertexVector.clear();
        if(!skipUntil(fp, "point")) break;
        if(!readVerticesVRML(fp, vertexVector, allVertexVector)) break;
        if(!skipUntil(fp, "coordIndex")) break;
        if(!readElementsVRML(fp, vertexVector, region, elements, true)) break;
      }
      else if(!strcmp(str, "IndexedTriangleStripSet")){
        region++;
        vertexVector.clear();
        if(!skipUntil(fp, "vertex")) break;
        if(!readVerticesVRML(fp, vertexVector, allVertexVector)) break;
        if(!skipUntil(fp, "coordIndex")) break;
        if(!readElementsVRML(fp, vertexVector, region, elements, true)) break;
      }
      else if(!strcmp(str, "IndexedFaceSet") || !strcmp(str, "IndexedLineSet")){
        region++;
        if(!skipUntil(fp, "coordIndex")) break;
        if(!readElementsVRML(fp, vertexVector, region, elements)) break;
      }
      else if(!strcmp(str, "DEF")){
        char str1[256], str2[256];
        if(!sscanf(buffer, "%s %s %s", str1, str2, str)) break;
        if(!strcmp(str, "Coordinate")){
          vertexVector.clear();
          if(!skipUntil(fp, "point")) break;
          if(!readVerticesVRML(fp, vertexVector, allVertexVector)) break;
        }
        else if(!strcmp(str, "IndexedFaceSet") || !strcmp(str, "IndexedLineSet")){
          region++;
          if(!skipUntil(fp, "coordIndex")) break;
          if(!readElementsVRML(fp, vertexVector, region, elements)) break;
        }
      }
    }
  }

  for(int i = 0; i < (int)(sizeof(elements)/sizeof(elements[0])); i++) 
    _storeElementsInEntities(elements[i]);
  _associateEntityWithMeshVertices();
  storeVerticesInEntities(allVertexVector);

  fclose(fp);
  return 1;
}

int GModel::writeVRML(const std::string &name, bool saveAll, double scalingFactor)
{
  FILE *fp = fopen(name.c_str(), "w");
  if(!fp){
    Msg::Error("Unable to open file '%s'", name.c_str());
    return 0;
  }

  if(noPhysicalGroups()) saveAll = true;

  indexMeshVertices(saveAll);

  fprintf(fp, "#VRML V1.0 ascii\n");
  fprintf(fp, "#created by Gmsh\n");
  fprintf(fp, "Coordinate3 {\n");
  fprintf(fp, "  point [\n");

  for(viter it = firstVertex(); it != lastVertex(); ++it)
    for(unsigned int i = 0; i < (*it)->mesh_vertices.size(); i++) 
      (*it)->mesh_vertices[i]->writeVRML(fp, scalingFactor);
  for(eiter it = firstEdge(); it != lastEdge(); ++it)
    for(unsigned int i = 0; i < (*it)->mesh_vertices.size(); i++)
      (*it)->mesh_vertices[i]->writeVRML(fp, scalingFactor);
  for(fiter it = firstFace(); it != lastFace(); ++it)
    for(unsigned int i = 0; i < (*it)->mesh_vertices.size(); i++) 
      (*it)->mesh_vertices[i]->writeVRML(fp, scalingFactor);

  fprintf(fp, "  ]\n");
  fprintf(fp, "}\n");

  for(eiter it = firstEdge(); it != lastEdge(); ++it){
    if(saveAll || (*it)->physicals.size()){
      fprintf(fp, "DEF Curve%d IndexedLineSet {\n", (*it)->tag());
      fprintf(fp, "  coordIndex [\n");
      for(unsigned int i = 0; i < (*it)->lines.size(); i++)
        (*it)->lines[i]->writeVRML(fp);
      fprintf(fp, "  ]\n");
      fprintf(fp, "}\n");
    }
  }

  for(fiter it = firstFace(); it != lastFace(); ++it){
    if(saveAll || (*it)->physicals.size()){
      fprintf(fp, "DEF Surface%d IndexedFaceSet {\n", (*it)->tag());
      fprintf(fp, "  coordIndex [\n");
      for(unsigned int i = 0; i < (*it)->triangles.size(); i++)
        (*it)->triangles[i]->writeVRML(fp);
      for(unsigned int i = 0; i < (*it)->quadrangles.size(); i++)
        (*it)->quadrangles[i]->writeVRML(fp);
      fprintf(fp, "  ]\n");
      fprintf(fp, "}\n");
    }
  }

  fclose(fp);
  return 1;
}

int GModel::readUNV(const std::string &name)
{
  FILE *fp = fopen(name.c_str(), "r");
  if(!fp){
    Msg::Error("Unable to open file '%s'", name.c_str());
    return 0;
  }

  char buffer[256];
  std::map<int, MVertex*> vertexMap;
  std::map<int, std::vector<MElement*> > elements[7];
  std::map<int, std::map<int, std::string> > physicals[4];

  while(!feof(fp)) {
    if(!fgets(buffer, sizeof(buffer), fp)) break;
    if(!strncmp(buffer, "    -1", 6)){
      if(!fgets(buffer, sizeof(buffer), fp)) break;      
      if(!strncmp(buffer, "    -1", 6))
        if(!fgets(buffer, sizeof(buffer), fp)) break;
      int record = 0;
      sscanf(buffer, "%d", &record);
      if(record == 2411){ // nodes
        while(fgets(buffer, sizeof(buffer), fp)){
          if(!strncmp(buffer, "    -1", 6)) break;
          int num, dum;
          if(sscanf(buffer, "%d %d %d %d", &num, &dum, &dum, &dum) != 4) break;
          if(!fgets(buffer, sizeof(buffer), fp)) break;
          double x, y, z;
          for(unsigned int i = 0; i < strlen(buffer); i++)
            if(buffer[i] == 'D') buffer[i] = 'E';
          if(sscanf(buffer, "%lf %lf %lf", &x, &y, &z) != 3) break;
          vertexMap[num] = new MVertex(x, y, z, 0, num);
        }
      }
      else if(record == 2412){ // elements
        while(fgets(buffer, sizeof(buffer), fp)){
          if(strlen(buffer) < 3) continue; // possible line ending after last fscanf
          if(!strncmp(buffer, "    -1", 6)) break;
          int num, type, elementary, physical, color, numNodes;
          if(sscanf(buffer, "%d %d %d %d %d %d", &num, &type, &elementary, &physical, 
                    &color, &numNodes) != 6) break;
          if(elementary < 0) elementary = 1;
          if(physical < 0) physical = 0;
          switch(type){
          case 11: case 21: case 22: case 31:
          case 23: case 24: case 32:
            // beam elements
            if(!fgets(buffer, sizeof(buffer), fp)) break;
            int dum;
            if(sscanf(buffer, "%d %d %d", &dum, &dum, &dum) != 3) break;
            break;
          }
          int n[30];
          for(int i = 0; i < numNodes; i++) if(!fscanf(fp, "%d", &n[i])) return 0;
          std::vector<MVertex*> vertices;
          if(!getVertices(numNodes, n, vertexMap, vertices)) return 0;

          int dim = -1;
          switch(type){
          case 11: case 21: case 22: case 31:
            elements[0][elementary].push_back(new MLine(vertices, num));
            dim = 1;
            break;
          case 23: case 24: case 32:
            elements[0][elementary].push_back
              (new MLine3(vertices[0], vertices[2], vertices[1], num));
            dim = 1;
            break;
          case 41: case 51: case 61: case 74: case 81: case 91: 
            elements[1][elementary].push_back(new MTriangle(vertices, num));
            dim = 2;
            break;
          case 42: case 52: case 62: case 72: case 82: case 92: 
            elements[1][elementary].push_back
              (new MTriangle6(vertices[0], vertices[2], vertices[4], vertices[1], 
                              vertices[3], vertices[5], num));
            dim = 2;
            break;
          case 44: case 54: case 64: case 71: case 84: case 94: 
            elements[2][elementary].push_back(new MQuadrangle(vertices, num));
            dim = 2;
            break;
          case 45: case 55: case 65: case 75: case 85: case 95:
            elements[2][elementary].push_back
              (new MQuadrangle8(vertices[0], vertices[2], vertices[4], vertices[6], 
                                vertices[1], vertices[3], vertices[5], vertices[7],
                                num));
            dim = 2;
            break;
          case 111:
            elements[3][elementary].push_back(new MTetrahedron(vertices, num));
            dim = 3; 
            break;
          case 118: 
            elements[3][elementary].push_back
              (new MTetrahedron10(vertices[0], vertices[2], vertices[4], vertices[9], 
                                  vertices[1], vertices[3], vertices[5], vertices[8],
                                  vertices[6], vertices[7], num));
            dim = 3;
            break;
          case 104: case 115:  
            elements[4][elementary].push_back(new MHexahedron(vertices, num));
            dim = 3; 
            break;
          case 105: case 116:
            elements[4][elementary].push_back
              (new MHexahedron20(vertices[0], vertices[2], vertices[4], vertices[6], 
                                 vertices[12], vertices[14], vertices[16], vertices[18],
                                 vertices[1], vertices[7], vertices[8], vertices[3],  
                                 vertices[9], vertices[5], vertices[10], vertices[11], 
                                 vertices[13], vertices[19], vertices[15], vertices[17], 
                                 num));
            dim = 3; 
            break;
          case 101: case 112:
            elements[5][elementary].push_back(new MPrism(vertices, num));
            dim = 3; 
            break;
          case 102: case 113:
            elements[5][elementary].push_back
              (new MPrism15(vertices[0], vertices[2], vertices[4], vertices[9], 
                            vertices[11], vertices[13], vertices[1], vertices[5], 
                            vertices[6], vertices[3], vertices[7], vertices[8],
                            vertices[10], vertices[14], vertices[12], num));
            dim = 3;
            break;
          }
  
          if(dim >= 0 && physical && (!physicals[dim].count(elementary) || 
                                      !physicals[dim][elementary].count(physical)))
            physicals[dim][elementary][physical] = "unnamed";
        }
      }
    }
  }
  
  for(int i = 0; i < (int)(sizeof(elements)/sizeof(elements[0])); i++) 
    _storeElementsInEntities(elements[i]);
  _associateEntityWithMeshVertices();
  storeVerticesInEntities(vertexMap);
  for(int i = 0; i < 4; i++)  
    storePhysicalTagsInEntities(this, i, physicals[i]);

  fclose(fp);
  return 1;
}

template<class T>
static void writeElementsUNV(FILE *fp, const std::vector<T*> &ele, int saveAll, 
                             int &num, int elementary, std::vector<int> &physicals)
{
  for(unsigned int i = 0; i < ele.size(); i++)
    if(saveAll)
      ele[i]->writeUNV(fp, ++num, elementary, 0);
    else
      for(unsigned int j = 0; j < physicals.size(); j++)
        ele[i]->writeUNV(fp, ++num, elementary, physicals[j]);
}

int GModel::writeUNV(const std::string &name, bool saveAll, bool saveGroupsOfNodes, 
                     double scalingFactor)
{
  FILE *fp = fopen(name.c_str(), "w");
  if(!fp){
    Msg::Error("Unable to open file '%s'", name.c_str());
    return 0;
  }

  if(noPhysicalGroups()) saveAll = true;

  indexMeshVertices(saveAll);

  // nodes
  fprintf(fp, "%6d\n", -1);
  fprintf(fp, "%6d\n", 2411);
  for(viter it = firstVertex(); it != lastVertex(); ++it)
    for(unsigned int i = 0; i < (*it)->mesh_vertices.size(); i++) 
      (*it)->mesh_vertices[i]->writeUNV(fp, scalingFactor);
  for(eiter it = firstEdge(); it != lastEdge(); ++it)
    for(unsigned int i = 0; i < (*it)->mesh_vertices.size(); i++)
      (*it)->mesh_vertices[i]->writeUNV(fp, scalingFactor);
  for(fiter it = firstFace(); it != lastFace(); ++it)
    for(unsigned int i = 0; i < (*it)->mesh_vertices.size(); i++) 
      (*it)->mesh_vertices[i]->writeUNV(fp, scalingFactor);
  for(riter it = firstRegion(); it != lastRegion(); ++it)
    for(unsigned int i = 0; i < (*it)->mesh_vertices.size(); i++) 
      (*it)->mesh_vertices[i]->writeUNV(fp, scalingFactor);
  fprintf(fp, "%6d\n", -1);  

  // elements
  fprintf(fp, "%6d\n", -1);
  fprintf(fp, "%6d\n", 2412);
  int num = 0;
  for(eiter it = firstEdge(); it != lastEdge(); ++it){
    writeElementsUNV(fp, (*it)->lines, saveAll, num, (*it)->tag(), (*it)->physicals);
  }
  for(fiter it = firstFace(); it != lastFace(); ++it){
    writeElementsUNV(fp, (*it)->triangles, saveAll, num, (*it)->tag(), (*it)->physicals);
    writeElementsUNV(fp, (*it)->quadrangles, saveAll, num, (*it)->tag(), (*it)->physicals);
  }
  for(riter it = firstRegion(); it != lastRegion(); ++it){
    writeElementsUNV(fp, (*it)->tetrahedra, saveAll, num, (*it)->tag(), (*it)->physicals);
    writeElementsUNV(fp, (*it)->hexahedra, saveAll, num, (*it)->tag(), (*it)->physicals);
    writeElementsUNV(fp, (*it)->prisms, saveAll, num, (*it)->tag(), (*it)->physicals);
  }
  fprintf(fp, "%6d\n", -1);

  // groups of nodes for physical groups
  if(saveGroupsOfNodes){
    fprintf(fp, "%6d\n", -1);
    fprintf(fp, "%6d\n", 2477);
    std::map<int, std::vector<GEntity*> > groups[4];
    getPhysicalGroups(groups);
    int gr = 1;
    for(int dim = 1; dim <= 3; dim++){
      for(std::map<int, std::vector<GEntity*> >::iterator it = groups[dim].begin();
          it != groups[dim].end(); it++){
        std::set<MVertex*> nodes;
	std::vector<GEntity *> &entities = it->second;
        for(unsigned int i = 0; i < entities.size(); i++){
	  for(unsigned int j = 0; j < entities[i]->getNumMeshElements(); j++){
	    MElement *e = entities[i]->getMeshElement(j);
	    for (int k = 0; k < e->getNumVertices(); k++)
	      nodes.insert(e->getVertex(k));
	  }
	}
        fprintf(fp, "%10d%10d%10d%10d%10d%10d%10d%10d\n", 
                gr, 0, 0, 0, 0, 0, 0, (int)nodes.size());
        fprintf(fp, "PERMANENT GROUP%d\n", gr++);
        int row = 0;
        for(std::set<MVertex*>::iterator it2 = nodes.begin(); it2 != nodes.end(); it2++){
          if(row == 2) {
            fprintf(fp, "\n");
            row = 0;
          }
          fprintf(fp, "%10d%10d%10d%10d", 7, (*it2)->getIndex(), 0, 0);
          row++;
        }
        fprintf(fp, "\n");
      }
    }
    fprintf(fp, "%6d\n", -1);
  }

  fclose(fp);
  return 1;
}

int GModel::readMESH(const std::string &name)
{
  FILE *fp = fopen(name.c_str(), "r");
  if(!fp){
    Msg::Error("Unable to open file '%s'", name.c_str());
    return 0;
  }

  char buffer[256];
  fgets(buffer, sizeof(buffer), fp);

  char str[256];
  int format;
  sscanf(buffer, "%s %d", str, &format);
  if(format != 1){
    Msg::Error("Medit mesh import only available for ASCII files");
    return 0;
  }

  std::vector<MVertex*> vertexVector;
  std::map<int, std::vector<MElement*> > elements[4];
  std::vector<MVertex*> corners,ridges;

  while(!feof(fp)) {
    if(!fgets(buffer, 256, fp)) break;
    if(buffer[0] != '#'){ // skip comments and empty lines
      str[0]='\0';
      sscanf(buffer, "%s", str);
      if(!strcmp(str, "Dimension")){
        if(!fgets(buffer, sizeof(buffer), fp)) break;
      }
      else if(!strcmp(str, "Vertices")){
        if(!fgets(buffer, sizeof(buffer), fp)) break;
        int nbv;
        sscanf(buffer, "%d", &nbv);
        Msg::Info("%d vertices", nbv);
        vertexVector.resize(nbv);
        for(int i = 0; i < nbv; i++) {
          if(!fgets(buffer, sizeof(buffer), fp)) break;
          int dum;
          double x, y, z;
          sscanf(buffer, "%lf %lf %lf %d", &x, &y, &z, &dum);
          vertexVector[i] = new MVertex(x, y, z);
        }
      }
      else if(!strcmp(str, "Edges")){
        if(!fgets(buffer, sizeof(buffer), fp)) break;
        int nbe;
        sscanf(buffer, "%d", &nbe);
        Msg::Info("%d edges", nbe);
        for(int i = 0; i < nbe; i++) {
          if(!fgets(buffer, sizeof(buffer), fp)) break;
          int n[2], cl;
          sscanf(buffer, "%d %d %d", &n[0], &n[1], &cl);
          for(int j = 0; j < 2; j++) n[j]--;
          std::vector<MVertex*> vertices;
          if(!getVertices(2, n, vertexVector, vertices)) return 0;
          elements[3][cl].push_back(new MLine(vertices));
        }
      }
      else if(!strcmp(str, "Triangles")){
        if(!fgets(buffer, sizeof(buffer), fp)) break;
        int nbe;
        sscanf(buffer, "%d", &nbe);
        Msg::Info("%d triangles", nbe);
        for(int i = 0; i < nbe; i++) {
          if(!fgets(buffer, sizeof(buffer), fp)) break;
          int n[3], cl;
          sscanf(buffer, "%d %d %d %d", &n[0], &n[1], &n[2], &cl);
          for(int j = 0; j < 3; j++) n[j]--;
          std::vector<MVertex*> vertices;
          if(!getVertices(3, n, vertexVector, vertices)) return 0;
          elements[0][cl].push_back(new MTriangle(vertices));
        }
      }
      else if(!strcmp(str, "Corners")){
        if(!fgets(buffer, sizeof(buffer), fp)) break;
        int nbe;
        sscanf(buffer, "%d", &nbe);
        Msg::Info("%d corners", nbe);
        for(int i = 0; i < nbe; i++) {
          if(!fgets(buffer, sizeof(buffer), fp)) break;
          int  n[1];
          sscanf(buffer, "%d",&n[0]);
          for(int j = 0; j < 1; j++) n[j]--;
	  //          std::vector<MVertex*> vertices;
	  //          if(!getVertices(1, n, vertexVector, vertices)) return 0;
	  //          corners.push_back(vertices[0]);
        }
      }
      else if(!strcmp(str, "Ridges")){
        if(!fgets(buffer, sizeof(buffer), fp)) break;
        int nbe;
        sscanf(buffer, "%d", &nbe);
        Msg::Info("%d ridges", nbe);
        for(int i = 0; i < nbe; i++) {
          if(!fgets(buffer, sizeof(buffer), fp)) break;
          int  n[1];
          sscanf(buffer, "%d",&n[0]);
          for(int j = 0; j < 1; j++) n[j]--;
	  //          std::vector<MVertex*> vertices;
	  //          if(!getVertices(1, n, vertexVector, vertices)) return 0;
	  //          ridges.push_back(vertices[0]);
        }
      }
      else if(!strcmp(str, "Quadrilaterals")) {
        if(!fgets(buffer, sizeof(buffer), fp)) break;
        int nbe;
        sscanf(buffer, "%d", &nbe);
        Msg::Info("%d quadrangles", nbe);
        for(int i = 0; i < nbe; i++) {
          if(!fgets(buffer, sizeof(buffer), fp)) break;
          int n[4], cl;
          sscanf(buffer, "%d %d %d %d %d", &n[0], &n[1], &n[2], &n[3], &cl);
          for(int j = 0; j < 4; j++) n[j]--;
          std::vector<MVertex*> vertices;
          if(!getVertices(4, n, vertexVector, vertices)) return 0;
          elements[1][cl].push_back(new MQuadrangle(vertices));
        }
      }
      else if(!strcmp(str, "Tetrahedra")) {
        if(!fgets(buffer, sizeof(buffer), fp)) break;
        int nbe;
        sscanf(buffer, "%d", &nbe);
        Msg::Info("%d tetrahedra", nbe);
        for(int i = 0; i < nbe; i++) {
          if(!fgets(buffer, sizeof(buffer), fp)) break;
          int n[4], cl;
          sscanf(buffer, "%d %d %d %d %d", &n[0], &n[1], &n[2], &n[3], &cl);
          for(int j = 0; j < 4; j++) n[j]--;
          std::vector<MVertex*> vertices;
          if(!getVertices(4, n, vertexVector, vertices)) return 0;
          elements[2][cl].push_back(new MTetrahedron(vertices));
        }
      }
    }
  }

  for(int i = 0; i < (int)(sizeof(elements)/sizeof(elements[0])); i++) 
    _storeElementsInEntities(elements[i]);
  _associateEntityWithMeshVertices();
  storeVerticesInEntities(vertexVector);

  fclose(fp);
  return 1;
}

int GModel::writeMESH(const std::string &name, bool saveAll, double scalingFactor)
{
  FILE *fp = fopen(name.c_str(), "w");
  if(!fp){
    Msg::Error("Unable to open file '%s'", name.c_str());
    return 0;
  }

  if(noPhysicalGroups()) saveAll = true;

  int numVertices = indexMeshVertices(saveAll);

  fprintf(fp, " MeshVersionFormatted 1\n");
  fprintf(fp, " Dimension\n");
  fprintf(fp, " 3\n");

  fprintf(fp, " Vertices\n");
  fprintf(fp, " %d\n", numVertices);
  for(viter it = firstVertex(); it != lastVertex(); ++it)
    for(unsigned int i = 0; i < (*it)->mesh_vertices.size(); i++) 
      (*it)->mesh_vertices[i]->writeMESH(fp, scalingFactor);
  for(eiter it = firstEdge(); it != lastEdge(); ++it)
    for(unsigned int i = 0; i < (*it)->mesh_vertices.size(); i++)
      (*it)->mesh_vertices[i]->writeMESH(fp, scalingFactor);
  for(fiter it = firstFace(); it != lastFace(); ++it)
    for(unsigned int i = 0; i < (*it)->mesh_vertices.size(); i++) 
      (*it)->mesh_vertices[i]->writeMESH(fp, scalingFactor);
  for(riter it = firstRegion(); it != lastRegion(); ++it)
    for(unsigned int i = 0; i < (*it)->mesh_vertices.size(); i++) 
      (*it)->mesh_vertices[i]->writeMESH(fp, scalingFactor);
  
  int numTriangles = 0, numQuadrangles = 0, numTetrahedra = 0;
  for(fiter it = firstFace(); it != lastFace(); ++it){
    if(saveAll || (*it)->physicals.size()){
      numTriangles += (*it)->triangles.size();
      numQuadrangles += (*it)->quadrangles.size();
    }
  }
  for(riter it = firstRegion(); it != lastRegion(); ++it){
    if(saveAll || (*it)->physicals.size()){
      numTetrahedra += (*it)->tetrahedra.size();
    }
  }

  if(numTriangles){
    fprintf(fp, " Triangles\n");
    fprintf(fp, " %d\n", numTriangles);
    for(fiter it = firstFace(); it != lastFace(); ++it){
      if(saveAll || (*it)->physicals.size()){
        for(unsigned int i = 0; i < (*it)->triangles.size(); i++)
          (*it)->triangles[i]->writeMESH(fp, (*it)->tag());
      }
    }
  }
  if(numQuadrangles){
    fprintf(fp, " Quadrilaterals\n");
    fprintf(fp, " %d\n", numQuadrangles);
    for(fiter it = firstFace(); it != lastFace(); ++it){
      if(saveAll || (*it)->physicals.size()){
        for(unsigned int i = 0; i < (*it)->triangles.size(); i++)
          (*it)->quadrangles[i]->writeMESH(fp, (*it)->tag());
      }
    }
  }
  if(numTetrahedra){
    fprintf(fp, " Tetrahedra\n");
    fprintf(fp, " %d\n", numTetrahedra);
    for(riter it = firstRegion(); it != lastRegion(); ++it){
      if(saveAll || (*it)->physicals.size()){
        for(unsigned int i = 0; i < (*it)->tetrahedra.size(); i++)
          (*it)->tetrahedra[i]->writeMESH(fp, (*it)->tag());
      }
    }
  }

  fprintf(fp, " End\n");
  
  fclose(fp);
  return 1;
}

static int getFormatBDF(char *buffer, int &keySize)
{
  if(buffer[keySize] == '*'){ keySize++; return 2; } // long fields
  for(unsigned int i = 0; i < strlen(buffer); i++)
    if(buffer[i] == ',') return 0; // free fields
  return 1; // small fields;
}

static double atofBDF(char *str)
{
  int len = strlen(str);
  // classic numbers with E or D exponent notation
  for(int i = 0; i < len; i++){
    if(str[i] == 'E' || str[i] == 'e') {
      return atof(str);
    }
    else if(str[i] == 'D' || str[i] == 'd'){ 
      str[i] = 'E'; 
      return atof(str);
    }
  }
  // special Nastran floating point format (e.g. "-7.-1" instead of
  // "-7.E-01" or "2.3+2" instead of "2.3E+02")
  char tmp[32];
  int j = 0, leading_minus = 1;
  for(int i = 0; i < len; i++){
    if(leading_minus && str[i] != ' '  && str[i] != '-') leading_minus = 0;
    if(!leading_minus && str[i] == '-') tmp[j++] = 'E';
    if(str[i] == '+') tmp[j++] = 'E';
    tmp[j++] = str[i];
  }
  tmp[j] = '\0';
  return atof(tmp);
}

static int readVertexBDF(FILE *fp, char *buffer, int keySize, 
                         int *num, double *x, double *y, double *z)
{
  char tmp[5][32];
  int j = keySize;

  switch(getFormatBDF(buffer, keySize)){
  case 0: // free field
    for(int i = 0; i < 5; i++){
      tmp[i][16] = '\0';
      strncpy(tmp[i], &buffer[j + 1], 16);
      for(int k = 0; k < 16; k++){ if(tmp[i][k] == ',') tmp[i][k] = '\0'; }
      j++;
      while(j < (int)strlen(buffer) && buffer[j] != ',') j++;
    }
    break;
  case 1: // small field
    for(int i = 0; i < 5; i++) tmp[i][8] = '\0';
    strncpy(tmp[0], &buffer[8], 8);
    strncpy(tmp[2], &buffer[24], 8); 
    strncpy(tmp[3], &buffer[32], 8); 
    strncpy(tmp[4], &buffer[40], 8); 
    break;
  case 2: // long field
    for(int i = 0; i < 5; i++) tmp[i][16] = '\0';
    strncpy(tmp[0], &buffer[8], 16);
    strncpy(tmp[2], &buffer[40], 16);
    strncpy(tmp[3], &buffer[56], 16);
    char buffer2[256];
    for(unsigned int i = 0; i < sizeof(buffer2); i++) buffer2[i] = '\0';
    if(!fgets(buffer2, sizeof(buffer2), fp)) return 0;
    strncpy(tmp[4], &buffer2[8], 16);
    break;
  }

  *num = atoi(tmp[0]);
  *x = atofBDF(tmp[2]);
  *y = atofBDF(tmp[3]);
  *z = atofBDF(tmp[4]);
  return 1;
}

static bool emptyFieldBDF(char *field, int length)
{
  for(int i = 0; i < length; i++)
    if(field[i] != '\0' && field[i] != ' ' && field[i] != '\n' && field[i] != '\r')
      return false;
  return true;
}

static void readLineBDF(char *buffer, int format, std::vector<char*> &fields)
{
  int cmax = (format == 2) ? 16 : 8; // max char per (center) field
  int nmax = (format == 2) ? 4 : 8; // max num of (center) fields per line

  if(format == 0){ // free fields
    for(unsigned int i = 0; i < strlen(buffer); i++){
      if(buffer[i] == ',') fields.push_back(&buffer[i + 1]);
    }
  }
  else{ // small or long fields
    for(int i = 0; i < nmax + 1; i++){
      if(!emptyFieldBDF(&buffer[8 + cmax * i], cmax))
        fields.push_back(&buffer[8 + cmax * i]);
    }
  }
}

static int readElementBDF(FILE *fp, char *buffer, int keySize, int numVertices, 
                          int &num, int &region, std::vector<MVertex*> &vertices,
                          std::map<int, MVertex*> &vertexMap)
{
  char buffer2[256], buffer3[256];
  std::vector<char*> fields;
  int format = getFormatBDF(buffer, keySize);

  for(unsigned int i = 0; i < sizeof(buffer2); i++) buffer2[i] = buffer3[i] = '\0';

  readLineBDF(buffer, format, fields);

  if(((int)fields.size() - 2 < abs(numVertices)) || 
     (numVertices < 0 && (fields.size() == 9))){
    if(fields.size() == 9) fields.pop_back();
    if(!fgets(buffer2, sizeof(buffer2), fp)) return 0;
    readLineBDF(buffer2, format, fields);
  }

  if(((int)fields.size() - 2 < abs(numVertices)) || 
     (numVertices < 0 && (fields.size() == 17))){
    if(fields.size() == 17) fields.pop_back();
    if(!fgets(buffer3, sizeof(buffer3), fp)) return 0;
    readLineBDF(buffer3, format, fields);
  }

  // negative 'numVertices' gives the minimum required number of vertices
  if((int)fields.size() - 2 < abs(numVertices)){
    Msg::Error("Wrong number of vertices %d for element", fields.size() - 2);
    return 0;
  }

  int n[30], cmax = (format == 2) ? 16 : 8; // max char per (center) field
  char tmp[32];
  tmp[cmax] = '\0';
  strncpy(tmp, fields[0], cmax); num = atoi(tmp);
  strncpy(tmp, fields[1], cmax); region = atoi(tmp);
  for(unsigned int i = 2; i < fields.size(); i++){
    strncpy(tmp, fields[i], cmax); n[i - 2] = atoi(tmp);
  }

  // ignore the extra fields when we know how many vertices we need
  int numCheck = (numVertices > 0) ? numVertices : fields.size() - 2;
  if(!getVertices(numCheck, n, vertexMap, vertices)) return 0;
  return 1;
}

int GModel::readBDF(const std::string &name)
{
  FILE *fp = fopen(name.c_str(), "r");
  if(!fp){
    Msg::Error("Unable to open file '%s'", name.c_str());
    return 0;
  }

  char buffer[256];
  std::map<int, MVertex*> vertexMap;
  std::map<int, std::vector<MElement*> > elements[7];

  // nodes can be defined after elements, so parse the file twice

  while(!feof(fp)) {
    for(unsigned int i = 0; i < sizeof(buffer); i++) buffer[i] = '\0';
    if(!fgets(buffer, sizeof(buffer), fp)) break;
    if(buffer[0] != '$'){ // skip comments
      if(!strncmp(buffer, "GRID", 4)){
        int num;
        double x, y, z;
        if(!readVertexBDF(fp, buffer, 4, &num, &x, &y, &z)) break;
        vertexMap[num] = new MVertex(x, y, z, 0, num);
      }
    }
  }
  Msg::Info("%d vertices", vertexMap.size());

  rewind(fp);
  while(!feof(fp)) {
    for(unsigned int i = 0; i < sizeof(buffer); i++) buffer[i] = '\0';
    if(!fgets(buffer, sizeof(buffer), fp)) break;
    if(buffer[0] != '$'){ // skip comments
      int num, region;
      std::vector<MVertex*> vertices;
      if(!strncmp(buffer, "CBAR", 4)){
        if(readElementBDF(fp, buffer, 4, 2, num, region, vertices, vertexMap))
          elements[0][region].push_back(new MLine(vertices, num));
      }
      else if(!strncmp(buffer, "CROD", 4)){
        if(readElementBDF(fp, buffer, 4, 2, num, region, vertices, vertexMap))
          elements[0][region].push_back(new MLine(vertices, num));
      }
      else if(!strncmp(buffer, "CBEAM", 5)){
        if(readElementBDF(fp, buffer, 5, 2, num, region, vertices, vertexMap))
          elements[0][region].push_back(new MLine(vertices, num));
      }
      else if(!strncmp(buffer, "CTRIA3", 6)){
        if(readElementBDF(fp, buffer, 6, 3, num, region, vertices, vertexMap))
          elements[1][region].push_back(new MTriangle(vertices, num));
      }
      else if(!strncmp(buffer, "CTRIA6", 6)){
        if(readElementBDF(fp, buffer, 6, 6, num, region, vertices, vertexMap))
          elements[1][region].push_back(new MTriangle6(vertices, num));
      }
      else if(!strncmp(buffer, "CQUAD4", 6)){
        if(readElementBDF(fp, buffer, 6, 4, num, region, vertices, vertexMap))
          elements[2][region].push_back(new MQuadrangle(vertices, num));
      }
      else if(!strncmp(buffer, "CQUAD8", 6)){
        if(readElementBDF(fp, buffer, 6, 8, num, region, vertices, vertexMap))
          elements[2][region].push_back(new MQuadrangle8(vertices, num));
      }
      else if(!strncmp(buffer, "CQUAD", 5)){
        if(readElementBDF(fp, buffer, 5, -4, num, region, vertices, vertexMap)){
          if(vertices.size() == 9)
            elements[2][region].push_back(new MQuadrangle9(vertices, num));
          else if(vertices.size() == 8)
            elements[2][region].push_back(new MQuadrangle8(vertices, num));
          else
            elements[2][region].push_back(new MQuadrangle(vertices, num));
        }
      }
      else if(!strncmp(buffer, "CTETRA", 6)){
        if(readElementBDF(fp, buffer, 6, -4, num, region, vertices, vertexMap)){
          if(vertices.size() == 10)
            elements[3][region].push_back
	      (new MTetrahedron10(vertices[0], vertices[1], vertices[2], vertices[3], 
				  vertices[4], vertices[5], vertices[6], vertices[7], 
				  vertices[9], vertices[8], num));
          else
            elements[3][region].push_back(new MTetrahedron(vertices, num));
        }
      }
      else if(!strncmp(buffer, "CHEXA", 5)){
        if(readElementBDF(fp, buffer, 5, -8, num, region, vertices, vertexMap)){
          if(vertices.size() == 20)
            elements[4][region].push_back
	      (new MHexahedron20(vertices[0], vertices[1], vertices[2], vertices[3], 
				 vertices[4], vertices[5], vertices[6], vertices[7], 
				 vertices[8], vertices[11], vertices[12], vertices[9], 
				 vertices[13], vertices[10], vertices[14], vertices[15], 
				 vertices[16], vertices[19], vertices[17], vertices[18], 
				 num));
          else
            elements[4][region].push_back(new MHexahedron(vertices, num));
        }
      }
      else if(!strncmp(buffer, "CPENTA", 6)){
        if(readElementBDF(fp, buffer, 6, -6, num, region, vertices, vertexMap)){
          if(vertices.size() == 15)
            elements[5][region].push_back
	      (new MPrism15(vertices[0], vertices[1], vertices[2], vertices[3],
			    vertices[4], vertices[5], vertices[6], vertices[8],
			    vertices[9], vertices[7], vertices[10], vertices[11],
			    vertices[12], vertices[14], vertices[13], num));
          else
            elements[5][region].push_back(new MPrism(vertices, num));
        }
      }
      else if(!strncmp(buffer, "CPYRAM", 6)){
        if(readElementBDF(fp, buffer, 6, 5, num, region, vertices, vertexMap))
          elements[6][region].push_back(new MPyramid(vertices, num));
      }
    }
  }
  
  for(int i = 0; i < (int)(sizeof(elements)/sizeof(elements[0])); i++) 
    _storeElementsInEntities(elements[i]);
  _associateEntityWithMeshVertices();
  storeVerticesInEntities(vertexMap);

  fclose(fp);
  return 1;
}

int GModel::writeBDF(const std::string &name, int format, bool saveAll, 
                     double scalingFactor)
{
  FILE *fp = fopen(name.c_str(), "w");
  if(!fp){
    Msg::Error("Unable to open file '%s'", name.c_str());
    return 0;
  }

  if(noPhysicalGroups()) saveAll = true;

  indexMeshVertices(saveAll);

  fprintf(fp, "$ Created by Gmsh\n");

  for(viter it = firstVertex(); it != lastVertex(); ++it)
    for(unsigned int i = 0; i < (*it)->mesh_vertices.size(); i++) 
      (*it)->mesh_vertices[i]->writeBDF(fp, format, scalingFactor);
  for(eiter it = firstEdge(); it != lastEdge(); ++it)
    for(unsigned int i = 0; i < (*it)->mesh_vertices.size(); i++)
      (*it)->mesh_vertices[i]->writeBDF(fp, format, scalingFactor);
  for(fiter it = firstFace(); it != lastFace(); ++it)
    for(unsigned int i = 0; i < (*it)->mesh_vertices.size(); i++) 
      (*it)->mesh_vertices[i]->writeBDF(fp, format, scalingFactor);
  for(riter it = firstRegion(); it != lastRegion(); ++it)
    for(unsigned int i = 0; i < (*it)->mesh_vertices.size(); i++) 
      (*it)->mesh_vertices[i]->writeBDF(fp, format, scalingFactor);
  
  for(eiter it = firstEdge(); it != lastEdge(); ++it){
    if(saveAll || (*it)->physicals.size()){
      for(unsigned int i = 0; i < (*it)->lines.size(); i++)
        (*it)->lines[i]->writeBDF(fp, format, (*it)->tag());
    }
  }
  for(fiter it = firstFace(); it != lastFace(); ++it){
    if(saveAll || (*it)->physicals.size()){
      for(unsigned int i = 0; i < (*it)->triangles.size(); i++)
        (*it)->triangles[i]->writeBDF(fp, format, (*it)->tag());
      for(unsigned int i = 0; i < (*it)->quadrangles.size(); i++)
        (*it)->quadrangles[i]->writeBDF(fp, format, (*it)->tag());
    }
  }
  for(riter it = firstRegion(); it != lastRegion(); ++it){
    if(saveAll || (*it)->physicals.size()){
      for(unsigned int i = 0; i < (*it)->tetrahedra.size(); i++)
        (*it)->tetrahedra[i]->writeBDF(fp, format, (*it)->tag());
      for(unsigned int i = 0; i < (*it)->hexahedra.size(); i++)
        (*it)->hexahedra[i]->writeBDF(fp, format, (*it)->tag());
      for(unsigned int i = 0; i < (*it)->prisms.size(); i++)
        (*it)->prisms[i]->writeBDF(fp, format, (*it)->tag());
      for(unsigned int i = 0; i < (*it)->pyramids.size(); i++)
        (*it)->pyramids[i]->writeBDF(fp, format, (*it)->tag());
    }
  }
  
  fprintf(fp, "ENDDATA\n");
   
  fclose(fp);
  return 1;
}

int GModel::readP3D(const std::string &name)
{
  FILE *fp = fopen(name.c_str(), "r");
  if(!fp){
    Msg::Error("Unable to open file '%s'", name.c_str());
    return 0;
  }

  int numBlocks = 0;
  if(fscanf(fp, "%d", &numBlocks) != 1 || numBlocks <= 0) return 0;

  std::vector<int> Ni(numBlocks), Nj(numBlocks), Nk(numBlocks);
  for(int n = 0; n < numBlocks; n++)
    if(fscanf(fp, "%d %d %d", &Ni[n], &Nj[n], &Nk[n]) != 3) return 0;

  for(int n = 0; n < numBlocks; n++){
    if(Nk[n] == 1){
      GFace *gf = new discreteFace(this, getNumFaces() + 1);
      add(gf);
      gf->transfinite_vertices.resize(Ni[n]);
      for(int i = 0; i < Ni[n]; i++)
        gf->transfinite_vertices[i].resize(Nj[n]);
      for(int coord = 0; coord < 3; coord++){
        for(int i = 0; i < Ni[n]; i++){
          for(int j = 0; j < Nj[n]; j++){
            double d;
            if(fscanf(fp, "%lf", &d) != 1) return 0;
            if(coord == 0){
              MVertex *v = new MVertex(d, 0., 0., gf);
              gf->transfinite_vertices[i][j] = v;
              gf->mesh_vertices.push_back(v);
            }
            else if(coord == 1){
              gf->transfinite_vertices[i][j]->y() = d;
            }
            else if(coord == 2){
              gf->transfinite_vertices[i][j]->z() = d;
            }
          }
        }
      }
      for(unsigned int i = 0; i < gf->transfinite_vertices.size() - 1; i++)
        for(unsigned int j = 0; j < gf->transfinite_vertices[0].size() - 1; j++)
          gf->quadrangles.push_back
            (new MQuadrangle(gf->transfinite_vertices[i    ][j    ],
                             gf->transfinite_vertices[i + 1][j    ],
                             gf->transfinite_vertices[i + 1][j + 1],
                             gf->transfinite_vertices[i    ][j + 1]));
    }
    else{
      GRegion *gr = new discreteRegion(this, getNumRegions() + 1);
      add(gr);
      gr->transfinite_vertices.resize(Ni[n]);
      for(int i = 0; i < Ni[n]; i++){
        gr->transfinite_vertices[i].resize(Nj[n]);
        for(int j = 0; j < Nj[n]; j++){
          gr->transfinite_vertices[i][j].resize(Nk[n]);
        }
      }
      for(int coord = 0; coord < 3; coord++){
        for(int i = 0; i < Ni[n]; i++){
          for(int j = 0; j < Nj[n]; j++){
            for(int k = 0; k < Nk[n]; k++){
              double d;
              if(fscanf(fp, "%lf", &d) != 1) return 0;
              if(coord == 0){
                MVertex *v = new MVertex(d, 0., 0., gr);
                gr->transfinite_vertices[i][j][k] = v;
                gr->mesh_vertices.push_back(v);
              }
              else if(coord == 1){
                gr->transfinite_vertices[i][j][k]->y() = d;
              }
              else if(coord == 2){
                gr->transfinite_vertices[i][j][k]->z() = d;
              }
            }
          }
        }
      }
      for(unsigned int i = 0; i < gr->transfinite_vertices.size() - 1; i++)
        for(unsigned int j = 0; j < gr->transfinite_vertices[0].size() - 1; j++)
          for(unsigned int k = 0; k < gr->transfinite_vertices[0][0].size() - 1; k++)
            gr->hexahedra.push_back
              (new MHexahedron(gr->transfinite_vertices[i    ][j    ][k    ],
                               gr->transfinite_vertices[i + 1][j    ][k    ],
                               gr->transfinite_vertices[i + 1][j + 1][k    ],
                               gr->transfinite_vertices[i    ][j + 1][k    ],
                               gr->transfinite_vertices[i    ][j    ][k + 1],
                               gr->transfinite_vertices[i + 1][j    ][k + 1],
                               gr->transfinite_vertices[i + 1][j + 1][k + 1],
                               gr->transfinite_vertices[i    ][j + 1][k + 1]));
    }
  }  
  
  fclose(fp);
  return 1;
}

int GModel::writeP3D(const std::string &name, bool saveAll, double scalingFactor)
{
  FILE *fp = fopen(name.c_str(), "w");
  if(!fp){
    Msg::Error("Unable to open file '%s'", name.c_str());
    return 0;
  }

  if(noPhysicalGroups()) saveAll = true;

  std::vector<GFace*> faces;
  for(fiter it = firstFace(); it != lastFace(); ++it)
    if((*it)->transfinite_vertices.size() && 
       (*it)->transfinite_vertices[0].size() &&
       ((*it)->physicals.size() || saveAll)) faces.push_back(*it);

  std::vector<GRegion*> regions;
  for(riter it = firstRegion(); it != lastRegion(); ++it)
    if((*it)->transfinite_vertices.size() && 
       (*it)->transfinite_vertices[0].size() && 
       (*it)->transfinite_vertices[0][0].size() && 
       ((*it)->physicals.size() || saveAll)) regions.push_back(*it);
  
  if(faces.empty() && regions.empty()){
    Msg::Warning("No structured grids to save");
    fclose(fp);
    return 0;
  }

  fprintf(fp, "%d\n", (int)(faces.size() + regions.size()));
  
  for(unsigned int i = 0; i < faces.size(); i++)
    fprintf(fp, "%d %d 1\n", 
            (int)faces[i]->transfinite_vertices.size(),
            (int)faces[i]->transfinite_vertices[0].size());

  for(unsigned int i = 0; i < regions.size(); i++)
    fprintf(fp, "%d %d %d\n", 
            (int)regions[i]->transfinite_vertices.size(),
            (int)regions[i]->transfinite_vertices[0].size(),
            (int)regions[i]->transfinite_vertices[0][0].size());
  
  for(unsigned int i = 0; i < faces.size(); i++){
    GFace *gf = faces[i];
    for(int coord = 0; coord < 3; coord++){
      for(unsigned int j = 0; j < gf->transfinite_vertices.size(); j++){
        for(unsigned int k = 0; k < gf->transfinite_vertices[j].size(); k++){
          MVertex *v = gf->transfinite_vertices[j][k];
          double d = (coord == 0) ? v->x() : (coord == 1) ? v->y() : v->z();
          fprintf(fp, "%g ", d * scalingFactor);
        }
        fprintf(fp, "\n");
      }
    }
  }
  
  for(unsigned int i = 0; i < regions.size(); i++){
    GRegion *gr = regions[i];
    for(int coord = 0; coord < 3; coord++){
      for(unsigned int j = 0; j < gr->transfinite_vertices.size(); j++){
        for(unsigned int k = 0; k < gr->transfinite_vertices[j].size(); k++){
          for(unsigned int l = 0; l < gr->transfinite_vertices[j][k].size(); l++){
            MVertex *v = gr->transfinite_vertices[j][k][l];
            double d = (coord == 0) ? v->x() : (coord == 1) ? v->y() : v->z();
            fprintf(fp, "%g ", d * scalingFactor);
          }
          fprintf(fp, "\n");
        }
      }
    }
  }
  
  fclose(fp);
  return 1;
}

int GModel::writeVTK(const std::string &name, bool binary, bool saveAll,
                     double scalingFactor)
{
  Msg::Error("VTK export is experimental:");
  Msg::Error(" * vertex ordering for second order elements is wrong");
  Msg::Error(" * binary export crashes paraview on Mac: can somebody test");
  Msg::Error("   on another platform? I *think* I followed the spec, but");
  Msg::Error("   I probably missed something...");

  FILE *fp = fopen(name.c_str(), binary ? "wb" : "w");
  if(!fp){
    Msg::Error("Unable to open file '%s'", name.c_str());
    return 0;
  }

  if(noPhysicalGroups()) saveAll = true;

  // get the number of vertices and index the vertices in a continuous
  // sequence
  int numVertices = indexMeshVertices(saveAll);

  fprintf(fp, "# vtk DataFile Version 2.0\n");
  fprintf(fp, "%s, Created by Gmsh\n", getName().c_str());
  if(binary)
    fprintf(fp, "BINARY\n");
  else
    fprintf(fp, "ASCII\n");
  fprintf(fp, "DATASET UNSTRUCTURED_GRID\n");

  // get all the entities in the model
  std::vector<GEntity*> entities = getEntities();

  // write mesh vertices
  fprintf(fp, "POINTS %d double\n", numVertices);
  for(unsigned int i = 0; i < entities.size(); i++)
    for(unsigned int j = 0; j < entities[i]->mesh_vertices.size(); j++) 
      entities[i]->mesh_vertices[j]->writeVTK(fp, binary, scalingFactor);
  fprintf(fp, "\n");
  
  // loop over all elements we need to save and count vertices
  int numElements = 0, totalNumInt = 0;
  for(unsigned int i = 0; i < entities.size(); i++){
    if(entities[i]->physicals.size() || saveAll){
      numElements += entities[i]->getNumMeshElements();
      for(unsigned int j = 0; j < entities[i]->getNumMeshElements(); j++){
	if(entities[i]->getMeshElement(j)->getTypeForVTK())
	  totalNumInt += entities[i]->getMeshElement(j)->getNumVertices() + 1;
      }
    }
  }

  // print vertex indices in ascii or binary
  fprintf(fp, "CELLS %d %d\n", numElements, totalNumInt);
  for(unsigned int i = 0; i < entities.size(); i++){
    if(entities[i]->physicals.size() || saveAll){
      for(unsigned int j = 0; j < entities[i]->getNumMeshElements(); j++){
	if(entities[i]->getMeshElement(j)->getTypeForVTK())
	  entities[i]->getMeshElement(j)->writeVTK(fp, binary);
      }
    }
  }
  fprintf(fp, "\n");
  
  // print element types in ascii or binary  
  fprintf(fp, "CELL_TYPES %d\n", numElements);
  for(unsigned int i = 0; i < entities.size(); i++){
    if(entities[i]->physicals.size() || saveAll){
      for(unsigned int j = 0; j < entities[i]->getNumMeshElements(); j++){
	int type = entities[i]->getMeshElement(j)->getTypeForVTK();
	if(type){
	  if(binary) 
	    fwrite(&type, sizeof(int), 1, fp);
	  else
	    fprintf(fp, "%d\n", type);
	}
      }
    }
  }
  
  fclose(fp);
  return 1;
}

int GModel::readVTK(const std::string &name)
{
  Msg::Error("VTK reader is not implemented yet");
  return 0;
}
