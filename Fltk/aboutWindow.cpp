// Gmsh - Copyright (C) 1997-2009 C. Geuzaine, J.-F. Remacle
//
// See the LICENSE.txt file for license information. Please report all
// bugs and problems to <gmsh@geuz.org>.

#include <stdio.h>
#include <FL/Fl_Browser.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include "FlGui.h"
#include "aboutWindow.h"
#include "paletteWindow.h"
#include "CommandLine.h"
#include "StringUtils.h"
#include "OS.h"
#include "Context.h"

static void help_license_cb(Fl_Widget *w, void *data)
{
  std::string prog = FixWindowsPath(CTX::instance()->webBrowser);
  SystemCall(ReplacePercentS(prog, "http://geuz.org/gmsh/doc/LICENSE.txt"));
}

static void help_credits_cb(Fl_Widget *w, void *data)
{
  std::string prog = FixWindowsPath(CTX::instance()->webBrowser);
  SystemCall(ReplacePercentS(prog, "http://geuz.org/gmsh/doc/CREDITS.txt"));
}

static void help_hide_cb(Fl_Widget *w, void *data)
{
  ((Fl_Window*)data)->hide();
}

aboutWindow::aboutWindow()
{
  char buffer[1024];
  int width = 28 * FL_NORMAL_SIZE;
  int height = 17 * BH;

  win = new paletteWindow
    (width, height, CTX::instance()->nonModalWindows ? true : false, "About Gmsh");
  win->box(GMSH_WINDOW_BOX);

  {
    Fl_Browser *o = new Fl_Browser(0, 0, width, height - 2 * WB - BH);
    o->box(FL_FLAT_BOX);
    o->has_scrollbar(0); // no scrollbars
    o->add(" ");
    o->add("@c@b@.Gmsh");
    o->add("@c@.A three-dimensional finite element mesh generator");
    o->add("@c@.with built-in pre- and post-processing facilities");
    o->add(" ");
    o->add("@c@.Copyright (C) 1997-2009");
    o->add("@c@.Christophe Geuzaine and Jean-Francois Remacle");
    o->add(" ");
    o->add("@c@.Please send all questions and bug reports to");
    o->add("@c@b@.gmsh@geuz.org");
    o->add(" ");
    sprintf(buffer, "@c@.Version: %s", GetGmshVersion());
    o->add(buffer);
    sprintf(buffer, "@c@.License: %s", GetGmshShortLicense());
    o->add(buffer);
    sprintf(buffer, "@c@.Graphical user interface toolkit: FLTK %d.%d.%d", 
            FL_MAJOR_VERSION, FL_MINOR_VERSION, FL_PATCH_VERSION);
    o->add(buffer);
    sprintf(buffer, "@c@.Build OS: %s", GetGmshBuildOS());
    o->add(buffer);
    sprintf(buffer, "@c@.Build date: %s", GetGmshBuildDate());
    o->add(buffer);
    sprintf(buffer, "@c@.Build host: %s", GetGmshBuildHost());
    o->add(buffer);
    std::vector<std::string> lines = SplitWhiteSpace(GetGmshBuildOptions(), 30);
    for(unsigned int i = 0; i < lines.size(); i++){
      if(!i)
        sprintf(buffer, "@c@.Build options:%s", lines[i].c_str());
      else
        sprintf(buffer, "@c@.%s", lines[i].c_str());
        o->add(buffer);
      }
    sprintf(buffer, "@c@.Packaged by: %s", GetGmshPackager());
    o->add(buffer);
    o->add(" ");
    o->add("@c@.Visit http://www.geuz.org/gmsh/ for more information");
    o->add(" ");
    o->callback(help_hide_cb, (void*)win);
  }

  {
    Fl_Button *o = new Fl_Button
      (width/2 - BB - WB/2, height - BH - WB, BB, BH, "License");
    o->callback(help_license_cb);
  }

  {
    Fl_Button *o = new Fl_Button
      (width/2 + WB/2, height - BH - WB, BB, BH, "Credits");
    o->callback(help_credits_cb);
  }

  win->position(Fl::x() + Fl::w()/2 - width / 2,
                Fl::y() + Fl::h()/2 - height / 2);
  win->end();
}
