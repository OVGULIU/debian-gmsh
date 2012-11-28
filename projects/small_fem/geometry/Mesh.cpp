#include <vector>
#include <sstream>

#include "GeoExtractor.h"
#include "Exception.h"
#include "Mesh.h"


using namespace std;

Mesh::Mesh(const std::string fileName){ 
  // New Mode //
  model = new GModel("SmallFEM");

  // Read Mesh //
  if(!model->readMSH(fileName))
    throw Exception("Can't open file: %s", fileName.c_str());

  // Get Entity //
  vector<GEntity*> entity;
  model->getEntities(entity);

  // Extract Element //
  pair<
    map<const MElement*, unsigned int, ElementComparator>*,
    multimap<int, const MElement*>*
    >
    elementsExtracted = GeoExtractor::extractElement(entity);

  element  = elementsExtracted.first;
  physical = elementsExtracted.second;

  // Extract Nodes, Edges and Faces //
  vertex = GeoExtractor::extractVertex(*element);
  edge   = GeoExtractor::extractEdge(*element);
  face   = GeoExtractor::extractFace(*element);
  
  // Number Geometry //
  nextId = 0;
  number();
}

Mesh::~Mesh(void){
  // Delete Elements //
  
  // WARNING
  // Mesh is *NOT* responsible for
  // Deleting MElement*
  delete element;
  delete physical;

  // Delete Vertices //

  // WARNING
  // Mesh is *NOT* responsible for
  // Deleting MVertex*
  delete vertex;

  // Delete Edges //
  const map<const MEdge*, unsigned int, EdgeComparator>::iterator
    endE = edge->end();
  
  map<const MEdge*, unsigned int, EdgeComparator>::iterator 
    itE = edge->begin();

  for(; itE != endE; itE++)
    delete itE->first;

  delete edge;  

  // Delete Faces //
  const map<const MFace*, unsigned int, FaceComparator>::iterator
    endF = face->end();
  
  map<const MFace*, unsigned int, FaceComparator>::iterator 
    itF = face->begin();

  for(; itF != endF; itF++)
    delete itF->first;

  delete face;  

  // Delete Model //
  delete model;
}

unsigned int Mesh::getGlobalId(const MElement& element) const{
  map<const MElement*, 
      unsigned int, 
      ElementComparator>::iterator it = 
    this->element->find(&element);

  if(it == this->element->end())
    throw 
      Exception("Element not found");

  return it->second;   
}

unsigned int Mesh::getGlobalId(const MVertex& vertex) const{
  map<const MVertex*, 
      unsigned int, 
      VertexComparator>::iterator it = 
    this->vertex->find(&vertex);

  if(it == this->vertex->end())
    throw 
      Exception("Vertex not found");

  return it->second; 
}

unsigned int Mesh::getGlobalId(const MEdge& edge) const{
  // Look for Edge //
  map<const MEdge*, 
      unsigned int, 
      EdgeComparator>::iterator it = 
    this->edge->find(&edge);

  if(it == this->edge->end()){
    throw 
      Exception("Edge not found");
  }

  return it->second; 
}

unsigned int Mesh::getGlobalId(const MFace& face) const{
  // Look for Face //
  map<const MFace*, 
      unsigned int, 
      FaceComparator>::iterator it = 
    this->face->find(&face);

  if(it == this->face->end()){
    throw 
      Exception("Face not found");
  }

  return it->second; 
}

const vector<const MVertex*> Mesh::getAllVertex(void) const{
  // Init
  const unsigned int size = vertex->size();

  map<const MVertex*, unsigned int, VertexComparator>::iterator
    itV = vertex->begin();

  // Alloc
  vector<const MVertex*> v(size);

  // Fill Vector
  for(unsigned int i = 0; i < size; i++, itV++)
    v[i] = itV->first;

  // Return 
  return v;
}

void Mesh::number(void){
  // Get Iterators //
  const map<const MElement*, unsigned int, ElementComparator>::iterator
    endEl = element->end();           

  const map<const MVertex*, unsigned int, VertexComparator>::iterator
    endV = vertex->end();           

  const map<const MEdge*, unsigned int, EdgeComparator>::iterator
    endEd = edge->end();           

  const map<const MFace*, unsigned int, FaceComparator>::iterator
    endF = face->end();           
  
  map<const MElement*, unsigned int, ElementComparator>::iterator
    itEl = element->begin();

  map<const MVertex*, unsigned int, VertexComparator>::iterator
    itV = vertex->begin();           
  
  map<const MEdge*, unsigned int, EdgeComparator>::iterator
    itEd = edge->begin();

  map<const MFace*, unsigned int, FaceComparator>::iterator
    itF = face->begin();
  
  // Number Vertices //
  for(; itV != endV; itV++){
    itV->second = nextId;
    nextId++;
  }

  // Number Edges //
  for(; itEd != endEd; itEd++){
    itEd->second = nextId;
    nextId++;
  }

  // Number Faces //
  for(; itF != endF; itF++){
    itF->second = nextId;
    nextId++;
  }

  // Number Elements //
  for(; itEl != endEl; itEl++){
    itEl->second = nextId;
    nextId++;
  }
}

GroupOfElement Mesh::getFromPhysical(int physicalId) const{
  const std::pair<std::multimap<int, const MElement*>::iterator, 
                  std::multimap<int, const MElement*>::iterator> p = 
    physical->equal_range(physicalId);
  
  return GroupOfElement(p.first, p.second, *this);
}

string Mesh::toString(void) const{
  // Iterators //
  const map<const MElement*, unsigned int, ElementComparator>::iterator
    endEl = element->end();           

  const map<const MVertex*, unsigned int, VertexComparator>::iterator
    endV = vertex->end();           

  const map<const MEdge*, unsigned int, EdgeComparator>::iterator
    endEd = edge->end();           

  const map<const MFace*, unsigned int, FaceComparator>::iterator
    endF = face->end();           
  
  map<const MElement*, unsigned int, ElementComparator>::iterator
    itEl = element->begin();

  map<const MVertex*, unsigned int, VertexComparator>::iterator
    itV = vertex->begin();           
  
  map<const MEdge*, unsigned int, EdgeComparator>::iterator
    itEd = edge->begin();

  map<const MFace*, unsigned int, FaceComparator>::iterator
    itF = face->begin();
  
  stringstream stream;
  

  // Header //
  stream << "***********************************************"    
	 << endl
	 << "*                     Mesh                    *"    
	 << endl
	 << "***********************************************"
	 << endl; 


  // Elements //
  stream << "*                                             *"
	 << endl
	 << "* This mesh contains the following Elements:  *" 
	 << endl;
  
  for(; itEl != endEl; itEl++)
    stream << "*   -- Element "
	   << getGlobalId(*itEl->first)
	   << endl;

  stream << "*                                             *"
	 << endl
	 << "***********************************************"  
	 << endl;  


  // Vertices //
  stream << "*                                             *"
	 << endl
	 << "* This mesh contains the following Vertex:    *" 
	 << endl;
  
  for(; itV != endV; itV++)
    stream << "*   -- Vertex "
	   << getGlobalId(*itV->first)
	   << endl
	   << "*    (["
	   << itV->first->x()
	   << ", "
	   << itV->first->y()
	   << ", "
	   << itV->first->z()
	   << "])"
	   << endl;

  stream << "*                                             *"
	 << endl
	 << "***********************************************"  
	 << endl;

  
  // Edges //
  stream << "*                                             *"
	 << endl
	 << "* This mesh contains the following Edges:     *" 
	 << endl;
  
  for(; itEd != endEd; itEd++)
    stream << "*   -- Edge "
	   << getGlobalId(*itEd->first)
	   << endl
	   << "*    (["
	   << itEd->first->getVertex(0)->x()
	   << ", "
	   << itEd->first->getVertex(0)->y()
	   << ", "
	   << itEd->first->getVertex(0)->z()
	   << "], ["
	   << itEd->first->getVertex(1)->x()
	   << ", "
	   << itEd->first->getVertex(1)->y()
	   << ", "
	   << itEd->first->getVertex(1)->z()
	   << "])"
	   << endl;

  stream << "*                                             *"
	 << endl
	 << "***********************************************"  
	 << endl;

  // Faces //
  stream << "*                                             *"
	 << endl
	 << "* This mesh contains the following Faces:     *" 
	 << endl;
  
  for(; itF != endF; itF++)
    stream << "*   -- Face "
	   << getGlobalId(*itF->first)
	   << endl;

  stream << "*                                             *"
	 << endl
	 << "***********************************************"  
	 << endl;

  
  // Retrun //
  return stream.str();
}
