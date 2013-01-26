#include "BasisGenerator.h"
#include "GaussIntegration.h"
#include "Jacobian.h"
#include "GroupOfElement.h"

#include "FormulationProjectionScalar.h"

using namespace std;

FormulationProjectionScalar::
FormulationProjectionScalar(double (*f)(fullVector<double>& xyz),
			    FunctionSpaceScalar& fs){
  // Save f //
  this->f = f;

  // Save fspace //
  fspace = &fs;
  basis  = &fs.getBasis(0);

  // Domain //
  GroupOfElement& goe = fs.getSupport();

  // Gaussian Quadrature Data  //
  // NB: We need to integrad f_i * f_j or f_i * g
  fullMatrix<double> gC;
  fullVector<double> gW;

  // Look for 1st element to get element type
  // (We suppose only one type of Mesh !!)
  gaussIntegration::get(goe.get(0).getType(), 2 * basis->getOrder(), gC, gW);

  // Local Terms //
  basis->preEvaluateFunctions(gC);
  goe.orientAllElements(*basis);

  Jacobian jac(goe, gC);
  jac.computeJacobians();

  localTerms1 = new TermHOne(jac, *basis, gW);
  localTerms2 = new TermProjectionHOne(jac, *basis, gW, gC, f);
}

FormulationProjectionScalar::~FormulationProjectionScalar(void){
  delete localTerms2;
  delete localTerms1;
}

double FormulationProjectionScalar::weak(unsigned int dofI, unsigned int dofJ,
                                         const GroupOfDof& god) const{

  return localTerms1->getTerm(dofI, dofJ, god);
}

double FormulationProjectionScalar::rhs(unsigned int equationI,
					const GroupOfDof& god) const{

  return localTerms2->getTerm(0, equationI, god);
}

const FunctionSpace& FormulationProjectionScalar::fs(void) const{
  return *fspace;
}
