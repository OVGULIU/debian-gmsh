// Gmsh - Copyright (C) 1997-2008 C. Geuzaine, J.-F. Remacle
//
// See the LICENSE.txt file for license information. Please report all
// bugs and problems to <gmsh@geuz.org>.

#ifndef _ANNOTATE_H_
#define _ANNOTATE_H_

#include "Plugin.h"

extern "C"
{
  GMSH_Plugin *GMSH_RegisterAnnotatePlugin();
}

class GMSH_AnnotatePlugin : public GMSH_Post_Plugin
{
private:
  static double callback(int num, int action, double value, double *opt,
                         double step, double min, double max);
  static const char *callbackStr(int num, int action, const char *value, const char **opt);
public:
  GMSH_AnnotatePlugin(){}
  void getName(char *name) const;
  void getInfos(char *author, char *copyright, char *helpText) const;
  void catchErrorMessage(char *errorMessage) const;
  int getNbOptions() const;
  StringXNumber *getOption(int iopt);  
  int getNbOptionsStr() const;
  StringXString *getOptionStr(int iopt);  
  PView *execute(PView *);

  static double callbackX(int, int, double);
  static double callbackY(int, int, double);
  static double callbackZ(int, int, double);
  static double callback3D(int, int, double);
  static double callbackFontSize(int, int, double);
  static const char *callbackText(int, int, const char *);
  static const char *callbackFont(int, int, const char *);
  static const char *callbackAlign(int, int, const char *);
  static void draw();
};

#endif
