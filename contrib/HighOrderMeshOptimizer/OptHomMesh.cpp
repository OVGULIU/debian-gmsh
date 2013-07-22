// Copyright (C) 2013 ULg-UCL
//
// Permission is hereby granted, free of charge, to any person
// obtaining a copy of this software and associated documentation
// files (the "Software"), to deal in the Software without
// restriction, including without limitation the rights to use, copy,
// modify, merge, publish, distribute, and/or sell copies of the
// Software, and to permit persons to whom the Software is furnished
// to do so, provided that the above copyright notice(s) and this
// permission notice appear in all copies of the Software and that
// both the above copyright notice(s) and this permission notice
// appear in supporting documentation.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT OF THIRD PARTY RIGHTS. IN NO EVENT SHALL THE
// COPYRIGHT HOLDER OR HOLDERS INCLUDED IN THIS NOTICE BE LIABLE FOR
// ANY CLAIM, OR ANY SPECIAL INDIRECT OR CONSEQUENTIAL DAMAGES, OR ANY
// DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
// WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
// ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
// OF THIS SOFTWARE.
//
// Please report all bugs and problems to the public mailing list
// <gmsh@geuz.org>.
//
// Contributors: Thomas Toulorge, Jonathan Lambrechts

#include "GmshMessage.h"
#include "GRegion.h"
#include "MTriangle.h"
#include "MQuadrangle.h"
#include "MTetrahedron.h"
#include "ParamCoord.h"
#include "OptHomMesh.h"

Mesh::Mesh(const std::set<MElement*> &els, std::set<MVertex*> &toFix, bool fixBndNodes)
{

  _dim = (*els.begin())->getDim();

  if (fixBndNodes) {
    if (_dim == 2) _pc = new ParamCoordPhys2D;
    else _pc = new ParamCoordPhys3D;
    Msg::Debug("METHOD: Fixing boundary nodes and using physical coordinates");
  }
  else {
    _pc = new ParamCoordParent;
    Msg::Debug("METHOD: Freeing boundary nodes and using parent parametric coordinates");
  }

  // Initialize elements, vertices, free vertices and element->vertices
  // connectivity
  const int nElements = els.size();
  _nPC = 0;
  _el.resize(nElements);
  _el2FV.resize(nElements);
  _el2V.resize(nElements);
  _nBezEl.resize(nElements);
  _nNodEl.resize(nElements);
  _indPCEl.resize(nElements);
  int iEl = 0;
  for(std::set<MElement*>::const_iterator it = els.begin();
      it != els.end(); ++it, ++iEl) {
    _el[iEl] = *it;
    const JacobianBasis *jac = _el[iEl]->getJacobianFuncSpace();
    _nBezEl[iEl] = jac->getNumJacNodes();
    _nNodEl[iEl] = jac->getNumMapNodes();
    for (int iVEl = 0; iVEl < jac->getNumMapNodes(); iVEl++) {
      MVertex *vert = _el[iEl]->getVertex(iVEl);
      int iV = addVert(vert);
      _el2V[iEl].push_back(iV);
      const int nPCV = _pc->nCoord(vert);
      bool isFV = false;
      if (fixBndNodes)
        isFV = (vert->onWhat()->dim() == _dim) && (toFix.find(vert) == toFix.end());
      else
        isFV = (vert->onWhat()->dim() >= 1) && (toFix.find(vert) == toFix.end());
      if (isFV) {
        int iFV = addFreeVert(vert,iV,nPCV,toFix);
        _el2FV[iEl].push_back(iFV);
        for (int i=_startPCFV[iFV]; i<_startPCFV[iFV]+nPCV; i++)
          _indPCEl[iEl].push_back(i);
      }
      else _el2FV[iEl].push_back(-1);
    }
  }

  // Initial coordinates
  _ixyz.resize(nVert());
  for (int iV = 0; iV < nVert(); iV++) _ixyz[iV] = _vert[iV]->point();
  _iuvw.resize(nFV());
  for (int iFV = 0; iFV < nFV(); iFV++) _iuvw[iFV] = _pc->getUvw(_freeVert[iFV]);

  // Set current coordinates
  _xyz = _ixyz;
  _uvw = _iuvw;

  // Set normals to 2D elements (with magnitude of inverse Jacobian) or initial
  // Jacobians of 3D elements
  if (_dim == 2) {
    _scaledNormEl.resize(nEl());
    for (int iEl = 0; iEl < nEl(); iEl++) calcScaledNormalEl2D(iEl);
  }
  else {
    _invStraightJac.resize(nEl(),1.);
    double dumJac[3][3];
    for (int iEl = 0; iEl < nEl(); iEl++)
      _invStraightJac[iEl] = 1. / _el[iEl]->getPrimaryJacobian(0.,0.,0.,dumJac);
  }

}

void Mesh::calcScaledNormalEl2D(int iEl)
{

  const JacobianBasis *jac = _el[iEl]->getJacobianFuncSpace();

  fullMatrix<double> primNodesXYZ(jac->getNumPrimMapNodes(),3);
  for (int i=0; i<jac->getNumPrimMapNodes(); i++) {
    const int &iV = _el2V[iEl][i];
    primNodesXYZ(i,0) = _xyz[iV].x();
    primNodesXYZ(i,1) = _xyz[iV].y();
    primNodesXYZ(i,2) = _xyz[iV].z();
  }

  _scaledNormEl[iEl].resize(1,3);
  const double norm = jac->getPrimNormal2D(primNodesXYZ,_scaledNormEl[iEl]);

  _scaledNormEl[iEl](0,0) /= norm;         // Re-scaling normal here is faster than an
  _scaledNormEl[iEl](0,1) /= norm;         // extra scaling operation on the Jacobian
  _scaledNormEl[iEl](0,2) /= norm;

}

int Mesh::addVert(MVertex* vert)
{
  std::vector<MVertex*>::iterator itVert = find(_vert.begin(),_vert.end(),vert);
  if (itVert == _vert.end()) {
    _vert.push_back(vert);
    return _vert.size()-1;
  }
  else return std::distance(_vert.begin(),itVert);

}

int Mesh::addFreeVert(MVertex* vert, const int iV, const int nPCV,
                      std::set<MVertex*> &toFix)
{
  std::vector<MVertex*>::iterator itVert = find(_freeVert.begin(),
                                                _freeVert.end(),vert);
  if (itVert == _freeVert.end()) {
    const int iStart = (_startPCFV.size() == 0)? 0 : _startPCFV.back()+_nPCFV.back();
    const bool forcedV = (vert->onWhat()->dim() < 2) || (toFix.find(vert) != toFix.end());
    _freeVert.push_back(vert);
    _fv2V.push_back(iV);
    _startPCFV.push_back(iStart);
    _nPCFV.push_back(nPCV);
    _nPC += nPCV;
    _forced.push_back(forcedV);
    return _freeVert.size()-1;
  }
  else return std::distance(_freeVert.begin(),itVert);

}

void Mesh::getUvw(double *it)
{
  for (int iFV = 0; iFV < nFV(); iFV++) {
    SPoint3 &uvwV = _uvw[iFV];
    *it = uvwV[0]; it++;
    if (_nPCFV[iFV] >= 2) { *it = uvwV[1]; it++; }
    if (_nPCFV[iFV] == 3) { *it = uvwV[2]; it++; }
  }

}

void Mesh::updateMesh(const double *it)
{
  for (int iFV = 0; iFV < nFV(); iFV++) {
    int iV = _fv2V[iFV];
    SPoint3 &uvwV = _uvw[iFV];
    uvwV[0] = *it; it++;
    if (_nPCFV[iFV] >= 2) { uvwV[1] = *it; it++; }
    if (_nPCFV[iFV] == 3) { uvwV[2] = *it; it++; }
    _xyz[iV] = _pc->uvw2Xyz(_freeVert[iFV],uvwV);
  }

}

void Mesh::distSqToStraight(std::vector<double> &dSq)
{
  std::vector<SPoint3> sxyz(nVert());
  for (int iEl = 0; iEl < nEl(); iEl++) {
    MElement *el = _el[iEl];
    const polynomialBasis *lagrange = (polynomialBasis*)el->getFunctionSpace();
    const polynomialBasis *lagrange1 = (polynomialBasis*)el->getFunctionSpace(1);
    int nV = lagrange->points.size1();
    int nV1 = lagrange1->points.size1();
    for (int i = 0; i < nV1; ++i) {
      sxyz[_el2V[iEl][i]] = _vert[_el2V[iEl][i]]->point();
    }
    int dim = lagrange->points.size2();
    for (int i = nV1; i < nV; ++i) {
      double f[256];
      lagrange1->f(lagrange->points(i, 0), dim > 1 ? lagrange->points(i, 1) : 0.,
                   dim > 2 ? lagrange->points(i, 2) : 0., f);
      for (int j = 0; j < nV1; ++j)
        sxyz[_el2V[iEl][i]] += sxyz[_el2V[iEl][j]] * f[j];
    }
  }

  for (int iV = 0; iV < nVert(); iV++) {
    SPoint3 d = _xyz[iV]-sxyz[iV];
    dSq[iV] = d[0]*d[0]+d[1]*d[1]+d[2]*d[2];
  }
}

void Mesh::updateGEntityPositions()
{
  for (int iV = 0; iV < nVert(); iV++)
    _vert[iV]->setXYZ(_xyz[iV].x(),_xyz[iV].y(),_xyz[iV].z());
  for (int iFV = 0; iFV < nFV(); iFV++)
    _pc->exportParamCoord(_freeVert[iFV], _uvw[iFV]);
}

void Mesh::metricMinAndGradients(int iEl, std::vector<double> &lambda,
                                 std::vector<double> &gradLambda)
{
  const JacobianBasis *jacBasis = _el[iEl]->getJacobianFuncSpace();
  const int &numJacNodes = jacBasis->getNumJacNodes();
  const int &numMapNodes = jacBasis->getNumMapNodes();
  const int &numPrimMapNodes = jacBasis->getNumPrimMapNodes();
  fullVector<double> lambdaJ(numJacNodes), lambdaB(numJacNodes);
  fullMatrix<double> gradLambdaJ(numJacNodes, 2 * numMapNodes);
  fullMatrix<double> gradLambdaB(numJacNodes, 2 * numMapNodes);

  // Coordinates of nodes
  fullMatrix<double> nodesXYZ(numMapNodes,3), nodesXYZStraight(numPrimMapNodes,3);
  for (int i = 0; i < numMapNodes; i++) {
    int &iVi = _el2V[iEl][i];
    nodesXYZ(i,0) = _xyz[iVi].x();
    nodesXYZ(i,1) = _xyz[iVi].y();
    nodesXYZ(i,2) = _xyz[iVi].z();
    if (i < numPrimMapNodes) {
      nodesXYZStraight(i,0) = _ixyz[iVi].x();
      nodesXYZStraight(i,1) = _ixyz[iVi].y();
      nodesXYZStraight(i,2) = _ixyz[iVi].z();
    }
  }

  jacBasis->getMetricMinAndGradients(nodesXYZ,nodesXYZStraight,lambdaJ,gradLambdaJ);

  //l2b.mult(lambdaJ, lambdaB);
  //l2b.mult(gradLambdaJ, gradLambdaB);
  lambdaB = lambdaJ;
  gradLambdaB = gradLambdaJ;

  int iPC = 0;
  std::vector<SPoint3> gXyzV(numJacNodes);
  std::vector<SPoint3> gUvwV(numJacNodes);
  for (int l = 0; l < numJacNodes; l++) {
    lambda[l] = lambdaB(l);
  }
  for (int i = 0; i < numMapNodes; i++) {
    int &iFVi = _el2FV[iEl][i];
    if (iFVi >= 0) {
      for (int l = 0; l < numJacNodes; l++) {
        gXyzV [l] = SPoint3(gradLambdaB(l,i+0*numMapNodes),
                            gradLambdaB(l,i+1*numMapNodes),/*BDB(l,i+2*nbNod)*/ 0.);
      }
      _pc->gXyz2gUvw(_freeVert[iFVi],_uvw[iFVi],gXyzV,gUvwV);
      for (int l = 0; l < numJacNodes; l++) {
        gradLambda[indGSJ(iEl,l,iPC)] = gUvwV[l][0];
        if (_nPCFV[iFVi] >= 2) gradLambda[indGSJ(iEl,l,iPC+1)] = gUvwV[l][1];
        if (_nPCFV[iFVi] == 3) gradLambda[indGSJ(iEl,l,iPC+2)] = gUvwV[l][2];
      }
      iPC += _nPCFV[iFVi];
    }
  }
}

void Mesh::scaledJacAndGradients(int iEl, std::vector<double> &sJ,
                                 std::vector<double> &gSJ)
{
  const JacobianBasis *jacBasis = _el[iEl]->getJacobianFuncSpace();
  const int &numJacNodes = jacBasis->getNumJacNodes();
  const int &numMapNodes = jacBasis->getNumMapNodes();
  fullMatrix<double> JDJ (numJacNodes,3*numMapNodes+1);
  fullMatrix<double> BDB (numJacNodes,3*numMapNodes+1);

  // Coordinates of nodes
  fullMatrix<double> nodesXYZ(numMapNodes,3), normals(_dim,3);
  for (int i = 0; i < numMapNodes; i++) {
    int &iVi = _el2V[iEl][i];
    nodesXYZ(i,0) = _xyz[iVi].x();
    nodesXYZ(i,1) = _xyz[iVi].y();
    nodesXYZ(i,2) = _xyz[iVi].z();
  }

  // Calculate Jacobian and gradients, scale if 3D (already scaled by
  // regularization normals in 2D)
  jacBasis->getSignedJacAndGradients(nodesXYZ,_scaledNormEl[iEl],JDJ);
  if (_dim == 3) JDJ.scale(_invStraightJac[iEl]);

  // Transform Jacobian and gradients from Lagrangian to Bezier basis
  jacBasis->lag2Bez(JDJ,BDB);

  // Scaled jacobian
  for (int l = 0; l < numJacNodes; l++) sJ [l] = BDB (l,3*numMapNodes);

  // Gradients of the scaled jacobian
  int iPC = 0;
  std::vector<SPoint3> gXyzV(numJacNodes);
  std::vector<SPoint3> gUvwV(numJacNodes);
  for (int i = 0; i < numMapNodes; i++) {
    int &iFVi = _el2FV[iEl][i];
    if (iFVi >= 0) {
      for (int l = 0; l < numJacNodes; l++)
        gXyzV [l] = SPoint3(BDB(l,i+0*numMapNodes), BDB(l,i+1*numMapNodes),
                            BDB(l,i+2*numMapNodes));
      _pc->gXyz2gUvw(_freeVert[iFVi],_uvw[iFVi],gXyzV,gUvwV);
      for (int l = 0; l < numJacNodes; l++) {
        gSJ[indGSJ(iEl,l,iPC)] = gUvwV[l][0];
        if (_nPCFV[iFVi] >= 2) gSJ[indGSJ(iEl,l,iPC+1)] = gUvwV[l][1];
        if (_nPCFV[iFVi] == 3) gSJ[indGSJ(iEl,l,iPC+2)] = gUvwV[l][2];
      }
      iPC += _nPCFV[iFVi];
    }
  }

}

void Mesh::pcScale(int iFV, std::vector<double> &scale)
{
  // Calc. derivative of x, y & z w.r.t. parametric coordinates
  const SPoint3 dX(1.,0.,0.), dY(0.,1.,0.), dZ(0.,0.,1.);
  SPoint3 gX, gY, gZ;
  _pc->gXyz2gUvw(_freeVert[iFV],_uvw[iFV],dX,gX);
  _pc->gXyz2gUvw(_freeVert[iFV],_uvw[iFV],dY,gY);
  _pc->gXyz2gUvw(_freeVert[iFV],_uvw[iFV],dZ,gZ);

  // Scale = inverse norm. of vector (dx/du, dy/du, dz/du)
  scale[0] = 1./sqrt(gX[0]*gX[0]+gY[0]*gY[0]+gZ[0]*gZ[0]);
  if (_nPCFV[iFV] >= 2) scale[1] = 1./sqrt(gX[1]*gX[1]+gY[1]*gY[1]+gZ[1]*gZ[1]);
  if (_nPCFV[iFV] == 3) scale[2] = 1./sqrt(gX[2]*gX[2]+gY[2]*gY[2]+gZ[2]*gZ[2]);
}

void Mesh::writeMSH(const char *filename)
{
  FILE *f = fopen(filename, "w");

  fprintf(f, "$MeshFormat\n");
  fprintf(f, "2.2 0 8\n");
  fprintf(f, "$EndMeshFormat\n");

  fprintf(f, "$Nodes\n");
  fprintf(f, "%d\n", nVert());
  for (int i = 0; i < nVert(); i++)
    fprintf(f, "%d %22.15E %22.15E %22.15E\n", i + 1, _xyz[i].x(), _xyz[i].y(), _xyz[i].z());
  fprintf(f, "$EndNodes\n");

  fprintf(f, "$Elements\n");
  fprintf(f, "%d\n", nEl());
  for (int iEl = 0; iEl < nEl(); iEl++) {
    fprintf(f, "%d %d 2 0 0", iEl+1, _el[iEl]->getTypeForMSH());
    for (size_t iVEl = 0; iVEl < _el2V[iEl].size(); iVEl++)
      fprintf(f, " %d", _el2V[iEl][iVEl] + 1);
    fprintf(f, "\n");
  }
  fprintf(f, "$EndElements\n");

  fclose(f);
}
