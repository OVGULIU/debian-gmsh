#ifndef _TETNODEBASIS_H_
#define _TETNODEBASIS_H_

#include "BasisHierarchical0From.h"

/**
   @class TetNodeBasis
   @brief A Node Basis for Tetrahedra

   This class can instantiate a Node-Based Basis
   (high or low order) for Tetrahedra.@n

   It uses
   <a href="http://www.hpfem.jku.at/publications/szthesis.pdf">Zaglmayr's</a>
   Basis for @em high @em order Polynomial%s generation.@n
 */

class TetNodeBasis: public BasisHierarchical0From{
 public:
  //! @param order The order of the Basis
  //!
  //! Returns a new Node-Basis for Tetrahedra of the given order
  TetNodeBasis(unsigned int order);

  //! Deletes this Basis
  //!
  virtual ~TetNodeBasis(void);
};

#endif
