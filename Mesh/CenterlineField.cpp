// Gmsh - Copyright (C) 1997-2012 C. Geuzaine, J.-F. Remacle
//
// See the LICENSE.txt file for license information. Please report all
// bugs and problems to <gmsh@geuz.org>.
//
// Contributor(s):
//   Emilie Marchandise
//

#include "CenterlineField.h"

#include <vector>
#include <map>
#include <set>
#include <list>
#include <string>
#include <fstream>
#include <sstream>
#include "OS.h"
#include "GModel.h"
#include "MElement.h"
#include "MTriangle.h"
#include "MVertex.h"
#include "MLine.h"
#include "GEntity.h"
#include "Field.h"
#include "GFace.h"
#include "discreteEdge.h"
#include "discreteFace.h"
#include "GEdgeCompound.h"
#include "GFaceCompound.h"
#include "meshGFace.h"
#include "meshGEdge.h"

#if defined(HAVE_ANN)
#include <ANN/ANN.h>

#include <algorithm>

void erase(std::vector<MLine*>& lines, MLine* l) {
  std::vector<MLine*>::iterator it = std::remove(lines.begin(), lines.end(), l);
  lines.erase(it, lines.end());
}
double computeLength(std::vector<MLine*> lines){

  double length= 0.0;
  for (int i = 0; i< lines.size(); i++){
    length += lines[i]->getInnerRadius(); 
  }
  return length;
}

bool isClosed(std::set<MEdge, Less_Edge> &theCut){

  std::multiset<MVertex*> boundV;
  std::set<MEdge,Less_Edge>::iterator it = theCut.begin();
  for (; it!= theCut.end(); it++){
    boundV.insert(it->getVertex(0));
    boundV.insert(it->getVertex(1));
  }
  std::multiset<MVertex*>::iterator itb = boundV.begin();
  for ( ; itb != boundV.end(); ++itb){
    if (boundV.count(*itb) != 2) {
      return false;
    }
  }
  return true;

  // std::list<MEdge> segments;
  // std::map<MVertex*, MEdge> boundv;
  // std::set<MEdge,Less_Edge>::iterator it = theCut.begin();
  // for (; it!= theCut.end(); it++){
  //   segments.push_back(*it);
  // }

  // // find a lonely MEdge
  // for (std::list<MEdge>::iterator it = segments.begin(); 
  //      it != segments.end(); ++it){
  //   MVertex *vL = it->getVertex(0);
  //   MVertex *vR = it->getVertex(1);
  //   std::map<MVertex*,MEdge>::iterator it1 = boundv.find(vL);
  //   if (it1 == boundv.end()) boundv.insert(std::make_pair(vL,*it));
  //   else  boundv.erase(it1);
  //   std::map<MVertex*,MEdge>::iterator it2 = boundv.find(vR);
  //   if (it2 == boundv.end()) boundv.insert(std::make_pair(vR,*it));
  //   else boundv.erase(it2);
  // }

  // if (boundv.size() == 0) return true; 
  // else return false;

}

void orderMLines(std::vector<MLine*> &lines, MVertex *vB, MVertex *vE){

  std::vector<MLine*> _m;
  std::list<MLine*> segments;
  std::map<MVertex*, MLine*> boundv;
  std::vector<int> _orientation;

  // store all lines in a list : segments
  for (unsigned int i = 0; i < lines.size(); i++){
    segments.push_back(lines[i]);
  }

  // find a lonely MLine
  for (std::list<MLine*>::iterator it = segments.begin(); 
       it != segments.end(); ++it){
    MVertex *vL = (*it)->getVertex(0);
    MVertex *vR = (*it)->getVertex(1);
    std::map<MVertex*,MLine*>::iterator it1 = boundv.find(vL);
    if (it1 == boundv.end()) boundv.insert(std::make_pair(vL,*it));
    else  boundv.erase(it1);
    std::map<MVertex*,MLine*>::iterator it2 = boundv.find(vR);
    if (it2 == boundv.end()) boundv.insert(std::make_pair(vR,*it));
    else boundv.erase(it2);
  }

  // find the first MLine and erase it from the list segments
  MLine *firstLine;
  if (boundv.size() == 2){   // non periodic
    MVertex *v = (boundv.begin())->first;
    if ( v == vB) firstLine = (boundv.begin())->second;
    else{
      MVertex *v = (boundv.rbegin())->first;
      if (v == vB) firstLine = (boundv.rbegin())->second;
      else{ Msg::Error("begin vertex not found for branch"); exit(1);}
    }
    for (std::list<MLine*>::iterator it = segments.begin(); 
         it != segments.end(); ++it){
      if (*it == firstLine){
        segments.erase(it);
        break;
      }
    }
  }
  else{
    Msg::Error("line is wrong (it has %d end points)",  boundv.size());
  }

  // loop over all segments to order segments and store it in the list _m
  _m.push_back(firstLine);
  _orientation.push_back(1);
  MVertex *first = _m[0]->getVertex(0);
  MVertex *last = _m[0]->getVertex(1);
  while (first != last){
    if (segments.empty())break;
    bool found = false;
    for (std::list<MLine*>::iterator it = segments.begin(); 
         it != segments.end(); ++it){
      MLine *e = *it;
      if (e->getVertex(0) == last){
        _m.push_back(e);
        segments.erase(it);
        _orientation.push_back(1);
        last = e->getVertex(1);
        found = true;
        break;
      }
      else if (e->getVertex(1) == last){
        _m.push_back(e);
        segments.erase(it);
        _orientation.push_back(0);
        last = e->getVertex(0);
        found = true;
        break;
      }
    }
    if (!found  && _orientation[0]==1){ //reverse orientation of first Line
      if (_m.size() == 1 ){
        MVertex *temp = first;
        first = last;
        last = temp;
        _orientation[0] = 0;
      }
      else {
        Msg::Error("lines is wrong");
        return;
      }
    }
  }

  //lines is now a list of ordered MLines
  lines = _m;
  
  //special case reverse orientation
  if (lines.size() < 2) return;
  if (_orientation[0] && lines[0]->getVertex(1) != lines[1]->getVertex(1)
      && lines[0]->getVertex(1) != lines[1]->getVertex(0)){
    for (unsigned int i = 0; i < lines.size(); i++) 
      _orientation[i] = !_orientation[i];
  }

  // if (junctions.find(lines[0]->getVertex(0)) != junctions.end()) vB =  lines[0]->getVertex(0);
  // else if (junctions.find(lines[0]->getVertex(1)) != junctions.end()) vB =  lines[0]->getVertex(1);
  // else printf("no vB junc found %d %d \n", lines[0]->getVertex(0)->getNum(), lines[0]->getVertex(1)->getNum());
  // if (junctions.find(lines[lines.size()-1]->getVertex(0)) != junctions.end()) vE =  lines[lines.size()-1]->getVertex(0);
  // else if (junctions.find(lines[lines.size()-1]->getVertex(1)) != junctions.end()) vE =  lines[lines.size()-1]->getVertex(1);	
  // else printf("no vE junc found %d %d \n", lines[0]->getVertex(0)->getNum(), lines[0]->getVertex(1)->getNum());
  // printf("in order vB= %d =%d \n", vB->getNum(), vE->getNum());
}

static void recurConnectByMEdge(const MEdge &e,
				std::multimap<MEdge, MTriangle*, Less_Edge> &e2e,
				std::set<MTriangle*> &group,
				std::set<MEdge, Less_Edge> &touched, 
				std::set<MEdge, Less_Edge> &theCut)
{
  if (touched.find(e) != touched.end()) return;
  touched.insert(e);
  for (std::multimap <MEdge, MTriangle*, Less_Edge>::iterator it = e2e.lower_bound(e);
       it != e2e.upper_bound(e); ++it){
    group.insert(it->second);
    for (int i = 0; i < it->second->getNumEdges(); ++i){
      MEdge me = it->second->getEdge(i);
      if (theCut.find(me) != theCut.end()){ 
	touched.insert(me); //break; 
      }
      else recurConnectByMEdge(me, e2e, group, touched, theCut);
    }
  }
}


void cutTriangle(MTriangle *tri, 
		 std::map<MEdge,MVertex*,Less_Edge> &cutEdges, 
		 std::set<MVertex*> &cutVertices,
		 std::vector<MTriangle*> &newTris, 
		 std::set<MEdge,Less_Edge> &newCut){

  MVertex *c[3] = {0,0,0};
  for (int j=0;j<3;j++){
    MEdge ed = tri->getEdge(j); 
    std::map<MEdge,MVertex*,Less_Edge> :: iterator it = cutEdges.find(ed);
    if (it != cutEdges.end()){
      c[j] = it->second;
    }
  }
  if (c[0] && c[1]){
    newTris.push_back(new MTriangle (c[0],tri->getVertex(1),c[1]));
    newTris.push_back(new MTriangle (tri->getVertex(0),c[0],tri->getVertex(2)));
    newTris.push_back(new MTriangle (tri->getVertex(2),c[0],c[1]));
    newCut.insert(MEdge(c[0],c[1]));
  }
  else if (c[0] && c[2]){
    newTris.push_back(new MTriangle (tri->getVertex(0),c[0],c[2]));
    newTris.push_back(new MTriangle (c[0],tri->getVertex(1),tri->getVertex(2)));
    newTris.push_back(new MTriangle (tri->getVertex(2),c[2],c[0]));
    newCut.insert(MEdge(c[0],c[2]));
  }
  else if (c[1] && c[2]){
    newTris.push_back(new MTriangle (tri->getVertex(2),c[2],c[1]));
    newTris.push_back(new MTriangle (tri->getVertex(0),tri->getVertex(1),c[2]));
    newTris.push_back(new MTriangle (c[2],tri->getVertex(1),c[1]));
    newCut.insert(MEdge(c[1],c[2]));
  }
  else if (c[0]){
    newTris.push_back(new MTriangle (tri->getVertex(0),c[0],tri->getVertex(2)));
    newTris.push_back(new MTriangle (tri->getVertex(2),c[0],tri->getVertex(1)));
    if (cutVertices.find (tri->getVertex(0)) != cutVertices.end()){
      newCut.insert(MEdge(c[0],tri->getVertex(0)));
    }
    else if (cutVertices.find (tri->getVertex(1)) != cutVertices.end()) {
      newCut.insert(MEdge(c[0],tri->getVertex(1)));
    }
    else if (cutVertices.find (tri->getVertex(2)) != cutVertices.end()){
      newCut.insert(MEdge(c[0],tri->getVertex(2)));
    }
  }
  else if (c[1]){
    newTris.push_back(new MTriangle (tri->getVertex(1),c[1],tri->getVertex(0)));
    newTris.push_back(new MTriangle (tri->getVertex(0),c[1],tri->getVertex(2)));
    if (cutVertices.find (tri->getVertex(0)) != cutVertices.end()){
      newCut.insert(MEdge(c[1],tri->getVertex(0)));
    }
    else if (cutVertices.find (tri->getVertex(1)) != cutVertices.end()) {
      newCut.insert(MEdge(tri->getVertex(1), c[1]));
    }
    else if (cutVertices.find (tri->getVertex(2)) != cutVertices.end()){
      newCut.insert(MEdge(c[1],tri->getVertex(2)));
    }
  }
  else if (c[2]){
      newTris.push_back(new MTriangle (tri->getVertex(0),tri->getVertex(1), c[2]));
      newTris.push_back(new MTriangle (tri->getVertex(1),tri->getVertex(2), c[2]));
    if (cutVertices.find (tri->getVertex(0)) != cutVertices.end()){
      newCut.insert(MEdge(c[2],tri->getVertex(0)));
    }
    else if (cutVertices.find (tri->getVertex(1)) != cutVertices.end()) {
      newCut.insert(MEdge(c[2], tri->getVertex(1)));
    }
    else if (cutVertices.find (tri->getVertex(2)) != cutVertices.end()){
      newCut.insert(MEdge(c[2], tri->getVertex(2)));
    }
  }
  else {
    newTris.push_back(tri);
    if (cutVertices.find (tri->getVertex(0)) != cutVertices.end() &&
	cutVertices.find (tri->getVertex(1)) != cutVertices.end())
      newCut.insert(MEdge(tri->getVertex(0),tri->getVertex(1)));
    else if (cutVertices.find (tri->getVertex(1)) != cutVertices.end() &&
	cutVertices.find (tri->getVertex(2)) != cutVertices.end())
      newCut.insert(MEdge(tri->getVertex(1),tri->getVertex(2)));
    else if (cutVertices.find (tri->getVertex(2)) != cutVertices.end() &&
	cutVertices.find (tri->getVertex(0)) != cutVertices.end())
      newCut.insert(MEdge(tri->getVertex(2),tri->getVertex(0)));
  }

}

Centerline::Centerline(std::string fileName): kdtree(0), nodes(0){
  
  recombine = CTX::instance()->mesh.recombineAll;

  index = new ANNidx[1];
  dist = new ANNdist[1];

  printf("cetreline filename =%s \n", fileName.c_str());
  importFile(fileName);
  buildKdTree();

  update_needed = false;

}
Centerline::Centerline(): kdtree(0), nodes(0){

  index = new ANNidx[1];
  dist = new ANNdist[1];

  recombine = CTX::instance()->mesh.recombineAll;
  fileName = "centerlines.vtk";//default

  options["FileName"] = new FieldOptionString (fileName, "File name for the centerlines", &update_needed);
  callbacks["cutMesh"] = new cutAction(this, "Cut the initial mesh in different mesh partitions using the centerlines \n"); 
    //callbacks["test"] = new FieldCallbackGeneric<MathEvalField>(this, MathEvalField::myFunc, "description")
 
}

Centerline::~Centerline(){
  if (mod) delete mod;
  if (split) delete split;
  if(kdtree) delete kdtree;
  if(nodes) annDeallocPts(nodes);
  delete[]index;
  delete[]dist;
}

void Centerline::importFile(std::string fileName){

  current = GModel::current();
  std::vector<GEntity*> entities ;
  current->getEntities(entities) ; 
  for(unsigned int i = 0; i < entities.size(); i++){
    if(entities[i]->dim() != 2) continue; 
    recombine = std::max(recombine, (double)(((GFace*)entities[i])->meshAttributes.recombine));
    for(int j = 0; j < entities[i]->getNumMeshElements(); j++){ 
      MElement *e = entities[i]->getMeshElement(j);
      if (e->getType() != TYPE_TRI){
  	Msg::Error("Centerline split only implemented for tri meshes so far ..."); 
  	exit(1);
      }
      else{
	triangles.push_back((MTriangle*)e);
      }	
    }
  }
  entities.clear();

  mod = new GModel();
  mod->readVTK(fileName.c_str());
  mod->removeDuplicateMeshVertices(1.e-8);
  mod->getEntities(entities);  
  current->setAsCurrent();  

  int maxN = 0.0;
  for(unsigned int i = 0; i < entities.size(); i++){
     if( entities[i]->dim() == 1){
     for(unsigned int ele = 0; ele < entities[i]->getNumMeshElements(); ele++){ 
       MLine *l = (MLine*) entities[i]->getMeshElement(ele);
       MVertex *v0 = l->getVertex(0);
       MVertex *v1 = l->getVertex(1);
       std::map<MVertex*, int>::iterator it0 = colorp.find(v0);
       std::map<MVertex*, int>::iterator it1 = colorp.find(v1);
       if (it0 == colorp.end() || it1 == colorp.end()){
	 lines.push_back(l);
	 colorl.insert(std::make_pair(l, entities[i]->tag()));
	 maxN = std::max(maxN, entities[i]->tag());
       }
       if (it0 == colorp.end()) colorp.insert(std::make_pair(v0, entities[i]->tag()));
       if (it1 == colorp.end()) colorp.insert(std::make_pair(v1, entities[i]->tag()));
     }
   }
 }

  createBranches(maxN);

 
}


void Centerline::createBranches(int maxN){

  //sort colored lines and create edges
  std::vector<std::vector<MLine*> > color_edges;
  color_edges.resize(maxN);
  std::multiset<MVertex*> allV;
  std::map<MLine*, int>::iterator itl = colorl.begin();
  while (itl != colorl.end()){
    int color = itl->second;
    MLine* l = itl->first;
    allV.insert(l->getVertex(0));
    allV.insert(l->getVertex(1));
    color_edges[color-1].push_back(l);
    itl++;
  }

  //detect junctions
  std::multiset<MVertex*>::iterator it = allV.begin();
  for ( ; it != allV.end(); ++it){
    if (allV.count(*it) != 2) {
      junctions.insert(*it);
    }
  }
  // printf("junctions =%d \n", junctions.size());
  // std::set<MVertex*>::iterator itt = junctions.begin();
  // for ( ; itt != junctions.end(); ++itt){
  //   MVertex *v = *itt;
  //   printf("JUNC = %d \n", v->getNum());
  // }
   
  //split edges
  int tag = 0;
  for(unsigned int i = 0; i < color_edges.size(); ++i){
    std::vector<MLine*> lines = color_edges[i];
    //printf("EDGE %d line = %d \n", lines.size());
    while (!lines.empty()) {
      std::vector<MLine*> myLines;
      std::vector<MLine*>::iterator itl = lines.begin();
      MVertex *vB = (*itl)->getVertex(0);
      MVertex *vE = (*itl)->getVertex(1);
      //printf("VB =%d vE = %d \n", vB->getNum(), vE->getNum());
      myLines.push_back(*itl);
      erase(lines, *itl); 
      itl = lines.begin();
      while ( !( junctions.find(vE) != junctions.end() &&
		 junctions.find(vB) != junctions.end()) ) {
  	MVertex *v1 = (*itl)->getVertex(0);
  	MVertex *v2 = (*itl)->getVertex(1);
	//printf("line %d, VB = %d vE = %d v1=%d v2=%d \n", (*itl)->getNum(), vB->getNum(), vE->getNum(), v1->getNum(), v2->getNum());
	bool goVE = (junctions.find(vE) == junctions.end()) ? true : false ;
	bool goVB = (junctions.find(vB) == junctions.end()) ? true : false;
      	if (v1 == vE && goVE){
    	  myLines.push_back(*itl);
  	  erase(lines, *itl);
	  itl = lines.begin();
  	  vE = v2;
  	}
  	else if ( v2 == vE && goVE){
    	  myLines.push_back(*itl);
  	  erase(lines, *itl);
	  itl = lines.begin();
  	  vE = v1;
  	}
  	else if ( v1 == vB && goVB){
    	  myLines.push_back(*itl);
  	  erase(lines, *itl);
	  itl = lines.begin();
  	  vB = v2;
  	}
  	else if ( v2 == vB && goVB){
   	  myLines.push_back(*itl);
	  erase(lines, *itl);
	  itl = lines.begin();
  	  vB = v1;
  	}
	else itl++;
      }
      if (vB == vE) { Msg::Error("Begin and end points branch are the same \n");break;}
      orderMLines(myLines, vB, vE); 
      std::vector<Branch> children;
      Branch newBranch ={ tag++, myLines, computeLength(myLines), vB, vE, children, 1.e6, 0.0};
      //printf("branch = %d VB = %d VE %d \n", myLines.size(), vB->getNum(), vE->getNum());
      edges.push_back(newBranch) ;
    }
  }
  printf("in/outlets =%d branches =%d \n", (int)color_edges.size(), (int)edges.size());
  
  //create children
  for(unsigned int i = 0; i < edges.size(); ++i) {
    MVertex *vE = edges[i].vE;
    std::vector<Branch> myChildren;
    for (std::vector<Branch>::iterator it = edges.begin(); it != edges.end(); ++it){
      Branch myBranch = *it;
      if (myBranch.vB == vE) myChildren.push_back(myBranch);
    }
    edges[i].children = myChildren;
  }

  //compute radius
  distanceToLines();
  computeRadii();

  //print info
  // for(unsigned int i = 0; i < edges.size(); ++i) {
  //   printf("EDGE =%d  tag=%d length = %g childs = %d \n", i, edges[i].tag, edges[i].length, edges[i].children.size());
  //    for(unsigned int j = 0; j < edges[i].children.size(); ++j) {
  //      printf("children (%d) =%d \n", edges[i].children.size(),  edges[i].children[j].tag);
  //    }
  // }
  // std::set<MVertex*>::iterator itj = junctions.begin();
  // for ( ; itj != junctions.end(); ++itj){
  //   MVertex *v = *itj;
  //   printf("JUNC = %d \n", v->getNum());
  // }

}

void Centerline::distanceToLines(){
  Msg::Info("Centerline: computing distance to lines ");

  //SOLUTION 1: REVERSE ANN TREE (SURFACE POINTS IN TREE)
  ANNkd_tree *kdtreeR; 
  ANNpointArray nodesR;
  ANNidxArray indexR = new ANNidx[1];
  ANNdistArray distR = new ANNdist[1];

  std::set<MVertex*> allVS; 
  for(int j = 0; j < triangles.size(); j++)
    for(int k = 0; k<3; k++) allVS.insert(triangles[j]->getVertex(k));
  
  int nbSNodes = allVS.size();
  nodesR = annAllocPts(nbSNodes, 3);
  int ind = 0;
  std::set<MVertex*>::iterator itp = allVS.begin();
  while (itp != allVS.end()){
    MVertex *v = *itp;
    nodesR[ind][0] = v->x(); 
    nodesR[ind][1] = v->y(); 
    nodesR[ind][2] = v->z(); 
    itp++; ind++;
  }
  kdtreeR = new ANNkd_tree(nodesR, nbSNodes, 3);

  for(unsigned int i = 0; i < lines.size(); i++){
    MLine *l = lines[i];
    MVertex *v1 = l->getVertex(0);
    MVertex *v2 = l->getVertex(1);
    double midp[3] = {0.5*(v1->x()+v2->x()), 0.5*(v1->y()+v1->y()),0.5*(v1->z()+v2->z())};
    kdtreeR->annkSearch(midp, 1, indexR, distR);
    double minRad = sqrt(distR[0]);
    radiusl.insert(std::make_pair(lines[i], minRad)); 
  }
  delete kdtreeR;
  annDeallocPts(nodesR);
  delete[]indexR;
  delete[]distR;
  
  // //SOLUTION 2: DISTANCE TO LINES (QUITE SLOW)
  // std::vector<SPoint3> pts;
  // std::set<SPoint3> pts_set;
  // std::vector<double> distances;
  // std::vector<double> distancesE;
  // std::vector<SPoint3> closePts;

  // for(unsigned int i = 0; i < surfaces.size(); i++){ 
  //   for(int j = 0; j < surfaces[i]->getNumMeshElements(); j++){ 
  //     MElement *e = surfaces[i]->getMeshElement(j);
  //     for (int k = 0; k < e->getNumVertices(); k++){
  // 	MVertex *v = e->getVertex(k);
  // 	pts_set.insert(SPoint3(v->x(), v->y(),v->z()));
  //     }
  //   }
  // }
  // std::set<SPoint3>::iterator its =  pts_set.begin();
  // for (; its != pts_set.end(); its++){
  //   pts.push_back(*its);
  // } 
  // if (pts.size() == 0) {Msg::Error("Centerline needs a GModel with a surface \n"); exit(1);}
 
  // for(unsigned int i = 0; i < lines.size(); i++){
  //   std::vector<double> iDistances;
  //   std::vector<SPoint3> iClosePts;
  //   std::vector<double> iDistancesE;
  //   MLine *l = lines[i];
  //   MVertex *v1 = l->getVertex(0);
  //   MVertex *v2 = l->getVertex(1);
  //   SPoint3 p1(v1->x(), v1->y(), v1->z());
  //   SPoint3 p2(v2->x(), v2->y(), v2->z());
  //   signedDistancesPointsLine(iDistances, iClosePts, pts, p1,p2);

  //   double minRad = std::abs(iDistances[0]);
  //   for (unsigned int kk = 1; kk< pts.size(); kk++) {
  //     if (std::abs(iDistances[kk]) < minRad)
  // 	minRad = std::abs(iDistances[kk]);
  //   }
  //   radiusl.insert(std::make_pair(lines[i], minRad)); 
  // }
     
}
void Centerline::computeRadii(){

  for(unsigned int i = 0; i < edges.size(); ++i) {
    std::vector<MLine*> lines = edges[i].lines;
    for(unsigned int j = 0; j < lines.size(); ++j) {
      MLine *l = lines[j];
      std::map<MLine*,double>::iterator itr = radiusl.find(l);
      if (itr != radiusl.end()){
	edges[i].minRad = std::min(itr->second, edges[i].minRad);
	edges[i].maxRad = std::max(itr->second, edges[i].maxRad);
      }
      else printf("ARGG line not found \n");
    }    
  }

}
void Centerline::buildKdTree(){

  FILE * f = fopen("myPOINTS.pos","w");
  fprintf(f, "View \"\"{\n");

  int nbPL = 3;  //10 points per line
  int nbNodes  = (lines.size()+1) + (nbPL*lines.size());

  nodes = annAllocPts(nbNodes, 3);
  int ind = 0;
  std::map<MVertex*, int>::iterator itp = colorp.begin();
  while (itp != colorp.end()){
     MVertex *v = itp->first;
     nodes[ind][0] = v->x(); 
     nodes[ind][1] = v->y(); 
     nodes[ind][2] = v->z(); 
     itp++; ind++;
  }
  for(unsigned int k = 0; k < lines.size(); ++k){ 
   MVertex *v0 = lines[k]->getVertex(0);
   MVertex *v1 = lines[k]->getVertex(1);
   SVector3 P0(v0->x(),v0->y(), v0->z());
   SVector3 P1(v1->x(),v1->y(), v1->z());
   for (int j= 1; j < nbPL+1; j++){
     double inc = (double)j/(double)(nbPL+1);
     SVector3 Pj = P0+inc*(P1-P0);
     nodes[ind][0] = Pj.x(); 
     nodes[ind][1] = Pj.y();  
     nodes[ind][2] = Pj.z();  
     ind++;
   }
 }

 kdtree = new ANNkd_tree(nodes, nbNodes, 3);

 for(unsigned int i = 0; i < nbNodes; ++i){
   fprintf(f, "SP(%g,%g,%g){%g};\n",
	   nodes[i][0], nodes[i][1],nodes[i][2],1.0);
 }
 fprintf(f,"};\n");
  fclose(f);

}

void Centerline::remeshSplitMesh(){

  // Remesh new faces (Compound Lines and Compound Surfaces)
  Msg::Info("*** Starting parametrize compounds:");
  double t0 = Cpu();

  //Parametrize Compound Lines
  int NE = split->getMaxElementaryNumber(1);
  for (int i=0; i < NE; i++){
    std::vector<GEdge*>e_compound;
    GEdge *pe = split->getEdgeByTag(i+1);//split edge
    e_compound.push_back(pe);
    int num_gec = NE+i+1;
    Msg::Info("Parametrize Compound Line (%d) = %d discrete edge", 
              num_gec, pe->tag());
    GEdgeCompound *gec = new GEdgeCompound(split, num_gec, e_compound);
    split->add(gec);
    gec->parametrize();
  }

  // Parametrize Compound surfaces
  std::set<MVertex*> allNod; 
  std::list<GEdge*> U0;
  int NF = split->getMaxElementaryNumber(2);
  for (int i=0; i < NF; i++){
    std::list<GFace*> f_compound;
    GFace *pf =  split->getFaceByTag(i+1);//split face 
    f_compound.push_back(pf);  
    int num_gfc = NF+i+1;   
    Msg::Info("Parametrize Compound Surface (%d) = %d discrete face",
              num_gfc, pf->tag());
    GFaceCompound::typeOfMapping typ = GFaceCompound::CONFORMAL;
    GFaceCompound *gfc = new GFaceCompound(split, num_gfc, f_compound, U0,
					   typ, 0);
    gfc->meshAttributes.recombine = recombine;
    split->add(gfc);
    gfc->parametrize();
  }
  double t1 = Cpu();
  Msg::Info("*** Parametrize compounds done (%g s)", t1-t0);
 
  //set centerline field
  FieldManager *fields = split->getFields();
  fields->reset();
  int id = fields->newId();
  (*fields)[id] = this;
  fields->background_field = id;

  Msg::Info("*** Starting meshing 1D edges ...:");
  for (int i = 0; i < NE; i++){
    GEdge *gec = split->getEdgeByTag(NE+i+1);
    meshGEdge mge;
    mge(gec);
  }
  double t2 = Cpu();
  Msg::Info("*** Meshing 1D edges done (%gs)", t2-t1);

  Msg::Info("*** Starting mesh of surface ");
  for (int i=0; i < NF; i++){
    GFace *gfc =  split->getFaceByTag(NF+i+1);
    meshGFace mgf;
    mgf(gfc);
  }

  // // Removing discrete Vertices - Edges - Faces
  // int NV = split->getMaxElementaryNumber(0) - numv + 1;
  // for (int i=0; i < NV; i++){
  //   GVertex *pv = gf->model()->getVertexByTag(numv+i);
  //   gf->model()->remove(pv);
  // }
  // for (int i=0; i < NE; i++){
  //   GEdge *gec = split->getEdgeByTag(nume+NE+i);
  //   GEdge *pe = split->getEdgeByTag(nume+i);
  //   gf->model()->remove(pe); 
  //   gf->model()->remove(gec);
  // }
  // for (int i=0; i < NF; i++){
  //   GFace *gfc = split->getFaceByTag(numf+NF+i);
  //   GFace *pf  = split->getFaceByTag(numf+i);
  //   gf->model()->remove(pf); 
  //   gf->model()->remove(gfc);
  // }

  // // Put new mesh in a new discreteFace
  // for(std::set<MVertex*>::iterator it = allNod.begin(); it != allNod.end(); ++it){
  //   gf->mesh_vertices.push_back(*it);
  // }

  // // Remove mesh_vertices that belong to l_edges
  // std::list<GEdge*> l_edges = gf->edges();
  // for(std::list<GEdge*>::iterator it = l_edges.begin(); it != l_edges.end(); it++){
  //   std::vector<MVertex*> edge_vertices = (*it)->mesh_vertices;
  //   std::vector<MVertex*>::const_iterator itv = edge_vertices.begin();
  //   for(; itv != edge_vertices.end(); itv++){
  //     std::vector<MVertex*>::iterator itve = std::find
  //       (gf->mesh_vertices.begin(), gf->mesh_vertices.end(), *itv);
  //     if (itve != gf->mesh_vertices.end()) gf->mesh_vertices.erase(itve);
  //   }
  //   MVertex *vB = (*it)->getBeginVertex()->mesh_vertices[0];
  //   std::vector<MVertex*>::iterator itvB = std::find
  //     (gf->mesh_vertices.begin(), gf->mesh_vertices.end(), vB);
  //   if (itvB != gf->mesh_vertices.end()) gf->mesh_vertices.erase(itvB);
  //   MVertex *vE = (*it)->getEndVertex()->mesh_vertices[0];
  //   std::vector<MVertex*>::iterator itvE = std::find
  //     (gf->mesh_vertices.begin(), gf->mesh_vertices.end(), vE);
  //   if (itvE != gf->mesh_vertices.end()) gf->mesh_vertices.erase(itvE);

  //   //if l_edge is a compond
  //   if((*it)->getCompound()){
  //     GEdgeCompound *gec = (*it)->getCompound();
  //     std::vector<MVertex*> edge_vertices = gec->mesh_vertices;
  //     std::vector<MVertex*>::const_iterator itv = edge_vertices.begin();
  //     for(; itv != edge_vertices.end(); itv++){
  //       std::vector<MVertex*>::iterator itve = std::find
  //         (gf->mesh_vertices.begin(), gf->mesh_vertices.end(), *itv);
  //       if (itve != gf->mesh_vertices.end()) gf->mesh_vertices.erase(itve);
  //     }
  //     MVertex *vB = (*it)->getBeginVertex()->mesh_vertices[0];
  //     std::vector<MVertex*>::iterator itvB = std::find
  //       (gf->mesh_vertices.begin(), gf->mesh_vertices.end(), vB);
  //     if (itvB != gf->mesh_vertices.end()) gf->mesh_vertices.erase(itvB);
  //     MVertex *vE = (*it)->getEndVertex()->mesh_vertices[0];
  //     std::vector<MVertex*>::iterator itvE = std::find
  //       (gf->mesh_vertices.begin(), gf->mesh_vertices.end(), vE);
  //     if (itvE != gf->mesh_vertices.end()) gf->mesh_vertices.erase(itvE);
  //   }
  // }

  // double t3 = Cpu();
  // Msg::Info("*** Mesh of surface %d done by assembly %d remeshed faces (%g s)",
  //           gf->tag(), NF, t3-t2);
  // Msg::Info("-----------------------------------------------------------");
 
  // gf->coherenceNormals();
  // gf->meshStatistics.status = GFace::DONE; 

}
void Centerline::createFaces(){
 
  std::vector<std::vector<MTriangle*> > faces;

  std::multimap<MEdge, MTriangle*, Less_Edge> e2e;
  for(unsigned int i = 0; i < triangles.size(); ++i){
    for(int j = 0; j < 3; j++){
      e2e.insert(std::make_pair(triangles[i]->getEdge(j), triangles[i]));
    }
  }
  int iGroup = 0;
  while(!e2e.empty()){
    std::set<MTriangle*> group;
    std::set<MEdge, Less_Edge> touched;
    group.clear();
    touched.clear();
    std::multimap<MEdge, MTriangle*, Less_Edge>::iterator ite = e2e.begin();
    MEdge me = ite->first;
    while (theCut.find(me) != theCut.end()){
      ite++;
      me = ite->first;
    }
    recurConnectByMEdge(me,e2e, group, touched, theCut);
    std::vector<MTriangle*> temp;
    temp.insert(temp.begin(), group.begin(), group.end());
    faces.push_back(temp);
    for(std::set<MEdge, Less_Edge>::iterator it = touched.begin();
        it != touched.end(); ++it)
      e2e.erase(*it);
  }
  printf("%d faces created \n", faces.size());

  //create discFaces
  int numBef = split->getMaxElementaryNumber(2) + 1;
  for(unsigned int i = 0; i < faces.size(); ++i){
    int numF = split->getMaxElementaryNumber(2) + 1;
    printf("creating discrete face %d \n", numF);
    discreteFace *f = new discreteFace(current, numF);
    split->add(f);
    discFaces.push_back(f);
    std::set<MVertex*> myVertices;
    std::vector<MTriangle*> myFace = faces[i];
    for(int j= 0; j< myFace.size(); j++){
      MTriangle *t = myFace[j];
      f->triangles.push_back(t);
      for (int k= 0; k< 3; k++){
	MVertex *v = t->getVertex(k);
	myVertices.insert(v);
      }
    }
    f->mesh_vertices.insert(f->mesh_vertices.begin(),
  			    myVertices.begin(), myVertices.end());
  }

}

void Centerline::cutMesh(){
 
  if (update_needed){
    std::ifstream input;
    input.open(fileName.c_str());
    if(StatFile(fileName)) Msg::Fatal("Centerline file '%s' does not exist", fileName.c_str());
    importFile(fileName);
    buildKdTree();
    printf("fileName =%s , recombine =%g \n", fileName.c_str(), recombine);
    update_needed = false;
  }
  
  Msg::Info("Splitting surface mesh (%d tris) with centerline %s ", triangles.size(), fileName.c_str());
  split = new GModel(); 

  //splitMesh
  for(unsigned int i = 0; i < edges.size(); i++){
    std::vector<MLine*> lines = edges[i].lines;
    double L = edges[i].length;
    double R = edges[i].maxRad;
    double AR = L/R;
    printf("*** branch =%d \n", i, AR);
    if( AR > 8.){
      int nbSplit = (int)ceil(AR/3.5);
      double li  = L/nbSplit;
      double lc = 0.0;
      for (int j= 0; j < lines.size(); j++){
	lc += lines[j]->getInnerRadius();
	if (lc > li && nbSplit > 1) {
	  MVertex *v1 = lines[j]->getVertex(0);
	  MVertex *v2 = lines[j]->getVertex(1);
	  SVector3 pt(v1->x(), v1->y(), v1->z());
	  SVector3 dir(v2->x()-v1->x(),v2->y()-v1->y(),v2->z()-v1->z());
	  printf("-> cut length (AR=%g) split %d\n", AR, nbSplit);
	  cutByDisk(pt, dir, edges[i].maxRad);
	  nbSplit--;
	  lc = 0.0;
	}
      }
    }
    if(edges[i].children.size() > 0.0 && AR > 1.5){
      MVertex *v1 = lines[lines.size()-1]->getVertex(1);//end vertex
      MVertex *v2;
      if(L/R < 2.0) v2 = lines[0]->getVertex(0);
      else if (lines.size() > 4) v2 = lines[lines.size()-4]->getVertex(0);
      else v2 = lines[lines.size()-1]->getVertex(0);
      SVector3 pt(v1->x(), v1->y(), v1->z());
      SVector3 dir(v2->x()-v1->x(),v2->y()-v1->y(),v2->z()-v1->z());
      printf("-->> cut bifurcation \n");
      cutByDisk(pt, dir, edges[i].maxRad);
    }
 }

  //create discreteFaces
  createFaces();
  split->createTopologyFromFaces(discFaces);

  //write
  Msg::Info("Writing splitted mesh 'myPARTS.msh'");
  split->writeMSH("myPARTS.msh", 2.2, false, true);

  //remesh splitted mesh
  remeshSplitMesh();
  Msg::Info("Writing new mesh 'myMESH.msh'");
  split->writeMSH("myMESH.msh", 2.2, false, true);

  //print 
  // FILE * f2 = fopen("myCUTLINES.pos","w");
  // fprintf(f2, "View \"\"{\n");
  // std::set<MEdge,Less_Edge>::iterator itp =  theCut.begin();
  //  while (itp != theCut.end()){
  //    MEdge l = *itp;
  //    fprintf(f2, "SL(%g,%g,%g,%g,%g,%g){%g,%g};\n",
  //             l.getVertex(0)->x(), l.getVertex(0)->y(), l.getVertex(0)->z(),
  //             l.getVertex(1)->x(), l.getVertex(1)->y(), l.getVertex(1)->z(),
  //             1.0,1.0);
  //    itp++; 
  // }
  // fprintf(f2,"};\n");
  // fclose(f2);


  Msg::Info("Splitting mesh by centerlines done ");
  exit(1);

}



void Centerline::cutByDisk(SVector3 &PT, SVector3 &NORM, double &maxRad){
   
  double a = NORM.x();
  double b = NORM.y();
  double c = NORM.z();
  double d = -a * PT.x() - b * PT.y() - c * PT.z();
  printf("cut disk (R=%g)= %g %g %g %g \n", maxRad, a, b, c, d);
 
  const double EPS = 0.007;
  std::set<MEdge,Less_Edge> allEdges;
  for(unsigned int i = 0; i < triangles.size(); i++){ 
    for ( unsigned int j= 0; j <  3; j++)
      allEdges.insert(triangles[i]->getEdge(j)); 
  }
  bool closedCut = false;
  int step = 0;
  while (!closedCut && step < 20){
    double rad = 1.3*maxRad+0.15*step*maxRad;
    std::map<MEdge,MVertex*,Less_Edge> cutEdges;
    std::set<MVertex*> cutVertices;
    std::vector<MTriangle*> newTris; 
    std::set<MEdge,Less_Edge> newCut;
    cutEdges.clear();
    cutVertices.clear();
    newTris.clear();
    newCut.clear();
    for (std::set<MEdge,Less_Edge>::iterator it = allEdges.begin();
	 it != allEdges.end() ; ++it){
      MEdge me = *it;
      SVector3 P1(me.getVertex(0)->x(),me.getVertex(0)->y(), me.getVertex(0)->z());
      SVector3 P2(me.getVertex(1)->x(),me.getVertex(1)->y(), me.getVertex(1)->z());
      double V1 = a * P1.x() + b * P1.y() + c * P1.z() + d;
      double V2 = a * P2.x() + b * P2.y() + c * P2.z() + d;
      bool inters = (V1*V2<=0.0) ? true: false;
      bool inDisk = ((norm(P1-PT) < rad ) || (norm(P2-PT) < rad)) ? true : false; 
      double rdist = -V1/(V2-V1);
      if (inters && rdist > EPS && rdist < 1.-EPS){
	SVector3 PZ = P1+rdist*(P2-P1);
	MVertex *newv = new MVertex (PZ.x(), PZ.y(), PZ.z());
	if (inDisk) cutEdges.insert(std::make_pair(*it,newv));
      }
      else if (inters && rdist <= EPS && inDisk )
	cutVertices.insert(me.getVertex(0));
      else if (inters && rdist >= 1.-EPS && inDisk)
	cutVertices.insert(me.getVertex(1));
    }
    for(unsigned int i = 0; i < triangles.size(); i++){ 
      cutTriangle(triangles[i], cutEdges,cutVertices, newTris, newCut);
    }
    if (isClosed(newCut)) {
      triangles.clear();
      triangles = newTris;
      theCut.insert(newCut.begin(),newCut.end());
      //if (step > 1) printf("YOUPIIIIIIIIIIIIIIIIIIIIIII \n");
      break;
    }
    else {
      if (step ==19) {printf("no closed cut %d \n", (int)newCut.size()); };
      step++;
      // // triangles = newTris;
      // // theCut.insert(newCut.begin(),newCut.end());
      // char name[256];
      // sprintf(name, "myCUT-%d.pos", step);
      // FILE * f2 = fopen(name,"w");
      // fprintf(f2, "View \"\"{\n");
      // std::set<MEdge,Less_Edge>::iterator itp =  newCut.begin();
      // while (itp != newCut.end()){
      // 	MEdge l = *itp;
      // 	fprintf(f2, "SL(%g,%g,%g,%g,%g,%g){%g,%g};\n",
      // 		  l.getVertex(0)->x(), l.getVertex(0)->y(), l.getVertex(0)->z(),
      // 		  l.getVertex(1)->x(), l.getVertex(1)->y(), l.getVertex(1)->z(),
      // 		  1.0,1.0);
      // 	  itp++; 
      // 	}
      // 	fprintf(f2,"};\n");
      // 	fclose(f2);
    }
  }

  return;

}

double Centerline::operator() (double x, double y, double z, GEntity *ge){

  if (update_needed){
    std::ifstream input;
    input.open(fileName.c_str());
    if(StatFile(fileName)) Msg::Fatal("Centerline file '%s' does not exist", fileName.c_str());
    importFile(fileName);
    buildKdTree();
    printf("fileName =%s , recombine =%g \n", fileName.c_str(),  recombine);
    exit(1);
    update_needed = false;
  }

  double xyz[3] = {x,y,z };
  int num_neighbours = 1;
  kdtree->annkSearch(xyz, num_neighbours, index, dist);
  double d = sqrt(dist[0]);
  double lc = 2*M_PI*d/30.0; //30 mesh elements along circle
  return lc;

}

void  Centerline::operator() (double x, double y, double z, SMetric3 &metr, GEntity *ge){
  Msg::Error("This anisotropic operator of CenterlineField is not implemnted yet ");
  return;
}

void Centerline::printSplit() const{

  FILE * f = fopen("mySPLIT.pos","w");
  fprintf(f, "View \"\"{\n");
  for(unsigned int i = 0; i < edges.size(); ++i){
    std::vector<MLine*> lines = edges[i].lines;
    for(unsigned int k = 0; k < lines.size(); ++k){
      MLine *l = lines[k];
      fprintf(f, "SL(%g,%g,%g,%g,%g,%g){%g,%g};\n",
	      l->getVertex(0)->x(), l->getVertex(0)->y(), l->getVertex(0)->z(),
	      l->getVertex(1)->x(), l->getVertex(1)->y(), l->getVertex(1)->z(),
	      (double)i, (double)i);
    }
  }
  fprintf(f,"};\n");
  fclose(f);

  // FILE * f3 = fopen("myJUNCTIONS.pos","w");
  // fprintf(f3, "View \"\"{\n");
  //  std::set<MVertex*>::const_iterator itj = junctions.begin();
  //  while (itj != junctions.end()){
  //    MVertex *v =  *itj;
  //    fprintf(f3, "SP(%g,%g,%g){%g};\n",
  // 	     v->x(),  v->y(), v->z(),
  // 	     (double)v->getNum());
  //    itj++; 
  // }
  // fprintf(f3,"};\n");
  // fclose(f3);

  FILE * f4 = fopen("myRADII.pos","w");
  fprintf(f4, "View \"\"{\n");
  for(unsigned int i = 0; i < lines.size(); ++i){
    MLine *l = lines[i];
    std::map<MLine*,double>::const_iterator itc =  radiusl.find(l);
    fprintf(f, "SL(%g,%g,%g,%g,%g,%g){%g,%g};\n",
 	    l->getVertex(0)->x(), l->getVertex(0)->y(), l->getVertex(0)->z(),
 	    l->getVertex(1)->x(), l->getVertex(1)->y(), l->getVertex(1)->z(),
 	    itc->second,itc->second);
  }
  fprintf(f4,"};\n");
  fclose(f4);
 
}

#endif
