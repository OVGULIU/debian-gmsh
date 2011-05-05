// Gmsh - Copyright (C) 1997-2009 C. Geuzaine, J.-F. Remacle
//
// See the LICENSE.txt file for license information. Please report all
// bugs and problems to <gmsh@geuz.org>.

#include <string.h>
#include <FL/Fl_Tabs.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Return_Button.H>
#include <FL/Fl_Browser.H>
#include "GmshMessage.h"
#include "GmshRemote.h"
#include "GmshSocket.h"
#include "GModel.h"
#include "PView.h"
#include "OpenFile.h"
#include "drawContext.h"
#include "FlGui.h"
#include "menuWindow.h"
#include "solverWindow.h"
#include "paletteWindow.h"
#include "messageWindow.h"
#include "fileDialogs.h"
#include "StringUtils.h"
#include "Options.h"
#include "OS.h"
#include "Context.h"

class FlGmshServer : public GmshServer{
 private:
  GmshRemote *_remote;
 public:
  FlGmshServer(GmshRemote *remote) : _remote(remote), GmshServer() {}
  ~FlGmshServer() {}
  int SystemCall(const char *str)
  { 
    return ::SystemCall(str); 
  }
  int NonBlockingWait(int socket, double waitint, double timeout)
  { 
    // This routine polls the socket at least every 'waitint' seconds,
    // and for at most timout seconds (or indefinitely if timeout ==
    // 0). The routine returns 0 as soon as data is available and 1 if
    // there was en error or if the process was killed. Otherwise it
    // just tends to current GUI events. This is easier to manage than
    // non-blocking IO, and simpler than using threads. Another
    // possibility would be to use Fl::add_fd().
    double start = GetTimeInSeconds();
    while(1){
      if(timeout > 0 && GetTimeInSeconds() - start > timeout)
        return 2; // timout

      if((_remote->executable.size() && _remote->getPid() < 0) ||
         (_remote->executable.empty() && !CTX::instance()->solver.listen))
        return 1; // process has been killed or we stopped listening

      // check if there is data (call select with a zero timeout to
      // return immediately, i.e., do polling)
      int ret = Select(0, 0, socket);

      if(ret == 0){ 
        // nothing available: wait at most waitint seconds, and in the
        // meantime respond to FLTK events
        FlGui::instance()->wait(waitint);
      }
      else if(ret > 0){ 
        return 0; // data is there!
      }
      else{ 
        // an error happened
        _remote->setPid(-1);
        _remote->setServer(0);
        return 1;
      }
    }
  }
};

void GmshRemote::run(std::string args)
{
 new_connection:
  
  std::string prog = FixWindowsPath(executable);
  
  if(!clientServer) {
    std::string command = prog + " " + args;
#if !defined(WIN32)
    command += " &";
#endif
    SystemCall(command);
    return;
  }

  // find solver num
  int solverNum = 0;
  for(std::map<int, GmshRemote*>::iterator it = _all.begin(); 
      it != _all.end(); it++){
    if(this == it->second) break;
    solverNum++;
  }

  // make command buttons inactive while running
  if(solverNum >= 0 && solverNum < NB_SOLVER_MAX){
    for(unsigned int i = 0; i < buttonName.size(); i++)
      if(buttonName[i].size())
        FlGui::instance()->solver[solverNum]->command[i]->deactivate();
  }
  
  _pid = 0;
  _server = 0;
  FlGmshServer *server = new FlGmshServer(this);
 
  std::string sockname = getSocketName();
  std::string command;

  if(prog.size()){
    command = prog + " " + args + " " + ReplacePercentS
      (socketSwitch, std::string("\"") + sockname + "\"");
#if !defined(WIN32)
    command += " &";
#endif
  }
  
  int sock;
  try{
    sock = server->Start(command.c_str(), sockname.c_str(), 5.);
  }
  catch(const char *err){
    Msg::Error("%s (on socket '%s')", err, sockname.c_str());
    sock = -1;
  }
  
  if(sock < 0){
    server->Shutdown();
    delete server;
    // reactivate buttons  
    for(unsigned int i = 0; i < buttonName.size(); i++)
      if(buttonName[i].size())
        FlGui::instance()->solver[solverNum]->command[i]->activate();
    return;
  }

  Msg::Info("Running '%s'...", name.c_str());

  bool initOption[5] = {true, true, true, true, true};  
  while(1) {

    if((prog.size() && _pid < 0) || 
       (prog.empty() && !CTX::instance()->solver.listen)) break;
    
    int stop = server->NonBlockingWait(sock, 0.1, 0.);
    
    if(stop || (prog.size() && _pid < 0) || 
       (prog.empty() && !CTX::instance()->solver.listen)) break;
    
    int type, length;
    if(server->ReceiveHeader(&type, &length)){
      double timer = GetTimeInSeconds();
      char *message = new char[length + 1];
      if(server->ReceiveString(length, message)){
        switch (type) {
        case GmshSocket::GMSH_START:
          _pid = atoi(message);
          _server = server;
          break;
        case GmshSocket::GMSH_STOP:
          _pid = -1;
          _server = 0;
          break;
        case GmshSocket::GMSH_PROGRESS:
          Msg::StatusBar(2, false, "%s %s", name.c_str(), message);
          break;
        case GmshSocket::GMSH_OPTION_1:
        case GmshSocket::GMSH_OPTION_2:
        case GmshSocket::GMSH_OPTION_3:
        case GmshSocket::GMSH_OPTION_4:
        case GmshSocket::GMSH_OPTION_5:
          {
            int i = (int)type - (int)GmshSocket::GMSH_OPTION_1;
            if(initOption[i]){
              optionValue[i].clear();
              initOption[i] = false;
            }
            optionValue[i].push_back(message);
          }
          break;
        case GmshSocket::GMSH_MERGE_FILE:
          if(mergeViews) {
            int n = PView::list.size();
            MergeFile(message);
            drawContext::global()->draw();
            if(n != (int)PView::list.size()) 
              FlGui::instance()->menu->setContext(menu_post, 0);
          }
          break;
        case GmshSocket::GMSH_PARSE_STRING:
          ParseString(message);
          drawContext::global()->draw();
          break;
        case GmshSocket::GMSH_INFO:
          Msg::Direct("%-8.8s: %s", name.c_str(), message);
          break;
        case GmshSocket::GMSH_WARNING:
          Msg::Direct(2, "%-8.8s: %s", name.c_str(), message);
          break;
        case GmshSocket::GMSH_ERROR:
          Msg::Direct(1, "%-8.8s: %s", name.c_str(), message);
          break;
        case GmshSocket::GMSH_SPEED_TEST:
          Msg::Info("got %d Mb message in %g seconds", strlen(message) 
                    / 1024 / 1024, GetTimeInSeconds() - timer);
          break;
        case GmshSocket::GMSH_VERTEX_ARRAY:
          PView::fillVertexArray(length, message);
          drawContext::global()->draw();
          break;
        default:
          Msg::Warning("Unknown message type");
          Msg::Direct("%-8.8s: %s", name.c_str(), message);
          break;
        }
        FlGui::instance()->check();
      }
      else{
        Msg::Warning("Failed to receive message body on socket: aborting");
        break;
      }
      delete [] message;
    }
    else{
      // didn't get any header, just abort
      break;
    }
  }

  if(solverNum >= 0 && solverNum < NB_SOLVER_MAX){
    // some options have been changed: refill the menus
    if(!initOption[0] || !initOption[1] || !initOption[2] || !initOption[3] ||
       !initOption[4]){
      for(unsigned int i = 0; i < optionName.size(); i++) {
        if(optionName[i].empty()) break;
        FlGui::instance()->solver[solverNum]->choice[i]->clear();
        for(unsigned int j = 0; j < optionValue[i].size(); j++)
          FlGui::instance()->solver[solverNum]->choice[i]->add(optionValue[i][j].c_str());
        FlGui::instance()->solver[solverNum]->choice[i]->value(0);
      }
    }
    // reactivate buttons  
    for(unsigned int i = 0; i < buttonName.size(); i++)
      if(buttonName[i].size())
        FlGui::instance()->solver[solverNum]->command[i]->activate();
  }
  
  _server = 0;
  server->Shutdown();
  delete server;

  Msg::Info("...done running '%s'", name.c_str());

  if(prog.empty()){
    Msg::Info("Client disconnected: starting new connection");
    goto new_connection;
  }
}

void solver_cb(Fl_Widget *w, void *data)
{
  int num = (int)(long)data;

  std::vector<std::string> split = SplitFileName(GModel::current()->getFileName());

  // if the input file field is empty, fill it with a name guessed
  // from the current model name (only if this file actually exists)
  if(!strlen(FlGui::instance()->solver[num]->input[0]->value())){
    std::string inputFile = split[0] + split[1] + 
      GmshRemote::get(num)->inputFileExtension;
    if(!StatFile(inputFile)){
      FlGui::instance()->solver[num]->input[0]->value(inputFile.c_str());
      GmshRemote::get(num)->inputFileName = inputFile;
    }
  }

  // if the mesh file field is empty, fill it with a name guessed with
  // from the current model name
  if(!strlen(FlGui::instance()->solver[num]->input[1]->value())){
    std::string meshFile = split[0] + split[1] + ".msh";
    FlGui::instance()->solver[num]->input[1]->value(meshFile.c_str());
    GmshRemote::get(num)->meshFileName = meshFile;
  }

  // show the window before calling Solver() to avoid race condition on
  // Windows (if the message window pops up due to an error, the window
  // callbacks get messed up)
  FlGui::instance()->solver[num]->win->show();

  GmshRemote::get(num)->runToGetOptions();
}

static void solver_ok_cb(Fl_Widget *w, void *data)
{
  int num = (int)(long)data;

  opt_solver_client_server
    (num, GMSH_SET, FlGui::instance()->solver[num]->menu->menu()[0].value() ? 1 : 0);
  opt_solver_popup_messages
    (num, GMSH_SET, FlGui::instance()->solver[num]->menu->menu()[1].value() ? 1 : 0);
  opt_solver_merge_views
    (num, GMSH_SET, FlGui::instance()->solver[num]->menu->menu()[2].value() ? 1 : 0);

  opt_solver_mesh_name
    (num, GMSH_SET, FlGui::instance()->solver[num]->input[1]->value());

  bool retry = false;

  std::string input = FlGui::instance()->solver[num]->input[0]->value();
  if(opt_solver_input_name(num, GMSH_GET, "") != input) retry = true;
  opt_solver_input_name(num, GMSH_SET, input);

  std::string exe = FlGui::instance()->solver[num]->input[2]->value();
  if(opt_solver_executable(num, GMSH_GET, "") != exe) retry = true;
  opt_solver_executable(num, GMSH_SET, exe);

  if(retry) solver_cb(0, data);
}

static void solver_choose_executable_cb(Fl_Widget *w, void *data)
{
  int num = (int)(long)data;
  std::string pattern = "*";
#if defined(WIN32)
  pattern += ".exe";
#endif

  if(file_chooser(0, 0, "Choose", pattern.c_str())){
    FlGui::instance()->solver[num]->input[2]->value(file_chooser_get_name(1).c_str());
    solver_ok_cb(w, data);
  }
}

static void solver_file_open_cb(Fl_Widget *w, void *data)
{
  int num = (int)(long)data;
  std::string pattern = "*" + GmshRemote::get(num)->inputFileExtension;

  if(file_chooser(0, 0, "Choose", pattern.c_str())) {
    FlGui::instance()->solver[num]->input[0]->value(file_chooser_get_name(1).c_str());
    solver_ok_cb(w, data);
  }
}

static void solver_file_edit_cb(Fl_Widget *w, void *data)
{
  int num = (int)(long)data;
  std::string prog = FixWindowsPath(CTX::instance()->editor);
  std::string file = FixWindowsPath(FlGui::instance()->solver[num]->input[0]->value());
  SystemCall(ReplacePercentS(prog, file));
}

static void solver_choose_mesh_cb(Fl_Widget *w, void *data)
{
  int num = (int)(long)data;
  if(file_chooser(0, 0, "Choose", "*.msh")){
    FlGui::instance()->solver[num]->input[1]->value(file_chooser_get_name(1).c_str());
    solver_ok_cb(w, data);
  }
}

static int numPercentS(std::string &in)
{
  int n = 0;
  for(int i = 0; i < (int)in.size() - 1; i++) {
    if(in[i] == '%' && in[i + 1] == 's') {
      i++;
      n++;
    }
  }
  return n;
}

static void solver_command_cb(Fl_Widget *w, void *data)
{
  int num = ((int *)data)[0];
  int idx = ((int *)data)[1];

  if(GmshRemote::get(num)->popupMessages)
    FlGui::instance()->messages->show(true);

  GmshRemote::get(num)->inputFileName =
    FlGui::instance()->solver[num]->input[0]->value();

  GmshRemote::get(num)->meshFileName = 
    FlGui::instance()->solver[num]->input[1]->value();

  int optionIndex = 0;
  for(int i = 0; i < idx; i++)
    optionIndex += numPercentS(GmshRemote::get(num)->buttonSwitch[i]);

  int optionChoice = FlGui::instance()->solver[num]->choice[optionIndex]->value();
  GmshRemote::get(num)->runCommand(idx, optionIndex, optionChoice);
}

static void solver_kill_cb(Fl_Widget *w, void *data)
{
  int num = (int)(long)data;
  GmshRemote::get(num)->kill();
}

solverWindow::solverWindow(int solverIndex, int deltaFontSize)
{
  FL_NORMAL_SIZE -= deltaFontSize;

  int numOptions = GmshRemote::get(solverIndex)->optionName.size();
  for(unsigned int i = 0; i < GmshRemote::get(solverIndex)->optionName.size(); i++){
    if(GmshRemote::get(solverIndex)->optionName[i].empty()){
      numOptions = i;
      break;
    }
  }

  int width = 32 * FL_NORMAL_SIZE;
  int height = (5 + numOptions) * BH + 5 * WB;
  int BBS = (width - 9 * WB) / 6;
  int LL = width - (int)(2.75 * BBS);
  
  win = new paletteWindow
    (width, height, CTX::instance()->nonModalWindows ? true : false, "Solver");
  win->box(GMSH_WINDOW_BOX);
  {
    Fl_Tabs *o = new Fl_Tabs
      (WB, WB, width - 2 * WB, height - 2 * WB);
    {
      Fl_Group *g = new Fl_Group
        (WB, WB + BH, width - 2 * WB, height - 2 * WB - BH, "Controls");

      menu = new Fl_Menu_Button
        (2 * WB, 2 * WB + 1 * BH, BBS / 2, BH);
      menu->add("Client-server", 0, 0, 0, FL_MENU_TOGGLE);
      menu->add("Pop-up messages",  0, 0, 0, FL_MENU_TOGGLE);
      menu->add("Auto-load results", 0, 0, 0, FL_MENU_TOGGLE);
      menu->callback(solver_ok_cb, (void *)solverIndex);

      input[2] = new Fl_Input
        (2 * WB + BBS / 2, 2 * WB + 1 * BH, LL - BBS / 2, BH, "Command");
      input[2]->callback(solver_ok_cb, (void *)solverIndex);

      Fl_Button *b1 = new Fl_Button
        (width - 2 * WB - BBS, 2 * WB + 1 * BH, BBS, BH, "Choose");
      b1->callback(solver_choose_executable_cb, (void *)solverIndex);

      Fl_Button *b4 = new Fl_Button
        (2 * WB, 2 * WB + 2 * BH, BBS, BH, "Edit");
      b4->callback(solver_file_edit_cb, (void *)solverIndex);

      input[0] = new Fl_Input
        (2 * WB + BBS, 2 * WB + 2 * BH, LL - BBS, BH, "Input file");
      input[0]->callback(solver_ok_cb, (void *)solverIndex);

      Fl_Button *b3 = new Fl_Button
        (width - 2 * WB - BBS, 2 * WB + 2 * BH, BBS, BH, "Choose");
      b3->callback(solver_file_open_cb, (void *)solverIndex);

      input[1] = new Fl_Input
        (2 * WB, 2 * WB + 3 * BH, LL, BH, "Mesh file");
      input[1]->callback(solver_ok_cb, (void *)solverIndex);

      Fl_Button *b5 = new Fl_Button
        (width - 2 * WB - BBS, 2 * WB + 3 * BH, BBS, BH, "Choose");
      b5->callback(solver_choose_mesh_cb, (void *)solverIndex);

      for(int i = 0; i < 3; i++) {
        input[i]->align(FL_ALIGN_RIGHT);
      }

      for(int i = 0; i < numOptions; i++) {
        choice[i] = new Fl_Choice
          (2 * WB, 2 * WB + (4 + i) * BH, LL, BH, 
           GmshRemote::get(solverIndex)->optionName[i].c_str());
        choice[i]->align(FL_ALIGN_RIGHT);
      }

      static int arg[NB_SOLVER_MAX][5][2];
      for(unsigned int i = 0; i < GmshRemote::get(solverIndex)->buttonName.size(); i++) {
        if(GmshRemote::get(solverIndex)->buttonName[i].size()){
          arg[solverIndex][i][0] = solverIndex;
          arg[solverIndex][i][1] = i;
          command[i] = new Fl_Button
            ((2 + i) * WB + i * BBS, 3 * WB + (4 + numOptions) * BH,
             BBS, BH, GmshRemote::get(solverIndex)->buttonName[i].c_str());
          command[i]->callback(solver_command_cb, (void *)arg[solverIndex][i]);
        }
      }

      {
        Fl_Button *b = new Fl_Button
          (width - 2 * WB - BBS, 3 * WB + (4 + numOptions) * BH, BBS, BH, "Kill");
        b->callback(solver_kill_cb, (void *)solverIndex);
      }
     
      g->end();
    }
    {
      Fl_Group *g = new Fl_Group
        (WB, WB + BH, width - 2 * WB, height - 2 * WB, "About");

      Fl_Browser *o = new Fl_Browser
        (2 * WB, 2 * WB + 1 * BH, width - 4 * WB, height - 4 * WB - BH);
      o->add(" ");
      add_multiline_in_browser
        (o, "@c@b@.", GmshRemote::get(solverIndex)->name.c_str(), false);
      o->add(" ");
      add_multiline_in_browser
        (o, "@c@. ", GmshRemote::get(solverIndex)->help.c_str(), false);

      g->end();
    }
    o->end();
  }

  win->position
    (CTX::instance()->solverPosition[0], CTX::instance()->solverPosition[1]);
  win->end();

  FL_NORMAL_SIZE += deltaFontSize;
}
