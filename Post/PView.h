// Gmsh - Copyright (C) 1997-2008 C. Geuzaine, J.-F. Remacle
//
// See the LICENSE.txt file for license information. Please report all
// bugs and problems to <gmsh@geuz.org>.

#ifndef _PVIEW_H_
#define _PVIEW_H_

#include <vector>
#include <string>
#include "SPoint3.h"

class PViewData;
class PViewOptions;
class VertexArray;
class smooth_normals;
class GMSH_Post_Plugin;

// A post-processing view.
class PView{
 private:
  static int _globalNum;
  // unique tag of the view (> 0)
  int _num;
  // index of the view in the current view list
  int _index;
  // flag to mark that the view has changed
  bool _changed;
  // tag of the source view if this view is an alias, zero otherwise
  int _aliasOf;
  // eye position (for transparency sorting)
  SPoint3 _eye;
  // the options
  PViewOptions *_options;
  // the data
  PViewData *_data;
  // initialize private stuff
  void _init();

 public:
  // create a new view with list-based data, allocated or not
  PView(bool allocate=true);
  // construct a new view using the given data
  PView(PViewData *data);
  // construct a new view, alias of the view "ref"
  PView(PView *ref, bool copyOptions=true);
  // construct a new list-based view from a simple 2D dataset
  PView(std::string xname, std::string yname,
        std::vector<double> &x, std::vector<double> &y);
  // default destructor
  ~PView();

  // delete the vertex arrays, used to draw the view efficiently
  void deleteVertexArrays();

  // get/set the display options
  PViewOptions *getOptions(){ return _options; }  
  void setOptions(PViewOptions *val=0);  

  // get/set the view data
  PViewData *getData(bool useAdaptiveIfAvailable=false);
  void setData(PViewData *val){ _data = val; }

  // get the view number (unique and immutable)
  int getNum(){ return _num; }

  // get/set the view index (in the view list)
  int getIndex(){ return _index; }
  void setIndex(int val){ _index = val; }

  // get/set the changed flag
  bool &getChanged(){ return _changed; }
  void setChanged(bool val);

  // check if the view is an alias ("light copy") of another view
  int getAliasOf(){ return _aliasOf; }

  // get/set the eye position (for transparency calculations)
  SPoint3 &getEye(){ return _eye; }
  void setEye(SPoint3 &p){ _eye = p; }

  // the static list of all loaded views
  static std::vector<PView*> list;

  // combine view
  static void combine(bool time, int how, bool remove);

  // find view by name (if noTimeStep >= 0, return view only if it
  // does *not* contain that timestep; if partition >=0, return view
  // only if it does *not* contain that partition)
  static PView *getViewByName(std::string name, int timeStep=-1, 
                              int partition=-1);

  // IO read routines (these are global: they can create multiple
  // views)
  static bool readPOS(std::string fileName, int fileIndex=-1);
  static bool readMSH(std::string fileName, int fileIndex=-1);
  static bool readMED(std::string fileName, int fileIndex=-1);

  // IO write routine
  bool write(std::string fileName, int format, bool append=false);

  // vertex arrays to draw the elements efficiently
  VertexArray *va_points, *va_lines, *va_triangles, *va_vectors;

  // smoothed normals
  smooth_normals *normals;
};

#endif
