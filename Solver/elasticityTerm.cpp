// Gmsh - Copyright (C) 1997-2009 C. Geuzaine, J.-F. Remacle
//
// See the LICENSE.txt file for license information. Please report all
// bugs and problems to <gmsh@geuz.org>.

#include "elasticityTerm.h"
#include "Numeric.h"

void elasticityTerm::elementMatrix(SElement *se, fullMatrix<double> &m) const
{
  MElement *e = se->getMeshElement();
  int nbNodes = e->getNumVertices();
  int integrationOrder = 3 * (e->getPolynomialOrder() - 1) ;
  int npts;
  IntPt *GP;
  double jac[3][3];
  double invjac[3][3];
  double Grads[256][3], grads[100][3];
  e->getIntegrationPoints(integrationOrder, &npts, &GP);
  
  m.set_all(0.);
  
  double FACT = _E / (1 + _nu);
  double C11 = FACT * (1 - _nu) / (1 - 2 * _nu);
  double C12 = FACT * _nu / (1 - 2 * _nu);
  double C44 = (C11 - C12) / 2;
  const double C[6][6] =
    { {C11, C12, C12,    0,   0,   0}, 
      {C12, C11, C12,    0,   0,   0}, 
      {C12, C12, C11,    0,   0,   0}, 
      {  0,   0,   0,  C44,   0,   0},
      {  0,   0,   0,    0, C44,   0}, 
      {  0,   0,   0,    0,   0, C44} };
  
  fullMatrix<double> H(6, 6);
  fullMatrix<double> B(6, 3 * nbNodes);
  fullMatrix<double> BTH(3 * nbNodes, 6);
  fullMatrix<double> BT(3 * nbNodes, 6);
  for (int i = 0; i < 6; i++)
    for (int j = 0; j < 6; j++)
      H(i, j) = C[i][j];
  
  for (int i = 0; i < npts; i++){
    const double u = GP[i].pt[0];
    const double v = GP[i].pt[1];
    const double w = GP[i].pt[2];
    const double weight = GP[i].weight;
    const double detJ = e->getJacobian(u, v, w, jac);
    e->getGradShapeFunctions(u, v, w, grads);
    inv3x3(jac, invjac);
    B.set_all(0.);
    BT.set_all(0.);
    for (int j = 0; j < nbNodes; j++){
      Grads[j][0] = invjac[0][0] * grads[j][0] + invjac[0][1] * grads[j][1] + 
        invjac[0][2] * grads[j][2];
      Grads[j][1] = invjac[1][0] * grads[j][0] + invjac[1][1] * grads[j][1] + 
        invjac[1][2] * grads[j][2];
      Grads[j][2] = invjac[2][0] * grads[j][0] + invjac[2][1] * grads[j][1] + 
        invjac[2][2] * grads[j][2];
      
      BT(j, 0) = B(0, j) = Grads[j][0];
      BT(j, 3) = B(3, j) = Grads[j][1];
      BT(j, 4) = B(4, j) = Grads[j][2];
      
      BT(j + nbNodes, 1) = B(1, j + nbNodes) = Grads[j][1];
      BT(j + nbNodes, 3) = B(3, j + nbNodes) = Grads[j][0];
      BT(j + nbNodes, 5) = B(5, j + nbNodes) = Grads[j][2];
      
      BT(j + 2 * nbNodes, 2) = B(2, j + 2 * nbNodes) = Grads[j][2];
      BT(j + 2 * nbNodes, 4) = B(4, j + 2 * nbNodes) = Grads[j][0];
      BT(j + 2 * nbNodes, 5) = B(5, j + 2 * nbNodes) = Grads[j][1];
    }
    BTH.set_all(0.);
    BTH.gemm(BT, H); 
    m.gemm(BTH, B, weight * detJ, 1.);
  } 
}

void elasticityTerm::elementVector(SElement *se, fullVector<double> &m) const
{
  MElement *e = se->getMeshElement();
  int nbNodes = e->getNumVertices();
  int integrationOrder = 2 * e->getPolynomialOrder();
  int npts;
  IntPt *GP;
  double jac[3][3];
  double ff[256];
  e->getIntegrationPoints(integrationOrder, &npts, &GP);
  
  m.scale(0.);
  
  for (int i = 0; i < npts; i++){
    const double u = GP[i].pt[0];
    const double v = GP[i].pt[1];
    const double w = GP[i].pt[2];
    const double weight = GP[i].weight;
    const double detJ = e->getJacobian(u, v, w, jac);
    e->getShapeFunctions(u, v, w, ff);
    for (int j = 0; j < nbNodes; j++){
      m(j) += _volumeForce.x() *ff[j] * weight * detJ *.5;
      m(j + nbNodes) += _volumeForce.y() *ff[j] * weight * detJ *.5;
      m(j + 2 * nbNodes) += _volumeForce.z() *ff[j] * weight * detJ *.5;
    }
  } 
}
