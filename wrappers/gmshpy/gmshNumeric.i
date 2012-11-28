%feature("autodoc", "1");
%module gmshNumeric

%include std_string.i
%include std_vector.i

%{
  #include "GmshConfig.h"

  #include "GaussIntegration.h"
  #include "JacobianBasis.h"
  #include "fullMatrix.h"
  #include "nodalBasis.h"
  #include "polynomialBasis.h"
  #include "pyramidalBasis.h"
%}

%include "GaussIntegration.h"
%include "JacobianBasis.h"
%include "fullMatrix.h"
%template(fullMatrixDouble) fullMatrix<double>;
%template(fullVectorDouble) fullVector<double>;
%include "nodalBasis.h"
%include "polynomialBasis.h"
%include "pyramidalBasis.h"
