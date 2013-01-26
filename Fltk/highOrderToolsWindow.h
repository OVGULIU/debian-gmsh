// Gmsh - Copyright (C) 1997-2013 C. Geuzaine, J.-F. Remacle
//
// See the LICENSE.txt file for license information. Please report all
// bugs and problems to the public mailing list <gmsh@geuz.org>.

#ifndef _HIGHORDERTOOLS_WINDOW_H_
#define _HIGHORDERTOOLS_WINDOW_H_

#include <FL/Fl_Window.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Browser.H>
#include <FL/Fl_Multi_Browser.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Value_Input.H>
#include <FL/Fl_Output.H>

class highOrderToolsWindow{
 public:
  bool CAD, complete;
  int meshOrder;
  Fl_Window *win;
  Fl_Check_Button *butt[20];
  Fl_Value_Input *value[20];
  Fl_Choice *choice[20];
  Fl_Button *push[20];
  Fl_Output *output[20];
  Fl_Browser *messages;
 public:
  highOrderToolsWindow(int deltaFontSize=0);
  void show(bool redrawOnly);
};

void highordertools_cb(Fl_Widget *w, void *data);

#endif
