// Gmsh - Copyright (C) 1997-2011 C. Geuzaine, J.-F. Remacle
//
// See the LICENSE.txt file for license information. Please report all
// bugs and problems to <gmsh@geuz.org>.

#include <stdio.h>
#include <string.h>
#include <FL/Fl_Box.H>
#include <FL/Fl_Return_Button.H>
#include <FL/fl_ask.H>
#include "FlGui.h"
#include "messageWindow.h"
#include "paletteWindow.h"
#include "fileDialogs.h"
#include "GmshMessage.h"
#include "OS.h"
#include "Context.h"

void message_cb(Fl_Widget *w, void *data)
{
  FlGui::instance()->messages->show();
}

static void message_auto_scroll_cb(Fl_Widget *w, void *data)
{
  CTX::instance()->msgAutoScroll = FlGui::instance()->messages->butt->value();
}

static void message_copy_cb(Fl_Widget *w, void *data)
{
  std::string buff;
  for(int i = 1; i <= FlGui::instance()->messages->browser->size(); i++) {
    if(FlGui::instance()->messages->browser->selected(i)) {
      const char *c = FlGui::instance()->messages->browser->text(i);
      if(strlen(c) > 5 && c[0] == '@')
        buff += std::string(&c[5]);
      else
        buff += std::string(c);
      buff += "\n";
    }
  }
  // bof bof bof
  Fl::copy(buff.c_str(), buff.size(), 0);
  Fl::copy(buff.c_str(), buff.size(), 1);
}

static void message_clear_cb(Fl_Widget *w, void *data)
{
  FlGui::instance()->messages->browser->clear();
}

static void message_save_cb(Fl_Widget *w, void *data)
{
 test:
  if(fileChooser(FILE_CHOOSER_CREATE, "Save", "*")) {
    std::string name = fileChooserGetName(1);
    if(CTX::instance()->confirmOverwrite) {
      if(!StatFile(name))
        if(!fl_choice("File '%s' already exists.\n\nDo you want to replace it?", 
                      "Cancel", "Replace", NULL, name.c_str()))
          goto test;
    }
    FlGui::instance()->messages->save(name.c_str());
  }
}

messageWindow::messageWindow(int deltaFontSize)
{
  FL_NORMAL_SIZE -= deltaFontSize;

  int width = CTX::instance()->msgSize[0];
  int height = CTX::instance()->msgSize[1];

  win = new paletteWindow
    (width, height, CTX::instance()->nonModalWindows ? true : false, "Message Console");
  win->box(GMSH_WINDOW_BOX);

  browser = new Fl_Browser(0, 0, width, height - 2 * WB - BH);
  browser->box(FL_FLAT_BOX);
  browser->textfont(FL_COURIER);
  browser->textsize(FL_NORMAL_SIZE - 1);
  browser->type(FL_MULTI_BROWSER);
  browser->callback(message_copy_cb);

  {
    butt = new Fl_Check_Button
      (width - 3 * BB - 3 * WB, height - BH - WB, BB, BH, "Auto scroll");
    butt->type(FL_TOGGLE_BUTTON);
    butt->callback(message_auto_scroll_cb);
  }
  {
    Fl_Return_Button *o = new Fl_Return_Button
      (width - 2 * BB - 2 * WB, height - BH - WB, BB, BH, "Clear");
    o->callback(message_clear_cb);
  }
  {
    Fl_Button *o = new Fl_Button
      (width - BB - WB, height - BH - WB, BB, BH, "Save");
    o->callback(message_save_cb);
  }

  win->resizable(new Fl_Box(1, 1, 4, 4));
  win->size_range(3 * BB + 4 * WB, 100);

  win->position(CTX::instance()->msgPosition[0], CTX::instance()->msgPosition[1]);
  win->end();

  FL_NORMAL_SIZE += deltaFontSize;
}

void messageWindow::add(const char *msg)
{
  browser->add(msg, 0);
  if(win->shown() && CTX::instance()->msgAutoScroll)
    browser->bottomline(browser->size());
}

void messageWindow::save(const char *filename)
{
  FILE *fp = fopen(filename, "w");

  if(!fp) {
    Msg::Error("Unable to open file '%s'", filename);
    return;
  }

  Msg::StatusBar(2, true, "Writing '%s'...", filename);
  for(int i = 1; i <= browser->size(); i++) {
    const char *c = browser->text(i);
    if(c[0] == '@')
      fprintf(fp, "%s\n", &c[5]);
    else
      fprintf(fp, "%s\n", c);
  }
  Msg::StatusBar(2, true, "Done writing '%s'", filename);
  fclose(fp);
}

void messageWindow::show(bool redrawOnly)
{
  if(CTX::instance()->msgAutoScroll)
    browser->bottomline(browser->size());

  if(win->shown() && redrawOnly)
    win->redraw();
  else
    win->show();
}
