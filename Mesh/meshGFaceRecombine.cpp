// Gmsh - Copyright (C) 1997-2012 C. Geuzaine, J.-F. Remacle
//
// See the LICENSE.txt file for license information. Please report all
// bugs and problems to <gmsh@geuz.org>.
//
// Contributor(s):
//   Amaury Johnen (a.johnen@ulg.ac.be)
//

#define REC2D_WAIT_TIME .001
#define REC2D_WAIT_TM_2 .001
#define REC2D_WAIT_TM_3 .001

// #define REC2D_SMOOTH
 #define REC2D_DRAW

#include <cmath>
#include "meshGFaceRecombine.h"
#include "MTriangle.h"
#include "MQuadrangle.h"
#include "PView.h"
#include "meshGFace.h"
#ifdef REC2D_SMOOTH
  #include "meshGFaceOptimize.h"
#endif
#ifdef REC2D_DRAW
  #include "drawContext.h"
  #include "FlGui.h"
  #include "Context.h"
  #include "OS.h"
#endif

// TODO : enlever ramaining tri, regarder pour pointing


  #include "meshGFaceOptimize.h" // test Blossom
Recombine2D *Recombine2D::_cur = NULL;
Rec2DData *Rec2DData::_cur = NULL;
double **Rec2DVertex::_qualVSnum = NULL;
double **Rec2DVertex::_gains = NULL;

static void __draw(bool atOrigin = true, double dt = REC2D_WAIT_TM_2)
{
  if (atOrigin) Recombine2D::drawStateOrigin();
  double time = Cpu();
  CTX::instance()->mesh.changed = ENT_ALL;
  drawContext::global()->draw();
  while (Cpu()-time < dt)
    FlGui::instance()->check();
}

bool edgesInOrder(Rec2DEdge **edges, int numEdges)
{
  Rec2DVertex **v, *v0, *v1;
  v = new Rec2DVertex*[numEdges];
  v0 = edges[0]->getVertex(0);
  v1 = edges[0]->getVertex(1);
  if (edges[1]->getVertex(0) == v0 || edges[1]->getVertex(1) == v0) {
    v[0] = v0;
    if (edges[1]->getVertex(0) == v0)
      v[1] = edges[1]->getVertex(1);
    else
      v[1] = edges[1]->getVertex(0);
  }
  else if (edges[1]->getVertex(0) == v1 || edges[1]->getVertex(1) == v1) {
    v[0] = v1;
    if (edges[1]->getVertex(0) == v1)
      v[1] = edges[1]->getVertex(1);
    else
      v[1] = edges[1]->getVertex(0);
  }
  else {
    Msg::Error("edges not in order (1)");
    for (int i = 0; i < numEdges; ++i) {
      edges[i]->print();
    }
    return false;
  }
  for (int i = 2; i < numEdges; ++i) {
    if (edges[i]->getVertex(0) == v[i-1])
      v[i] = edges[i]->getVertex(1);
    else if (edges[i]->getVertex(1) == v[i-1])
      v[i] = edges[i]->getVertex(0);
    else {
      Msg::Error("edges not in order (2)");
      for (int i = 0; i < numEdges; ++i) {
        edges[i]->print();
      }
      return false;
    }
  }
  if (v[0] == v1 && v[numEdges-1] != v0 || v[0] == v0 && v[numEdges-1] != v1) {
    Msg::Error("edges not in order (3)");
    for (int i = 0; i < numEdges; ++i) {
      edges[i]->print();
    }
    return false;
  }
   delete v;
  return true;
}

void __crash()
{
  Msg::Info(" ");
  Recombine2D::drawStateOrigin();
  int a[2];
  int e = 0;
  for (int i = 0; i < 10000000; ++i) e+=a[i];
  Msg::Info("%d",e);
}

void __wait(double dt = REC2D_WAIT_TM_3)
{
  Msg::Info(" ");
  double time = Cpu();
  while (Cpu()-time < dt)
    FlGui::instance()->check();
}

int otherParity(int a)
{
  if (a % 2)
    return a - 1;
  return a + 1;
}

namespace std //swap
{
  template <>
  void swap(Rec2DData::Action& a0, Rec2DData::Action& a1)
  {
    ((Rec2DAction*)a0.action)->_dataAction = &a1;
    ((Rec2DAction*)a1.action)->_dataAction = &a0;
    const Rec2DAction *ra0 = a0.action;
    a0.action = a1.action;
    a1.action = ra0;
  }
}

/**  Recombine2D  **/
/*******************/
Recombine2D::Recombine2D(GFace *gf) : _gf(gf), _strategy(0), _numChange(0),
                                      elist(NULL)
{
  if (Recombine2D::_cur != NULL) {
    Msg::Warning("[Recombine2D] An instance already in execution");
    _data = NULL;
    return;
  }
  Recombine2D::_cur = this;
  
  orientMeshGFace orienter;
  orienter(_gf);
  Rec2DVertex::initStaticTable();
  backgroundMesh::set(_gf);
  _bgm = backgroundMesh::current();
  _data = new Rec2DData();
  
  static int po = -1;
  if (++po < 1) {
    Msg::Warning("FIXME Why {mesh 2} then {mesh 0} then {mesh 2} imply not corner vertices");
    Msg::Warning("FIXME Why more vertices after first mesh generation");
    Msg::Warning("FIXME Update of Action pointing to edge and vertex (when change)");
  }
  
  // Be able to compute geometrical angle at corners
  std::map<MVertex*, AngleData> mapCornerVert;
  {
    std::list<GEdge*> listge = gf->edges();
    std::list<GEdge*>::iterator itge = listge.begin();
    for (; itge != listge.end(); ++itge) {
      mapCornerVert[(*itge)->getBeginVertex()->getMeshElement(0)->getVertex(0)]
        ._gEdges.push_back(*itge);
      mapCornerVert[(*itge)->getEndVertex()->getMeshElement(0)->getVertex(0)]
        ._gEdges.push_back(*itge);
    }
  }
  // Create the 'Rec2DEdge', the 'Rec2DVertex' and the 'Rec2DElement'
  {
    std::map<MVertex*, Rec2DVertex*> mapVert;
    std::map<MVertex*, Rec2DVertex*>::iterator itV;
    std::map<MVertex*, AngleData>::iterator itCorner;
    // triangles
    for (unsigned int i = 0; i < gf->triangles.size(); ++i) {
      MTriangle *t = gf->triangles[i];
      
      Rec2DVertex *rv[3];
      for (int j = 0; j < 3; ++j) {
        MVertex *v = t->getVertex(j);
        if ( (itCorner = mapCornerVert.find(v)) != mapCornerVert.end() ) {
          if (!itCorner->second._rv)
            itCorner->second._rv = new Rec2DVertex(v);
          rv[j] = itCorner->second._rv;
          itCorner->second._mElements.push_back((MElement*)t);
        }
        else if ( (itV = mapVert.find(v)) != mapVert.end() ) {
          rv[j] = itV->second;
        }
        else {
          rv[j] = new Rec2DVertex(v);
          mapVert[v] = rv[j];
        }
      }
      const Rec2DEdge *re[3];
      for (int j = 0; j < 3; ++j) {
        if ( !(re[j] = Rec2DVertex::getCommonEdge(rv[j], rv[(j+1)%3])) )
          re[j] = new Rec2DEdge(rv[j], rv[(j+1)%3]);
      }
      
      new Rec2DElement(t, re, rv);
    }
    // quadrangles
    for (unsigned int i = 0; i < gf->quadrangles.size(); ++i) {
      MQuadrangle *q = gf->quadrangles[i];
      
      Rec2DVertex *rv[4];
      for (int j = 0; j < 4; ++j) {
        MVertex *v = q->getVertex(j);
        if ( (itCorner = mapCornerVert.find(v)) != mapCornerVert.end() ) {
          if (!itCorner->second._rv)
            itCorner->second._rv = new Rec2DVertex(v);
          rv[j] = itCorner->second._rv;
          itCorner->second._mElements.push_back((MElement*)q);
        }
        else if ( (itV = mapVert.find(v)) == mapVert.end() ) {
          rv[j] = itV->second;
        }
        else {
          rv[j] = new Rec2DVertex(v);
          mapVert[v] = rv[j];
        }
      }
      const Rec2DEdge *re[4];
      for (int j = 0; j < 4; ++j) {
        if ( !(re[j] = Rec2DVertex::getCommonEdge(rv[i], rv[(i+1)%3])) )
          re[j] = new Rec2DEdge(rv[i], rv[(i+1)%3]);
      }
      
      new Rec2DElement(q, re, rv);
    }
  }
  // update corner
  {
    std::map<MVertex*, AngleData>::iterator it = mapCornerVert.begin();
    for (; it != mapCornerVert.end(); ++it) {
      double angle = _geomAngle(it->first,
                                it->second._gEdges,
                                it->second._mElements);
      new Rec2DVertex(it->second._rv, angle);
    }
  }
  mapCornerVert.clear();
  // update boundary, create the 'Rec2DTwoTri2Quad'
  {
    Rec2DData::iter_re it = Rec2DData::firstEdge();
    for (; it != Rec2DData::lastEdge(); ++it) {
      Rec2DVertex *rv0 = (*it)->getVertex(0), *rv1 = (*it)->getVertex(1);
      if ((*it)->isOnBoundary()) {
        rv0->setOnBoundary();
        rv1->setOnBoundary();
      }
      else {
        std::vector<Rec2DElement*> elem;
        Rec2DVertex::getCommonElements(rv0, rv1, elem);
        if (elem.size() != 2)
          Msg::Error("[Recombine2D] %d elements instead of 2 for neighbour link",
                     elem.size()                                                 );
        else {
          elem[0]->addNeighbour(*it, elem[1]);
          elem[1]->addNeighbour(*it, elem[0]);
#ifdef REC2D_ONLY_RECO
          new Rec2DTwoTri2Quad(elem[0], elem[1]);
#else
          new Rec2DCollapse(new Rec2DTwoTri2Quad(elem[0], elem[1]));
#endif
        }
      }
    }
  }
  // set parity on boundary, create the 'Rec2DFourTri2Quad'
  {
    Rec2DData::iter_rv it = Rec2DData::firstVertex();
    for (; it != Rec2DData::lastVertex(); ++it) {
      Rec2DVertex *rv = *it;
      if (rv->getOnBoundary()) {
        if (!rv->getParity()) {
          int base = Rec2DData::getNewParity();
          rv->setBoundaryParity(base, base+1);
        }
      }
      else if (rv->getNumElements() == 4) {
        ;
      }
    }
  }
  
  Rec2DData::checkObsolete();
  _data->printState();
}

Recombine2D::~Recombine2D()
{
  delete _data;
  if (Recombine2D::_cur == this)
    Recombine2D::_cur = NULL;
}

bool Recombine2D::recombine()
{
  static int a = -1;
  if (++a < 1) Msg::Error("FIXME Need new definition Recombine2D::recombine()");
  /*Rec2DAction *nextAction;
  while ((nextAction = Rec2DData::getBestAction())) {
#ifdef REC2D_DRAW
    FlGui::instance()->check();
    double time = Cpu();
    nextAction->color(0, 0, 200);
    CTX::instance()->mesh.changed = ENT_ALL;
    drawContext::global()->draw();
#endif
    
    if (false) { // if !(remain all quad)
      delete nextAction;
#ifdef REC2D_DRAW
      nextAction->color(190, 0, 0);
      while (Cpu()-time < REC2D_WAIT_TIME)
        FlGui::instance()->check();
#endif
      continue;
    }
    ++_numChange;
    std::vector<Rec2DVertex*> newPar;
    nextAction->apply(newPar);
    Recombine2D::incNumChange();
    
    // forall v in newPar : check obsoletes action;
    
#ifdef REC2D_DRAW
    _gf->triangles = _data->_tri;
    _gf->quadrangles = _data->_quad;
    CTX::instance()->mesh.changed = ENT_ALL;
    drawContext::global()->draw();
    while (Cpu()-time < REC2D_WAIT_TIME)
      FlGui::instance()->check();
#endif
    
    delete nextAction;
  }
  
  _data->printState();
#ifdef REC2D_SMOOTH
    laplaceSmoothing(_gf,100);
#endif
    CTX::instance()->mesh.changed = ENT_ALL;
    drawContext::global()->draw();*/
  return 1;
}

double Recombine2D::recombine(int depth)
{
  Msg::Info("Recombining with depth %d", depth);
  double time = Cpu();
  Rec2DData::clearChanges();
  double bestGlobalQuality;
#ifdef REC2D_DRAW // draw state at origin
    _gf->triangles = _data->_tri;
    _gf->quadrangles = _data->_quad;
    CTX::instance()->mesh.changed = ENT_ALL;
    drawContext::global()->draw();
    while (Cpu()-time < REC2D_WAIT_TIME)
      FlGui::instance()->check();
    time = Cpu();
#endif
  drawStateOrigin();
  Rec2DNode *root = new Rec2DNode(NULL, NULL, depth);
  bestGlobalQuality = root->getGlobQual();
  Rec2DNode *currentNode = root->selectBestNode();
  
  //static int num = 20, i = 0;
  //static double dx = .0, dy = .0;
  
  while (currentNode) {
    //Msg::Info("%d %d", Rec2DData::getNumZeroAction(), Rec2DData::getNumOneAction());
    //_data->checkQuality();
    FlGui::instance()->check();
#if 0//def REC2D_DRAW // draw all states
    if ( !((i+1) % ((int)std::sqrt(num)+1)) ) {
      dx = .0;
      dy -= 1.1;
    }
    else
      dx += 1.1;
    drawState(dx, dy);
    CTX::instance()->mesh.changed = ENT_ALL;
    drawContext::global()->draw();
    while (Cpu()-time < REC2D_WAIT_TIME)
      FlGui::instance()->check();
    ++i;
    time = Cpu();
#endif
    currentNode->lookahead(depth);
    bestGlobalQuality = currentNode->getGlobQual();
#ifdef REC2D_DRAW // draw state at origin
    drawStateOrigin();
    while (Cpu()-time < REC2D_WAIT_TIME)
      FlGui::instance()->check();
    time = Cpu();
#endif
    //currentNode = currentNode->selectBestNode();
    currentNode = Rec2DNode::selectBestNode(currentNode);
  }
  
  Msg::Info("-------- %g", Rec2DData::getGlobalQuality());
  return Rec2DData::getGlobalQuality();
  //_data->printState();
}

void Recombine2D::compareWithBlossom()
{
  Msg::Error("..............begin..............");
  recombineWithBlossom(_gf, .0, 1.1, elist, t2n);
  _data->_quad = _gf->quadrangles;
  __draw();
  setStrategy(3);
  int num = 2;
  double dx = .0, dy = .0, time = Cpu();
  for (int d = 1; d < 7; ++d) {
    recombine(d);
    if ( d % ((int)std::sqrt(num)+1) ) {
      dx += 3.3;
    }
    else {
      dx = .0;
      dy -= 1.1;
    }
    drawState(dx, dy, true);
    CTX::instance()->mesh.changed = ENT_ALL;
    drawContext::global()->draw();
    //while (Cpu()-time < REC2D_WAIT_TIME)
    //  FlGui::instance()->check();
    time = Cpu();
  }
  int totalBlossom = 0;
  for (int k = 0; k < _cur->elist[0]; ++k) totalBlossom += 100-_cur->elist[1+3*k+2];
  Msg::Info("Blossom %d", totalBlossom);
  delete elist;
  t2n.clear();
  
  Rec2DData::clearChanges();
#ifdef REC2D_DRAW // draw state at origin
    _gf->triangles = _data->_tri;
    _gf->quadrangles = _data->_quad;
    CTX::instance()->mesh.changed = ENT_ALL;
    drawContext::global()->draw();
    while (Cpu()-time < REC2D_WAIT_TIME)
      FlGui::instance()->check();
    time = Cpu();
#endif
}

int Recombine2D::getNumTri() const
{
  return _data->getNumTri();
}

void Recombine2D::clearChanges()
{
  Rec2DData::clearChanges();
#if 0//def REC2D_DRAW
    CTX::instance()->mesh.changed = ENT_ALL;
    drawContext::global()->draw();
#endif
}

bool Recombine2D::developTree()
{
  Rec2DNode root(NULL, NULL);
  _data->printState();
  
  Msg::Info("best global value : %g", root.getBestSequenceReward());
  Msg::Info("num end node : %d", Rec2DData::getNumEndNode());
  
  Rec2DData::sortEndNode();
  Rec2DData::drawEndNode(100);
  //_gf->triangles.clear();
  return 1;
}

void Recombine2D::nextTreeActions(std::vector<Rec2DAction*> &actions,
                                  const std::vector<Rec2DElement*> &neighbourEl,
                                  const Rec2DNode *node                         )
{
  static int a = -1;
  if (++a < 1) Msg::Warning("FIXME check entities");
  Rec2DData::checkEntities();
  actions.clear();
#ifdef REC2D_ONLY_RECO
  if (neighbourEl.size()) {
    for (unsigned int i = 0; i < neighbourEl.size(); ++i) {
      if (neighbourEl[i]->getNumActions() == 1)
        actions.push_back(neighbourEl[i]->getAction(0));
    }
    if (actions.size()) return;
  }
  else if (node) {
    node = node->getFather();
    while (node && node->isInSequence()) {
      std::vector<Rec2DElement*> elem;
      node->getAction()->getNeighbElemWithActions(elem);
      for (unsigned int i = 0; i < elem.size(); ++i) {
        if (elem[i]->getNumActions() == 1) {
          actions.push_back(elem[i]->getAction(0));
        }
      }
      if (actions.size()) return;
      node = node->getFather();
    }
  }
  
  Rec2DData::getUniqueOneActions(actions);
  if (actions.size()) return;
#endif
  if (_cur->_strategy == 4) {
    Msg::Warning("FIXME remove this part");
    Rec2DAction *action;
    if ((action = Rec2DData::getBestAction()))
      actions.push_back(action);
    return;
  }
  std::vector<Rec2DElement*> elements;
  Rec2DAction *ra = NULL;
  Rec2DElement *rel = NULL;
  switch (_cur->_strategy) {
    case 0 : // random triangle of random action
      ra = Rec2DData::getRandomAction();
      if (!ra) return;
      rel = ra->getRandomElement();
      break;
      
    default :
    case 3 : // triangle of best neighbour action
      for (unsigned int i = 0; i < neighbourEl.size(); ++i)
        neighbourEl[i]->getMoreUniqueActions(actions);
      if (actions.size()) {
        (*std::max_element(actions.begin(), actions.end(), lessRec2DAction()))
          ->getElements(elements);
        for (unsigned int i = 0; i < elements.size(); ++i) {
          for (unsigned int j = 0; j < neighbourEl.size(); ++j) {
            if (elements[i] == neighbourEl[j]) {
              rel = elements[i];
              goto end;
            }
          }
        }
      }
    end :
      if (rel) break;
      
    case 2 : // random neighbour triangle
      if (neighbourEl.size()) {
        rel = neighbourEl[rand() % (int)neighbourEl.size()];
        break;
      }
      
    case 1 : // random triangle of best action
#ifdef REC2D_ONLY_RECO
      if (_cur->_strategy != 1 && node) {
        node = node->getFather();
        while (node && node->isInSequence()) {
          std::vector<Rec2DElement*> elem;
          node->getAction()->getNeighbElemWithActions(elem);
          for (unsigned int i = 0; i < elem.size(); ++i)
            elem[i]->getMoreUniqueActions(actions);
          if (actions.size()) {
            (*std::max_element(actions.begin(), actions.end(), lessRec2DAction()))
              ->getElements(elements);
            for (unsigned int i = 0; i < elements.size(); ++i) {
              for (unsigned int j = 0; j < elem.size(); ++j) {
                if (elements[i] == elem[j]) {
                  rel = elements[i];
                  goto end2;
                }
              }
            }
          }
          actions.clear();
          node = node->getFather();
        }
      }
    end2 :
      if (rel) break;
#endif
      ra = Rec2DData::getBestAction();
      if (!ra) return;
      rel = ra->getRandomElement();
      break;
  }
  rel->getActions(actions);
  unsigned int i = 0;
  while (i < actions.size()) {
    if (actions[i]->isObsolete()) {
#if 0//def REC2D_DRAW
      static int a = -1;
      if (++a < 1) Msg::Warning("FIXME Normal to be here ?");
      actions[i]->color(190, 0, 0);
      double time = Cpu();
      CTX::instance()->mesh.changed = ENT_ALL;
      drawContext::global()->draw();
      while (Cpu()-time < 5)
        FlGui::instance()->check();
#endif
      actions[i] = actions.back();
      actions.pop_back();
    }
    else
      ++i;
  }
}

void Recombine2D::drawState(double shiftx, double shifty, bool color) const
{
  //_data->drawElements(shiftx, shifty);
  _data->drawTriangles(shiftx, shifty);
  _data->drawChanges(shiftx, shifty, color);
  CTX::instance()->mesh.changed = ENT_ALL;
  drawContext::global()->draw();
}

void Recombine2D::drawStateOrigin()
{
  _cur->_gf->triangles = _cur->_data->_tri;
  _cur->_gf->quadrangles = _cur->_data->_quad;
  //CTX::instance()->mesh.changed = ENT_ALL;
  //drawContext::global()->draw();
  //FlGui::instance()->check();
}

void Recombine2D::printState() const
{
  _data->printState();
}

double Recombine2D::_geomAngle(const MVertex *v,
                               const std::vector<GEdge*> &gEdge,
                               const std::vector<MElement*> &elem) const //*
{
  if (gEdge.size() != 2) {
    Msg::Error("[Recombine2D] Wrong number of edge : %d, returning pi/2",
                 gEdge.size()                                            );
    return M_PI / 2.;
  }
  static const double prec = 100.;
  
  SVector3 vectv = SVector3(v->x(), v->y(), v->z());
  SVector3 firstDer0, firstDer1;
  
  for (unsigned int k = 0; k < 2; ++k) {
    GEdge *ge = gEdge[k];
    SVector3 vectlb = ge->position(ge->getLowerBound());
    SVector3 vectub = ge->position(ge->getUpperBound());
    vectlb -= vectv;
    vectub -= vectv;
    double normlb, normub;
    if ((normlb = norm(vectlb)) > prec * (normub = norm(vectub)))
      firstDer1 = -1. * ge->firstDer(ge->getUpperBound());
    else if (normub > prec * normlb)
      firstDer1 = ge->firstDer(ge->getLowerBound());
    else {
      Msg::Error("[Recombine2D] Bad precision, returning pi/2");
      return M_PI / 2.;
    }
    if (k == 0)
      firstDer0 = firstDer1;
  }
  
  firstDer0 = firstDer0 * (1. / norm(firstDer0));
  firstDer1 = firstDer1 * (1. / norm(firstDer1));
  
  double angle1 = acos(dot(firstDer0, firstDer1));
  double angle2 = 2. * M_PI - angle1;
  
  double angleMesh = .0;
  for (unsigned int i = 0; i < elem.size(); ++i) {
    MElement *el = elem[i];
    int k = 0, numV = el->getNumVertices();
    while (el->getVertex(k) != v && k < numV) ++k;
    if (k == numV) {
      Msg::Error("[Recombine2D] Wrong element, returning pi/2");
      return M_PI / 2.;
    }
    if (el->getNumVertices() > 3)
      Msg::Warning("[Recombine2D] angle3Vertices() ok for triangle... but not for other");
    angleMesh += angle3Vertices(el->getVertex((k+numV-1) % numV), v,
                                el->getVertex((k+1) % numV)    );
  }
  
  if (angleMesh < M_PI)
    return angle1;
  return angle2;
}

void Recombine2D::add(MQuadrangle *q)
{
  _cur->_gf->quadrangles.push_back(q);
  _cur->_data->_quad.push_back(q);
}

void Recombine2D::add(MTriangle *t)
{
  _cur->_gf->triangles.push_back(t);
  _cur->_data->_tri.push_back(t);
}


void Recombine2D::colorFromBlossom(const Rec2DElement *tri1,
                                   const Rec2DElement *tri2,
                                   const Rec2DElement *quad )
{
  if (!_cur->elist) {
    Msg::Error("no list");
    return;
  }
  int i1 = _cur->t2n[tri1->getMElement()];
  int i2 = _cur->t2n[tri2->getMElement()];
  int k = -1;
  while (++k < _cur->elist[0] && _cur->elist[1+3*k] != i1 && _cur->elist[1+3*k] != i2);
  if (k < _cur->elist[0] && _cur->elist[1+3*k+1] == i1 || _cur->elist[1+3*k+1] == i2) {
    unsigned int col = CTX::instance()->packColor(200, 120, 225, 255);
    quad->getMElement()->setCol(col);
  }
}

void Recombine2D::colorFromBlossom(const Rec2DElement *tri1,
                                   const Rec2DElement *tri2,
                                   const MElement *quad     )
{
  if (!_cur->elist) {
    Msg::Error("no list");
    return;
  }
  int i1 = _cur->t2n[tri1->getMElement()];
  int i2 = _cur->t2n[tri2->getMElement()];
  int k = -1;
  while (++k < _cur->elist[0] && _cur->elist[1+3*k] != i1 && _cur->elist[1+3*k] != i2);
  if (k < _cur->elist[0] && _cur->elist[1+3*k+1] == i1 || _cur->elist[1+3*k+1] == i2) {
    unsigned int col = CTX::instance()->packColor(200, 120, 225, 255);
    ((MElement*)quad)->setCol(col);
  }
}


/**  Rec2DData  **/
/*****************/
bool Rec2DData::gterAction::operator()(const Action *ra1, const Action *ra2) const
{
  return *ra2->action < *ra1->action;
}

bool Rec2DData::lessAction::operator()(const Action *ra1, const Action *ra2) const
{
  return *ra1->action < *ra2->action;
}

Rec2DData::Rec2DData() : _remainingTri(0)
{
  if (Rec2DData::_cur != NULL) {
    Msg::Error("[Rec2DData] An instance in execution");
    return;
  }
  Rec2DData::_cur = this;
  _numEdge = _numVert = 0;
  _valEdge = _valVert = .0;
#ifdef REC2D_RECO_BLOS
  _valQuad = 0;
#endif
}

Rec2DData::~Rec2DData()
{
  if (Rec2DData::_cur == this)
    Rec2DData::_cur = NULL;
}

void Rec2DData::add(const Rec2DEdge *re)
{
  if (re->_pos > -1) {
    Msg::Error("[Rec2DData] edge already there");
    return;
  }
  ((Rec2DEdge*)re)->_pos = _cur->_edges.size();
  _cur->_edges.push_back((Rec2DEdge*)re);
}

void Rec2DData::add(const Rec2DVertex *rv)
{
  if (rv->_pos > -1) {
    Msg::Error("[Rec2DData] vert %d already there (size %d)", rv, _cur->_vertices.size());
    return;
  }
  ((Rec2DVertex*)rv)->_pos = _cur->_vertices.size();
  _cur->_vertices.push_back((Rec2DVertex*)rv);
}

void Rec2DData::add(const Rec2DElement *rel)
{
  if (rel->_pos > -1) {
    Msg::Error("[Rec2DData] elem already there");
    return;
  }
  if (rel->isTri()) ++_cur->_remainingTri;
  ((Rec2DElement*)rel)->_pos = _cur->_elements.size();
  _cur->_elements.push_back((Rec2DElement*)rel);
  
#ifdef REC2D_DRAW
  MTriangle *t = rel->getMTriangle();
  if (t)
    _cur->_tri.push_back(t);
  MQuadrangle *q = rel->getMQuadrangle();
  if (q)
    _cur->_quad.push_back(q);
#endif
}

void Rec2DData::add(const Rec2DAction *ra)
{
  if (ra->_dataAction) {
    Msg::Error("[Rec2DData] action already there");
    ra->printIdentity();
    return;
  }
  _cur->_actions.push_back(new Action(ra, _cur->_actions.size()));
  ((Rec2DAction*)ra)->_dataAction = _cur->_actions.back();
}

void Rec2DData::rmv(const Rec2DEdge *re)
{
  if (re->_pos < 0) {
    Msg::Error("[Rec2DData] edge not there");
    return;
  }
  _cur->_edges.back()->_pos = re->_pos;
  _cur->_edges[re->_pos] = _cur->_edges.back();
  _cur->_edges.pop_back();
  ((Rec2DEdge*)re)->_pos = -1;
}

void Rec2DData::rmv(const Rec2DVertex *rv)
{
  if (rv->_pos < 0) {
    Msg::Error("[Rec2DData] vert not there");
    return;
  }
  _cur->_vertices.back()->_pos = rv->_pos;
  _cur->_vertices[rv->_pos] = _cur->_vertices.back();
  _cur->_vertices.pop_back();
  ((Rec2DVertex*)rv)->_pos = -1;
}

void Rec2DData::rmv(const Rec2DElement *rel)
{
  if (rel->_pos < 0) {
    Msg::Error("[Rec2DData] vert not there");
    return;
  }
  if (rel->isTri()) --_cur->_remainingTri;
  _cur->_elements.back()->_pos = rel->_pos;
  _cur->_elements[rel->_pos] = _cur->_elements.back();
  _cur->_elements.pop_back();
  ((Rec2DElement*)rel)->_pos = -1;
  
#ifdef REC2D_DRAW
  MTriangle *t = rel->getMTriangle();
  if (t) {
    for (unsigned int i = 0; i < _cur->_tri.size(); ++i) {
      if (_cur->_tri[i] == t) {
        _cur->_tri[i] = _cur->_tri.back();
        _cur->_tri.pop_back();
        return;
      }
    }
    Msg::Warning("[Rec2DData] Didn't erased mtriangle :(");
  }
  MQuadrangle *q = rel->getMQuadrangle();
  if (q) {
    for (unsigned int i = 0; i < _cur->_quad.size(); ++i) {
      if (_cur->_quad[i] == q) {
        _cur->_quad[i] = _cur->_quad.back();
        _cur->_quad.pop_back();
        return;
      }
    }
    Msg::Warning("[Rec2DData] Didn't erased mquadrangle :(");
  }
#endif
}

void Rec2DData::rmv(const Rec2DAction *ra)
{
  if (!ra->_dataAction) {
    Msg::Error("[Rec2DData] action not there");
    return;
  }
  std::swap(*((Action*)ra->_dataAction), *_cur->_actions.back());
  ((Rec2DAction*)ra)->_dataAction = NULL;
  delete _cur->_actions.back();
  _cur->_actions.pop_back();
}

bool Rec2DData::has(const Rec2DAction *ra)
{
  for (unsigned int i = 0; i < _cur->_actions.size(); ++i)
    if (_cur->_actions[i]->action == ra) return true;
  return false;
}

#ifdef REC2D_ONLY_RECO
void Rec2DData::addHasZeroAction(const Rec2DElement *rel)
{
  std::pair<std::set<Rec2DElement*>::iterator, bool> rep;
  rep = _cur->_elementWithZeroAction.insert((Rec2DElement*)rel);
  if (!rep.second)
    Msg::Error("[Rec2DData] elementWithZeroAction already there");
}

void Rec2DData::rmvHasZeroAction(const Rec2DElement *rel)
{
  if (!_cur->_elementWithZeroAction.erase((Rec2DElement*)rel))
    Msg::Error("[Rec2DData] elementWithZeroAction not there");
}

bool Rec2DData::hasHasZeroAction(const Rec2DElement *rel)
{
  return _cur->_elementWithZeroAction.find((Rec2DElement*)rel)
         != _cur->_elementWithZeroAction.end();
}

int Rec2DData::getNumZeroAction()
{
  return _cur->_elementWithZeroAction.size();
}

void Rec2DData::addHasOneAction(const Rec2DElement *rel)
{
  std::pair<std::set<Rec2DElement*>::iterator, bool> rep;
  rep = _cur->_elementWithOneAction.insert((Rec2DElement*)rel);
  if (!rep.second)
    Msg::Error("[Rec2DData] elementWithOneAction already there");
}

void Rec2DData::rmvHasOneAction(const Rec2DElement *rel)
{
  if (!_cur->_elementWithOneAction.erase((Rec2DElement*)rel))
    Msg::Error("[Rec2DData] elementWithOneAction not there");
}

bool Rec2DData::hasHasOneAction(const Rec2DElement *rel)
{
  return _cur->_elementWithOneAction.find((Rec2DElement*)rel)
         != _cur->_elementWithOneAction.end();
}

int Rec2DData::getNumOneAction()
{
  return _cur->_elementWithOneAction.size();
}

Rec2DAction* Rec2DData::getOneAction()
{
  if (_cur->_elementWithOneAction.size())
    return (*_cur->_elementWithOneAction.begin())->getAction(0);
  return NULL;
}

void Rec2DData::getUniqueOneActions(std::vector<Rec2DAction*> &vec)
{
  std::set<Rec2DElement*> elemWOA = _cur->_elementWithOneAction;
  std::set<Rec2DElement*>::iterator it = elemWOA.begin();
  std::vector<Rec2DAction*> *v;
  int minNum = 0;
  if (it != elemWOA.end()) {
    Rec2DAction *ra = (*it)->getAction(0);
    Rec2DDataChange *dataChange = Rec2DData::getNewDataChange();
    ra->apply(dataChange, v);
    minNum = Rec2DData::getNumOneAction();
    Rec2DData::revertDataChange(dataChange);
    vec.push_back(ra);
    ++it;
  }
  while (it != elemWOA.end()) {
    Rec2DAction *ra = (*it)->getAction(0);
    Rec2DDataChange *dataChange = Rec2DData::getNewDataChange();
    ra->apply(dataChange, v);
    if (minNum > Rec2DData::getNumOneAction()) {
      minNum = Rec2DData::getNumOneAction();
      vec.clear();
      vec.push_back(ra);
    }
    else if (minNum == Rec2DData::getNumOneAction()) {
      unsigned int i = 0;
      while (i < vec.size() && vec[i] != ra) ++i;
      if (i == vec.size()) vec.push_back(ra);
    }
    Rec2DData::revertDataChange(dataChange);
    ++it;
  }
}

#endif
void Rec2DData::printState() const
{
  Msg::Info(" ");
  Msg::Info("State");
  Msg::Info("-----");
  Msg::Info("numEdge %d (%d), valEdge %g => %g", _numEdge, _edges.size(), (double)_valEdge, (double)_valEdge/(double)_numEdge);
  Msg::Info("numVert %d (%d), valVert %g => %g", _numVert, _vertices.size(), (double)_valVert, (double)_valVert/(double)_numVert);
  Msg::Info("Element (%d)", _elements.size());
  Msg::Info("global Value %g", Rec2DData::getGlobalQuality());
  Msg::Info("num action %d", _actions.size());
  std::map<int, std::vector<Rec2DVertex*> >::const_iterator it = _parities.begin();
  for (; it != _parities.end(); ++it) {
    Msg::Info("par %d, #%d", it->first, it->second.size());
  }
  Msg::Info(" ");
  iter_re ite;
  long double valEdge = .0;
  int numEdge = 0;
  for (ite = firstEdge(); ite != lastEdge(); ++ite) {
    valEdge += (long double)(*ite)->getWeightedQual();
    numEdge += (*ite)->getWeight();
  }
  Msg::Info("valEdge : %g >< %g, numEdge %d >< %d (real><data)",
            (double)valEdge, (double)_valEdge,
            numEdge, _numEdge);
  iter_rv itv;
  long double valVert = .0;
  int numVert = 0;
  for (itv = firstVertex(); itv != lastVertex(); ++itv) {
    valVert += (long double)(*itv)->getQual();
    if ((*itv)->getParity() == -1 || (*itv)->getParity() == 1)
      Msg::Error("parity %d, I'm very angry", (*itv)->getParity());
    ++numVert;
  }
  Msg::Info("valVert : %g >< %g, numVert %d >< %d (real><data)",
            (double)valVert, (double)_valVert,
            numVert, _numVert);
}

void Rec2DData::checkQuality() const
{
  iter_re ite;
  long double valEdge = .0;
  int numEdge = 0;
  for (ite = firstEdge(); ite != lastEdge(); ++ite) {
    valEdge += (long double)(*ite)->getWeightedQual();
    numEdge += (*ite)->getWeight();
  }
  iter_rv itv;
  long double valVert = .0;
  int numVert = 0;
  for (itv = firstVertex(); itv != lastVertex(); ++itv) {
    valVert += (long double)(*itv)->getQual();
    numVert += 2;
  }
  if (fabs(valVert - _valVert) > 1e-14 || fabs(valEdge - _valEdge) > 1e-14) {
    Msg::Error("Vert : %g >< %g (%g), %d >< %d", (double)valVert, (double)_valVert, (double)(valVert-_valVert), numVert, _numVert);
    Msg::Error("Edge : %g >< %g (%g), %d >< %d", (double)valEdge, (double)_valEdge, (double)(valEdge-_valEdge), numEdge, _numEdge);
  }
}

bool Rec2DData::checkEntities()
{
  iter_rv itv;
  for (itv = firstVertex(); itv != lastVertex(); ++itv) {
    if (!(*itv)->checkCoherence()) {
      Msg::Error("Incoherence vertex");
      //__crash();
      return false;
    }
  }
  iter_re ite;
  for (ite = firstEdge(); ite != lastEdge(); ++ite) {
    if (!(*ite)->checkCoherence()) {
      Msg::Error("Incoherence edge");
      //__crash();
      return false;
    }
  }
  iter_rel itel;
  for (itel = firstElement(); itel != lastElement(); ++itel) {
    if (!(*itel)->checkCoherence()) {
      Msg::Error("Incoherence element");
      //__crash();
      return false;
    }
  }
#ifdef REC2D_ONLY_RECO
  std::set<Rec2DElement*>::iterator it = _cur->_elementWithOneAction.begin();
  while (it != _cur->_elementWithOneAction.end()) {
    if ((*it)->getNumActions() != 1) {
      Msg::Error("Incoherence element with one action");
      //__crash();
      return false;
    }
    ++it;
  }
  it = _cur->_elementWithZeroAction.begin();
  while (it != _cur->_elementWithZeroAction.end()) {
    if ((*it)->getNumActions() != 0) {
      Msg::Error("Incoherence element with zero action");
      //__crash();
      return false;
    }
    ++it;
  }
#endif
  for (unsigned int i = 0; i < _cur->_actions.size(); ++i) {
    if (_cur->_actions[i]->position != (int)i ||
        _cur->_actions[i]->action->getDataAction() != _cur->_actions[i]) {
      Msg::Error("Wrong link Action <-> Rec2DAction");
      //__crash();
      //return false;
    }
    if (!_cur->_actions[i]->action->checkCoherence()) {
      _cur->printState();
      double time = Cpu();
      while (Cpu()-time < REC2D_WAIT_TM_2)
        FlGui::instance()->check();
      Msg::Error("Incoherence action");
      //__crash();
      //return false;
    }
  }
  return true;
}

void Rec2DData::printActions() const
{
  
  std::map<int, std::vector<double> > data;
  for (unsigned int i = 0; i < _actions.size(); ++i) {
    std::vector<Rec2DElement*> tri;
    _actions[i]->action->getElements(tri);
    Msg::Info("action %d (%d, %d) -> reward %g",
              _actions[i]->action,
              tri[0]->getNum(),
              tri[1]->getNum(),
              ((Rec2DAction*)_actions[i]->action)->getReward());
      //Msg::Info("action %d -> reward %g", *it, _actions[i]->getReward());
    data[tri[0]->getNum()].resize(1);
    data[tri[1]->getNum()].resize(1);
    //data[tri[0]->getNum()][0] = (*it)->getReward();
    //data[tri[1]->getNum()][0] = (*it)->getReward();
    //(*it)->print();
  }
  new PView("Jmin_bad", "ElementData", Recombine2D::getGFace()->model(), data);
  Msg::Info(" ");
}

int Rec2DData::getNewParity()
{
  if (_cur->_parities.empty())
    return 2;
  std::map<int, std::vector<Rec2DVertex*> >::iterator it;
  it = _cur->_parities.end();
  --it;
  return (it->first/2)*2 + 2;
}

Rec2DDataChange* Rec2DData::getNewDataChange()
{
  _cur->_changes.push_back(new Rec2DDataChange());
  return _cur->_changes.back();
}

bool Rec2DData::revertDataChange(Rec2DDataChange *rdc)
{
  if (_cur->_changes.back() != rdc)
    return false;
  _cur->_changes.pop_back();
  rdc->revert();
  delete rdc;
  return true;
}

void Rec2DData::clearChanges()
{
  for (int i = (int) _cur->_changes.size() - 1; i > -1; --i) {
    _cur->_changes[i]->revert();
    delete _cur->_changes[i];
  }
  _cur->_changes.clear();
}

void Rec2DData::removeParity(const Rec2DVertex *rv, int p)
{
  std::map<int, std::vector<Rec2DVertex*> >::iterator it;
  if ( (it = _cur->_parities.find(p)) == _cur->_parities.end() ) {
    Msg::Error("[Rec2DData] Don't have parity %d", p);
    return;
  }
  bool b = false;
  std::vector<Rec2DVertex*> *vect = &it->second;
  unsigned int i = 0;
  while (i < vect->size()) {
    if (vect->at(i) == rv) {
      vect->at(i) = vect->back();
      vect->pop_back();
      if (b)
        Msg::Error("[Rec2DData] Two or more times same vertex");
      b = true;
    }
    else
      ++i;
  }
  if (!b)
    Msg::Error("[Rec2DData] No vertex 1");
  else if (vect->empty())
    _cur->_parities.erase(it);
}

void Rec2DData::associateParity(int pOld, int pNew, Rec2DDataChange *rdc)
{
  if (pOld/2 == pNew/2) {
    Msg::Error("[Rec2DData] Do you want to make a mess of parities ?");
    return;
  }
  if (_cur->_parities.find(pNew) == _cur->_parities.end()) {
    Msg::Warning("[Rec2DData] That's strange, isn't it ?");
    static int a = -1;
    if (++a == 10)
      Msg::Warning("[Rec2DData] AND LOOK AT ME WHEN I TALK TO YOU !");
  }
  
  std::map<int, std::vector<Rec2DVertex*> >::iterator it;
  std::vector<Rec2DVertex*> *vect, *vectNew;
  {
    it = _cur->_parities.find(pOld);
    if (it == _cur->_parities.end()) {
      static int a = -1;
      if (++a < 1)
        Msg::Error("[Rec2DData] You are mistaken, I'll never talk to you again !");
      return;
    }
    vect = &it->second;
    if (rdc) {
      rdc->saveParity(*vect);
      //rdc->checkObsoleteActions(_vertices, 4);
    }
    for (unsigned int i = 0; i < vect->size(); ++i)
      (*vect)[i]->setParityWD(pOld, pNew);
    rdc->checkObsoleteActions(&(*vect)[0], vect->size());
    vectNew = &_cur->_parities[pNew];
    vectNew->insert(vectNew->end(), vect->begin(), vect->end());
    _cur->_parities.erase(it);
  }
  
  pOld = otherParity(pOld);
  pNew = otherParity(pNew);
  {
    it = _cur->_parities.find(pOld);
    if (it == _cur->_parities.end()) {
      Msg::Error("[Rec2DData] What ?");
      return;
    }
    vect = &it->second;
    if (rdc)
      rdc->saveParity(*vect);
    for (unsigned int i = 0; i < vect->size(); ++i)
      (*vect)[i]->setParityWD(pOld, pNew);
    rdc->checkObsoleteActions(&(*vect)[0], vect->size());
    vectNew = &_cur->_parities[pNew];
    vectNew->insert(vectNew->end(), vect->begin(), vect->end());
    _cur->_parities.erase(it);
  }
}

double Rec2DData::getGlobalQuality()
{
#ifdef REC2D_RECO_BLOS
  return (double)_cur->_valQuad;
#else
#ifdef REC2D_VERT_ONLY
  return (double)_cur->_valVert / (double)_cur->_numVert;
#else
  double a = (double)_cur->_valVert / (double)_cur->_numVert;
  return a * (double)_cur->_valEdge / (double)_cur->_numEdge;
#endif
#endif
}

double Rec2DData::getGlobalQuality(int numVert, double valVert,
                                   int numEdge, double valEdge )
{
#ifdef REC2D_RECO_BLOS
  Msg::Info("Is it ok ?");
  return (double)_cur->_valQuad;
#else
#ifdef REC2D_VERT_ONLY
  return ((double)_cur->_valVert + valVert) / ((double)_cur->_numVert + numVert);
#else
  double a = ((double)_cur->_valVert + valVert) / (double)(_cur->_numVert + numVert);
  return a * ((double)_cur->_valEdge + valEdge) / (double)(_cur->_numEdge + numEdge);
#endif
#endif
}

Rec2DAction* Rec2DData::getBestAction()
{
  if (_cur->_actions.size() == 0)
    return NULL;
  Action *ac = *std::max_element(_cur->_actions.begin(),
                                 _cur->_actions.end(), lessAction());
  
  return (Rec2DAction*)ac->action;
}

Rec2DAction* Rec2DData::getRandomAction()
{
  if (_cur->_actions.size() == 0)
    return NULL;
  int index = rand() % (int)_cur->_actions.size();
  return (Rec2DAction*)_cur->_actions[index]->action;
}

void Rec2DData::checkObsolete()
{
  std::vector<Rec2DAction*> obsoletes;
  for (unsigned int i = 0; i < _cur->_actions.size(); ++i) {
    if (_cur->_actions[i]->action->isObsolete())
      obsoletes.push_back((Rec2DAction*)_cur->_actions[i]->action);
  }
  
  for (unsigned int i = 0; i < obsoletes.size(); ++i)
    delete obsoletes[i];
}

void Rec2DData::drawTriangles(double shiftx, double shifty) const
{
  iter_rel it = firstElement();
  for (; it != lastElement(); ++it) {
    if ((*it)->isTri())
      (*it)->createElement(shiftx, shifty);
  }
}

void Rec2DData::drawElements(double shiftx, double shifty) const
{
  iter_rel it = firstElement();
  for (; it != lastElement(); ++it)
      (*it)->createElement(shiftx, shifty);
}

void Rec2DData::drawChanges(double shiftx, double shifty, bool color) const
{
  std::map<int, std::vector<double> > data;
  int k = 0;
  for (unsigned int i = 0; i < _changes.size(); ++i) {
    if (_changes[i]->getAction()->haveElem()) {
      MElement *el = _changes[i]->getAction()->createMElement(shiftx, shifty);
      if (color)
        Recombine2D::colorFromBlossom(_changes[i]->getAction()->getElement(0),
                                      _changes[i]->getAction()->getElement(1),
                                      el                                     );
      data[el->getNum()].push_back(++k);
    }
  }
  new PView("Changes", "ElementData", Recombine2D::getGFace()->model(), data);
}

void Rec2DData::addEndNode(const Rec2DNode *rn)
{
  static int k = 0;
  if (_cur->_endNodes.size() > 9999) {
    Msg::Info("%d", ++k);
    sortEndNode();
    int newLast = 4999;
    for (unsigned int i = newLast + 1; i < 10000; ++i) {
      if (_cur->_endNodes[i]->canBeDeleted())
        delete _cur->_endNodes[i];
      else
        _cur->_endNodes[++newLast] = _cur->_endNodes[i];
    }
    _cur->_endNodes.resize(newLast + 1);
  }
  _cur->_endNodes.push_back((Rec2DNode*)rn);
}

void Rec2DData::drawEndNode(int num)
{
  double dx = .0, dy = .0;
  for (int i = 0; i < num && i < (int)_cur->_endNodes.size(); ++i) {
    std::map<int, std::vector<double> > data;
    Rec2DNode *currentNode = _cur->_endNodes[i];
    //Msg::Info("%d -> %g", i+1, currentNode->getGlobQual());
    //int k = 0;
    if ( !((i+1) % ((int)std::sqrt(num)+1)) ) {
      dx = .0;
      dy -= 1.2;
    }
    else
      dx += 1.2;
    currentNode->draw(dx, dy);
    Rec2DData::clearChanges();
    /*while (currentNode && currentNode->getAction()) {
      //Msg::Info("%g", currentNode->getGlobQual());
      //data[currentNode->getNum()].push_back(currentNode->getGlobQual());
      if (currentNode->getAction()->haveElem())
        data[currentNode->getAction()->getNum(dx, dy)].push_back(++k);
      currentNode = currentNode->getFather();
    }*/
    //new PView("Jmin_bad", "ElementData", Recombine2D::getGFace()->model(), data);
  }
}

void Rec2DData::sortEndNode()
{
  std::sort(_cur->_endNodes.begin(),
            _cur->_endNodes.end(),
            gterRec2DNode()             );
}

/**  Rec2DChange  **/
/*******************/
#ifdef REC2D_RECO_BLOS
Rec2DChange::Rec2DChange(int d) : _info(NULL)
{
  _entity = new int(d);
  _type = ValQuad;
  Rec2DData::addQuadQual(d);
}
#endif

Rec2DChange::Rec2DChange(Rec2DEdge *re, bool toHide) : _entity(re), _info(NULL)
{
  if (toHide) {
    re->hide();
    _type = HideEdge;
  }
  else
    _type = CreatedEdge;
}

Rec2DChange::Rec2DChange(Rec2DVertex *rv, bool toHide) : _entity(rv), _info(NULL)
{
  if (toHide) {
    rv->hide();
    _type = HideVertex;
  }
  else
    _type = CreatedVertex;
}

Rec2DChange::Rec2DChange(Rec2DElement *rel, bool toHide) : _entity(rel), _info(NULL)
{
  if (toHide) {
    rel->hide();
    _type = HideElement;
  }
  else
    _type = CreatedElement;
}

Rec2DChange::Rec2DChange(Rec2DAction *ra, bool toHide) : _entity(ra), _info(NULL)
{
  if (toHide) {
    ra->hide();
    _type = HideAction;
  }
  else {
    static int a = -1;
    if (++a < 1) Msg::Warning("FIXME peut pas faire sans pointing ?");
    _type = CreatedAction;
  }
}

Rec2DChange::Rec2DChange(const std::vector<Rec2DAction*> &actions, bool toHide) : _info(NULL)
{
  std::vector<Rec2DAction*> *vect = new std::vector<Rec2DAction*>();
  *vect = actions;
  _entity = vect;
  if (toHide) {
    for (unsigned int i = 0; i < actions.size(); ++i)
      actions[i]->hide();
    _type = HideActions;
  }
  else
    _type = CreatedActions;
}

Rec2DChange::Rec2DChange(Rec2DVertex *rv, int newPar, Rec2DChangeType type)
{
  if (type == ChangePar) {
    _type = ChangePar;
    _entity = rv;
    int *oldPar = new int;
    *oldPar = rv->getParity();
    _info = oldPar;
    rv->setParity(newPar);
    return;
  }
  _type = Error;
  _entity = _info = NULL;
}

Rec2DChange::Rec2DChange(const std::vector<Rec2DVertex*> &verts,
                         Rec2DChangeType type                   )
{
  if (type == SavePar) {
    _type = SavePar;
    std::vector<Rec2DVertex*> *vect = new std::vector<Rec2DVertex*>();
    *vect = verts;
    _entity = vect;
    std::vector<int> *parities = new std::vector<int>(verts.size());
    _info = parities;
    for (unsigned int i = 0; i < verts.size(); ++i)
      (*parities)[i] = verts[i]->getParity();
    return;
  }
  _type = Error;
  _entity = _info = NULL;
}

Rec2DChange::Rec2DChange(Rec2DVertex *rv, SPoint2 newCoord)
: _type(Relocate), _entity(rv)
{
  SPoint2 *oldCoord = new SPoint2();
  rv->getParam(oldCoord);
  _info = oldCoord;
  rv->relocate(newCoord);
}

Rec2DChange::Rec2DChange(Rec2DVertex *rv,
                         const std::vector<Rec2DElement*> &elem,
                         Rec2DChangeType type                   )
{
  std::vector<Rec2DElement*> *vect = new std::vector<Rec2DElement*>();
  _type = type;
  _entity = rv;
  _info = vect;
  switch (type) {
    case AddElem :
      *vect = elem;
      for (unsigned int i = 0; i < elem.size(); ++i)
        rv->add(elem[i]);
      break;
      
    case RemoveElem :
      *vect = elem;
      for (int i = (int)elem.size()-1; i > -1; --i)
        rv->rmv(elem[i]);
      break;
      
    default :
      delete vect;
      _type = Error;
      _entity = _info = NULL;
  }
}

Rec2DChange::Rec2DChange(Rec2DVertex *rv0, Rec2DVertex *rv1,
                         const std::vector<Rec2DEdge*> &edges,
                         Rec2DChangeType type                 )
{
  if (type == SwapVertInEdge) {
    _type = type;
    std::vector<Rec2DEdge*> *vect = new std::vector<Rec2DEdge*>();
    *vect = edges;
    _entity = vect;
    std::pair<Rec2DVertex*, Rec2DVertex*> *pairVert;
    pairVert = new std::pair<Rec2DVertex*, Rec2DVertex*>(rv1, rv0);
    _info = pairVert;
    for (unsigned int i = 0; i < edges.size(); ++i)
      edges[i]->swap(rv0, rv1);
    return;
  }
  _type = Error;
  _entity = _info = NULL;
}

Rec2DChange::Rec2DChange(Rec2DVertex *rv0, Rec2DVertex *rv1,
                         const std::vector<Rec2DAction*> &actions,
                         Rec2DChangeType type                     )
{
  if (type == SwapVertInAction) {
    _type = type;
    std::vector<Rec2DAction*> *vect = new std::vector<Rec2DAction*>();
    *vect = actions;
    _entity = vect;
    std::pair<Rec2DVertex*, Rec2DVertex*> *pairVert;
    pairVert = new std::pair<Rec2DVertex*, Rec2DVertex*>(rv1, rv0);
    _info = pairVert;
    for (unsigned int i = 0; i < actions.size(); ++i)
      actions[i]->swap(rv0, rv1);
    return;
  }
  _type = Error;
  _entity = _info = NULL;
}

Rec2DChange::Rec2DChange(Rec2DVertex *rv0, Rec2DVertex *rv1,
                         const std::vector<Rec2DElement*> &elem,
                         Rec2DChangeType type                   )
{
  if (type == SwapMVertInElement) {
    _type = type;
    std::vector<Rec2DElement*> *vect = new std::vector<Rec2DElement*>();
    *vect = elem;
    _entity = vect;
    std::pair<Rec2DVertex*, Rec2DVertex*> *pairVert;
    pairVert = new std::pair<Rec2DVertex*, Rec2DVertex*>(rv1, rv0);
    _info = pairVert;
    for (unsigned int i = 0; i < elem.size(); ++i)
      elem[i]->swapMVertex(rv0, rv1);
    return;
  }
  _type = Error;
  _entity = _info = NULL;
}

Rec2DChange::Rec2DChange(Rec2DEdge *re0, Rec2DEdge *re1,
                         const std::vector<Rec2DAction*> &actions,
                         Rec2DChangeType type                     )
{
  if (type == SwapEdgeInAction) {
    _type = type;
    std::vector<Rec2DAction*> *vect = new std::vector<Rec2DAction*>();
    *vect = actions;
    _entity = vect;
    std::pair<Rec2DEdge*, Rec2DEdge*> *pairVert;
    pairVert = new std::pair<Rec2DEdge*, Rec2DEdge*>(re1, re0);
    _info = pairVert;
    for (unsigned int i = 0; i < actions.size(); ++i)
      actions[i]->swap(re0, re1);
    return;
  }
  _type = Error;
  _entity = _info = NULL;
}

Rec2DChange::Rec2DChange(Rec2DEdge *re0, Rec2DEdge *re1,
                         Rec2DChangeType type           )
{
  if (type == SwapEdgeInElem) {
    _type = type;
    Rec2DElement *rel = Rec2DEdge::getTheOnlyElement(re0);
    if (!rel) {
      Msg::Error("[Rec2DDataChange] invalid swapping edges");
      _type = Error;
      _entity = _info = NULL;
      return;
    }
    _entity = rel;
    std::pair<Rec2DEdge*, Rec2DEdge*> *pairEdge;
    pairEdge = new std::pair<Rec2DEdge*, Rec2DEdge*>(re1, re0);
    _info = pairEdge;
    rel->swap(re0, re1);
    return;
  }
  _type = Error;
  _entity = _info = NULL;
}

void Rec2DChange::revert()
{
  switch (_type) {
#ifdef REC2D_RECO_BLOS
    case ValQuad :
      Rec2DData::addQuadQual(-*(int*)_entity);
      break;
#endif
      
    case HideEdge :
      ((Rec2DEdge*)_entity)->reveal();
      break;
      
    case HideVertex :
      ((Rec2DVertex*)_entity)->reveal();
      break;
      
    case HideElement :
      ((Rec2DElement*)_entity)->reveal();
      break;
      
    case CreatedEdge :
      delete (Rec2DEdge*)_entity;
      break;
      
    case CreatedVertex :
      delete (Rec2DVertex*)_entity;
      break;
      
    case CreatedElement :
      delete (Rec2DElement*)_entity;
      break;
      
    case HideAction :
      ((Rec2DAction*)_entity)->reveal();
      break;
      
    case CreatedAction :
      delete (Rec2DAction*)_entity;
      break;
      
    case HideActions :
      {
        std::vector<Rec2DAction*> *vect = (std::vector<Rec2DAction*>*)_entity;
        for (unsigned int i = 0; i < vect->size(); ++i)
          (*vect)[i]->reveal();
        delete vect;
      }
      break;
      
    case CreatedActions :
      {
        std::vector<Rec2DAction*> *vect = (std::vector<Rec2DAction*>*)_entity;
        for (unsigned int i = 0; i < vect->size(); ++i)
          delete (*vect)[i];
        delete vect;
      }
      break;
      
    case SwapEdgeInAction :
      {
        std::vector<Rec2DAction*> *vect = (std::vector<Rec2DAction*>*)_entity;
        std::pair<Rec2DEdge*, Rec2DEdge*> *pairEdge;
        pairEdge = (std::pair<Rec2DEdge*, Rec2DEdge*>*)_info;
        for (unsigned int i = 0; i < vect->size(); ++i)
          (*vect)[i]->swap(pairEdge->first, pairEdge->second);
        delete vect;
        delete pairEdge;
      }
      break;
      
    case SwapEdgeInElem :
      {
        std::pair<Rec2DEdge*, Rec2DEdge*> *pairEdge;
        pairEdge = (std::pair<Rec2DEdge*, Rec2DEdge*>*)_info;
        ((Rec2DElement*)_entity)->swap(pairEdge->first, pairEdge->second);
        delete pairEdge;
      }
      break;
      
    case SwapVertInAction :
      {
        std::vector<Rec2DAction*> *vect = (std::vector<Rec2DAction*>*)_entity;
        std::pair<Rec2DVertex*, Rec2DVertex*> *pairVert;
        pairVert = (std::pair<Rec2DVertex*, Rec2DVertex*>*)_info;
        for (unsigned int i = 0; i < vect->size(); ++i)
          (*vect)[i]->swap(pairVert->first, pairVert->second);
        delete vect;
        delete pairVert;
      }
      break;
      
    case SwapVertInEdge :
      {
        std::vector<Rec2DEdge*> *edges = (std::vector<Rec2DEdge*>*)_entity;
        std::pair<Rec2DVertex*, Rec2DVertex*> *pairVert;
        pairVert = (std::pair<Rec2DVertex*, Rec2DVertex*>*)_info;
        for (unsigned int i = 0; i < edges->size(); ++i)
          (*edges)[i]->swap(pairVert->first, pairVert->second);
        delete edges;
        delete pairVert;
      }
      break;
      
    case SwapMVertInElement :
      {
        std::vector<Rec2DElement*> *elem = (std::vector<Rec2DElement*>*)_entity;
        std::pair<Rec2DVertex*, Rec2DVertex*> *pairVert;
        pairVert = (std::pair<Rec2DVertex*, Rec2DVertex*>*)_info;
        for (unsigned int i = 0; i < elem->size(); ++i)
          (*elem)[i]->swapMVertex(pairVert->first, pairVert->second);
        delete elem;
        delete pairVert;
      }
      break;
      
    case RemoveElem :
      {
        std::vector<Rec2DElement*> *elem = (std::vector<Rec2DElement*>*)_info;
        for (unsigned int i = 0; i < elem->size(); ++i)
          ((Rec2DVertex*)_entity)->add((*elem)[i]);
        delete elem;
      }
      break;
      
    case AddElem :
      {
        std::vector<Rec2DElement*> *elem = (std::vector<Rec2DElement*>*)_info;
        for (unsigned int i = 0; i < elem->size(); ++i)
          ((Rec2DVertex*)_entity)->rmv((*elem)[i]);
        delete elem;
      }
      break;
      
    case Relocate :
      ((Rec2DVertex*)_entity)->relocate(*(SPoint2*)_info);
      delete (SPoint2*)_info;
      break;
      
    case ChangePar :
      ((Rec2DVertex*)_entity)->setParity(*(int*)_info);
      delete (int*)_info;
      break;
      
    case SavePar :
      {
        std::vector<Rec2DVertex*> *verts = (std::vector<Rec2DVertex*>*)_entity;
        std::vector<int> *parities = (std::vector<int>*)_info;
        for (unsigned int i = 0; i < verts->size(); ++i)
          (*verts)[i]->setParity((*parities)[i]);
        delete verts;
        delete parities;
      }
      break;
      
    case Error :
      Msg::Error("[Rec2DChange] There was an error");
      return;
      
    case Reverted :
      Msg::Error("[Rec2DChange] Multiple revert");
      return;
      
    default :
      Msg::Error("[Rec2DChange] Unknown type (%d)", _type);
      return;
  }
  _type = Reverted;
}


/**  Rec2DDataChange  **/
/***********************/
void Rec2DDataChange::checkObsoleteActions(Rec2DVertex *const*verts, int size)
{
  std::vector<Rec2DAction*> actions;
  for (int i = 0; i < size; ++i) {
    verts[i]->getMoreUniqueActions(actions);
  }
  for (unsigned int i = 0; i < actions.size(); ++i) {
    if (actions[i]->isObsolete())
      hide(actions[i]);
  }
}

Rec2DDataChange::~Rec2DDataChange()
{
  for (unsigned int i = 0; i < _changes.size(); ++i)
    delete _changes[i];
}

void Rec2DDataChange::swapFor(Rec2DEdge *re0, Rec2DEdge *re1)
{
  std::vector<Rec2DAction*> actions;
  Rec2DElement *elem[2];
  Rec2DEdge::getElements(re0, elem);
  if (elem[0]) elem[0]->getMoreUniqueActions(actions);
  if (elem[1]) elem[1]->getMoreUniqueActions(actions);
  Rec2DAction::removeDuplicate(actions);
  _changes.push_back(new Rec2DChange(re0, re1, actions, SwapEdgeInAction));
  _changes.push_back(new Rec2DChange(re0, re1, SwapEdgeInElem));
}

void Rec2DDataChange::swapFor(Rec2DVertex *rv0, Rec2DVertex *rv1)
{
  std::vector<Rec2DElement*> elem;
  std::vector<Rec2DEdge*> edges;
  std::vector<Rec2DAction*> actions;
  rv0->getElements(elem);
  rv0->getEdges(edges);
  for (unsigned int i = 0; i < elem.size(); ++i)
    elem[i]->getMoreUniqueActions(actions);
  Rec2DAction::removeDuplicate(actions);
  _changes.push_back(new Rec2DChange(rv0, elem, RemoveElem));
  _changes.push_back(new Rec2DChange(rv0, rv1, edges, SwapVertInEdge));
  _changes.push_back(new Rec2DChange(rv0, rv1, actions, SwapVertInAction));
  _changes.push_back(new Rec2DChange(rv1, elem, AddElem));
  _changes.push_back(new Rec2DChange(rv0, rv1, elem, SwapMVertInElement));
}

void Rec2DDataChange::revert()
{
  for (int i = (int)_changes.size() - 1; i > -1; --i)
    _changes[i]->revert();
}


/**  Rec2DAction  **/
/*******************/
bool lessRec2DAction::operator()(const Rec2DAction *ra1, const Rec2DAction *ra2) const
{
  return *ra1 < *ra2;
}

bool gterRec2DAction::operator()(const Rec2DAction *ra1, const Rec2DAction *ra2) const
{
  return *ra2 < *ra1;
}

Rec2DAction::Rec2DAction()
: _globQualIfExecuted(.0), _lastUpdate(-2), _numPointing(0), _dataAction(NULL)
{
}

bool Rec2DAction::operator<(const Rec2DAction &other) const
{
  return getReward() < other.getReward();
}

void Rec2DAction::removeDuplicate(std::vector<Rec2DAction*> &actions)
{
  unsigned int i = -1;
  while (++i < actions.size()) {
    Rec2DAction *ra = actions[i]->getBase();
    if (ra) for (unsigned int j = 0; j < actions.size(); ++j) {
      if (ra == actions[j]) {
        actions[i] = actions.back();
        actions.pop_back();
        --i;
        break;
      }
    }
  }
}

double Rec2DAction::getReward() const
{
  if (_lastUpdate < Recombine2D::getNumChange())
    ((Rec2DAction*)this)->_computeGlobQual();
  
  return _globQualIfExecuted/* - Rec2DData::getGlobalQuality()*/;
}

double Rec2DAction::getRealReward() const
{
  if (_lastUpdate < Recombine2D::getNumChange())
    ((Rec2DAction*)this)->_computeReward();
  
  return _reward;
}


/**  Rec2DTwoTri2Quad  **/
/************************/
/*(2)---0---(0)
 * | {0}   / |
 * 1    4    3
 * | /   {1} |
 *(1)---2---(3)
 */
Rec2DTwoTri2Quad::Rec2DTwoTri2Quad(Rec2DElement *el0, Rec2DElement *el1)
{
  _triangles[0] = el0;
  _triangles[1] = el1;
  _edges[4] = Rec2DElement::getCommonEdge(el0, el1);
  
  std::vector<Rec2DEdge*> edges;
  el0->getMoreEdges(edges);
  el1->getMoreEdges(edges);
  int k = -1;
  for (unsigned int i = 0; i < edges.size(); ++i) {
    if (edges[i] != _edges[4])
      _edges[++k] = edges[i];
  }
  if (k > 3)
    Msg::Error("[Rec2DTwoTri2Quad] too much edges");
  // reoder edges if needed
  if (edges[1] == _edges[4]) {
    Rec2DEdge *re = _edges[0];
    _edges[0] = _edges[1];
    _edges[1] = re;
  }
  if (edges[4] == _edges[4]) {
    Rec2DEdge *re = _edges[2];
    _edges[2] = _edges[3];
    _edges[3] = re;
  }
  
  _vertices[0] = _edges[4]->getVertex(0);
  _vertices[1] = _edges[4]->getVertex(1);
  _vertices[2] = _triangles[0]->getOtherVertex(_vertices[0], _vertices[1]);
  _vertices[3] = _triangles[1]->getOtherVertex(_vertices[0], _vertices[1]);
  if (_vertices[0] == _edges[1]->getOtherVertex(_vertices[2])) {
    Rec2DVertex *rv = _vertices[0];
    _vertices[0] = _vertices[1];
    _vertices[1] = rv;
  }
  
  _triangles[0]->add(this);
  _triangles[1]->add(this);
  Rec2DData::add(this);
#ifdef REC2D_RECO_BLOS
  _rt = new RecombineTriangle(MEdge(_vertices[0]->getMVertex(),
                                    _vertices[1]->getMVertex() ),
                              _triangles[0]->getMElement(),
                              _triangles[1]->getMElement()       );
#endif
  
  if (!edgesInOrder(_edges, 4)) Msg::Error("recomb |%d|%d|", _triangles[0]->getNum(), _triangles[1]->getNum());
}

void Rec2DTwoTri2Quad::operator delete(void *p)
{
  if (!p) return;
  Rec2DTwoTri2Quad *ra = (Rec2DTwoTri2Quad*)p;
  if (ra->_dataAction) { // ra->hide()
    ra->_triangles[0]->rmv(ra);
    ra->_triangles[1]->rmv(ra);
    Rec2DData::rmv(ra);
  }
  if (!ra->_numPointing) {
    //Msg::Info("del ac %d", p);
    if (ra->_col) {
      static int a = -1;
      if (++a < 1) Msg::Warning("FIXME Il faut supprimer les hidden � la fin...");
      Rec2DData::addHidden((Rec2DAction*)p);
    }
    else
      free(p);
  }
}

void Rec2DTwoTri2Quad::hide()
{
  if (_triangles[0])
    _triangles[0]->rmv(this);
  if (_triangles[1])
    _triangles[1]->rmv(this);
  Rec2DData::rmv(this);
}

void Rec2DTwoTri2Quad::reveal()
{ 
  if (_triangles[0])
    _triangles[0]->add(this);
  if (_triangles[1])
    _triangles[1]->add(this);
  Rec2DData::add(this);
}

bool Rec2DTwoTri2Quad::checkCoherence(const Rec2DAction *action) const
{
  Rec2DEdge *edge4 = Rec2DElement::getCommonEdge(_triangles[0], _triangles[1]);
  if (!edge4) {
    if (!_triangles[0]->has(_edges[4]) || !_triangles[1]->has(_edges[4])) {
      Msg::Error("inco action [-1], |%d|%d|", _triangles[0]->getNum(), _triangles[1]->getNum());
      return false;
    }
  }
  else if (_edges[4] != edge4) {
    Msg::Error("inco action [0], |%d|%d|", _triangles[0]->getNum(), _triangles[1]->getNum());
    if (_edges[4])
      _edges[4]->print();
    else Msg::Info("no edge");
    if (Rec2DElement::getCommonEdge(_triangles[0], _triangles[1]))
      Rec2DElement::getCommonEdge(_triangles[0], _triangles[1])->print();
    else Msg::Info("no edge");
    return false;
  }
  
  std::vector<Rec2DEdge*> edges;
  Rec2DEdge *re[4];
  _triangles[0]->getMoreEdges(edges);
  _triangles[1]->getMoreEdges(edges);
  int k = -1;
  for (unsigned int i = 0; i < edges.size(); ++i) {
    if (edges[i] != _edges[4])
      re[++k] = edges[i];
  }
  if (k > 3)
    Msg::Error("[Rec2DTwoTri2Quad] too much edges");
  // reoder edges if needed
  if (edges[1] == _edges[4]) {
    Rec2DEdge *e = re[0];
    re[0] = re[1];
    re[1] = e;
  }
  if (edges[4] == _edges[4]) {
    Rec2DEdge *e = re[2];
    re[2] = re[3];
    re[3] = e;
  }
  for (int i = 0; i < 4; ++i) {
    if (re[i] != _edges[i]) {
      Msg::Error("inco action [1], |%d|%d|", _triangles[0]->getNum(), _triangles[1]->getNum());
      for (int i = 0; i < 4; ++i) {
        _edges[i]->print();
        re[i]->print();
        Msg::Info(" ");
      }
      return false;
    }
  }
  
  if (_edges[0]->getOtherVertex(_vertices[2]) == _edges[4]->getVertex(0)) {
    if (_vertices[0] != _edges[4]->getVertex(0) || _vertices[1] != _edges[4]->getVertex(1)) {
      Msg::Error("inco action [2], |%d|%d|", _triangles[0]->getNum(), _triangles[1]->getNum());
      return false;
    }
  }
  else {
    if (_vertices[0] != _edges[4]->getVertex(1) || _vertices[1] != _edges[4]->getVertex(0)) {
      Msg::Error("inco action [3], |%d|%d|", _triangles[0]->getNum(), _triangles[1]->getNum());
      return false;
    }
  }
  if (_vertices[2] != _triangles[0]->getOtherVertex(_vertices[0], _vertices[1])) {
    Msg::Error("inco action [4], |%d|%d|", _triangles[0]->getNum(), _triangles[1]->getNum());
    return false;
  }
  if (_vertices[3] != _triangles[1]->getOtherVertex(_vertices[0], _vertices[1])) {
    Msg::Error("inco action [5], |%d|%d|", _triangles[0]->getNum(), _triangles[1]->getNum());
    return false;
  }
  
  const Rec2DAction *ra;
  if (action) ra = action;
  else ra = this;
  if (!_triangles[0]->has(ra) || !_triangles[1]->has(ra) || _triangles[0] == _triangles[1]) {
    Msg::Error("inco action [6], |%d|%d|", _triangles[0]->getNum(), _triangles[1]->getNum());
    return false;
  }
  
  if (isObsolete()) {
    int p[4];
    p[0] = _vertices[0]->getParity();
    p[1] = _vertices[1]->getParity();
    p[2] = _vertices[2]->getParity();
    p[3] = _vertices[3]->getParity();
    Msg::Info("%d %d %d %d", p[0], p[1], p[2], p[3]);
    Msg::Error("inco action [7], |%d|%d|", _triangles[0]->getNum(), _triangles[1]->getNum());
    return false;
  }
  
  return true;
}

void Rec2DTwoTri2Quad::printIdentity() const
{
  Msg::Info("Recombine |%d|%d|", _triangles[0]->getNum(), _triangles[1]->getNum());
}

void Rec2DTwoTri2Quad::printVertices() const
{
  for (int i = 0; i < 4; ++i) {
    Msg::Info("vert %d : %g", _vertices[i]->getNum(), _vertices[i]->getQual());
    //_vertices[i]->printQual();
  }
}

bool Rec2DTwoTri2Quad::has(const Rec2DElement *rel) const
{
  return rel == _triangles[0] || rel == _triangles[1];
}

//void Rec2DTwoTri2Quad::print()
//{
//  Msg::Info("Printing Action %d |%d|%d|...", this, _triangles[0]->getNum(), _triangles[1]->getNum());
//  Msg::Info("edge0 %g (%g, %g)", _edges[0]->getQual(), _edges[0]->getQualL(), _edges[0]->getQualO());
//  Msg::Info("edge1 %g (%g, %g)", _edges[1]->getQual(), _edges[1]->getQualL(), _edges[1]->getQualO());
//  Msg::Info("edge2 %g (%g, %g)", _edges[2]->getQual(), _edges[2]->getQualL(), _edges[2]->getQualO());
//  Msg::Info("edge3 %g (%g, %g)", _edges[3]->getQual(), _edges[3]->getQualL(), _edges[3]->getQualO());
//  Msg::Info("edge4 %g (%g, %g)", _edges[4]->getQual(), _edges[4]->getQualL(), _edges[4]->getQualO());
//  Msg::Info("angles %g - %g", _vertices[0]->getAngle(), _vertices[1]->getAngle());
//  Msg::Info("merge0 %g", _vertices[0]->getGainRecomb(_triangles[0], _triangles[1]));
//  Msg::Info("merge1 %g", _vertices[1]->getGainRecomb(_triangles[0], _triangles[1]));
//  //_vertices[0]->printGainMerge(_triangles[0], _triangles[1]);
//  //_vertices[1]->printGainMerge(_triangles[0], _triangles[1]);
//}

void Rec2DTwoTri2Quad::printReward() const
{
#ifdef REC2D_VERT_ONLY
  Msg::Info("reward rec : %g  %g  %g  %g",
            _vertices[0]->getGainRecomb(_triangles[0], _triangles[1], _edges[4], _edges[0], _edges[3]),
            _vertices[1]->getGainRecomb(_triangles[0], _triangles[1], _edges[4], _edges[1], _edges[2]),
            _vertices[2]->getGainQuad(_triangles[0], _edges[0], _edges[1]),
            _vertices[3]->getGainQuad(_triangles[1], _edges[2], _edges[3]));
#else
  double valEdge1 = _edges[4]->getQual();
  double valEdge2 = 0;
  for (int i = 0; i < 4; ++i)
    valEdge2 += _edges[i]->getQual();
  
  double valVert;
  valVert = _vertices[0]->getGainRecomb(_triangles[0], _triangles[1]);
  valVert += _vertices[1]->getGainRecomb(_triangles[0], _triangles[1]);
  Msg::Info("-%de%g +%de%g +0v%g -> %g", REC2D_EDGE_BASE, valEdge1, 4 * REC2D_EDGE_QUAD,
            valEdge2/4, valVert/2,
            Rec2DData::getGlobalQuality(0, valVert,
                          4*REC2D_EDGE_QUAD - REC2D_EDGE_BASE,
                          -REC2D_EDGE_BASE*valEdge1+REC2D_EDGE_QUAD*valEdge2));
#endif
}

void Rec2DTwoTri2Quad::_computeGlobQual()
{
#ifdef REC2D_RECO_BLOS
  _globQualIfExecuted = Rec2DData::getGlobalQuality() + (int)_rt->total_gain;
#else
#ifdef REC2D_VERT_ONLY
  _valVert = .0;
  _valVert += _vertices[0]->getGainRecomb(_triangles[0], _triangles[1],
                                          _edges[4], _edges[0], _edges[3]);
  _valVert += _vertices[1]->getGainRecomb(_triangles[0], _triangles[1],
                                          _edges[4], _edges[1], _edges[2]);
  _valVert += _vertices[2]->getGainQuad(_triangles[0], _edges[0], _edges[1]);
  _valVert += _vertices[3]->getGainQuad(_triangles[1], _edges[2], _edges[3]);
  
  _globQualIfExecuted = Rec2DData::getGlobalQuality(0, _valVert);
#else
  double valEdge = -REC2D_EDGE_BASE * _edges[4]->getQual();
  for (int i = 0; i < 4; ++i)
    valEdge += REC2D_EDGE_QUAD * _edges[i]->getQual();
  
  if (_vertices[0]->getLastUpdate() > _lastUpdate ||
      _vertices[1]->getLastUpdate() > _lastUpdate   ) {
    _valVert = _vertices[0]->getGainRecomb(_triangles[0], _triangles[1]);
    _valVert += _vertices[1]->getGainRecomb(_triangles[0], _triangles[1]);
  }
  
  _globQualIfExecuted =
    Rec2DData::getGlobalQuality(0, _valVert,
                                4*REC2D_EDGE_QUAD - REC2D_EDGE_BASE, valEdge);
#endif
  _lastUpdate = Recombine2D::getNumChange();
#endif
}

void Rec2DTwoTri2Quad::_computeReward()
{
#ifdef REC2D_RECO_BLOS
  _reward = (int)_rt->total_gain;
#else
#ifdef REC2D_VERT_ONLY
  _valVert = .0;
  _valVert += _vertices[0]->getGainRecomb(_triangles[0], _triangles[1],
                                          _edges[4], _edges[0], _edges[3]);
  _valVert += _vertices[1]->getGainRecomb(_triangles[0], _triangles[1],
                                          _edges[4], _edges[1], _edges[2]);
  _valVert += _vertices[2]->getGainQuad(_triangles[0], _edges[0], _edges[1]);
  _valVert += _vertices[3]->getGainQuad(_triangles[1], _edges[2], _edges[3]);
  
  _reward = Rec2DData::getGlobalQuality(0, _valVert) -
            Rec2DData::getGlobalQuality()             ;
#else
  double valEdge = -REC2D_EDGE_BASE * _edges[4]->getQual();
  for (int i = 0; i < 4; ++i)
    valEdge += REC2D_EDGE_QUAD * _edges[i]->getQual();
  
  if (_vertices[0]->getLastUpdate() > _lastUpdate ||
      _vertices[1]->getLastUpdate() > _lastUpdate   ) {
    _valVert = _vertices[0]->getGainRecomb(_triangles[0], _triangles[1]);
    _valVert += _vertices[1]->getGainRecomb(_triangles[0], _triangles[1]);
  }
  
  _reward =
    Rec2DData::getGlobalQuality(0, _valVert,
                                4*REC2D_EDGE_QUAD - REC2D_EDGE_BASE, valEdge) -
    Rec2DData::getGlobalQuality()                                              ;
#endif
#endif
  _lastUpdate = Recombine2D::getNumChange();
}

void Rec2DTwoTri2Quad::color(int a, int b, int c) const
{
#ifdef REC2D_DRAW
  unsigned int col = CTX::instance()->packColor(a, b, c, 255);
  _triangles[0]->getMElement()->setCol(col);
  _triangles[1]->getMElement()->setCol(col);
#endif
}

void Rec2DTwoTri2Quad::apply(std::vector<Rec2DVertex*> &newPar)
{
  static int a = -1;
  if (++a < 1) Msg::Error("FIXME Need new definition Rec2DTwoTri2Quad::apply(newPar)");
  /*if (isObsolete()) {
    Msg::Error("[Rec2DTwoTri2Quad] No way ! I won't apply ! Find someone else...");
    return;
  }
  
  int min = Rec2DData::getNewParity(), index = -1;
  for (int i = 0; i < 4; ++i) {
    if (_vertices[i]->getParity() && min > _vertices[i]->getParity()) {
      min = _vertices[i]->getParity();
      index = i;
    }
  }
  if (index == -1) {
    _vertices[0]->setParity(min);
    _vertices[1]->setParity(min);
    _vertices[2]->setParity(min+1);
    _vertices[3]->setParity(min+1);
  }
  else {
    for (int i = 0; i < 4; i += 2) {
      int par;
      if ((index/2) * 2 == i)
        par = min;
      else
        par = otherParity(min);
      for (int j = 0; j < 2; ++j) {
        if (!_vertices[i+j]->getParity())
          _vertices[i+j]->setParity(par);
        else if (_vertices[i+j]->getParity() != par &&
                 _vertices[i+j]->getParity() != otherParity(par))
          Rec2DData::associateParity(_vertices[i+j]->getParity(), par);
      }
    }
  }
  
  _triangles[0]->rmv(this);
  _triangles[1]->rmv(this);
  // hide() instead
  
  std::vector<Rec2DAction*> actions;
  _triangles[0]->getMoreUniqueActions(actions);
  _triangles[1]->getMoreUniqueActions(actions);
  for (unsigned int i = 0; i < actions.size(); ++i)
    delete actions[i];
  
  delete _triangles[0];
  delete _triangles[1];
  _triangles[0] = NULL;
  _triangles[1] = NULL;
  delete _edges[4];
  
  new Rec2DElement((MQuadrangle*)NULL, (const Rec2DEdge**)_edges);
  
  Recombine2D::incNumChange();*/
}

void Rec2DTwoTri2Quad::apply(Rec2DDataChange *rdc,
                             std::vector<Rec2DAction*> *&createdActions,
                             bool color) const
{
  //Msg::Info("applying Recombine |%d|%d|", _triangles[0]->getNum(), _triangles[1]->getNum());
  if (isObsolete()) {
    printIdentity();
    int p[4];
    p[0] = _vertices[0]->getParity();
    p[1] = _vertices[1]->getParity();
    p[2] = _vertices[2]->getParity();
    p[3] = _vertices[3]->getParity();
    Msg::Info("%d %d %d %d", p[0], p[1], p[2], p[3]);
    Msg::Error("[Rec2DTwoTri2Quad] I was obsolete !");
    Msg::Error("check entities = %d", Rec2DData::checkEntities());
    __crash();
  }
#ifdef REC2D_DRAW
  rdc->setAction(this);
#endif
  std::vector<Rec2DAction*> actions;
  _triangles[0]->getMoreUniqueActions(actions);
  _triangles[1]->getMoreUniqueActions(actions);
  rdc->hide(actions);
  _doWhatYouHaveToDoWithParity(rdc);
  rdc->hide(_triangles[0]);
  rdc->hide(_triangles[1]);
  rdc->hide(_edges[4]);
  Rec2DElement *rel = new Rec2DElement((MQuadrangle*)NULL, (const Rec2DEdge**)_edges);
  rdc->append(rel);
  //rdc->append(new Rec2DElement((MQuadrangle*)NULL, (const Rec2DEdge**)_edges));
#ifdef REC2D_RECO_BLOS
  rdc->add((int)_rt->total_gain);
  if (color) Recombine2D::colorFromBlossom(_triangles[0], _triangles[1], rel);
  static int a = -1;
  if (++a < 1) Msg::Warning("FIXME reward is int for blossom");
#endif
}

void Rec2DTwoTri2Quad::_doWhatYouHaveToDoWithParity(Rec2DDataChange *rdc) const
{
  int parMin = Rec2DData::getNewParity(), index = -1;
  for (int i = 0; i < 4; ++i) {
    if (_vertices[i]->getParity() && parMin > _vertices[i]->getParity()) {
      parMin = _vertices[i]->getParity();
      index = i;
    }
  }
  if (index == -1) {
    rdc->changeParity(_vertices[0], parMin);
    rdc->changeParity(_vertices[1], parMin);
    rdc->changeParity(_vertices[2], parMin+1);
    rdc->changeParity(_vertices[3], parMin+1);
  }
  else for (int i = 0; i < 4; i += 2) {
    int par;
    if ((index/2) * 2 == i)
      par = parMin;
    else
      par = otherParity(parMin);
    for (int j = 0; j < 2; ++j) {
      if (!_vertices[i+j]->getParity())
        rdc->changeParity(_vertices[i+j], par);
      else if (_vertices[i+j]->getParity() != par)
        Rec2DData::associateParity(_vertices[i+j]->getParity(), par, rdc);
    }
  }
  rdc->checkObsoleteActions(_vertices, 4);
}

bool Rec2DTwoTri2Quad::isObsolete() const
{
  int p[4];
  p[0] = _vertices[0]->getParity();
  p[1] = _vertices[1]->getParity();
  p[2] = _vertices[2]->getParity();
  p[3] = _vertices[3]->getParity();
  static int a = -1;
  if (++a < 1) Msg::Warning("p[0]/2 == p[1]/2 && p[0] != p[1]  || ...  ???");
  return (p[0]/2 == p[1]/2 && p[0] != p[1] ||
          p[2]/2 == p[3]/2 && p[2] != p[3] ||
          p[0] && (p[0] == p[2] || p[0] == p[3]) ||
          p[1] && (p[1] == p[2] || p[1] == p[3])   );
}

void Rec2DTwoTri2Quad::getElements(std::vector<Rec2DElement*> &elem) const
{
  elem.clear();
  elem.push_back(_triangles[0]);
  elem.push_back(_triangles[1]);
}

void Rec2DTwoTri2Quad::getNeighbourElements(std::vector<Rec2DElement*> &elem) const
{
  elem.clear();
  _triangles[0]->getMoreNeighbours(elem);
  _triangles[1]->getMoreNeighbours(elem);
  unsigned int i = 0;
  while (i < elem.size()) {
    if (elem[i] == _triangles[0] || elem[i] == _triangles[1]) {
      elem[i] = elem.back();
      elem.pop_back();
    }
    else ++i;
  }
}

void Rec2DTwoTri2Quad::getNeighbElemWithActions(std::vector<Rec2DElement*> &elem) const
{
  getNeighbourElements(elem);
  unsigned int i = 0;
  while (i < elem.size()) {
    if (!elem[i]->getNumActions()) {
      elem[i] = elem.back();
      elem.pop_back();
    }
    else ++i;
  }
}

int Rec2DTwoTri2Quad::getNum(double shiftx, double shifty)
{
  MQuadrangle *quad;
  if (shiftx == .0 && shifty == .0)
    quad = new MQuadrangle(_vertices[0]->getMVertex(),
                           _vertices[2]->getMVertex(),
                           _vertices[1]->getMVertex(),
                           _vertices[3]->getMVertex());
  else {
    MVertex *v0 = new MVertex(_vertices[0]->getMVertex()->x() + shiftx,
                              _vertices[0]->getMVertex()->y() + shifty,
                              _vertices[0]->getMVertex()->z()          );
    MVertex *v1 = new MVertex(_vertices[1]->getMVertex()->x() + shiftx,
                              _vertices[1]->getMVertex()->y() + shifty,
                              _vertices[1]->getMVertex()->z()          );
    MVertex *v2 = new MVertex(_vertices[2]->getMVertex()->x() + shiftx,
                              _vertices[2]->getMVertex()->y() + shifty,
                              _vertices[2]->getMVertex()->z()          );
    MVertex *v3 = new MVertex(_vertices[3]->getMVertex()->x() + shiftx,
                              _vertices[3]->getMVertex()->y() + shifty,
                              _vertices[3]->getMVertex()->z()          );
    quad = new MQuadrangle(v0, v2, v1, v3);
  }
  Recombine2D::add(quad);
  return quad->getNum();
}

MElement* Rec2DTwoTri2Quad::createMElement(double shiftx, double shifty)
{
  MQuadrangle *quad;
  if (shiftx == .0 && shifty == .0)
    quad = new MQuadrangle(_vertices[0]->getMVertex(),
                           _vertices[2]->getMVertex(),
                           _vertices[1]->getMVertex(),
                           _vertices[3]->getMVertex());
  else {
    MVertex *v0 = new MVertex(_vertices[0]->getMVertex()->x() + shiftx,
                              _vertices[0]->getMVertex()->y() + shifty,
                              _vertices[0]->getMVertex()->z()          );
    MVertex *v1 = new MVertex(_vertices[1]->getMVertex()->x() + shiftx,
                              _vertices[1]->getMVertex()->y() + shifty,
                              _vertices[1]->getMVertex()->z()          );
    MVertex *v2 = new MVertex(_vertices[2]->getMVertex()->x() + shiftx,
                              _vertices[2]->getMVertex()->y() + shifty,
                              _vertices[2]->getMVertex()->z()          );
    MVertex *v3 = new MVertex(_vertices[3]->getMVertex()->x() + shiftx,
                              _vertices[3]->getMVertex()->y() + shifty,
                              _vertices[3]->getMVertex()->z()          );
    quad = new MQuadrangle(v0, v2, v1, v3);
  }
  Recombine2D::add(quad);
  return quad;
}

void Rec2DTwoTri2Quad::swap(Rec2DVertex *rv0, Rec2DVertex *rv1)
{
  for (int i = 0; i < 4; ++i) {
    if (_vertices[i] == rv0) {
      _vertices[i] = rv1;
      return;
    }
  }
  Msg::Error("[Rec2DTwoTri2Quad] Can't swap your vertex (%d -> %d)", rv0->getNum(), rv1->getNum());
  Msg::Warning("[Rec2DTwoTri2Quad] I have %d %d %d %d", _vertices[0]->getNum(), _vertices[1]->getNum(), _vertices[2]->getNum(), _vertices[3]->getNum());
}

void Rec2DTwoTri2Quad::swap(Rec2DEdge *re0, Rec2DEdge *re1)
{
  for (int i = 0; i < 4; ++i) {
    if (_edges[i] == re0) {
      _edges[i] = re1;
      return;
    }
  }
  Msg::Error("[Rec2DTwoTri2Quad] Can't swap your edge (is middle : %d)", re0 == _edges[4]);
}

Rec2DElement* Rec2DTwoTri2Quad::getRandomElement() const
{
  return _triangles[rand() % 2];
}


/**  Rec2DCollapse  **/
/*********************/
Rec2DCollapse::Rec2DCollapse(Rec2DTwoTri2Quad *rec) : _rec(rec)//,
//  _triangles(rec->_triangles), _edges(_rec->_edges), _vertices(_rec->_vertices)
{
  _rec->_col = this;
  _rec->_triangles[0]->add(this);
  _rec->_triangles[1]->add(this);
  Rec2DData::add(this);
}

void Rec2DCollapse::operator delete(void *p)
{
  if (!p) return;
  Rec2DCollapse *ra = (Rec2DCollapse*)p;
  if (ra->_dataAction) { // ra->hide()
    ra->_rec->_triangles[0]->rmv(ra);
    ra->_rec->_triangles[1]->rmv(ra);
    Rec2DData::rmv(ra);
  }
  if (!ra->_numPointing) {
    //Msg::Info("del ac %d", p);
    ra->_rec->_col = NULL;
    free(p);
  }
}

void Rec2DCollapse::hide()
{
  if (_rec->_triangles[0])
    _rec->_triangles[0]->rmv(this);
  if (_rec->_triangles[1])
    _rec->_triangles[1]->rmv(this);
  Rec2DData::rmv(this);
}

void Rec2DCollapse::reveal()
{
  if (_rec->_triangles[0])
    _rec->_triangles[0]->add(this);
  if (_rec->_triangles[1])
    _rec->_triangles[1]->add(this);
  Rec2DData::add(this);
}

void Rec2DCollapse::printReward() const
{
#ifdef REC2D_VERT_ONLY
  SPoint2 p[2];
  _rec->_vertices[0]->getParam(&p[0]);
  _rec->_vertices[1]->getParam(&p[1]);
  int b0 = _rec->_vertices[0]->getOnBoundary();
  int b1 = _rec->_vertices[1]->getOnBoundary();
  
  double valVert = .0;
  if (b0 || b1) {
    int iOK = 1, iKO = 0;
    if (b0) {
      iOK = 0;
      iKO = 1;
    }
    double oldValVert = Rec2DData::getSumVert();
    _rec->_vertices[iKO]->relocate(p[iOK]);
    double qualRelocation = Rec2DData::getSumVert() - oldValVert;
    
    valVert += _rec->_vertices[iOK]->getGainMerge(_rec->_vertices[iKO],
                                                  &_rec->_edges[1], 2  );
    valVert += _rec->_vertices[2]->getGainTriLess(_rec->_edges[1]);
    valVert += _rec->_vertices[3]->getGainTriLess(_rec->_edges[2]);
    
    for (int i = 0; i < 4; ++i) {
      Msg::Info("vert %d : %g", _rec->_vertices[i]->getNum(), _rec->_vertices[i]->getQual());
    }
    
    Msg::Info("qual col %g %g %g %g", qualRelocation,
              _rec->_vertices[iOK]->getGainMerge(_rec->_vertices[iKO], &_rec->_edges[1], 2),
              _rec->_vertices[2]->getGainTriLess(_rec->_edges[1]),
              _rec->_vertices[3]->getGainTriLess(_rec->_edges[2])
             );
    
    _rec->_vertices[iKO]->relocate(p[iKO]);
  }
  else {
    double u = (_rec->_vertices[0]->u() + _rec->_vertices[1]->u()) / 2;
    double v = (_rec->_vertices[0]->v() + _rec->_vertices[1]->v()) / 2;
    SPoint2 pNew(u, v);
    double oldValVert = Rec2DData::getSumVert();
    _rec->_vertices[0]->relocate(pNew);
    _rec->_vertices[1]->relocate(pNew);
    double qualRelocation = Rec2DData::getSumVert() - oldValVert;
    
    valVert += _rec->_vertices[0]->getGainMerge(_rec->_vertices[1],
                                                &_rec->_edges[1], 2);
    valVert += _rec->_vertices[2]->getGainTriLess(_rec->_edges[0]);
    valVert += _rec->_vertices[3]->getGainTriLess(_rec->_edges[2]);
    
    for (int i = 0; i < 4; ++i) {
      Msg::Info("vert %d : %g", _rec->_vertices[i]->getNum(), _rec->_vertices[i]->getQual());
    }
    
    Msg::Info("qual col %g %g %g %g", qualRelocation,
              _rec->_vertices[0]->getGainMerge(_rec->_vertices[1], &_rec->_edges[1], 2),
              _rec->_vertices[2]->getGainTriLess(_rec->_edges[1]),
              _rec->_vertices[3]->getGainTriLess(_rec->_edges[2])
             );
    
    _rec->_vertices[0]->relocate(p[0]);
    _rec->_vertices[1]->relocate(p[1]);
  }
#else
  std::vector<Rec2DVertex*> verts;
  std::vector<Rec2DEdge*> edges;
  _rec->_vertices[0]->getEdges(edges);
  for (unsigned int i = 0; i < edges.size(); ++i) {
    Rec2DVertex *v = edges[i]->getOtherVertex(_rec->_vertices[0]);
    bool toAdd = true;
    for (int j = 0; j < 4; ++j) {
      if (v == _rec->_vertices[j]) {
        toAdd = false;
        break;
      }
    }
    if (toAdd) verts.push_back(v);
  }
  _rec->_vertices[1]->getEdges(edges);
  for (unsigned int i = 0; i < edges.size(); ++i) {
    Rec2DVertex *v = edges[i]->getOtherVertex(_rec->_vertices[1]);
    bool toAdd = true;
    for (int j = 0; j < 4; ++j) {
      if (v == _rec->_vertices[j]) {
        toAdd = false;
        break;
      }
    }
    if (toAdd) verts.push_back(v);
  }
  
  _rec->_vertices[0]->getMoreUniqueEdges(edges);
  _rec->_vertices[1]->getMoreUniqueEdges(edges);
  int numEdgeBef = edges.size(), weightEdgeBef = 0;
  double valEdgeBef = 0;
  for (unsigned int i = 0; i < edges.size(); ++i) {
    valEdgeBef += edges[i]->getWeightedQual();
    weightEdgeBef += edges[i]->getWeight();
  }
  
  int numVertOther = verts.size();
  double vert01Bef = 0, vert23Bef = 0, vertOtherBef = 0;
  vert01Bef += _rec->_vertices[0]->getQual();
  vert01Bef += _rec->_vertices[1]->getQual();
  vert23Bef += _rec->_vertices[2]->getQual();
  vert23Bef += _rec->_vertices[3]->getQual();
  for (unsigned int i = 0; i < verts.size(); ++i) {
    vertOtherBef += verts[i]->getQual();
  }
  
  Rec2DNode *n = new Rec2DNode(NULL, (Rec2DAction*)this, 0);
  n->makeChanges();
  
  edges.clear();
  _rec->_vertices[0]->getMoreUniqueEdges(edges);
  _rec->_vertices[1]->getMoreUniqueEdges(edges);
  int numEdgeAft = edges.size(), weightEdgeAft = 0;
  double valEdgeAft = 0;
  for (unsigned int i = 0; i < edges.size(); ++i) {
    valEdgeAft += edges[i]->getWeightedQual();
    weightEdgeAft += edges[i]->getWeight();
  }
  
  double vert01Aft = 0, vert23Aft = 0, vertOtherAft = 0;
  if (_rec->_vertices[0]->getNumElements())
    vert01Aft += _rec->_vertices[0]->getQual();
  if (_rec->_vertices[1]->getNumElements())
    vert01Aft += _rec->_vertices[1]->getQual();
  vert23Aft += _rec->_vertices[2]->getQual();
  vert23Aft += _rec->_vertices[3]->getQual();
  for (unsigned int i = 0; i < verts.size(); ++i) {
    vertOtherAft += verts[i]->getQual();
  }
  
  n->revertChanges();
  delete n;
  
  Msg::Info("-(%d)%de%g +(%d)%de%g "
            "-4v%g +2v%g +0v%g +(%d)0v%g", numEdgeBef, weightEdgeBef, valEdgeBef/weightEdgeBef,
                                           numEdgeAft, weightEdgeAft, valEdgeAft/weightEdgeAft,
                                           vert01Bef/4, vert01Aft/2, (vert23Aft-vert23Bef)/2,
                                           numVertOther, vertOtherAft-vertOtherBef);
#endif
}

void Rec2DCollapse::_computeGlobQual()
{
  std::vector<Rec2DVertex*> verts;
  _rec->_vertices[0]->getMoreNeighbourVertices(verts);
  _rec->_vertices[1]->getMoreNeighbourVertices(verts);
  unsigned int i = 0;
  while (i < verts.size() && verts[i]->getLastUpdate() <= _lastUpdate) ++i;
  if (i >= verts.size()) {
    _lastUpdate = Recombine2D::getNumChange();
    return;
  }
  
  SPoint2 p[2];
  _rec->_vertices[0]->getParam(&p[0]);
  _rec->_vertices[1]->getParam(&p[1]);
  int b0 = _rec->_vertices[0]->getOnBoundary();
  int b1 = _rec->_vertices[1]->getOnBoundary();
  
#ifdef REC2D_VERT_ONLY
  double valVert = .0;
  if (b0 || b1) {
    int iOK = 1, iKO = 0;
    if (b0) {
      iOK = 0;
      iKO = 1;
    }
    _rec->_vertices[iKO]->relocate(p[iOK]);
    
    valVert += _rec->_vertices[iOK]->getGainMerge(_rec->_vertices[iKO],
                                                  &_rec->_edges[1], 2  );
    valVert += _rec->_vertices[2]->getGainTriLess(_rec->_edges[1]);
    valVert += _rec->_vertices[3]->getGainTriLess(_rec->_edges[2]);
    _globQualIfExecuted = Rec2DData::getGlobalQuality(-1, valVert);
    
    _rec->_vertices[iKO]->relocate(p[iKO]);
  }
  else {
    double u = (_rec->_vertices[0]->u() + _rec->_vertices[1]->u()) / 2;
    double v = (_rec->_vertices[0]->v() + _rec->_vertices[1]->v()) / 2;
    SPoint2 pNew(u, v);
    _rec->_vertices[0]->relocate(pNew);
    _rec->_vertices[1]->relocate(pNew);
    
    valVert += _rec->_vertices[0]->getGainMerge(_rec->_vertices[1],
                                                &_rec->_edges[1], 2);
    valVert += _rec->_vertices[2]->getGainTriLess(_rec->_edges[0]);
    valVert += _rec->_vertices[3]->getGainTriLess(_rec->_edges[2]);
    _globQualIfExecuted = Rec2DData::getGlobalQuality(-1, valVert);
    
    _rec->_vertices[0]->relocate(p[0]);
    _rec->_vertices[1]->relocate(p[1]);
  }
#else
  int numEdge = 0;
  double valEdge = .0, valVert = .0;
  if (b0 || b1) {
    int iOK = 1, iKO = 0;
    if (b0) {
      iOK = 0;
      iKO = 1;
    }
    _rec->_vertices[iKO]->relocate(p[iOK]);
    
    valVert += _rec->_vertices[2]->getGainOneElemLess();
    valVert += _rec->_vertices[3]->getGainOneElemLess();
    valVert += _rec->_vertices[iOK]->getGainMerge(_rec->_vertices[iKO]);
    valEdge -= REC2D_EDGE_BASE * _rec->_edges[1]->getQual();
    valEdge -= REC2D_EDGE_BASE * _rec->_edges[2]->getQual();
    numEdge -= 2 * REC2D_EDGE_BASE;
    valEdge -= _rec->_edges[4]->getWeightedQual();
    numEdge -= _rec->_edges[4]->getWeight();
    
    _globQualIfExecuted = Rec2DData::getGlobalQuality(-1, valVert,
                                                      numEdge, valEdge);
    
    _rec->_vertices[iKO]->relocate(p[iKO]);
  }
  else {
    double u = (_rec->_vertices[0]->u() + _rec->_vertices[1]->u()) / 2;
    double v = (_rec->_vertices[0]->v() + _rec->_vertices[1]->v()) / 2;
    SPoint2 pNew(u, v);
    _rec->_vertices[0]->relocate(pNew);
    _rec->_vertices[1]->relocate(pNew);
    
    valVert += _rec->_vertices[2]->getGainOneElemLess();
    valVert += _rec->_vertices[3]->getGainOneElemLess();
    valVert += _rec->_vertices[0]->getGainMerge(_rec->_vertices[1]);
    valEdge -= REC2D_EDGE_BASE * _rec->_edges[1]->getQual();
    valEdge -= REC2D_EDGE_BASE * _rec->_edges[2]->getQual();
    numEdge -= 2 * REC2D_EDGE_BASE;
    valEdge -= _rec->_edges[4]->getWeightedQual();
    numEdge -= _rec->_edges[4]->getWeight();
    
    _globQualIfExecuted = Rec2DData::getGlobalQuality(-1, valVert,
                                                      numEdge, valEdge);
    
    _rec->_vertices[0]->relocate(p[0]);
    _rec->_vertices[1]->relocate(p[1]);
  }
#endif
  _lastUpdate = Recombine2D::getNumChange();
}

void Rec2DCollapse::_computeReward()
{
  Msg::Fatal("Need definition for compute reward");
}

void Rec2DCollapse::printIdentity() const
{
  Msg::Info("Collapse |%d|%d|", _rec->_triangles[0]->getNum(), _rec->_triangles[1]->getNum());
}

void Rec2DCollapse::apply(std::vector<Rec2DVertex*> &newPar)
{
  static int a = -1;
  if (++a < 1) Msg::Error("FIXME Need definition Rec2DCollapse::apply(newPar)");
}

void Rec2DCollapse::apply(Rec2DDataChange *rdc,
                          std::vector<Rec2DAction*> *&createdActions,
                          bool color) const
{
  //Msg::Info("applying Collapse |%d|%d|", _rec->_triangles[0]->getNum(), _rec->_triangles[1]->getNum());
  if (isObsolete()) {
    printIdentity();
    int p[2];
    p[0] = _rec->_vertices[0]->getParity();
    p[1] = _rec->_vertices[1]->getParity();
    Msg::Info("%d %d %d %d", _rec->_vertices[0]->getNum(), _rec->_vertices[1]->getNum(), _rec->_vertices[2]->getNum(), _rec->_vertices[3]->getNum());
    Msg::Info("%d %d - %d %d %d", p[0], p[1],
              _rec->_vertices[0]->getOnBoundary() && _rec->_vertices[1]->getOnBoundary(),
              !_rec->_vertices[2]->getOnBoundary() && _rec->_vertices[2]->getNumElements() < 4,
              !_rec->_vertices[3]->getOnBoundary() && _rec->_vertices[3]->getNumElements() < 4);
    _rec->_vertices[2]->printElem();
    _rec->_vertices[3]->printElem();
    Msg::Error("[Rec2DTwoTri2Quad] I was obsolete !");
    __crash();
  }
#ifdef REC2D_DRAW
  rdc->setAction(this);
#endif
  std::vector<Rec2DAction*> actions;
  _rec->_triangles[0]->getMoreUniqueActions(actions);
  _rec->_triangles[1]->getMoreUniqueActions(actions);
  rdc->hide(actions);
  rdc->hide(_rec->_triangles[0]);
  rdc->hide(_rec->_triangles[1]);
  rdc->hide(_rec->_edges[4]);
  Rec2DVertex *vOK, *vKO;
  if (_rec->_vertices[0]->getOnBoundary()) {
    SPoint2 p(_rec->_vertices[0]->u(), _rec->_vertices[0]->v());
    rdc->relocate(_rec->_vertices[1], p);
    vOK = _rec->_vertices[0];
    vKO = _rec->_vertices[1];
  }
  else if (_rec->_vertices[1]->getOnBoundary()) {
    SPoint2 p(_rec->_vertices[1]->u(), _rec->_vertices[1]->v());
    rdc->relocate(_rec->_vertices[0], p);
    vOK = _rec->_vertices[1];
    vKO = _rec->_vertices[0];
  }
  else {
    double u = (_rec->_vertices[0]->u() + _rec->_vertices[1]->u()) / 2;
    double v = (_rec->_vertices[0]->v() + _rec->_vertices[1]->v()) / 2;
    rdc->relocate(_rec->_vertices[1], SPoint2(u, v));
    rdc->relocate(_rec->_vertices[0], SPoint2(u, v));
    vOK = _rec->_vertices[0];
    vKO = _rec->_vertices[1];
  }
  bool edge12KO = _rec->_edges[1]->getVertex(0) == vKO ||
                  _rec->_edges[1]->getVertex(1) == vKO   ;
  rdc->swapFor(vKO, vOK);
  rdc->hide(vKO);
  
  int i0, i1, i2, i3;
  if (edge12KO) {
    i0 = 1; i1 = 2;
    i2 = 0; i3 = 3;
  }
  else {
    i0 = 0; i1 = 3;
    i2 = 1; i3 = 2;
  }
  {
    rdc->swapFor(_rec->_edges[i0], _rec->_edges[i2]);
    rdc->swapFor(_rec->_edges[i1], _rec->_edges[i3]);
    rdc->hide(_rec->_edges[i0]);
    rdc->hide(_rec->_edges[i1]);
    if (createdActions) {
      for (unsigned int i = 0; i < createdActions->size(); ++i) {
        (*createdActions)[i]->reveal();
        rdc->append((*createdActions)[i]);
      }
    }
    else {
      createdActions = new std::vector<Rec2DAction*>;
      Rec2DElement *elem[2];
      Rec2DAction *newAction;
      Rec2DEdge::getElements(_rec->_edges[i2], elem);
      if (elem[1] && elem[0]->isTri() && elem[1]->isTri()) {
        newAction = new Rec2DTwoTri2Quad(elem[0], elem[1]);
        rdc->append(newAction);
        createdActions->push_back(newAction);
        newAction = new Rec2DCollapse((Rec2DTwoTri2Quad*)newAction);
        rdc->append(newAction);
        createdActions->push_back(newAction);
      }
      Rec2DEdge::getElements(_rec->_edges[i3], elem);
      if (elem[1] && elem[0]->isTri() && elem[1]->isTri()) {
        newAction = new Rec2DTwoTri2Quad(elem[0], elem[1]);
        rdc->append(newAction);
        createdActions->push_back(newAction);
        newAction = new Rec2DCollapse((Rec2DTwoTri2Quad*)newAction);
        rdc->append(newAction);
        createdActions->push_back(newAction);
      }
    }
  }
  
  int parKO, parOK;
  if ((parKO = vKO->getParity())) {
    if (!(parOK = vOK->getParity()))
      rdc->changeParity(vOK, parKO);
    else if (parOK/2 != parKO/2)
      Rec2DData::associateParity(std::max(parOK, parKO), std::min(parOK, parKO), rdc);
  }
  
  rdc->checkObsoleteActions(_rec->_vertices, 4);
}

bool Rec2DCollapse::isObsolete() const
{
  int p[2];
  p[0] = _rec->_vertices[0]->getParity();
  p[1] = _rec->_vertices[1]->getParity();
  return p[0]/2 == p[1]/2 && p[0] != p[1] ||
         _rec->_vertices[0]->getOnBoundary() &&
         _rec->_vertices[1]->getOnBoundary()          ||
         !_rec->_vertices[2]->getOnBoundary() &&
         _rec->_vertices[2]->getNumElements() < 4     ||
         !_rec->_vertices[3]->getOnBoundary() &&
         _rec->_vertices[3]->getNumElements() < 4       ;
}

void Rec2DCollapse::getNeighbourElements(std::vector<Rec2DElement*> &elem) const
{
  _rec->getNeighbourElements(elem);
}

void Rec2DCollapse::getNeighbElemWithActions(std::vector<Rec2DElement*> &elem) const
{
  _rec->getNeighbElemWithActions(elem);
}

bool Rec2DCollapse::_hasIdenticalElement() const
{
  return _rec->_triangles[0]->hasIdenticalNeighbour() ||
         _rec->_triangles[1]->hasIdenticalNeighbour()   ;
}


/**  Rec2DEdge  **/
/*****************/
Rec2DEdge::Rec2DEdge(Rec2DVertex *rv0, Rec2DVertex *rv1)
: _rv0(rv0), _rv1(rv1), _lastUpdate(-1),
  _weight(REC2D_EDGE_BASE+2*REC2D_EDGE_QUAD), _pos(-1)
{
  _computeQual();
  reveal();
}

void Rec2DEdge::hide()
{
  _rv0->rmv(this);
  _rv1->rmv(this);
  Rec2DData::rmv(this);
  Rec2DData::addEdgeQual(-getWeightedQual(), -_weight);
}

void Rec2DEdge::reveal()
{
  _rv0->add(this);
  _rv1->add(this);
  Rec2DData::add(this);
  Rec2DData::addEdgeQual(getWeightedQual(), _weight);
}

bool Rec2DEdge::checkCoherence() const
{
  if (_rv0 == _rv1) return false;
  if (!_rv0->has(this) || !_rv1->has(this)) return false;
  return true;
  Rec2DElement *elem[2];
  Rec2DEdge::getElements(this, elem);
  if (elem[1]) {
    if (!elem[0]->has(this) || !elem[1]->has(this)) return false;
    if (!elem[0]->isNeighbour(this, elem[1]) ||
        !elem[1]->isNeighbour(this, elem[0])   ) return false;
  }
  else {
    if (!elem[0]->has(this)) return false;
    if (!elem[0]->isNeighbour(this, NULL)) return false;
  }
}

void Rec2DEdge::_computeQual()
{
  double alignment = _straightAlignment();
  double adimLength = _straightAdimLength();
  if (adimLength > 1)
    adimLength = 1./adimLength;
#ifdef REC2D_VERT_ONLY
  _qual = REC2D_COEF_ORIE * alignment +
          REC2D_COEF_LENG * adimLength +
          REC2D_COEF_ORLE * alignment * adimLength;
#else
  _qual = adimLength * (1 - REC2D_ALIGNMENT + REC2D_ALIGNMENT * alignment);
#endif
  _lastUpdate = Recombine2D::getNumChange();
}

void Rec2DEdge::print() const
{
  Rec2DElement *elem[2];
  Rec2DEdge::getElements(this, elem);
  int a, b = a = NULL;
  if (elem[0])
    a = elem[0]->getNum();
  if (elem[1])
    b = elem[1]->getNum();
  
  Msg::Info(" edge , %d--%d , %d/%d", _rv0->getNum(), _rv1->getNum(), a, b);
}

void Rec2DEdge::_addWeight(int w)
{
  _weight += w;
  if (_weight > REC2D_EDGE_BASE + 2*REC2D_EDGE_QUAD) {
    Rec2DElement *elem[2];
    Rec2DEdge::getElements(this, elem);
    int a, b = a = NULL;
    if (elem[0])
      a = elem[0]->getNum();
    if (elem[1])
      b = elem[1]->getNum();
    Msg::Error("[Rec2DEdge] Weight too high : %d (%d max) (im %d %d)",
               _weight, REC2D_EDGE_BASE + 2*REC2D_EDGE_QUAD, a, b     );
  }
  if (_weight < REC2D_EDGE_BASE)
    Msg::Error("[Rec2DEdge] Weight too low : %d (%d min)",
               _weight, REC2D_EDGE_BASE                   );
#ifdef REC2D_VERT_ONLY
  _rv0->addEdgeQual(w*getQual(), w);
  _rv1->addEdgeQual(w*getQual(), w);
#else
  Rec2DData::addEdgeQual(w*getQual(), w);
#endif
}

void Rec2DEdge::updateQual()
{
  double oldQual = _qual;
  _computeQual();
#ifdef REC2D_VERT_ONLY
  _rv0->addEdgeQual(_weight*(_qual-oldQual));
  _rv1->addEdgeQual(_weight*(_qual-oldQual));
#else
  Rec2DData::addEdgeQual(_weight*(_qual-oldQual));
#endif
  _lastUpdate = Recombine2D::getNumChange();
}

bool Rec2DEdge::isOnBoundary() const
{
  Rec2DElement *elem[2];
  Rec2DEdge::getElements(this, elem);
  return !elem[1];
}

Rec2DElement* Rec2DEdge::getTheOnlyElement(const Rec2DEdge *re)
{
  std::vector<Rec2DElement*> elem;
  Rec2DVertex::getCommonElements(re->getVertex(0), re->getVertex(1), elem);
  unsigned int i = 0;
  while (i < elem.size()) {
    if (!elem[i]->has(re)) {
      elem[i] = elem.back();
      elem.pop_back();
    }
    else
      ++i;
  }
  if (elem.size() == 1)
    return elem[0];
  if (elem.size() != 0) {
    Msg::Info("size(%d) %d %d", elem.size(), elem[0]->getNum(), elem[1]->getNum());
    Msg::Error("[Rec2DEdge] Edge bound %d elements, returning NULL", elem.size());
  }
  return NULL;
}

void Rec2DEdge::getElements(const Rec2DEdge *re, Rec2DElement **elem)
{
  elem[0] = NULL;
  elem[1] = NULL;
  std::vector<Rec2DElement*> vectElem;
  Rec2DVertex::getCommonElements(re->getVertex(0), re->getVertex(1), vectElem);
  unsigned int i = 0;
  while (i < vectElem.size()) {
    if (!vectElem[i]->has(re)) {
      vectElem[i] = vectElem.back();
      vectElem.pop_back();
    }
    else
      ++i;
  }
  switch (vectElem.size()) {
    case 2 :
      elem[1] = vectElem[1];
    case 1 :
      elem[0] = vectElem[0];
    case 0 :
      return;
    default :
      Msg::Error("[Rec2DEdge] my integrity is not respected :'(");
  }
}

void Rec2DEdge::swap(Rec2DVertex *oldRV, Rec2DVertex *newRV, bool upVert)
{
  if (upVert) {
    oldRV->rmv(this);
    newRV->add(this);
  }
  if (_rv0 == oldRV) {
    _rv0 = newRV;
    return;
  }
  if (_rv1 == oldRV) {
    _rv1 = newRV;
    return;
  }
  Msg::Error("[Rec2DEdge] oldRV not found");
}

double Rec2DEdge::_straightAdimLength() const
{
  double dx, dy, dz, *xyz0 = new double[3], *xyz1 = new double[3];
  _rv0->getxyz(xyz0);
  _rv1->getxyz(xyz1);
  dx = xyz0[0] - xyz1[0];
  dy = xyz0[1] - xyz1[1];
  dz = xyz0[2] - xyz1[2];
  double length = sqrt(dx * dx + dy * dy + dz * dz);
  delete[] xyz0;
  delete[] xyz1;
  
  double lc0 = (*Recombine2D::bgm())(_rv0->u(), _rv0->v(), .0);
  double lc1 = (*Recombine2D::bgm())(_rv1->u(), _rv1->v(), .0);
  
  return length * (1./lc0 + 1./lc1) / 2.;
}

double Rec2DEdge::_straightAlignment() const
{
  double angle0 = Recombine2D::bgm()->getAngle(_rv0->u(), _rv0->v(), .0);
  double angle1 = Recombine2D::bgm()->getAngle(_rv1->u(), _rv1->v(), .0);
  double angleEdge = atan2(_rv0->v()-_rv1->v(), _rv0->u()-_rv1->u());
  
  double alpha0 = angleEdge - angle0;
  double alpha1 = angleEdge - angle1;
  crossField2d::normalizeAngle(alpha0);
  crossField2d::normalizeAngle(alpha1);
  alpha0 = 1. - 4. * std::min(alpha0, .5 * M_PI - alpha0) / M_PI;
  alpha1 = 1. - 4. * std::min(alpha1, .5 * M_PI - alpha1) / M_PI;
  
  double lc0 = (*Recombine2D::bgm())(_rv0->u(), _rv0->v(), .0);
  double lc1 = (*Recombine2D::bgm())(_rv1->u(), _rv1->v(), .0);
  
  return (alpha0/lc0 + alpha1/lc1) / (1./lc0 + 1./lc1);
}

Rec2DVertex* Rec2DEdge::getOtherVertex(const Rec2DVertex *rv) const
{
  if (rv == _rv0)
    return _rv1;
  if (rv == _rv1)
    return _rv0;
  Msg::Error("[Rec2DEdge] You are wrong, I don't have this vertex !");
  return NULL;
}

//void Rec2DEdge::getUniqueActions(std::vector<Rec2DAction*> &actions) const
//{
//  actions.clear();
//  Rec2DElement *elem[2];
//  Rec2DEdge::getElements(this, elem);
//  if (elem[0]) elem[0]->getMoreUniqueActions(actions);
//  if (elem[1]) elem[1]->getMoreUniqueActions(actions);
//}


/**  Rec2DVertex  **/
/*******************/
Rec2DVertex::Rec2DVertex(MVertex *v)
: _v(v), _angle(4.*M_PI), _onWhat(1), _parity(0),
  _lastUpdate(-1), _sumQualAngle(.0), _pos(-1)
#ifdef REC2D_VERT_ONLY
  , _sumQualEdge(.0), _sumEdge(0), _sumAngle(0)
#endif
{
  reparamMeshVertexOnFace(_v, Recombine2D::getGFace(), _param);
  Rec2DData::add(this);
#ifdef REC2D_DRAW
  if (_v)
    _v->setIndex(_parity);
    //_v->setIndex(_onWhat);
#endif
}

Rec2DVertex::Rec2DVertex(Rec2DVertex *rv, double ang)
: _v(rv->_v), _angle(ang), _onWhat(-1), _parity(rv->_parity),
  _lastUpdate(rv->_lastUpdate),
  _sumQualAngle(rv->_sumQualAngle), _edges(rv->_edges),
  _elements(rv->_elements), _param(rv->_param), _pos(-1)
#ifdef REC2D_VERT_ONLY
  , _sumQualEdge(rv->_sumQualEdge), _sumEdge(rv->_sumEdge),
  _sumAngle(rv->_sumAngle)
#endif
{
  static int a = -1;
  if (++a < -1) Msg::Warning("FIXME Edges really necessary ?");
  
  for (unsigned int i = 0; i < _edges.size(); ++i) {
    _edges[i]->swap(rv, this, false);
  }
  reveal();
  
  rv->_edges.clear();
  delete rv;
#ifdef REC2D_DRAW
  if (_v)
    _v->setIndex(_parity);
    //_v->setIndex(_onWhat);
#endif
  _lastUpdate = Recombine2D::getNumChange();
}

void Rec2DVertex::hide()
{
  if (_elements.size() && _edges.size())
    Msg::Error("[Rec2DVertex] I have %d elements and %d edges", _elements.size(), _edges.size());
  if (_parity)
    Rec2DData::removeParity(this, _parity);
  
  Rec2DData::rmv(this);
  if  (_elements.size())
    Rec2DData::addVertQual(-getQual(), -1);
}

void Rec2DVertex::reveal()
{
  if (_parity)
    Rec2DData::addParity(this, _parity);
  
  Rec2DData::add(this);
  if  (_elements.size())
    Rec2DData::addVertQual(getQual(), 1);
}

bool Rec2DVertex::checkCoherence() const
{
  for (unsigned int i = 0; i < _edges.size(); ++i) {
    if (!_edges[i]->has(this)) return false;
    for (unsigned int j = 0; j < i; ++j) {
      if (_edges[i] == _edges[j]) return false;
    }
  }
  for (unsigned int i = 0; i < _elements.size(); ++i) {
    if (!_elements[i]->has(this)) return false;
    for (unsigned int j = 0; j < i; ++j) {
      if (_elements[i] == _elements[j]) return false;
    }
  }
  return true;
}

void Rec2DVertex::initStaticTable()
{
  // _qualVSnum[onWhat][numEl]; onWhat={0:edge,1:face}
  // _gains[onWhat][numEl];     onWhat={0:edge,1:face} (earning of adding an element)
  if (_qualVSnum == NULL) {
    int nE = 20, nF = 40; //edge, face
    
    _qualVSnum = new double*[2];
    _qualVSnum[0] = new double[nE];
    _qualVSnum[1] = new double[nF];
    _qualVSnum[0][0] = -10.;
    _qualVSnum[1][0] = -10.;
    for (int i = 1; i < nE; ++i)
      _qualVSnum[0][i] = 1. - fabs(2./i - 1.);
    for (int i = 1; i < nF; ++i)
      _qualVSnum[1][i] = std::max(1. - fabs(4./i - 1.), .0);
      
    _gains = new double*[2];
    _gains[0] = new double[nE-1];
    for (int i = 0; i < nE-1; ++i)
      _gains[0][i] = _qualVSnum[0][i+1] - _qualVSnum[0][i];
    _gains[1] = new double[nF-1];
    for (int i = 0; i < nF-1; ++i)
      _gains[1][i] = _qualVSnum[1][i+1] - _qualVSnum[1][i];
  }
}

Rec2DEdge* Rec2DVertex::getCommonEdge(const Rec2DVertex *rv0,
                                      const Rec2DVertex *rv1 )
{
  for (unsigned int i = 0; i < rv0->_edges.size(); ++i) {
    if (rv1->has(rv0->_edges[i]))
      return rv0->_edges[i];
  }
  //Msg::Warning("[Rec2DVertex] didn't find edge, returning NULL");
  return NULL;
}

void Rec2DVertex::getCommonElements(const Rec2DVertex *rv0,
                                    const Rec2DVertex *rv1,
                                    std::vector<Rec2DElement*> &elem)
{
  for (unsigned int i = 0; i < rv0->_elements.size(); ++i) {
    if (rv1->has(rv0->_elements[i]))
      elem.push_back(rv0->_elements[i]);
  }
}

bool Rec2DVertex::setBoundaryParity(int p0, int p1)
{
  if (_parity) {
    Msg::Error("[Rec2DVertex] Are you kidding me ? Already have a parity !");
    return false;
  }
  setParity(p0);
  int num = 0;
  Rec2DVertex *nextRV = NULL;
  for (unsigned int i = 0; i < _edges.size(); ++i) {
    if (_edges[i]->isOnBoundary()) {
      nextRV = _edges[i]->getOtherVertex(this);
      ++num;
    }
  }
  if (num != 2)
    Msg::Error("[Rec2DVertex] What's happening ? Am I on boundary or not ? TELL ME !");
  if (nextRV)
    return nextRV->_recursiveBoundParity(this, p1, p0); // alternate parity
  Msg::Error("[Rec2DVertex] Have I really to say that I didn't find neighbouring vertex ?");
  return false;
}

bool Rec2DVertex::_recursiveBoundParity(const Rec2DVertex *prevRV, int p0, int p1)
{
  if (_parity == p0)
    return true;
  if (_parity) {
    Msg::Error("[Rec2DVertex] Sorry, have parity %d, can't set it to %d... "
               "You failed ! Try again !", _parity, p0);
    return false;
  }
  setParity(p0);
  int num = 0;
  Rec2DVertex *nextRV = NULL;
  for (unsigned int i = 0; i < _edges.size(); ++i) {
    if (_edges[i]->isOnBoundary() && _edges[i]->getOtherVertex(this) != prevRV) {
      nextRV = _edges[i]->getOtherVertex(this);
      ++num;
    }
  }
  if (num != 1)
    Msg::Error("[Rec2DVertex] Holy shit !");
  if (nextRV)
    return nextRV->_recursiveBoundParity(this, p1, p0); // alternate parity
  Msg::Error("[Rec2DVertex] Have I really to say that I didn't find next vertex ?");
  return false;
}

void Rec2DVertex::setOnBoundary()
{
  if (_onWhat > 0) {
    double oldQual = getQual();
    _onWhat = 0;
    Rec2DData::addVertQual(getQual()-oldQual);
    _lastUpdate = Recombine2D::getNumChange();
  }
}

void Rec2DVertex::setParity(int p, bool tree)
{
  if (_parity && !tree) {
    //Msg::Warning("[Rec2DVertex] I don't like to do it. Think about that !");
    Rec2DData::removeParity(this, _parity);
  }
  
  if ((_parity = p))
    Rec2DData::addParity(this, _parity);
#ifdef REC2D_DRAW
  if (_v)
    //_v->setIndex(_parity);
    _v->setIndex(_onWhat);
#endif
}

void Rec2DVertex::setParityWD(int pOld, int pNew)
{
  static int a = -1;
  if (++a < 1)
    Msg::Warning("FIXME puis-je rendre fonction utilisable uniquement par recdata ?");
  if (pOld != _parity)
    Msg::Error("[Rec2DVertex] Old parity was not correct");
  _parity = pNew;
  
#ifdef REC2D_DRAW
  if (_v)
    _v->setIndex(_parity);
    //_v->setIndex(_onWhat);
#endif
}

void Rec2DVertex::relocate(SPoint2 p)
{
  _param = p;
  GPoint gpt = Recombine2D::getGFace()->point(p);
  _v->x() = gpt.x();
  _v->y() = gpt.y();
  _v->z() = gpt.z();
  for (unsigned int i = 0; i < _edges.size(); ++i) {
    _edges[i]->updateQual();
    _edges[i]->getOtherVertex(this)->_updateQualAngle();
  }
  _updateQualAngle();
}

void Rec2DVertex::_updateQualAngle()
{
  double oldQual = getQual();
  _sumQualAngle = .0;
#ifdef REC2D_VERT_ONLY
  _sumAngle = 0;
#endif
  for (unsigned int i = 0; i < _elements.size(); ++i) {
#ifdef REC2D_VERT_ONLY
    int n = _elements[i]->getAngleNb();
    _sumAngle += n;
    _sumQualAngle += n * _angle2Qual(_elements[i]->getAngle(this));
#else
    _sumQualAngle += _angle2Qual(_elements[i]->getAngle(this));
#endif
  }
  Rec2DData::addVertQual(getQual()-oldQual);
  _lastUpdate = Recombine2D::getNumChange();
}

//void Rec2DVertex::getMoreTriangles(std::set<Rec2DElement*> &tri) const
//{
//  for (unsigned int i = 0; i < _elements.size(); ++i) {
//    if (_elements[i]->isTri())
//      tri.insert(_elements[i]);
//  }
//}

void Rec2DVertex::getMoreNeighbourVertices(std::vector<Rec2DVertex*> &verts) const
{
  for (unsigned int i = 0; i < _edges.size(); ++i)
    verts.push_back(_edges[i]->getOtherVertex(this));
}

void Rec2DVertex::getMoreUniqueEdges(std::vector<Rec2DEdge*> &edges) const
{
  unsigned int size = edges.size();
  for (unsigned int i = 0; i < _edges.size(); ++i) {
    unsigned int j = -1;
    while (++j < size && edges[j] != _edges[i]);
    if (j == size)
      edges.push_back(_edges[i]);
  }
}

double Rec2DVertex::getQualDegree(int numEl) const
{
  int nEl = numEl > -1 ? numEl : _elements.size();
  if (_onWhat > -1)
    return _qualVSnum[_onWhat][nEl];
  if (nEl == 0) {
    Msg::Error("[Rec2DVertex] I don't want this anymore !");
    __crash();
    return -10.;
  }
  return std::max(1. - fabs(2./M_PI * _angle/(double)nEl - 1.), .0);
}

#ifdef REC2D_VERT_ONLY
double Rec2DVertex::getQual() const
{
#ifdef REC2D_ONLY_RECO  
  return _sumQualAngle/_sumAngle;
#else
  double qual = getQualDegree();
  qual =   (REC2D_COEF_ANGL + REC2D_COEF_ANDE*qual) * _sumQualAngle/_sumAngle
         + REC2D_COEF_DEGR * qual                                            ;
  return qual * _sumQualEdge / _sumEdge;
#endif
}

void Rec2DVertex::printQual() const
{
  Msg::Info("d:%g, a:%g, e:%g (sa:%d, se:%d)",
            getQualDegree(), _sumQualAngle/_sumAngle, _sumQualEdge / _sumEdge,
            _sumAngle, _sumEdge);
}

double Rec2DVertex::getQual(int numAngl, double valAngl,
                            int numEdge, double valEdge, int numElem) const
{
#ifdef REC2D_ONLY_RECO
  return valAngl / numAngl;
#else
  double qual = getQualDegree(numElem);
  qual =   (REC2D_COEF_ANGL + REC2D_COEF_ANDE*qual) * valAngl / numAngl
         + REC2D_COEF_DEGR * qual                                      ;
  return qual * valEdge / numEdge;
#endif
}

void Rec2DVertex::addEdgeQual(double val, int num)
{
  double oldQual = .0;
  if (_elements.size())
    oldQual = getQual();
  _sumQualEdge += val;
  _sumEdge += num;
  if (_sumEdge < 0 || _sumQualEdge < -1e12)
    Msg::Error("[Rec2DVertex] Negative sum edge");
  if (_elements.size())
    Rec2DData::addVertQual(getQual()-oldQual);
  _lastUpdate = Recombine2D::getNumChange();
}

double Rec2DVertex::getGainQuad(const Rec2DElement *rel,
                                const Rec2DEdge *re0,
                                const Rec2DEdge *re1    ) const
{
  double qualAngle = _sumQualAngle
                     + (REC2D_VERT_QUAD - REC2D_VERT_TRIA)
                       * _angle2Qual(rel->getAngle(this)) ;
  int sumAngle = _sumAngle + REC2D_VERT_QUAD - REC2D_VERT_TRIA;
  double qualEdge = _sumQualEdge
                    + REC2D_EDGE_QUAD * re0->getQual()
                    + REC2D_EDGE_QUAD * re1->getQual();
  int sumEdge = _sumEdge + 2 * REC2D_EDGE_QUAD;
  
  return getQual(sumAngle, qualAngle, sumEdge, qualEdge,
                 (int)_elements.size()                  ) - getQual();
}
#endif

double Rec2DVertex::getGainDegree(int n) const
{
  if (!n)
    return .0;
  if (_elements.size() + n < 0) {
    Msg::Error("[Rec2DVertex] gain for %d elements unavailable",
               _elements.size() + n                             );
    return .0;
  }
  
  if (_onWhat > -1) {
    switch (n) {
      case 1 :
        return _gains[_onWhat][_elements.size()];
      case -1 :
        return - _gains[_onWhat][_elements.size()-1];
      default :
        return _qualVSnum[_onWhat][_elements.size()+n]
               - _qualVSnum[_onWhat][_elements.size()-1];
    }
  }
  
  if (_elements.size() == 0) {
    Msg::Error("[Rec2DVertex] I don't want this anymore !");
    return 11. - fabs(2./M_PI * _angle/(double)(_elements.size() + n) - 1.);
  }
  
  if (_elements.size() + n == 0) {
    Msg::Error("[Rec2DVertex] I don't want this anymore !");
    return fabs(2./M_PI * _angle/(double)_elements.size() - 1.) - 11.;
  }
  
  return fabs(2./M_PI * _angle/(double)_elements.size() - 1.)
         - fabs(2./M_PI * _angle/(double)(_elements.size() + n) - 1.);
}

#ifdef REC2D_VERT_ONLY
double Rec2DVertex::getGainRecomb(const Rec2DElement *rel1,
                                  const Rec2DElement *rel2,
                                  const Rec2DEdge *re0,
                                  const Rec2DEdge *re1,
                                  const Rec2DEdge *re2     ) const
{
  double qualAngle = _sumQualAngle, qualEdge = _sumQualEdge;
  int sumAngle = _sumAngle, sumEdge = _sumEdge;
  
  qualAngle -= REC2D_VERT_TRIA * _angle2Qual(rel1->getAngle(this));
  qualAngle -= REC2D_VERT_TRIA * _angle2Qual(rel2->getAngle(this));
  qualAngle += REC2D_VERT_QUAD * (_angle2Qual(rel1->getAngle(this) +
                                              rel2->getAngle(this)  ));
  sumAngle += REC2D_VERT_QUAD - 2 * REC2D_VERT_TRIA;
  qualEdge -= re0->getWeightedQual();
  qualEdge += REC2D_EDGE_QUAD * re1->getQual();
  qualEdge += REC2D_EDGE_QUAD * re2->getQual();
  sumEdge += 2 * REC2D_EDGE_QUAD - REC2D_EDGE_BASE;
  
  return getQual(sumAngle, qualAngle, sumEdge, qualEdge,
                 (int)_elements.size()-1                ) - getQual();
}

double Rec2DVertex::getGainTriLess(const Rec2DEdge *re) const
{
  return getQual(_sumAngle - REC2D_VERT_TRIA, _sumQualAngle,
                 _sumEdge - REC2D_EDGE_BASE, _sumQualEdge - re->getQual(),
                 (int)_elements.size() - 1)
         - getQual();
}

double Rec2DVertex::getGainMerge(const Rec2DVertex *rv,
                                 const Rec2DEdge *const*edges, int nEdges) const
{
  int sumAngle = 0;
  double sumQualAngle = .0;
  int *numAngle = new int[_elements.size()];
  double *qualAngle = new double[_elements.size()];
  for (unsigned int i = 0; i < _elements.size(); ++i) {
    int n = _elements[i]->getAngleNb();
    numAngle[i] = n;
    qualAngle[i] = n * _angle2Qual(_elements[i]->getAngle(this));
  }
  for (unsigned int i = 0; i < rv->_elements.size(); ++i) {
    unsigned int j = 0;
    while (j < _elements.size() && _elements[j] != rv->_elements[i]) ++j;
    if (j >= _elements.size()) {
      int n = rv->_elements[i]->getAngleNb();
      sumAngle += n;
      sumQualAngle += n * _angle2Qual(rv->_elements[i]->getAngle(rv));
    }
    else {
      numAngle[j] = 0;
      qualAngle[j] = .0;
    }
  }
  for (unsigned int i = 0; i < _elements.size(); ++i) {
    sumAngle += numAngle[i];
    sumQualAngle += qualAngle[i];
  }
  int numElem = _elements.size() + rv->_elements.size() - 4;
  
  int sumEdge = 0;
  double sumQualEdge = .0;
  for (unsigned int i = 0; i < _edges.size(); ++i) {
    sumEdge += _edges[i]->getWeight();
    sumQualEdge += _edges[i]->getWeightedQual();
  }
  for (unsigned int i = 0; i < rv->_edges.size(); ++i) {
    sumEdge += rv->_edges[i]->getWeight();
    sumQualEdge += rv->_edges[i]->getWeightedQual();
  }
  for (int i = 0; i < nEdges; ++i) {
    sumEdge -= REC2D_EDGE_BASE;
    sumQualEdge -= REC2D_EDGE_BASE * edges[i]->getQual();
  }
  
  return Rec2DVertex::getQual(sumAngle, sumQualAngle,
                              sumEdge, sumQualEdge, numElem)
         - getQual() - rv->getQual()                        ;
}
#else
double Rec2DVertex::getGainRecomb(const Rec2DElement *rel1,
                                  const Rec2DElement *rel2 ) const
{
  double qualAngle = _sumQualAngle;
  qualAngle -= _angle2Qual(rel1->getAngle(this));
  qualAngle -= _angle2Qual(rel2->getAngle(this));
  qualAngle += _angle2Qual(rel1->getAngle(this) + rel2->getAngle(this));
  
  return qualAngle / (_elements.size()-1) - getQualAngle() + getGainDegree(-1);
}

double Rec2DVertex::getGainOneElemLess() const
{
  return getGainDegree(-1) + _sumQualAngle / (_elements.size()-1) - getQualAngle();
}

double Rec2DVertex::getGainMerge(const Rec2DVertex *rv) const
{
  double ans = .0, sumQualAngle = .0;
  ans -= getQual();
  ans -= rv->getQual();
  double *qualAngle = new double[_elements.size()];
  for (unsigned int i = 0; i < _elements.size(); ++i) {
    qualAngle[i] = _angle2Qual(_elements[i]->getAngle(this));
  }
  for (unsigned int i = 0; i < rv->_elements.size(); ++i) {
    unsigned int j = 0;
    while (j < _elements.size() && _elements[j] != rv->_elements[i]) ++j;
    if (j >= _elements.size())
      sumQualAngle += _angle2Qual(rv->_elements[i]->getAngle(rv));
    else
      qualAngle[j] = .0;
  }
  for (unsigned int i = 0; i < _elements.size(); ++i) {
    sumQualAngle += qualAngle[i];
  }
  int numElem = _elements.size() + rv->_elements.size() - 4;
  ans += getQualDegree(numElem) + sumQualAngle / numElem;
  return ans;
}
#endif

void Rec2DVertex::add(const Rec2DEdge *re)
{
  for (unsigned int i = 0; i < _edges.size(); ++i) {
    if (_edges[i] == re) {
      Msg::Error("[Rec2DVertex] Edge was already there");
      return;
    }
  }
#ifdef REC2D_VERT_ONLY
  double oldQual = .0;
  if (_elements.size())
    oldQual = getQual();
#endif
  _edges.push_back((Rec2DEdge*)re);
#ifdef REC2D_VERT_ONLY
  _sumQualEdge += re->getWeightedQual();
  _sumEdge += re->getWeight();
  if (_elements.size())
    Rec2DData::addVertQual(getQual()-oldQual);
  _lastUpdate = Recombine2D::getNumChange();
#endif
}

bool Rec2DVertex::has(const Rec2DEdge *re) const
{
  for (unsigned int i = 0; i < _edges.size(); ++i) {
    if (_edges[i] == re)
      return true;
  }
  return false;
}

void Rec2DVertex::rmv(const Rec2DEdge *re)
{
  unsigned int i = 0;
  while (i < _edges.size()) {
    if (_edges[i] == re) {
#ifdef REC2D_VERT_ONLY
      double oldQual = .0;
      if (_elements.size())
        oldQual = getQual();
#endif
      _edges[i] = _edges.back();
      _edges.pop_back();
#ifdef REC2D_VERT_ONLY
      _sumQualEdge -= re->getWeightedQual();
      _sumEdge -= re->getWeight();
      if (_sumEdge < 0 || _sumQualEdge < -1e12)
        Msg::Error("[Rec2DVertex] Negative sum edge");
      if (_elements.size())
        Rec2DData::addVertQual(getQual()-oldQual);
      _lastUpdate = Recombine2D::getNumChange();
#endif
      return;
    }
    ++i;
  }
  Msg::Error("[Rec2DVertex] Didn't removed edge, didn't have it");
}

void Rec2DVertex::add(const Rec2DElement *rel)
{
  for (unsigned int i = 0; i < _elements.size(); ++i) {
    if (_elements[i] == rel) {
      Msg::Error("[Rec2DVertex] Element was already there");
      return;
    }
  }
  double oldQual = .0;
  if (_elements.size())
    oldQual = getQual();
  
  _elements.push_back((Rec2DElement*)rel);
#ifdef REC2D_VERT_ONLY
  _sumAngle += rel->getAngleNb();
  _sumQualAngle += rel->getAngleNb() * _angle2Qual(rel->getAngle(this));
#else
  _sumQualAngle += _angle2Qual(rel->getAngle(this));
#endif
  if (_elements.size() == 1)
    Rec2DData::addVertQual(getQual(), 1);
  else
    Rec2DData::addVertQual(getQual()-oldQual);
  _lastUpdate = Recombine2D::getNumChange();
}

bool Rec2DVertex::has(const Rec2DElement *rel) const
{
  for (unsigned int i = 0; i < _elements.size(); ++i) {
    if (_elements[i] == rel)
      return true;
  }
  return false;
}

void Rec2DVertex::rmv(const Rec2DElement *rel)
{
  unsigned int i = 0;
  while (i < _elements.size()) {
    if (_elements[i] == rel) {
      double oldQual = getQual();
#ifdef REC2D_VERT_ONLY
      double n = _elements[i]->getAngleNb();
      _sumAngle -= n;
      _sumQualAngle -= n * _angle2Qual(_elements[i]->getAngle(this));
#else
      _sumQualAngle -= _angle2Qual(_elements[i]->getAngle(this));
#endif
      _elements[i] = _elements.back();
      _elements.pop_back();
      
      if (_elements.size())
        Rec2DData::addVertQual(getQual()-oldQual);
      else
        Rec2DData::addVertQual(-oldQual, -1);
      _lastUpdate = Recombine2D::getNumChange();
      return;
    }
    ++i;
  }
  Msg::Error("[Rec2DVertex] Didn't removed element, didn't have it");
}

void Rec2DVertex::printElem() const
{
  for (unsigned int i = 0; i < _elements.size(); ++i) {
    Msg::Info("%d - %d", i, _elements[i]->getNum());
  }
}

void Rec2DVertex::getMoreUniqueActions(std::vector<Rec2DAction*> &actions) const
{
  std::vector<Rec2DAction*> actions2;
  for (unsigned int i = 0; i < _elements.size(); ++i) {
    _elements[i]->getMoreUniqueActions(actions2);
  }
  int size = (int)actions.size();
  for (unsigned int i = 0; i < actions2.size(); ++i) {
    int j = -1;
    while (++j < size && actions2[i] != actions[j]);
    if (j == size) actions.push_back(actions2[i]);
  }
}


/**  Rec2DElement  **/
/********************/
Rec2DElement::Rec2DElement(MTriangle *t, const Rec2DEdge **re, Rec2DVertex **rv)
: _mEl((MElement *)t), _numEdge(3), _pos(-1)
{
  for (int i = 0; i < 3; ++i)
    _edges[i] = (Rec2DEdge*)re[i];
  for (int i = 0; i < 3; ++i)
    _elements[i] = Rec2DEdge::getTheOnlyElement(_edges[i]);
  _edges[3] = NULL;
  _elements[3] = NULL;
  
  reveal(rv);
  if (!edgesInOrder(_edges, 3)) Msg::Error("tri |%d|", getNum());
}

Rec2DElement::Rec2DElement(MQuadrangle *q, const Rec2DEdge **re, Rec2DVertex **rv)
: _mEl((MElement *)q), _numEdge(4), _pos(-1)
{
  for (int i = 0; i < 4; ++i)
    _edges[i] = (Rec2DEdge*)re[i];
  for (int i = 0; i < 4; ++i)
    _elements[i] = Rec2DEdge::getTheOnlyElement(_edges[i]);
  
  reveal(rv);
  if (!edgesInOrder(_edges, 4)) Msg::Error("quad |%d|", getNum());
}

void Rec2DElement::hide()
{
  if (_actions.size())
    Msg::Error("[Rec2DElement] I don't want to be hidden :'(");
  for (int i = 0; i < _numEdge; ++i) {
    if (_numEdge == 3)
      _edges[i]->remHasTri();
    if (_elements[i])
      _elements[i]->rmvNeighbour(_edges[i], this);
  }
  std::vector<Rec2DVertex*> vertices(_numEdge);
  getVertices(vertices);
  for (int i = 0; i < _numEdge; ++i) {
    vertices[i]->rmv(this);
  }
  if (_numEdge == 3)
    Rec2DData::rmvHasZeroAction(this);
  Rec2DData::rmv(this);
}

void Rec2DElement::reveal(Rec2DVertex **rv)
{
  for (int i = 0; i < _numEdge; ++i) {
    if (_numEdge == 3)
      _edges[i]->addHasTri();
    if (_elements[i])
      _elements[i]->addNeighbour(_edges[i], this);
  }
  
  if (rv) {
    for (int i = 0; i < _numEdge; ++i)
      rv[i]->add(this);
  }
  else {
    std::vector<Rec2DVertex*> vert;
    getVertices(vert);
    for (int i = 0; i < _numEdge; ++i)
      vert[i]->add(this);
  }
  if (_numEdge == 3)
    Rec2DData::addHasZeroAction(this);
  Rec2DData::add(this);
}

bool Rec2DElement::checkCoherence() const
{
  Rec2DVertex *v[4], *v0, *v1;
  v0 = _edges[0]->getVertex(0);
  v1 = _edges[0]->getVertex(1);
  if (_edges[1]->getVertex(0) == v0 || _edges[1]->getVertex(1) == v0) {
    v[0] = v0;
    if (_edges[1]->getVertex(0) == v0)
      v[1] = _edges[1]->getVertex(1);
    else
      v[1] = _edges[1]->getVertex(0);
  }
  else if (_edges[1]->getVertex(0) == v1 || _edges[1]->getVertex(1) == v1) {
    v[0] = v1;
    if (_edges[1]->getVertex(0) == v1)
      v[1] = _edges[1]->getVertex(1);
    else
      v[1] = _edges[1]->getVertex(0);
  }
  else {
    Msg::Error("not only %d vertex or edge not in order [1] (im %d)", _numEdge, getNum());
    for (int i = 0; i < _numEdge; ++i) {
      _edges[i]->print();
    }
    return false;
  }
  for (int i = 2; i < _numEdge; ++i) {
    if (_edges[i]->getVertex(0) == v[i-1])
      v[i] = _edges[i]->getVertex(1);
    else if (_edges[i]->getVertex(1) == v[i-1])
      v[i] = _edges[i]->getVertex(0);
    else {
      Msg::Error("not only %d vertex or edge not in order [2] (im %d)", _numEdge, getNum());
      for (int i = 0; i < _numEdge; ++i) {
        _edges[i]->print();
      }
      return false;
    }
  }
  if (v[0] == v1 && v[_numEdge-1] != v0 || v[0] == v0 && v[_numEdge-1] != v1) {
    Msg::Error("not only %d vertex or edge not in order [3] (im %d)", _numEdge, getNum());
    for (int i = 0; i < _numEdge; ++i) {
      _edges[i]->print();
    }
    return false;
  }
  
  for (int i = 1; i < _numEdge; ++i) {
    for (int j = 0; j < i; ++j) {
      if (_edges[i] == _edges[j]) return false;
      if (v[i] == v[j]) return false;
    }
  }
  
  for (int i = 0; i < _numEdge; ++i) {
    if (!v[i]->has(this)) {
      Msg::Error("vertex don't have me (im %d)", getNum());
      return false;
    }
    if (_elements[i] && (!_elements[i]->has(this) || !_elements[i]->has(_edges[i]))) {
      Msg::Error("does %d know me ? %d / does it know edge ? %d (im %d)",
                 _elements[i]->getNum(),
                 _elements[i]->has(this),
                 _elements[i]->has(_edges[i]),
                 getNum()                     );
      return false;
    }
  }
  
#ifdef REC2D_ONLY_RECO
  if (_numEdge == 3) {
    if (_actions.size() == 1 && !Rec2DData::hasHasOneAction(this)) {
      Msg::Error("Data doesn't know I've only 1 action(im %d)", getNum());
      return false;
    }
    if (_actions.size() == 0 && !Rec2DData::hasHasZeroAction(this)) {
      Msg::Error("Data doesn't know I've only 0 action(im %d)", getNum());
      return false;
    }
  }
#endif
  for (unsigned int i = 0; i < _actions.size(); ++i) {
    if (!_actions[i]->has(this)) {
      Msg::Error("action don't have me (im %d)", getNum());
      return false;
    }
  }
  return true;
}

bool Rec2DElement::has(const Rec2DVertex *rv) const
{
  std::vector<Rec2DVertex*> verts;
  getVertices(verts);
  for (unsigned int i = 0; i < verts.size(); ++i)
    if (verts[i] == rv) return true;
  return false;
}

bool Rec2DElement::has(const Rec2DElement *rel) const
{
  for (int i = 0; i < _numEdge; ++i)
    if (_elements[i] == rel) return true;
  return false;
}

void Rec2DElement::print() const
{
  int num[4];
  for (int i = 0; i < _numEdge; ++i) {
    if (_elements[i])
      num[i] = _elements[i]->getNum();
    else
      num[i] = 0;
  }
  if (_numEdge == 3)
    Msg::Info("tri %d - %d %d %d - nRA %d", getNum(), num[0], num[1], num[2], _actions.size());
  if (_numEdge == 4)
    Msg::Info("quad %d - %d %d %d %d - nRA %d", getNum(), num[0], num[1], num[2], num[3], _actions.size());
}

void Rec2DElement::add(Rec2DEdge *re)
{
  int i;
  for (i = 0; i < _numEdge; ++i) {
    if (_edges[i] == re) {
      Msg::Error("[Rec2DElement] Edge was already there");
      return;
    }
    if (_edges[i] == NULL) {
      _edges[i] = re;
      break;
    }
  }
  if (i == _numEdge)
    Msg::Error("[Rec2DElement] Already %d edges, can't add anymore", _numEdge);
  
  if (_numEdge == 3)
    re->addHasTri();
}

bool Rec2DElement::has(const Rec2DEdge *re) const
{
  for (int i = 0; i < _numEdge; ++i) {
    if (_edges[i] == re)
      return true;
  }
  return false;
}

void Rec2DElement::add(const Rec2DAction *ra)
{
  for (unsigned int i = 0; i < _actions.size(); ++i) {
    if (_actions[i] == ra) {
      Msg::Error("[Rec2DElement] Action was already there");
      return;
    }
  }
#ifdef REC2D_ONLY_RECO
  switch (_actions.size()) {
    case 0 :
      Rec2DData::addHasOneAction(this);
      Rec2DData::rmvHasZeroAction(this);
      break;
    case 1 :
      Rec2DData::rmvHasOneAction(this);
    default :;
  }
#endif
  _actions.push_back((Rec2DAction*)ra);
}

bool Rec2DElement::has(const Rec2DAction *ra) const
{
  for (unsigned int i = 0; i < _actions.size(); ++i) {
    if (_actions[i] == ra)
      return true;
  }
  return false;
}

void Rec2DElement::rmv(const Rec2DAction *ra)
{
  unsigned int i = 0;
  while (i < _actions.size()) {
    if (_actions[i] == ra) {
#ifdef REC2D_ONLY_RECO
      switch (_actions.size()) {
        case 1 :
          Rec2DData::rmvHasOneAction(this);
          Rec2DData::addHasZeroAction(this);
          break;
        case 2 :
          Rec2DData::addHasOneAction(this);
        default :;
      }
#endif
      _actions[i] = _actions.back();
      _actions.pop_back();
      return;
    }
    ++i;
  }
  Msg::Error("[Rec2DElement] Didn't removed action, didn't have it");
}

void Rec2DElement::addNeighbour(const Rec2DEdge *re, const Rec2DElement *rel)
{
  for (int i = 0; i < _numEdge; ++i) {
    if (_edges[i] == re) {
      if (_elements[i] != NULL && _elements[i] != rel)
        Msg::Error("[Rec2DElement] Have already a neighbour element");
      _elements[i] = (Rec2DElement*)rel;
      return;
    }
  }
  
  Rec2DElement *elem[2];
  Rec2DEdge::getElements(re, elem);
  int a, b = a = NULL;
  if (elem[0])
    a = elem[0]->getNum();
  if (elem[1])
    b = elem[1]->getNum();
  Msg::Error("[Rec2DElement] Edge not found (add) (im %d, edge %d %d)", getNum(), a, b);
}

void Rec2DElement::rmvNeighbour(const Rec2DEdge *re, const Rec2DElement *rel)
{
  for (int i = 0; i < _numEdge; ++i) {
    if (_edges[i] == re) {
      if (_elements[i] == rel)
        _elements[i] = NULL;
      else
        Msg::Error("[Rec2DElement] I didn't know this element was my neighbour...");
      return;
    }
  }
  Msg::Error("[Rec2DElement] Edge not found (rmv) (im %d)", getNum());
}

bool Rec2DElement::isNeighbour(const Rec2DEdge *re, const Rec2DElement *rel) const
{
  for (int i = 0; i < _numEdge; ++i) {
    if (_edges[i] == re) {
      if (_elements[i] == rel)
        return true;
      return false;
    }
  }
  return false;
}

bool Rec2DElement::hasIdenticalNeighbour() const
{
  for (int i = 1; i < _numEdge; ++i) {
    if (_elements[i]) {
      for (int j = 0; j < i; ++j) {
        if (_elements[i] == _elements[j])
          return true;
      }
    }
  }
  return false;
}

void Rec2DElement::swap(Rec2DEdge *re1, Rec2DEdge *re2)
{
  for (int i = 0; i < _numEdge; ++i) {
    if (_edges[i] == re1) {
      if (_numEdge == 3)
        re1->remHasTri();
      if (_elements[i])
        _elements[i]->rmvNeighbour(_edges[i], this);
      Rec2DElement *elem[2];
      Rec2DEdge::getElements(re2, elem);
      _edges[i] = (Rec2DEdge*)re2;
      if (_numEdge == 3)
        re2->addHasTri();
      if (elem[1])
        Msg::Error("[Rec2DElement] Invalid swapping (there are %d and %d)", elem[0]->getNum(), elem[1]->getNum());
      else if (elem[0]) {
        _elements[i] = elem[0];
        elem[0]->addNeighbour(re2, this);
      }
      else
        _elements[i] = NULL;
      return;
    }
  }
  Msg::Error("[Rec2DElement] Try to give me the good edge (im %d)", getNum());
}

void Rec2DElement::swapMVertex(Rec2DVertex *rv0, Rec2DVertex *rv1)
{
  for (int i = 0; i < _numEdge; ++i) {
    if (_mEl->getVertex(i) == rv0->getMVertex()) {
      _mEl->setVertex(i, rv1->getMVertex());
      return;
    }
  }
  Msg::Error("[Rec2DElement] Didn't find your MVertex");
}

double Rec2DElement::getAngle(const Rec2DVertex *rv) const
{
  std::vector<Rec2DVertex*> vert;
  getVertices(vert);
  
  int index = -1;
  for (int i = 0; i < _numEdge; ++i) {
    if (vert[i] == rv) {
      index = i;
      break;
    }
  }
  if (index == -1) {
    Msg::Error("[Rec2DElement] I don't have your vertex...");
    Msg::Info("im %d, this is %d", getNum(), rv->getNum());
    return -1.;
  }
  
  int i1 = (index+_numEdge-1)%_numEdge;
  int i0 = (index+1)%_numEdge;
  double ang =  atan2(vert[i0]->v() - rv->v(), vert[i0]->u() - rv->u())
                - atan2(vert[i1]->v() - rv->v(), vert[i1]->u() - rv->u());
  
  static unsigned int a = 0;
  if (++a < 2) Msg::Warning("FIXME use real angle instead of parametric angle");
  
  if (ang < .0)
    return ang + 2.*M_PI;
  return ang;
}

void Rec2DElement::getVertices(std::vector<Rec2DVertex*> &verts) const
{
  verts.resize(_numEdge);
  int i = 0, k = 0;
  while (k < _numEdge) {
    verts[k] = _edges[i/2]->getVertex(i%2);
    bool itsOK = true;
    for (int j = 0; j < k; ++j) {
      if (verts[k] == verts[j])
        itsOK = false;
    }
    if (itsOK) {
      // make sure they are well ordered (edges are ordered)
      if (k == 2 && _edges[i/2]->getVertex((i+1)%2) == verts[0]) {
        Rec2DVertex *rv = verts[0];
        verts[0] = verts[1];
        verts[1] = rv;
      }
      ++k;
    }
    ++i;
  }
}

void Rec2DElement::getMoreNeighbours(std::vector<Rec2DElement*> &elem) const
{
  for (int i = 0; i < _numEdge; ++i) {
    if (_elements[i]) elem.push_back(_elements[i]);
  }
}

void Rec2DElement::getMoreUniqueActions(std::vector<Rec2DAction*> &vectRA) const
{
  unsigned int size = vectRA.size();
  for (unsigned int i = 0; i < _actions.size(); ++i) {
    unsigned int j = -1;
    while (++j < size && vectRA[j] != _actions[i]);
    if (j == size)
      vectRA.push_back(_actions[i]);
  }
}

Rec2DEdge* Rec2DElement::getCommonEdge(const Rec2DElement *rel0,
                                       const Rec2DElement *rel1 )
{
  bool foundOne = false;
  for (int i = 0; i < rel0->_numEdge; ++i) {
    if (rel1->has(rel0->_edges[i])) {
      if (!foundOne) {
        return rel0->_edges[i];
        foundOne = true;
      }
      else {
        Msg::Warning("[Rec2DElement] More than one common edge");
        return NULL;
      }
    }
  }
  Msg::Error("[Rec2DElement] didn't find edge, returning NULL");
  return NULL;
}

Rec2DVertex* Rec2DElement::getOtherVertex(const Rec2DVertex *rv1,
                                          const Rec2DVertex *rv2 ) const
{
  if (_numEdge == 4) {
    Msg::Error("[Rec2DElement] I'm not a triangle");
    return NULL;
  }
  for (int i = 0; i < 2; ++i) {
    Rec2DVertex *rv = _edges[i]->getVertex(0);
    if (rv != rv1 && rv != rv2)
      return rv;
    rv = _edges[i]->getVertex(1);
    if (rv != rv1 && rv != rv2)
      return rv;
  }
  Msg::Error("[Rec2DElement] I should not be here... Why this happen to me ?");
  return NULL;
}

void Rec2DElement::createElement(double shiftx, double shifty) const
{
  MVertex *mv[4];
  
  std::vector<Rec2DVertex*> v;
  getVertices(v);
  
  for (unsigned int i = 0; i < v.size(); ++i) {
    mv[i] = new MVertex(v[i]->getMVertex()->x() + shiftx,
                        v[i]->getMVertex()->y() + shifty,
                        v[i]->getMVertex()->z()          );
  }
  if (v.size() == 3) {
    MTriangle *tri = new MTriangle(mv[0], mv[1], mv[2]);
    Recombine2D::add(tri);
  }
  else {
    MQuadrangle *quad = new MQuadrangle(mv[0], mv[1], mv[2], mv[3]);
    Recombine2D::add(quad);
  }
}

MQuadrangle* Rec2DElement::_createQuad() const
{
  if (_numEdge != 4) {
    Msg::Error("[Rec2DElement] Why do you ask me to do this ? You know I can't do it ! COULD YOU LEAVE ME KNOW ?");
    return new MQuadrangle(NULL, NULL, NULL, NULL);
  }
  MVertex *v1, *v2, *v3 = NULL, *v4 = NULL;
  v1 = _edges[0]->getVertex(0)->getMVertex();
  v2 = _edges[0]->getVertex(1)->getMVertex();
  int I = -1;
  for (int i = 1; i < 4; ++i) {
    if (v2 == _edges[i]->getVertex(0)->getMVertex()) {
      v3 = _edges[i]->getVertex(1)->getMVertex();
      I = i;
      break;
    }
    if (v2 == _edges[i]->getVertex(1)->getMVertex()) {
      v3 = _edges[i]->getVertex(0)->getMVertex();
      I = i;
      break;
    }
  }
  if (I == -1) {
    Msg::Error("[Rec2DElement] Edges not connected");
    return NULL;
  }
  for (int i = 1; i < 4; ++i) {
    if (i == I) continue;
    if (v3 == _edges[i]->getVertex(0)->getMVertex()) {
      v4 = _edges[i]->getVertex(1)->getMVertex();
      break;
    }
    if (v3 == _edges[i]->getVertex(1)->getMVertex()) {
      v4 = _edges[i]->getVertex(0)->getMVertex();
      break;
    }
  }
  return new MQuadrangle(v1, v2, v3, v4);
}


/**  Rec2DNode  **/
/*****************/
bool lessRec2DNode::operator()(Rec2DNode *rn1, Rec2DNode *rn2) const
{
  return *rn1 < *rn2;
}

bool gterRec2DNode::operator()(Rec2DNode *rn1, Rec2DNode *rn2) const
{
  return *rn2 < *rn1;
}

bool moreRec2DNode::operator()(Rec2DNode *rn1, Rec2DNode *rn2) const
{
  if (rn1->getNumTri() == rn2->getNumTri())
    return *rn2 < *rn1;
  return rn1->getNumTri() < rn2->getNumTri();
}


Rec2DNode::Rec2DNode(Rec2DNode *father, Rec2DAction *ra, int depth)
: _father(father), _ra(ra), _dataChange(NULL), _d(depth),
  _remainingTri(Rec2DData::getNumTri()), _reward(.0),
  _globalQuality(Rec2DData::getGlobalQuality()), _bestSeqReward(.0),
  _expectedSeqReward(.0), _createdActions(NULL)
#ifdef REC2D_ONLY_RECO
  , _notAllQuad(false)
#endif
{
  if (!depth && !ra) {
    Msg::Error("[Rec2DNode] Nothing to do");
    return;
  }
  
  for (int i = 0; i < REC2D_NUMB_SONS; ++i)
    _son[i] = NULL;
  if (_ra) _ra->addPointing();
  
  if (!depth) {
    _reward = _ra->getRealReward();
#ifdef REC2D_ONLY_RECO // check if not all quad
    int initialNum = Rec2DData::getNumZeroAction();
    Rec2DDataChange *dc = Rec2DData::getNewDataChange();
    std::vector<Rec2DAction*> *_v_;
    _ra->apply(dc, _v_);
    while (Rec2DData::getNumZeroAction() == initialNum &&
           Rec2DData::getNumOneAction()                  ) {
      Rec2DAction *ra = Rec2DData::getOneAction();
      ra->apply(dc, _v_);
    }
    if (Rec2DData::getNumZeroAction() > initialNum) {
      __draw();
      double time = Cpu();
      while (Cpu()-time < REC2D_WAIT_TIME)
        FlGui::instance()->check();
      _notAllQuad = true;
      Rec2DData::revertDataChange(dc);
    }
    Rec2DData::revertDataChange(dc);
#endif
    return;
  }
  
  // Apply changes
  std::vector<Rec2DElement*> neighbours; // for Collapses
  if (_ra) {
    _ra->getNeighbElemWithActions(neighbours);
    _dataChange = Rec2DData::getNewDataChange();
    _ra->apply(_dataChange, _createdActions);
    if (_createdActions) {
      for (unsigned int i = 0; i < _createdActions->size(); ++i)
        (*_createdActions)[i]->addPointing();
    }
    _remainingTri = Rec2DData::getNumTri();
    _reward = Rec2DData::getGlobalQuality() - _globalQuality;
  }
  
  // Create branches
  std::vector<Rec2DAction*> actions;
  Recombine2D::incNumChange();
  Recombine2D::nextTreeActions(actions, neighbours, this);
  if (actions.empty()) {
    Msg::Error("I don't understand why I'm here o_O");
    if (depth < 0) // when developping all the tree
      Rec2DData::addEndNode(this);
  }
  else _createSons(actions, depth);
  
  // Revert changes
  if (_dataChange) {
    if (!Rec2DData::revertDataChange(_dataChange))
      Msg::Error(" 1 - don't reverted changes");
    else
      Recombine2D::incNumChange();
    _dataChange = NULL;
  }
}

Rec2DNode::~Rec2DNode()
{
  int i = -1;
  while (++i < REC2D_NUMB_SONS && _son[i]) {
    _son[i]->rmvFather(this);
    delete _son[i];
  }
  if (_createdActions) {
    for (unsigned int i = 0; i < _createdActions->size(); ++i) {
      (*_createdActions)[i]->rmvPointing();
      delete (*_createdActions)[i];
    }
    delete _createdActions;
  }
  if (_ra) {
    _ra->rmvPointing();
    _ra = NULL;
  }
  if (_father) {
    _father->rmvSon(this);
    if (!_father->hasSon() && _father->canBeDeleted())
      delete _father;
  }
}

void Rec2DNode::rmvFather(Rec2DNode *n)
{
  if (_father != n) {
    Msg::Error("is not my father");
    return;
  }
  _father = NULL;
}

bool Rec2DNode::rmvSon(Rec2DNode *n)
{
  int indexSon = -1, i = -1;
  while (++i < REC2D_NUMB_SONS) {
    if (_son[i] == n) indexSon = i;
  }
  if (indexSon == -1) {
    Msg::Info("im %d", this);
    for (int i = 0; i < REC2D_NUMB_SONS; ++i) {
      Msg::Info("son%d %d", i, _son[i]);
    }
    Msg::Error("son %d not found", n);
    __crash();
    return false;
  }
  else {
    _son[indexSon] = NULL;
    orderSons();
  }
  return true;
}

void Rec2DNode::delSons()
{
  for (int i = 1; i < REC2D_NUMB_SONS; ++i) {
    if (_son[i]) _son[i]->rmvFather(this);
    delete _son[i];
    _son[i] = NULL;
  }
}

void Rec2DNode::orderSons()
{
  bool one = false;
  double bestReward = .0;
  int k1 = -1;
  for (int i = 0; i < REC2D_NUMB_SONS; ++i) {
    if (_son[i]) {
      if (!one) {
        bestReward = _son[i]->getBestSequenceReward();
        one = true;
      }
      if (bestReward < _son[i]->getBestSequenceReward()) {
        bestReward = _son[i]->getBestSequenceReward();
        Rec2DNode *tmp = _son[0];
        _son[0] = _son[i];
        _son[++k1] = tmp;
      }
      else _son[++k1] = _son[i];
    }
  }
}

int Rec2DNode::getNumSon() const 
{
  int num = 0;
  for (int i = 0; i < REC2D_NUMB_SONS; ++i) {
    if (_son[i]) ++num;
  }
  return num;
}

bool Rec2DNode::canBeDeleted() const
{
  return _father && _father->_father;
}

Rec2DNode* Rec2DNode::selectBestNode()
{
  if (!_son[0]) return NULL;
  
  _son[0]->printSequence();
  
  for (int i = 1; i < REC2D_NUMB_SONS; ++i) {
    if (_son[i]) _son[i]->delSons();
  }
  
  if (!_son[0]->makeChanges())
    Msg::Error("[Rec2DNode] No changes");
  
  return _son[0];
  
  /*Rec2DNode *_bestSon = _son[0];
  double bestExpectedReward = _son[0]->getExpectedSeqReward();
  
  for (int i = 1; i < REC2D_NUMB_SONS; ++i) {
    if (_son[i] && _son[i]->getExpectedSeqReward() > bestExpectedReward) {
      bestExpectedReward = _son[i]->getExpectedSeqReward();
      _bestSon = _son[i];
    }
  }
  if (_bestSon) _bestSon->printSequence();
  for (int i = 0; i < REC2D_NUMB_SONS; ++i) {
    if (_son[i] && _son[i] != _bestSon) {
      _son[i]->rmvFather(this);
      delete _son[i];
      _son[i] = NULL;
    }
  }
  if (_bestSon && !_bestSon->makeChanges())
    Msg::Error("[Rec2DNode] No changes");
  return _bestSon;*/
}

Rec2DNode* Rec2DNode::selectBestNode(Rec2DNode *rn)
{
  if (rn->_notAllQuad) {
    __wait(1);
    if (!Rec2DData::revertDataChange(rn->_dataChange))
      Msg::Error(" 4 - don't reverted changes");
    Rec2DNode *father = rn->_father;
    rn->rmvFather(father);
    delete rn;
    father->rmvSon(rn);
    if (!father->getNumSon()) father->_notAllQuad = true;
        static int a = 0;
        Msg::Info("__%d", ++a);
    return father;
  }
  
  if (!rn->_son[0]) return NULL;
  
  rn->_son[0]->printSequence();
  
  for (int i = 1; i < REC2D_NUMB_SONS; ++i) {
    if (rn->_son[i])
      rn->_son[i]->delSons();
  }
  
  if (!rn->_son[0]->makeChanges())
    Msg::Error("[Rec2DNode] No changes");
  
  return rn->_son[0];
}

void Rec2DNode::printSequence() const
{
  static int denom = 3;
  //_ra->color(183+72*(double)(_d+2)/denom, 255*(1-(double)(_d+2)/denom), 169*(1-(double)(_d+2)/denom));
  _ra->color(255, 255*(1-(double)_d/denom), 128*(1-(double)_d/denom));
  if (_son[0]) _son[0]->printSequence();
  else {__draw();double time = Cpu();
      while (Cpu()-time < REC2D_WAIT_TIME)
        FlGui::instance()->check();
  }
  _ra->color(183, 255, 169);
}

void Rec2DNode::lookahead(int depth)
{
  _d = depth;
  if (!_ra || depth < 1 || !_dataChange) {
    Msg::Error("[Rec2DNode] should not be here (lookahead)");
    return;
  }
  
  _bestSeqReward = .0;
  _expectedSeqReward = .0;
  
  std::vector<Rec2DElement*> neighbours;
  if (!_son[0])
    _ra->getNeighbElemWithActions(neighbours);
  
  if (_son[0]) _makeDevelopments(depth);
  else {
    std::vector<Rec2DAction*> actions;
    Recombine2D::incNumChange();
    Recombine2D::nextTreeActions(actions, neighbours, this);
    if (actions.size()) _createSons(actions, depth);
  }
}

void Rec2DNode::_createSons(const std::vector<Rec2DAction*> &actions, int depth)
{
  if (actions.empty() || _son[0]) {
    Msg::Error("[Rec2DNode] Nothing to do");
    return;
  }
  
  double num = .0, denom = .0, more = .0;
#ifdef REC2D_ONLY_RECO
  int k1 = -1, k2 = REC2D_NUMB_SONS;
  for (unsigned int i = 0; i < actions.size(); ++i) {
    Rec2DNode *rn = new Rec2DNode(this, actions[i], depth-1);
    if (rn->isNotAllQuad()) _son[--k2] = rn;
    else {
      _son[++k1] = rn;
      num += rn->getExpectedSeqReward() * rn->getExpectedSeqReward();
      denom += rn->getExpectedSeqReward();
      more += rn->getExpectedSeqReward();
      if (k1 == 0) _bestSeqReward = _son[k1]->getBestSequenceReward();
      else if (_bestSeqReward < _son[k1]->getBestSequenceReward()) {
        _bestSeqReward = _son[k1]->getBestSequenceReward();
        Rec2DNode *tmp = _son[0];
        _son[0] = _son[k1];
        _son[k1] = tmp;
      }
    }
  }
  ++k1;
  if (k1 > 0) more /= k1;
  
  for (int i = k2; i < REC2D_NUMB_SONS; ++i) {
    _son[i]->rmvFather(this);
    delete _son[i];
    _son[i] = NULL;
  }
  
  if (k1 == 0) {
    _notAllQuad = true;
  }
#else
  for (unsigned int i = 0; i < actions.size(); ++i) {
    _son[i] = new Rec2DNode(this, actions[i], depth-1);
    num += _son[i]->getExpectedSeqReward() * _son[i]->getExpectedSeqReward();
    denom += _son[i]->getExpectedSeqReward();
    more += _son[i]->getExpectedSeqReward();
    if (i == 0) _bestSeqReward = _son[i]->getBestSequenceReward();
    else if (_bestSeqReward < _son[i]->getBestSequenceReward()) {
      _bestSeqReward = _son[i]->getBestSequenceReward();
      Rec2DNode *tmp = _son[0];
      _son[0] = _son[i];
      _son[i] = tmp;
    }
  }
  more /= actions.size();
#endif
  if (_son[0]) _expectedSeqReward = (num + more * more) / (denom + more);
}

void Rec2DNode::_makeDevelopments(int depth)
{
  int numSon = 0, i = -1;
  while (++i < REC2D_NUMB_SONS && _son[i]) ++numSon;
  double num = .0, denom = .0, more = .0;
#ifdef REC2D_ONLY_RECO
  i = 0;
  int k2 = REC2D_NUMB_SONS;
  while (i < numSon) {
    _son[i]->_develop(depth-1);
    if (_son[i]->isNotAllQuad()) {
      if (_son[--k2]) {
        Rec2DNode *tmp = _son[k2];
        _son[k2] = _son[i];
        _son[i] = tmp;
        --numSon;
      }
      else {
        _son[k2] = _son[i];
        _son[i] = _son[--numSon];
        _son[numSon] = NULL;
      }
    }
    else {
      num += _son[i]->getExpectedSeqReward() * _son[i]->getExpectedSeqReward();
      denom += _son[i]->getExpectedSeqReward();
      more += _son[i]->getExpectedSeqReward();
      if (i == 0) _bestSeqReward = _son[i]->getBestSequenceReward();
      else if (_bestSeqReward < _son[i]->getBestSequenceReward()) {
        _bestSeqReward = _son[i]->getBestSequenceReward();
        Rec2DNode *tmp = _son[0];
        _son[0] = _son[i];
        _son[i] = tmp;
      }
      ++i;
    }
  }
  if (i != 0) more /= i;
  
  for (int i = k2; i < REC2D_NUMB_SONS; ++i) {
    _son[i]->rmvFather(this);
    delete _son[i];
    _son[i] = NULL;
  }
  if (i == 0) _notAllQuad = true;
#else
  i = -1;
  while (++i < numSon) {
    _son[i]->_develop(depth-1);
    num += _son[i]->getExpectedSeqReward() * _son[i]->getExpectedSeqReward();
    denom += _son[i]->getExpectedSeqReward();
    more += _son[i]->getExpectedSeqReward();
    if (i == 0) _bestSeqReward = _son[i]->getBestSequenceReward();
    else if (_bestSeqReward < _son[i]->getBestSequenceReward()) {
      _bestSeqReward = _son[i]->getBestSequenceReward();
      Rec2DNode *tmp = _son[0];
      _son[0] = _son[i];
      _son[i] = tmp;
    }
  }
  more /= numSon;
#endif
  if (_son[0]) _expectedSeqReward = (num + more * more) / (denom + more);
}

void Rec2DNode::_develop(int depth)
{
  _d = depth;
  if (!_ra || depth < 1) {
    Msg::Error("[Rec2DNode] should not be here (develop)");
    return;
  }
  
  _bestSeqReward = .0;
  _expectedSeqReward = .0;
  
  std::vector<Rec2DElement*> neighbours;
  if (!_son[0])
    _ra->getNeighbElemWithActions(neighbours);
  
  bool hadAction = _createdActions;
  _dataChange = Rec2DData::getNewDataChange();
  _ra->apply(_dataChange, _createdActions);
  if (_createdActions && !hadAction) {
    for (unsigned int i = 0; i < _createdActions->size(); ++i)
      (*_createdActions)[i]->addPointing();
  }
  _reward = Rec2DData::getGlobalQuality() - _globalQuality;
  _remainingTri = Rec2DData::getNumTri();
  
  if (_son[0]) _makeDevelopments(depth);
  else {
    std::vector<Rec2DAction*> actions;
    Recombine2D::incNumChange();
    Recombine2D::nextTreeActions(actions, neighbours, this);
    if (actions.size()) _createSons(actions, depth);
  }
  
  if (!Rec2DData::revertDataChange(_dataChange))
    Msg::Error(" 2 - don't reverted changes");
  else
    Recombine2D::incNumChange();
  _dataChange = NULL;
}

bool Rec2DNode::makeChanges()
{
  if (_dataChange || !_ra)
    return false;
  _dataChange = Rec2DData::getNewDataChange();
#if 0//def REC2D_DRAW // draw state at origin
  double time = Cpu();
  _ra->color(0, 0, 200);
  //_ra->printTypeRew();
  Msg::Info(" ");
  Recombine2D::drawStateOrigin();
  while (Cpu()-time < REC2D_WAIT_TIME)
    FlGui::instance()->check();
#endif
#ifdef REC2D_RECO_BLOS
  _ra->apply(_dataChange, _createdActions, true);
#else
  _ra->apply(_dataChange, _createdActions);
#endif
  Recombine2D::incNumChange();
  return true;
}

bool Rec2DNode::revertChanges()
{
  if (!_dataChange)
    return false;
  if (!Rec2DData::revertDataChange(_dataChange))
    Msg::Error(" 3 - don't reverted changes");
  else
    Recombine2D::incNumChange();
  _dataChange = NULL;
  return true;
}

bool Rec2DNode::operator<(Rec2DNode &other)
{
  return _globalQuality + _reward < other._globalQuality + other._reward;
}

