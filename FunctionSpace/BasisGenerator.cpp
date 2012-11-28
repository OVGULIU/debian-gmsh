#include "BasisGenerator.h"
#include "GmshDefines.h"
#include "Exception.h"

#include "LineNodeBasis.h"
#include "LineEdgeBasis.h"
#include "LineNedelecBasis.h"

#include "QuadNodeBasis.h"
#include "QuadEdgeBasis.h"

#include "TriNodeBasis.h"
#include "TriEdgeBasis.h"
#include "TriNedelecBasis.h"
#include "TriLagrangeBasis.h"

#include "TetNodeBasis.h"
#include "TetEdgeBasis.h"

#include "HexNodeBasis.h"
#include "HexEdgeBasis.h"


BasisGenerator::BasisGenerator(void){
}

BasisGenerator::~BasisGenerator(void){
}

Basis* BasisGenerator::generate(int elementType, 
				int basisType, 
				int order,
				std::string family){
  
  if(!family.compare(std::string("zaglmayr")))
    return generateZaglmayr(elementType, basisType, order);

  else if(!family.compare(std::string("lagrange")))
    return generateLagrange(elementType, basisType, order);

  else
    throw Exception("Unknwown Basis Family: %s", family.c_str());
}

Basis* BasisGenerator::generateZaglmayr(int elementType, 
					int basisType, 
					int order){
  
  switch(elementType){
  case TYPE_LIN: return linZaglmayrGen(basisType, order);
  case TYPE_TRI: return triZaglmayrGen(basisType, order);
  case TYPE_QUA: return quaZaglmayrGen(basisType, order);
  case TYPE_TET: return tetZaglmayrGen(basisType, order);
  case TYPE_HEX: return hexZaglmayrGen(basisType, order);

  default: throw Exception("Unknown Element Type (%d) for Basis Generation", 
			   elementType);
  }
}

Basis* BasisGenerator::generateLagrange(int elementType, 
					int basisType, 
					int order){
  if(basisType != 0)
    throw 
      Exception("Cannot Have a %d-Form Lagrange Basis (0-Form only)",
		basisType);

  switch(elementType){
  case TYPE_LIN: throw Exception("Lagrange Basis on Lines not Implemented");
  case TYPE_TRI: return new TriLagrangeBasis(order);
  case TYPE_QUA: throw Exception("Lagrange Basis on Quads not Implemented");
  case TYPE_TET: throw Exception("Lagrange Basis on Tets not Implemented");
  case TYPE_HEX: throw Exception("Lagrange Basis on Hexs not Implemented");

  default: throw Exception("Unknown Element Type (%d) for Basis Generation", 
			   elementType);
  }
}

Basis* BasisGenerator::linZaglmayrGen(int basisType, 
				      int order){
  switch(basisType){ 
  case  0: return new LineNodeBasis(order);
  case  1: 
    if (order == 0) return new LineNedelecBasis();
    else            return new LineEdgeBasis(order);

  case  2: throw Exception("2-form not implemented on Lines");
  case  3: throw Exception("3-form not implemented on Lines");

  default: throw Exception("There is no %d-form", basisType);
  }  
}

Basis* BasisGenerator::triZaglmayrGen(int basisType, 
				      int order){
  switch(basisType){
  case  0: return new TriNodeBasis(order);
  case  1: 
    if (order == 0) return new TriNedelecBasis();
    else            return new TriEdgeBasis(order);

  case  2: throw Exception("2-form not implemented on Triangles");
  case  3: throw Exception("3-form not implemented on Triangles");

  default: throw Exception("There is no %d-form", basisType);
  }  
}

Basis* BasisGenerator::quaZaglmayrGen(int basisType, 
				      int order){
  switch(basisType){
  case  0: return new QuadNodeBasis(order);
  case  1: return new QuadEdgeBasis(order);
  case  2: throw Exception("2-form not implemented on Quads");
  case  3: throw Exception("3-form not implemented on Quads");

  default: throw Exception("There is no %d-form", basisType);
  }  
}

Basis* BasisGenerator::tetZaglmayrGen(int basisType, 
				      int order){
  switch(basisType){
  case  0: return new TetNodeBasis(order);
  case  1: return new TetEdgeBasis(order);
  case  2: throw Exception("2-form not implemented on Tetrahedrons");
  case  3: throw Exception("3-form not implemented on Tetrahedrons");

  default: throw Exception("There is no %d-form", basisType);
  }  
}

Basis* BasisGenerator::hexZaglmayrGen(int basisType, 
				      int order){
  switch(basisType){
  case  0: return new HexNodeBasis(order);
  case  1: return new HexEdgeBasis(order);
  case  2: throw Exception("2-form not implemented on Hexs");
  case  3: throw Exception("3-form not implemented on Hexs");

  default: throw Exception("There is no %d-form", basisType);
  }  
}
