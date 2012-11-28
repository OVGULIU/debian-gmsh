#include "CurlBasis.h"

using namespace std;

CurlBasis::CurlBasis(const BasisVector& other){
  // Set Basis Type //
  order = other.getOrder() - 1;
  
  type = other.getType();
  dim  = other.getDim();
  
  nVertex = other.getNVertexBased();
  nEdge   = other.getNEdgeBased();
  nFace   = other.getNFaceBased();
  nCell   = other.getNCellBased();
  
  nEdgeClosure = other.getNEdgeClosure();
  nFaceClosure = other.getNFaceClosure();

  size = other.getSize();

  // Alloc Basis //
  node = new vector<vector<Polynomial>*>(nVertex);
  edge = new vector<vector<vector<Polynomial>*>*>(nEdgeClosure);
  face = new vector<vector<vector<Polynomial>*>*>(nFaceClosure);
  cell = new vector<vector<Polynomial>*>(nCell);

  for(int i = 0; i < nEdgeClosure; i++)
    (*edge)[i] = new vector<vector<Polynomial>*>(nEdge);

  for(int i = 0; i < nFaceClosure; i++)
    (*face)[i] = new vector<vector<Polynomial>*>(nFace);  

  // Node Based //
  for(int i = 0; i < nVertex; i++)
    (*node)[i] = 
      new vector<Polynomial>
      (Polynomial::curl(other.getNodeFunction(i)));
  
  // Edge Based //
  for(int i = 0; i < nEdgeClosure; i++)
    for(int j = 0; j < nEdge; j++)
      (*(*edge)[i])[j] = 
	new vector<Polynomial>
	(Polynomial::curl(other.getEdgeFunction(i, j)));

  // Face Based //
  for(int i = 0; i < nFaceClosure; i++)
    for(int j = 0; j < nFace; j++)
      (*(*face)[i])[j] = 
	new vector<Polynomial>
	(Polynomial::curl(other.getFaceFunction(i, j)));

  // Cell Based //
  for(int i = 0; i < nCell; i++)
    (*cell)[i] = 
      new vector<Polynomial>
      (Polynomial::curl(other.getCellFunction(i)));
}

CurlBasis::~CurlBasis(void){
  // Vertex Based //
  for(int i = 0; i < nVertex; i++)
    delete (*node)[i];
  
  delete node;


  // Edge Based //
  for(int c = 0; c < nEdgeClosure; c++){
    for(int i = 0; i < nEdge; i++)
      delete (*(*edge)[c])[i];
    
    delete (*edge)[c];
  }
  
  delete edge;


  // Face Based //
  for(int c = 0; c < nFaceClosure; c++){
    for(int i = 0; i < nFace; i++)
      delete (*(*face)[c])[i];
    
    delete (*face)[c];
  }

  delete face;


  // Cell Based //
  for(int i = 0; i < nCell; i++)
    delete (*cell)[i];
  
  delete cell;
}
