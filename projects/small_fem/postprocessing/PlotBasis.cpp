#include <cstdio>
#include <set>

#include "BasisScalar.h"
#include "BasisVector.h"
#include "Polynomial.h"

#include "PlotBasis.h"

using namespace std;

PlotBasis::PlotBasis(const Basis& basis,
		     const GroupOfElement& group, 
		     Writer& writer){

  this->writer = &writer;
  
  nFunction = basis.getSize();
  getGeometry(group);

  if(basis.isScalar()){
    isScalar = true;
    interpolate(static_cast<const BasisScalar&>(basis));
  }

  else{
    isScalar = false;
    interpolate(static_cast<const BasisVector&>(basis));
  }
}

PlotBasis::~PlotBasis(void){
  delete node;

  if(nodalScalarValue){
    for(int i = 0; i < nFunction; i++)
      delete nodalScalarValue[i];
   
    delete[] nodalScalarValue;
  }

  if(nodalVectorValue){
    for(int i = 0; i < nFunction; i++)
      delete nodalVectorValue[i];
    
    delete[] nodalVectorValue;
  }
}

void PlotBasis::plot(const string name) const{
  // Number of decimals in nFunction //
  // Used for '0' pading in sprintf
  int tmp = nFunction;
  int dec = 0;

  while(tmp != 0){
    dec++;
    tmp /= 10;
  }

  // Plot //
  char fileName[1024];
  
  for(int i = 0, j = 0; i < nFunction; i++, j++){
    // Basis Name
    sprintf(fileName, 
	    "%s%0*d", name.c_str(), dec, i + 1);
   
    // Set Values 
    if(isScalar)
      writer->setValues(*(nodalScalarValue[i]));

    else
      writer->setValues(*(nodalVectorValue[i]));

    // Plot
    writer->write(string(fileName));
  }
}

void PlotBasis::getGeometry(const GroupOfElement& group){
  // Get All Elements //
  element = &(group.getAll());
  E       = element->size();

  // Get All Vertices //
  set<MVertex*, MVertexLessThanNum> setVertex;

  for(int i = 0; i < E; i++){
    const int N = (*element)[i]->getNumVertices();
    MElement* myElement = 
      const_cast<MElement*>((*element)[i]);
    
    for(int j = 0; j < N; j++)
      setVertex.insert(myElement->getVertex(j));
  }

  // Serialize the set into a vector //
  node = new vector<MVertex*>(setVertex.begin(), 
			      setVertex.end());
  N    = node->size();
}


void PlotBasis::interpolate(const BasisScalar& basis){
  // Allocate //
  nodalVectorValue = NULL;
  nodalScalarValue = new vector<double>*[nFunction];

  for(int i = 0; i < nFunction; i++)
    nodalScalarValue[i] = new vector<double>(N);

  
  // Interpolate //
  const unsigned int nFNode = basis.getNVertexBased();
  const unsigned int nFEdge = basis.getNEdgeBased();
  const unsigned int nFFace = basis.getNFaceBased();
  const unsigned int nFCell = basis.getNCellBased();
  
  unsigned int f = 0;

  // Vertex Based
  for(unsigned int i = 0; i < nFNode; i++){
    for(int n = 0; n < N; n++)
      (*nodalScalarValue[f])[n] = 
	basis.getNodeFunction(i).at((*node)[n]->x(),
				    (*node)[n]->y(),
				    (*node)[n]->z());
    
    f++;
  }

  // Edge Based
  for(unsigned int i = 0; i < nFEdge; i++){
    for(int n = 0; n < N; n++)
      (*nodalScalarValue[f])[n] = 
	basis.getEdgeFunction(0, i).at((*node)[n]->x(),
				       (*node)[n]->y(),
				       (*node)[n]->z());
    
    f++;
  }

  // Face Based
  for(unsigned int i = 0; i < nFFace; i++){
    for(int n = 0; n < N; n++)
      (*nodalScalarValue[f])[n] = 
	basis.getFaceFunction(0, i).at((*node)[n]->x(),
				       (*node)[n]->y(),
				       (*node)[n]->z());
    
    f++;
  }

  // Cell Based
  for(unsigned int i = 0; i < nFCell; i++){
    for(int n = 0; n < N; n++)
      (*nodalScalarValue[f])[n] = 
	basis.getCellFunction(i).at((*node)[n]->x(),
				    (*node)[n]->y(),
				    (*node)[n]->z());
    
    f++;
  }
}

void PlotBasis::interpolate(const BasisVector& basis){
  // Allocate //
  nodalScalarValue = NULL;  
  nodalVectorValue = new vector<fullVector<double> >*[nFunction];

  for(int i = 0; i < nFunction; i++)
    nodalVectorValue[i] = new vector<fullVector<double> >(N);
  

  // Interpolate //
  const unsigned int nFNode = basis.getNVertexBased();
  const unsigned int nFEdge = basis.getNEdgeBased();
  const unsigned int nFFace = basis.getNFaceBased();
  const unsigned int nFCell = basis.getNCellBased();

  unsigned int f = 0;

  // Vertex Based
  for(unsigned int i = 0; i < nFNode; i++){
    for(int n = 0; n < N; n++)
      (*nodalVectorValue[f])[n] = 
	Polynomial::at(basis.getNodeFunction(i), 
		       (*node)[n]->x(),
		       (*node)[n]->y(),
		       (*node)[n]->z());
    f++;
  }

  // Edge Based
  for(unsigned int i = 0; i < nFEdge; i++){
    for(int n = 0; n < N; n++)
      (*nodalVectorValue[f])[n] = 
	Polynomial::at(basis.getEdgeFunction(0, i), 
		       (*node)[n]->x(),
		       (*node)[n]->y(),
		       (*node)[n]->z());    
    f++;
  }

  // Face Based
  for(unsigned int i = 0; i < nFFace; i++){
    for(int n = 0; n < N; n++)
      (*nodalVectorValue[f])[n] = 
	Polynomial::at(basis.getFaceFunction(0, i), 
		       (*node)[n]->x(),
		       (*node)[n]->y(),
		       (*node)[n]->z());
    f++;
  }

  // Cell Based
  for(unsigned int i = 0; i < nFCell; i++){
    for(int n = 0; n < N; n++)
      (*nodalVectorValue[f])[n] = 
	Polynomial::at(basis.getCellFunction(i), 
		       (*node)[n]->x(),
		       (*node)[n]->y(),
		       (*node)[n]->z());
    f++;
  }
}
