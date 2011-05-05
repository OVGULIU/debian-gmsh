// Gmsh - Copyright (C) 1997-2008 C. Geuzaine, J.-F. Remacle
//
// See the LICENSE.txt file for license information. Please report all
// bugs and problems to <gmsh@geuz.org>.

#ifndef _CUT_PARAMETRIC_H_
#define _CUT_PARAMETRIC_H_

#include <vector>
#include "Plugin.h"

extern "C"
{
  GMSH_Plugin *GMSH_RegisterCutParametricPlugin ();
}

class GMSH_CutParametricPlugin : public GMSH_Post_Plugin 
{ 
private:
  static double callback(int num, int action, double value, double *opt,
                         double step, double min, double max);
  static const char *callbackStr(int num, int action, const char *value, 
                                 const char **opt);
  static int fillXYZ();
  static int recompute;
  static std::vector<double> x, y, z;
public:
  GMSH_CutParametricPlugin(){}
  void getName(char *name) const;
  void getInfos(char *author, char *copyright, char *help_text) const;
  void catchErrorMessage(char *errorMessage) const;
  int getNbOptions() const;
  StringXNumber *getOption(int iopt);  
  int getNbOptionsStr() const;
  StringXString *getOptionStr(int iopt);  
  PView *execute(PView *);

  static double callbackMinU(int, int, double);
  static double callbackMaxU(int, int, double);
  static double callbackN(int, int, double);
  static double callbackConnect(int, int, double);
  static const char *callbackX(int, int, const char *);
  static const char *callbackY(int, int, const char *);
  static const char *callbackZ(int, int, const char *);
  static void draw();
};

#endif
