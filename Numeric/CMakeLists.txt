# Gmsh - Copyright (C) 1997-2010 C. Geuzaine, J.-F. Remacle
#
# See the LICENSE.txt file for license information. Please report all
# bugs and problems to <gmsh@geuz.org>.

set(SRC
  Numeric.cpp
    fullMatrix.cpp
    polynomialBasis.cpp
    JacobianBasis.cpp
  GaussQuadratureLin.cpp
    GaussQuadratureTri.cpp
    GaussQuadratureQuad.cpp
    GaussQuadratureTet.cpp
    GaussQuadratureHex.cpp
    GaussQuadraturePri.cpp
    GaussLegendreSimplex.cpp
    Gauss.cpp
  robustPredicates.cpp
  mathEvaluator.cpp
  Iso.cpp
  DivideAndConquer.cpp
)

file(GLOB HDR RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.h) 
append_gmsh_src(Numeric "${SRC};${HDR}")