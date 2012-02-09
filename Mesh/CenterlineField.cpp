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
#include <iostream>
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
#include "MQuadrangle.h"
#include "MElement.h"

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
  MVertex *old_v0  = tri->getVertex(0);
  MVertex *old_v1  = tri->getVertex(1);
  MVertex *old_v2  = tri->getVertex(2);
  if (c[0] && c[1]){
    newTris.push_back(new MTriangle (c[0],old_v1,c[1]));
    newTris.push_back(new MTriangle (old_v0,c[0],old_v2));
    newTris.push_back(new MTriangle (old_v2,c[0],c[1]));
    newCut.insert(MEdge(c[0],c[1]));
  }
  else if (c[0] && c[2]){
    newTris.push_back(new MTriangle (old_v0,c[0],c[2]));
    newTris.push_back(new MTriangle (c[0],old_v1,old_v2));
    newTris.push_back(new MTriangle (old_v2,c[2],c[0]));
    newCut.insert(MEdge(c[0],c[2]));
  }
  else if (c[1] && c[2]){
    newTris.push_back(new MTriangle (old_v2,c[2],c[1]));
    newTris.push_back(new MTriangle (old_v0,old_v1,c[2]));
    newTris.push_back(new MTriangle (c[2],old_v1,c[1]));
    newCut.insert(MEdge(c[1],c[2]));
  }
  else if (c[0]){
    newTris.push_back(new MTriangle (old_v0,c[0],old_v2));
    newTris.push_back(new MTriangle (old_v2,c[0],old_v1));
    if (cutVertices.find (old_v0) != cutVertices.end()){
      newCut.insert(MEdge(c[0],old_v0));
    }
    else if (cutVertices.find (old_v1) != cutVertices.end()) {
      newCut.insert(MEdge(c[0],old_v1));
    }
    else if (cutVertices.find (old_v2) != cutVertices.end()){
      newCut.insert(MEdge(c[0],old_v2));
    }
  }
  else if (c[1]){
    newTris.push_back(new MTriangle (old_v1,c[1],old_v0));
    newTris.push_back(new MTriangle (old_v0,c[1],old_v2));
    if (cutVertices.find (old_v0) != cutVertices.end()){
      newCut.insert(MEdge(c[1],old_v0));
    }
    else if (cutVertices.find (old_v1) != cutVertices.end()) {
      newCut.insert(MEdge(old_v1, c[1]));
    }
    else if (cutVertices.find (old_v2) != cutVertices.end()){
      newCut.insert(MEdge(c[1],old_v2));
    }
  }
  else if (c[2]){
      newTris.push_back(new MTriangle (old_v0,old_v1, c[2]));
      newTris.push_back(new MTriangle (old_v1,old_v2, c[2]));
    if (cutVertices.find (old_v0) != cutVertices.end()){
      newCut.insert(MEdge(c[2],old_v0));
    }
    else if (cutVertices.find (old_v1) != cutVertices.end()) {
      newCut.insert(MEdge(c[2], old_v1));
    }
    else if (cutVertices.find (old_v2) != cutVertices.end()){
      newCut.insert(MEdge(c[2], old_v2));
    }
  }
  else {
    newTris.push_back(tri);
    //newTris.push_back(new MTriangle (old_v0, old_v1,old_v2));
    if (cutVertices.find (old_v0) != cutVertices.end() &&
	cutVertices.find (old_v1) != cutVertices.end())
      newCut.insert(MEdge(old_v0,old_v1));
    else if (cutVertices.find (old_v1) != cutVertices.end() &&
	cutVertices.find (old_v2) != cutVertices.end())
      newCut.insert(MEdge(old_v1,old_v2));
    else if (cutVertices.find (old_v2) != cutVertices.end() &&
	cutVertices.find (old_v0) != cutVertices.end())
      newCut.insert(MEdge(old_v2,old_v0));
  }
}

Centerline::Centerline(std::string fileName): kdtree(0), nodes(0){
  
  recombine = CTX::instance()->mesh.recombineAll;

  index = new ANNidx[1];
  dist = new ANNdist[1];

  printf("centerline filename =%s \n", fileName.c_str());
  importFile(fileName);
  buildKdTree();
  nbPoints = 25;

  update_needed = false;
  is_cut = false;

}
Centerline::Centerline(): kdtree(0), nodes(0){

  index = new ANNidx[1];
  dist = new ANNdist[1];

  recombine = CTX::instance()->mesh.recombineAll;
  fileName = "centerlines.vtk";//default
  nbPoints = 25;

  options["FileName"] = new FieldOptionString (fileName, "File name for the centerlines", &update_needed);
  options["nbPoints"] = new FieldOptionInt(nbPoints, "Number of mesh elements in a circle");
  callbacks["cutMesh"] = new FieldCallbackGeneric<Centerline>(this, &Centerline::cutMesh, "Cut the initial mesh in different mesh partitions using the centerlines \n");
  is_cut = false;

}

Centerline::~Centerline(){
  if (mod) delete mod;
  if(kdtree) delete kdtree;
  if(nodes) annDeallocPts(nodes);
  delete[]index;
  delete[]dist;
}

void Centerline::importFile(std::string fileName){

  current = GModel::current();
  std::vector<GFace*> currentFaces = current->bindingsGetFaces();
  for (int i = 0; i < currentFaces.size(); i++){
    GFace *gf = currentFaces[i];
    if (gf->geomType() == GEntity::DiscreteSurface){
	for(unsigned int j = 0; j < gf->triangles.size(); j++)
	  triangles.push_back(gf->triangles[j]);
	gf->triangles.clear();
	gf->deleteVertexArrays();
	current->remove(gf);
    }
  }

  if(triangles.empty()){
    Msg::Error("Current GModel has no triangles ..."); 
    exit(1);
  }
    
  mod = new GModel();
  mod->load(fileName);
  mod->removeDuplicateMeshVertices(1.e-8);
  current->setAsCurrent();  

  int maxN = 0.0;
  std::vector<GEdge*> modEdges = mod->bindingsGetEdges();
 for (int i = 0; i < modEdges.size(); i++){
    GEdge *ge = modEdges[i];
    for(unsigned int j = 0; j < ge->lines.size(); j++){
      MLine *l = ge->lines[j];
      MVertex *v0 = l->getVertex(0);
      MVertex *v1 = l->getVertex(1);
      std::map<MVertex*, int>::iterator it0 = colorp.find(v0);
      std::map<MVertex*, int>::iterator it1 = colorp.find(v1);
      if (it0 == colorp.end() || it1 == colorp.end()){
	lines.push_back(l);
	colorl.insert(std::make_pair(l, ge->tag()));
	maxN = std::max(maxN, ge->tag());
       }
      if (it0 == colorp.end()) colorp.insert(std::make_pair(v0, ge->tag()));
      if (it1 == colorp.end()) colorp.insert(std::make_pair(v1, ge->tag()));
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

  printSplit();

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
  //int nbNodes  = (lines.size()+1) + (nbPL*lines.size());
  int nbNodes  = (colorp.size()) + (nbPL*lines.size());

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

void Centerline::createSplitCompounds(){

  NV = current->getMaxElementaryNumber(0);
  NE = current->getMaxElementaryNumber(1);
  NF = current->getMaxElementaryNumber(2);

  // Remesh new faces (Compound Lines and Compound Surfaces)
  Msg::Info("*** Starting parametrize compounds:");
  double t0 = Cpu();

  //Parametrize Compound Lines
  for (int i=0; i < NE; i++){
    std::vector<GEdge*>e_compound;
    GEdge *pe = current->getEdgeByTag(i+1);//current edge
    e_compound.push_back(pe);
    int num_gec = NE+i+1;
    Msg::Info("Parametrize Compound Line (%d) = %d discrete edge", 
              num_gec, pe->tag());
    GEdgeCompound *gec = new GEdgeCompound(current, num_gec, e_compound);
    current->add(gec);
    //gec->parametrize();
  }
 
  // Parametrize Compound surfaces
  std::list<GEdge*> U0;
  for (int i=0; i < NF; i++){
    std::list<GFace*> f_compound;
    GFace *pf =  current->getFaceByTag(i+1);//current face 
    f_compound.push_back(pf);  
    int num_gfc = NF+i+1;   
    Msg::Info("Parametrize Compound Surface (%d) = %d discrete face",
              num_gfc, pf->tag());
    GFaceCompound::typeOfMapping typ = GFaceCompound::CONFORMAL;
    GFaceCompound *gfc = new GFaceCompound(current, num_gfc, f_compound, U0,
					   typ, 0);
    gfc->meshAttributes.recombine = recombine;
    gfc->addPhysicalEntity(i+1);
    current->add(gfc);
    //gfc->parametrize();
    // for(unsigned int j = 0; j < pf->triangles.size(); ++j){
    //   MTriangle *t = pf->triangles[j];
    //   for(int k = 0; k < 3; k++){
    // 	MVertex *v = t->getVertex(k);
    // 	v->setEntity(pf);
    //   }
    //}
  }

}

void Centerline::cleanMesh(){

  for (int i=0; i < NF; i++){
    std::ostringstream oss;
    oss << "new" << NF+i+1 ;
    std::string name = oss.str();
    current->setPhysicalName(name, 2, i+1);
  }
  Msg::Info("Writing new splitted mesh mySPLITMESH.msh");
  current->writeMSH("mySPLITMESH.msh", 2.2, false, false);

  if (!is_cut) return;

  std::set<MVertex*> allNod; 
  std::list<GEdge*> U0;
  discreteFace * mySplitMesh; 

  mySplitMesh= new discreteFace(current, 2*NF+1);
  current->add(mySplitMesh);
  for (int i=0; i < NF; i++){
    GFace *gfc =  current->getFaceByTag(NF+i+1);
    for(unsigned int j = 0; j < gfc->triangles.size(); ++j){
      MTriangle *t = gfc->triangles[j];
      std::vector<MVertex *> v(3);
      for(int k = 0; k < 3; k++){
	v[k] = t->getVertex(k);
	allNod.insert(v[k]);
      }
      mySplitMesh->triangles.push_back(new MTriangle(v[0], v[1], v[2]));
    }
    for(unsigned int j = 0; j < gfc->quadrangles.size(); ++j){
      MQuadrangle *q = gfc->quadrangles[j];
      std::vector<MVertex *> v(4);
      for(int k = 0; k < 4; k++){
	v[k] = q->getVertex(k);
	allNod.insert(v[k]);
      }
      mySplitMesh->quadrangles.push_back(new MQuadrangle(v[0], v[1], v[2], v[3]));
    }
  }
  
  //Removing discrete Vertices - Edges - Faces
  for (int i=0; i < NV; i++){
    GVertex *gv = current->getVertexByTag(i+1);
    current->remove(gv);
  }
  for (int i=0; i < NE; i++){
    GEdge *ge = current->getEdgeByTag(i+1);
    GEdge *gec = current->getEdgeByTag(NE+i+1);
    current->remove(ge); 
    current->remove(gec);
  }
  for (int i=0; i < NF; i++){
    GFace *gf  = current->getFaceByTag(i+1);
    GFace *gfc = current->getFaceByTag(NF+i+1);
    current->remove(gf); 
    current->remove(gfc);
  }
  
  //Put new mesh in a new discreteFace
  for(std::set<MVertex*>::iterator it = allNod.begin(); it != allNod.end(); ++it)
    mySplitMesh->mesh_vertices.push_back(*it);
  mySplitMesh->meshStatistics.status = GFace::DONE; 
  current->createTopologyFromMesh();
  
}
void Centerline::createFaces(){
 
  std::vector<std::vector<MTriangle*> > faces;

  std::multimap<MEdge, MTriangle*, Less_Edge> e2e;
  for(unsigned int i = 0; i < triangles.size(); ++i)
    for(int j = 0; j < 3; j++)
      e2e.insert(std::make_pair(triangles[i]->getEdge(j), triangles[i]));
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
  printf("%d faces created \n", (int)faces.size());

  //create discFaces
  int numBef = current->getMaxElementaryNumber(2) + 1;
  for(unsigned int i = 0; i < faces.size(); ++i){
    int numF = current->getMaxElementaryNumber(2) + 1;
    discreteFace *f = new discreteFace(current, numF);
    current->add(f);
    discFaces.push_back(f);
    std::set<MVertex*> myVertices;
    std::vector<MTriangle*> myFace = faces[i];
    for(int j= 0; j< myFace.size(); j++){
      MTriangle *t = myFace[j];
      f->triangles.push_back(t);
      for (int k= 0; k< 3; k++){
	MVertex *v = t->getVertex(k);
	myVertices.insert(v);
	v->setEntity(f);
      }
    }
    f->mesh_vertices.insert(f->mesh_vertices.begin(),
  			    myVertices.begin(), myVertices.end());
  }

}

void Centerline::cutMesh(){
  
  is_cut = true;

  if (update_needed){
    std::ifstream input;
    input.open(fileName.c_str());
    if(StatFile(fileName)) Msg::Fatal("Centerline file '%s' does not exist", fileName.c_str());
    importFile(fileName);
    buildKdTree();
    update_needed = false;
  }

  // std::vector<GFace*> currentFaces =  current->bindingsGetFaces();
  // for (int i=0; i< currentFaces.size(); i++){
  //   printf("gf =%d \n", currentFaces[i]->tag());
  //   if(currentFaces[i]->geomType() == GEntity::CompoundSurface) {
  //     std::list<GFace*> cFaces = ((GFaceCompound*)currentFaces[i])->getCompounds();
  //     for (std::list<GFace*>::iterator it = cFaces.begin(); it!= cFaces.end(); it++) {
  //   	printf("comp  = %d\n", (*it)->tag());
  //     }
  //     if (cFaces.size() > 0) currentGFC.push_back(currentFaces[i]);
  //   }
  // }

  Msg::Info("Splitting surface mesh (%d tris) with centerline %s ", triangles.size(), fileName.c_str());
 
  //splitMesh
  for(unsigned int i = 0; i < edges.size(); i++){
    std::vector<MLine*> lines = edges[i].lines;
    double L = edges[i].length;
    double R = edges[i].minRad;
    double AR = L/R;
    printf("*** branch =%d \n", i, AR);
    if( AR > 4.5){
      int nbSplit = (int)ceil(AR/4.0);
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
  current->createTopologyFromFaces(discFaces);

  //write
  Msg::Info("Writing splitted mesh 'myPARTS.msh'");
  current->writeMSH("myPARTS.msh", 2.2, false, true);

  //create compounds
  createSplitCompounds();

  Msg::Info("Splitting mesh by centerlines done ");

}

void Centerline::cutByDisk(SVector3 &PT, SVector3 &NORM, double &maxRad){
   
  double a = NORM.x();
  double b = NORM.y();
  double c = NORM.z();
  double d = -a * PT.x() - b * PT.y() - c * PT.z();
  printf("cut disk (R=%g)= %g %g %g %g \n", maxRad, a, b, c, d);
 
  const double EPS = 0.007;
  std::set<MEdge,Less_Edge> allEdges;
  for(unsigned int i = 0; i < triangles.size(); i++) 
    for ( unsigned int j= 0; j <  3; j++)
      allEdges.insert(triangles[i]->getEdge(j)); 
  bool closedCut = false;
  int step = 0;
  while (!closedCut && step < 10){
    double rad = 1.2*maxRad+0.1*step*maxRad;
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
     update_needed = false;
   }

   double xyz[3] = {x,y,z };
   int num_neighbours = 1;
   kdtree->annkSearch(xyz, num_neighbours, index, dist);
   double d = sqrt(dist[0]);
   double lc = 2*M_PI*d/nbPoints; 
   return lc;

}

void  Centerline::operator() (double x, double y, double z, SMetric3 &metr, GEntity *ge){

  //printf("in operator metric  xyz centerline \n");
   if (update_needed){
     std::ifstream input;
     input.open(fileName.c_str());
     if(StatFile(fileName)) Msg::Fatal("Centerline file '%s' does not exist", fileName.c_str());
     importFile(fileName);
     buildKdTree();
     update_needed = false;
   }
   
   //double lc = operator()(x,y,z,ge);
   //metr = SMetric3(1./(lc*lc));

   double xyz[3] = {x,y,z };
   ANNidxArray index2 = new ANNidx[2];
   ANNdistArray dist2 = new ANNdist[2];
   int num_neighbours = 2;
   kdtree->annkSearch(xyz, num_neighbours, index2, dist2);
   double d = sqrt(dist2[0]);
   double lc = 2*M_PI*d/nbPoints; 
   SVector3  p0(nodes[index2[0]][0], nodes[index2[0]][1], nodes[index2[0]][2]);
   SVector3  p1(nodes[index2[1]][0], nodes[index2[1]][1], nodes[index2[1]][2]);
   SVector3 dir = p1-p0;
   dir.normalize();
   SVector3 dir1, dir2;
   if (dir[1]!=0.0 && dir[2]!=0.0){
     dir1 = SVector3(1.0, 0.0, -dir[0]/dir[2]);
     dir2 = SVector3 (dir[0]/dir[2], -(dir[0]*dir[0]+dir[2]*dir[2])/(dir[1]*dir[2]), 1.0);
   }
   else if (dir[0]!=0.0 && dir[2]!=0.0){
     dir1 = SVector3(-dir[1]/dir[0], 1.0, 0.0);
     dir2 = SVector3(1.0, dir[1]/dir[0], -(dir[1]*dir[1]+dir[0]*dir[0])/(dir[0]*dir[2]));
   }
   else if (dir[0]!=0.0 && dir[1]!=0.0){
     dir1 = SVector3(0.0, -dir[2]/dir[1], 1.0);
     dir2 = SVector3(-(dir[1]*dir[1]+dir[2]*dir[2])/(dir[0]*dir[1]), 1.0, dir[2]/dir[1]);
   }
   else if (dir[0]==0.0 && dir[1]==0.0){
     dir1 = SVector3(0.0, 1.0, 0.0);
     dir2 = SVector3(1.0, 0.0, 0.0);
   }
   else if (dir[1]==0.0 && dir[2]==0.0){
     dir1 = SVector3(0.0, 1.0, 0.0);
     dir2 = SVector3(0.0, 0.0, 1.0);
   }
   else if (dir[0]==0.0 && dir[2]==0.0){
     dir1 = SVector3(1.0, 0.0, 0.0);
     dir2 = SVector3(0.0, 0.0, 1.0);
   }
   else {printf("ARGHH EMI DO STHG \n"); exit(1);}
   // printf("XYZ =%g %g %g r=%g dir0 = %g %g %g dir1 = %g %g %g dir2 =%g %g %g\n", 
   // 	  x,y,z,d, dir[0], dir[1], dir[2], dir1[0], dir1[1], dir1[2],  dir2[0], dir2[1], dir2[2] );
   // printf("0x1 =%g 1x2=%g 2x1=%g \n", dot(dir, dir1), dot(dir1,dir2), dot(dir2,dir));
   dir1.normalize();
   dir2.normalize();
   
   //dir = SVector3(1.0, 0.0, 0.0);
   //dir1 = SVector3(0.0, 1.0, 0.0);
   //dir2 = SVector3(0.0, 0.0, 1.0);
   
   double lcA = 4.*lc;
   double lam1 = 1./(lcA*lcA);
   double lam2 = 1./(lc*lc);
   metr = SMetric3(lam1,lam2,lam2, dir, dir1, dir2);

   delete[]index2;
   delete[]dist2; 

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
    fprintf(f4, "SL(%g,%g,%g,%g,%g,%g){%g,%g};\n",
 	    l->getVertex(0)->x(), l->getVertex(0)->y(), l->getVertex(0)->z(),
 	    l->getVertex(1)->x(), l->getVertex(1)->y(), l->getVertex(1)->z(),
 	    itc->second,itc->second);
  }
  fprintf(f4,"};\n");
  fclose(f4);
 
}

#endif
