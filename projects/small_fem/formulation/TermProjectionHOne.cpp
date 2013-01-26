#include "Exception.h"
#include "TermProjectionHOne.h"

using namespace std;

TermProjectionHOne::TermProjectionHOne(const Jacobian& jac,
                                       const Basis& basis,
                                       const fullVector<double>& integrationWeights,
                                       const fullMatrix<double>& integrationPoints,
                                       double (*f)(fullVector<double>& xyz)){
  // Basis Check //
  if(basis.getType() != 0)
    throw
      Exception
      ("A H1 term must use a 0form basis");

  // Function to Project //
  this->f = f;

  // Gauss Points //
  gW = &integrationWeights;
  gC = &integrationPoints;
  nG = gW->size();

  // Basis & Orientations //
  this->basis     = &basis;
  nOrientation    = basis.getNOrientation();
  nFunction       = basis.getNFunction();
  orientationStat = &jac.getAllElements().getOrientationStats();

  // Compute Jacobians //
  this->jac = &jac;

  // Element Map //
  buildEMap();

  // Compute //
  computeC();
  computeB();
  computeA();

  // Clean up //
  clean();
}

TermProjectionHOne::~TermProjectionHOne(void){
  for(unsigned int s = 0; s < nOrientation; s++)
    delete aM[s];

  delete[] aM;
  delete   eMap;
}

void TermProjectionHOne::clean(void){
  for(unsigned int s = 0; s < nOrientation; s++)
    delete cM[s];

  delete[] cM;

  for(unsigned int s = 0; s < nOrientation; s++)
    delete bM[s];

  delete[] bM;
}

void TermProjectionHOne::buildEMap(void){
  const vector<const MElement*>& element = jac->getAllElements().getAll();

  eMap = new map<const MElement*, pair<unsigned int, unsigned int> >;

  unsigned int offset = 0;
  unsigned int j;

  for(unsigned int s = 0; s < nOrientation; s++){
    j = 0;

    for(unsigned int e = offset; e < offset + (*orientationStat)[s]; e++){
      eMap->insert(pair<const MElement*, pair<unsigned int, unsigned int> >
                       (element[e], pair<unsigned int, unsigned int>(s, j)));
      j++;
    }

    // New Offset
    offset += (*orientationStat)[s];
  }
}

void TermProjectionHOne::computeC(void){
  // Alloc //
  cM = new fullMatrix<double>*[nOrientation];

  for(unsigned int s = 0; s < nOrientation; s++)
    cM[s] = new fullMatrix<double>(nG, nFunction);

  // Fill //
  for(unsigned int s = 0; s < nOrientation; s++){
    // Get functions for this Orientation
    const fullMatrix<double>& phi =
      basis->getPreEvaluatedFunctions(s);

    // Loop on Gauss Points
    for(unsigned int g = 0; g < nG; g++)
      for(unsigned int i = 0; i < nFunction; i++)
        (*cM[s])(g, i) = (*gW)(g) * phi(i, g);
  }
}

void TermProjectionHOne::computeB(void){
  unsigned int offset = 0;
  unsigned int j;

  fullVector<double> xyz(3);
  SPoint3            pxyz;
  double             fxyz;

  // Alloc //
  bM = new fullMatrix<double>*[nOrientation];

  for(unsigned int s = 0; s < nOrientation; s++)
    bM[s] = new fullMatrix<double>((*orientationStat)[s], nG);

  // Fill //
  const vector<const MElement*>& element = jac->getAllElements().getAll();

  for(unsigned int s = 0; s < nOrientation; s++){
    // Loop On Element
    j = 0;

    for(unsigned int e = offset; e < offset + (*orientationStat)[s]; e++){
      // Get Jacobians
      const vector<const pair<const fullMatrix<double>*, double>*>& jacM =
        jac->getJacobian(*element[e]);

      for(unsigned int g = 0; g < nG; g++){
        // Compute f in the *physical* coordinate
        const_cast<MElement*>(element[e])
          ->pnt((*gC)(g, 0),
                (*gC)(g, 1),
                (*gC)(g, 2),
                pxyz);

        xyz(0) = pxyz.x();
        xyz(1) = pxyz.y();
        xyz(2) = pxyz.z();

        fxyz = f(xyz);

        (*bM[s])(j, g) = fabs(jacM[g]->second) * fxyz;
      }

      // Next Element in Orientation[s]
      j++;
    }

    // New Offset
    offset += (*orientationStat)[s];
  }
}

void TermProjectionHOne::computeA(void){
  // Alloc //
  aM = new fullMatrix<double>*[nOrientation];

  for(unsigned int s = 0; s < nOrientation; s++)
    aM[s] = new fullMatrix<double>((*orientationStat)[s], nFunction);

  // Fill //
  for(unsigned int s = 0; s < nOrientation; s++)
    // GEMM doesn't like matrices with 0 Elements
    if((*orientationStat)[s])
      aM[s]->gemm(*bM[s], *cM[s]);
}
