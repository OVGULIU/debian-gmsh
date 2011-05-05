#ifndef _MULTISCALE_LAPLACE_H_
#define _MULTISCALE_LAPLACE_H_

#include <vector>
#include <map>
#include <set>
#include "SPoint2.h"
#include "SPoint3.h"
#include "linearSystem.h"

class MElement;
class MVertex;

struct multiscaleLaplaceLevel {
  SPoint2 center;
  double  scale;
  double radius;
  int recur,region;
  std::vector<multiscaleLaplaceLevel*> children;
  std::vector<MElement *> elements;
  std::map<MVertex*,SPoint2> coordinates;
  std::vector<std::pair<SPoint2,multiscaleLaplaceLevel*> > cut;
  std::string _name;
};

class multiscaleLaplace{
public:
  multiscaleLaplace (std::vector<MElement *> &elements, 
                     std::map<MVertex*, SPoint3> &allCoordinates); 
  void cut (std::vector<MElement *> &elements);  
  typedef enum {HARMONIC=1,CONFORMAL=2, CONVEXCOMBINATION=3} typeOfMapping;

  linearSystem<double> *_lsys;
  multiscaleLaplaceLevel* root;
  void fillCoordinates (multiscaleLaplaceLevel & level,
                        std::map<MVertex*, SPoint3> &allCoordinates,
                        std::vector<double> &iScale, 
                        std::vector<SPoint2> &iCenter);
  void parametrize (multiscaleLaplaceLevel &); 
  void parametrize_method (multiscaleLaplaceLevel & level, 
                           std::set<MVertex*> &allNodes,
                           std::map<MVertex*,SPoint2> &solution, 
                           typeOfMapping tom);

  
};
#endif
