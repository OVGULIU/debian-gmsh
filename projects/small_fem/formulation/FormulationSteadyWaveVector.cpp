#include "BasisGenerator.h"
#include "GaussIntegration.h"
#include "Jacobian.h"

#include "FormulationSteadyWaveVector.h"

using namespace std;

// Pi  = atan(1) * 4
// Mu  = 4 * Pi * 10^-7
// Eps = 8.85418781762 * 10^−12
//const double FormulationSteadyWaveVector::mu  = 4 * atan(1) * 4 * 1E-7;
//const double FormulationSteadyWaveVector::eps = 8.85418781762E-12;

const double FormulationSteadyWaveVector::mu  = 1;
const double FormulationSteadyWaveVector::eps = 1;

FormulationSteadyWaveVector::FormulationSteadyWaveVector(GroupOfElement& goe,
							 double k,
							 unsigned int order){
  // Wave Number Squared //
  kSquare = k * k;

  // Function Space & Basis //
  basis  = BasisGenerator::generate(goe.get(0).getType(),
                                    1, order, "hierarchical");

  fspace = new FunctionSpaceVector(goe, *basis);

  // Gaussian Quadrature Data (Term One) //
  // NB: We need to integrad a rot * rot !
  //     and order(rot f) = order(f) - 1
  fullMatrix<double> gC1;
  fullVector<double> gW1;

  // Gaussian Quadrature Data (Term Two) //
  // NB: We need to integrad a f * f !
  fullMatrix<double> gC2;
  fullVector<double> gW2;

  // Look for 1st element to get element type
  // (We suppose only one type of Mesh !!)

  // if Order == 0 --> we want Nedelec Basis of ordre *almost* one //
  if(order == 0){
    gaussIntegration::get(goe.get(0).getType(), 0, gC1, gW1);
    gaussIntegration::get(goe.get(0).getType(), 2, gC2, gW2);
  }

  else{
    gaussIntegration::get(goe.get(0).getType(), (order - 1) + (order - 1), gC1, gW1);
    gaussIntegration::get(goe.get(0).getType(), order + order, gC2, gW2);
  }

  // Local Terms //
  basis->preEvaluateDerivatives(gC1);
  basis->preEvaluateFunctions(gC2);
  goe.orientAllElements(*basis);

  Jacobian jac1(goe, gC1);
  Jacobian jac2(goe, gC2);
  jac1.computeJacobians();
  jac2.computeInvertJacobians();

  localTerms1 = new TermHDiv(jac1, *basis, gW1);
  localTerms2 = new TermHCurl(jac2, *basis, gW2);
}

FormulationSteadyWaveVector::~FormulationSteadyWaveVector(void){
  delete basis;
  delete fspace;

  delete localTerms1;
  delete localTerms2;
}

double FormulationSteadyWaveVector::weak(unsigned int dofI, unsigned int dofJ,
                                         const GroupOfDof& god) const{
  return
    localTerms1->getTerm(dofI, dofJ, god) / mu -
    localTerms2->getTerm(dofI, dofJ, god) * eps * kSquare;
}

double FormulationSteadyWaveVector::rhs(unsigned int equationI,
					       const GroupOfDof& god) const{
  return 0;
}

const FunctionSpace& FormulationSteadyWaveVector::fs(void) const{
  return *fspace;
}
