// Gmsh - Copyright (C) 1997-2011 C. Geuzaine, J.-F. Remacle
//
// See the LICENSE.txt file for license information. Please report all
// bugs and problems to <gmsh@geuz.org>.
//
// Contributed by Matti Pellikka <matti.pellikka@tut.fi>.

#include "CellComplex.h"
#include "Cell.h"

#if defined(HAVE_KBIPACK)

bool Less_Cell::operator()(const Cell* c1, const Cell* c2) const 
{  
  if(c1->isCombined() && !c2->isCombined()) return false;
  if(!c1->isCombined() && c2->isCombined()) return true;
  if(c1->isCombined() && c2->isCombined()){
    return (c1->getNum() < c2->getNum());
  }
  
  if(c1->getNumSortedVertices() != c2->getNumSortedVertices()){
    return (c1->getNumSortedVertices() < c2->getNumSortedVertices());
  }    
  for(int i=0; i < c1->getNumSortedVertices();i++){
    if(c1->getSortedVertex(i) < c2->getSortedVertex(i)) return true;
    else if (c1->getSortedVertex(i) > c2->getSortedVertex(i)) return false;
  }
  return false;
}

Cell::Cell(MElement* image) :  
  _subdomain(false), _combined(false), _index(0), _immune(false), _image(NULL), 
  _delimage(false)
{
  _image = image;
  for(int i = 0; i < getNumVertices(); i++) 
    _vs.push_back(getVertex(i)->getNum()); 
  std::sort(_vs.begin(), _vs.end());
}

Cell::~Cell() 
{
  if(_delimage) delete _image; 
}

bool Cell::findBoundaryElements(std::vector<MElement*>& bdElements)
{
  if(_combined) return false;
  bdElements.clear();
  MElementFactory factory;
  for(int i = 0; i < getNumFacets(); i++){
    std::vector<MVertex*> vertices;
    getFacetVertices(i, vertices);
    int type = _image->getType();
    int newtype = 0;
    if(getDim() == 3){
      if(type == TYPE_TET) newtype = MSH_TRI_3;
      else if(type == TYPE_HEX) newtype = MSH_QUA_4;
      else if(type == TYPE_PRI) {
	if(vertices.size() == 3) newtype = MSH_TRI_3;
	else if(vertices.size() == 4) newtype = MSH_QUA_4;
      }
    }
    else if(getDim() == 2) newtype = MSH_LIN_2;
    else if(getDim() == 1) newtype = MSH_PNT;
    if(newtype == 0){
      printf("Error: mesh element %d not implemented yet! \n", type);
      return false;
    }
    MElement* element = factory.create(newtype, vertices, 0, 
				       _image->getPartition());
    bdElements.push_back(element);
  }
  return true;
}

int Cell::getNumFacets() const 
{
  if(_image == NULL || _combined){ 
    printf("ERROR: No image mesh element for cell. \n");
    return 0;
  }
  if(getDim() == 0) return 0;
  else if(getDim() == 1) return 2;
  else if(getDim() == 2) return _image->getNumEdges();
  else if(getDim() == 3) return _image->getNumFaces();
  else return 0;
}

void Cell::getFacetVertices(const int num, std::vector<MVertex*> &v) const 
{
  if(_image == NULL || _combined){ 
    printf("ERROR: No image mesh element for cell. \n");
    return;
  }
  if(getDim() == 0) return;
  else if(getDim() == 1) { v.resize(1); v[0] = getVertex(num); }
  else if(getDim() == 2) _image->getEdgeVertices(num, v);
  else if(getDim() == 3) _image->getFaceVertices(num, v);
  return;
}

int Cell::findBoundaryCellOrientation(Cell* cell) 
{
  if(_image == NULL || _combined){ 
    printf("ERROR: No image mesh element for cell. \n");
    return 0;
  }
  std::vector<MVertex*> v; 
  for(int i = 0; i < cell->getNumVertices(); i++) {
    v.push_back(cell->getVertex(i));
  }
  if(getDim() == 0) return 0;
  else if(getDim() == 1){
    if(v.size() != 1) return 0;
    else if(v[0] == getVertex(0)) return -1;
    else if(v[0] == getVertex(1)) return 1;
    else return 0;
  }
  else if(getDim() == 2){
    if(v.size() != 2) return 0;
    MEdge facet = MEdge(v[0], v[1]);
    int ithFacet = 0;
    int sign = 0;
    _image->getEdgeInfo(facet, ithFacet, sign);
    return sign;
  }
  else if(getDim() == 3){
    if(v.size() != 3) return 0;
    MFace facet = MFace(v);
    int ithFacet = 0;
    int sign = 0;
    int rot = 0;
    _image->getFaceInfo(facet, ithFacet, sign, rot);
    return sign;
  }
  else return 0;
}  

bool Cell::hasVertex(int vertex) const 
{
  std::vector<int>::const_iterator it = std::find(_vs.begin(), _vs.end(), 
						  vertex);
  if (it != _vs.end()) return true;
  else return false;
}

bool CombinedCell::hasVertex(int vertex) const
{
  for(std::map<Cell*, int, Less_Cell>::const_iterator cit = _cells.begin();
      cit != _cells.end(); cit++){
    if(cit->first->hasVertex(vertex)) return true;
  }
  return false;
}

void Cell::printCell() 
{
  printf("%d-cell: \n" , getDim());
  printf("Vertices: ");
  for(int i = 0; i < this->getNumSortedVertices(); i++){
    printf("%d ", this->getSortedVertex(i));
  }
  printf(", in subdomain: %d, ", inSubdomain());
  printf("combined: %d. \n" , isCombined() );
};

void Cell::restoreCell(){
  _bd = _obd;
  _cbd = _ocbd;
  _combined = false;
  _index = 0;
  _immune = false;   
}

void Cell::addBoundaryCell(int orientation, Cell* cell, 
			   bool orig, bool other) 
{
  if(orig) _obd.insert( std::make_pair(cell, orientation ) );
  biter it = _bd.find(cell);
  if(it != _bd.end()){
    int newOrientation = (*it).second + orientation;
    if(newOrientation != 0) (*it).second = newOrientation;
    else {
      _bd.erase(it);
      (*it).first->removeCoboundaryCell(this,false);
      return;
    }
  }
  else _bd.insert( std::make_pair(cell, orientation ) );
  if(other) cell->addCoboundaryCell(orientation, this, orig, false);
}

void Cell::addCoboundaryCell(int orientation, Cell* cell, 
			     bool orig, bool other) 
{
  if(orig) _ocbd.insert( std::make_pair(cell, orientation ) );
  biter it = _cbd.find(cell);
  if(it != _cbd.end()){
    int newOrientation = (*it).second + orientation;
    if(newOrientation != 0) (*it).second = newOrientation;
    else {
      _cbd.erase(it);
      (*it).first->removeBoundaryCell(this,false);
      return;
    }
  }
  else _cbd.insert( std::make_pair(cell, orientation ) );
  if(other) cell->addBoundaryCell(orientation, this, orig, false);
}

void Cell::removeBoundaryCell(Cell* cell, bool other) 
{
  biter it = _bd.find(cell);
  if(it != _bd.end()){
    _bd.erase(it);
    if(other) (*it).first->removeCoboundaryCell(this, false);
  }
}
 
void Cell::removeCoboundaryCell(Cell* cell, bool other) 
{
  biter it = _cbd.find(cell);
  if(it != _cbd.end()){
    _cbd.erase(it);
    if(other) (*it).first->removeBoundaryCell(this, false);
  }
}
   
bool Cell::hasBoundary(Cell* cell, bool orig)
{
  if(!orig){
    biter it = _bd.find(cell);
    if(it != _bd.end()) return true;
    return false;
  }
  else{
    biter it = _obd.find(cell);
    if(it != _obd.end()) return true;
    return false;
  }
}

bool Cell::hasCoboundary(Cell* cell, bool orig)
{
  if(!orig){
    biter it = _cbd.find(cell);
    if(it != _cbd.end()) return true;
    return false;
  }
  else{
    biter it = _ocbd.find(cell);
    if(it != _ocbd.end()) return true;
    return false;
  } 
}

void Cell::printBoundary(bool orig) 
{  
  for(biter it = firstBoundary(orig); it != lastBoundary(orig); it++){
    printf("Boundary cell orientation: %d ", (*it).second);
    Cell* cell2 = (*it).first;
    cell2->printCell();
  }
  if(firstBoundary(orig) == lastBoundary(orig)){
    printf("Cell boundary is empty. \n");
  }
}

void Cell::printCoboundary(bool orig) 
{
  for(biter it = firstCoboundary(orig); it != lastCoboundary(orig); it++){
    printf("Coboundary cell orientation: %d, ", (*it).second);
    Cell* cell2 = (*it).first;
    cell2->printCell();
    if(firstCoboundary(orig) == lastCoboundary(orig)){
      printf("Cell coboundary is empty. \n");
    }
  }
}

int CombinedCell::_globalNum = 0;
CombinedCell::CombinedCell(Cell* c1, Cell* c2, bool orMatch, bool co) : Cell() 
{  
  // use "smaller" cell as c2
  if(c1->getNumCells() < c2->getNumCells()){
    Cell* temp = c1;
    c1 = c2;
    c2 = temp;
  }
  
  _num = ++_globalNum;
  _index = c1->getIndex();
  _subdomain = c1->inSubdomain();
  _combined = true;

  // cells
  c1->getCells(_cells);
  std::map< Cell*, int, Less_Cell > c2Cells;
  c2->getCells(c2Cells);
  for(citer cit  = c2Cells.begin(); cit != c2Cells.end(); cit++){
    if(!orMatch) (*cit).second = -1*(*cit).second;
    _cells.insert(*cit);
  }

  // boundary cells
  for(biter it = c1->firstBoundary(); it != c1->lastBoundary(); it++){
    Cell* cell = (*it).first;
    int ori = (*it).second;
    cell->removeCoboundaryCell(c1, false); 
    this->addBoundaryCell(ori, cell, false, true);
  }
  for(biter it = c2->firstBoundary(); it != c2->lastBoundary(); it++){
    Cell* cell = (*it).first;
    if(!orMatch) (*it).second = -1*(*it).second;
    int ori = (*it).second;
    cell->removeCoboundaryCell(c2, false);    
    if(co && !c1->hasBoundary(cell)){
      this->addBoundaryCell(ori, cell, false, true);
    }
    else if(!co) this->addBoundaryCell(ori, cell, false, true);
  }
  c1->clearBoundary();
  c2->clearBoundary();

  // coboundary cells
  for(biter it = c1->firstCoboundary(); it != c1->lastCoboundary(); it++){
    Cell* cell = (*it).first;
    int ori = (*it).second;
    cell->removeBoundaryCell(c1, false); 
    this->addCoboundaryCell(ori, cell, false, true);
  }
  for(biter it = c2->firstCoboundary(); it != c2->lastCoboundary(); it++){
    Cell* cell = (*it).first;
    if(!orMatch) (*it).second = -1*(*it).second;
    int ori = (*it).second;
    cell->removeBoundaryCell(c2, false);    
    if(!co && !c1->hasCoboundary(cell)){
      this->addCoboundaryCell(ori, cell, false, true);
    }
    else if(co) this->addCoboundaryCell(ori, cell, false, true);
  }
  c1->clearCoboundary();
  c2->clearCoboundary();
  

}

CombinedCell::CombinedCell(std::vector<Cell*>& cells) : Cell() 
{  
  _num = ++_globalNum;
  _index = cells.at(0)->getIndex();
  _subdomain = cells.at(0)->inSubdomain();
  _combined = true;

  // cells
  for(unsigned int i = 0; i < cells.size(); i++){
    Cell* c = cells.at(i);
    _cells[c] = 1;
  }
}


#endif
