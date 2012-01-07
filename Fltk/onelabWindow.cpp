// Gmsh - Copyright (C) 1997-2011 C. Geuzaine, J.-F. Remacle
//
// See the LICENSE.txt file for license information. Please report all
// bugs and problems to <gmsh@geuz.org>.

#include <FL/Fl.H>
#include "GmshConfig.h"
#include "GmshMessage.h"

#if defined(HAVE_ONELAB)
#include "onelab.h"
#endif

#if defined(HAVE_ONELAB) && (FL_MAJOR_VERSION == 1) && (FL_MINOR_VERSION == 3)

#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Input_Choice.H>
#include <FL/fl_ask.H>
#include "inputRange.h"
#include "Context.h"
#include "GModel.h"
#include "GmshDefines.h"
#include "Options.h"
#include "OS.h"
#include "StringUtils.h"
#include "OpenFile.h"
#include "CreateFile.h"
#include "drawContext.h"
#include "PView.h"
#include "PViewData.h"
#include "PViewOptions.h"
#include "FlGui.h"
#include "paletteWindow.h"
#include "menuWindow.h"
#include "fileDialogs.h"
#include "onelabWindow.h"

// This file contains the Gmsh/FLTK specific parts of the OneLab
// interface. You'll need to reimplement this if you plan to build
// a different OneLab server.

class onelabGmshServer : public GmshServer{
 private:
  onelab::localNetworkClient *_client;
 public:
  onelabGmshServer(onelab::localNetworkClient *client) : GmshServer(), _client(client) {}
  ~onelabGmshServer() {}
  int SystemCall(const char *str)
  { 
    return ::SystemCall(str); 
  }
  int NonBlockingWait(int socket, double waitint, double timeout)
  { 
    double start = GetTimeInSeconds();
    while(1){
      if(timeout > 0 && GetTimeInSeconds() - start > timeout)
        return 2; // timout

      if(_client->getPid() < 0 ||
         (_client->getCommandLine().empty() &&  !CTX::instance()->solver.listen))
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
        _client->setPid(-1);
        _client->setGmshServer(0);
        return 1;
      }
    }
  }
};

bool onelab::localNetworkClient::run(const std::string &what)
{
 new_connection:
  _pid = 0;
  _gmshServer = 0;

  onelabGmshServer *server = new onelabGmshServer(this);

  std::string sockname;
  std::ostringstream tmp;
  if(!strstr(CTX::instance()->solver.socketName.c_str(), ":")){
    // Unix socket
    tmp << CTX::instance()->homeDir << CTX::instance()->solver.socketName << getId();
    sockname = FixWindowsPath(tmp.str());
  }
  else{
    // TCP/IP socket
    if(CTX::instance()->solver.socketName.size() && 
       CTX::instance()->solver.socketName[0] == ':')
      tmp << GetHostName(); // prepend hostname if only the port number is given
    tmp << CTX::instance()->solver.socketName << getId();
    sockname = tmp.str();
  }

  std::string command = FixWindowsPath(_commandLine);
  if(command.size()){
    command += " " + what + " " + _socketSwitch + " ";
  }
  else{
    Msg::Info("Listening on socket '%s'", sockname.c_str());
  }

  int sock;
  try{
    sock = server->Start(command.c_str(), sockname.c_str(),
                         CTX::instance()->solver.timeout);
  }
  catch(const char *err){
    Msg::Error("%s (on socket '%s')", err, sockname.c_str());
    sock = -1;
  }
  
  if(sock < 0){
    server->Shutdown();
    delete server;
    return false;
  }

  Msg::StatusBar(2, true, "Running '%s'...", _name.c_str());

  while(1) {

    if(_pid < 0 || (command.empty() && !CTX::instance()->solver.listen))
      break;
    
    int stop = server->NonBlockingWait(sock, 0.1, 0.);

    if(stop || _pid < 0 || (command.empty() && !CTX::instance()->solver.listen))
      break;

    double timer = GetTimeInSeconds();
    
    int type, length, swap;
    if(!server->ReceiveHeader(&type, &length, &swap)){
      Msg::Error("Did not receive message header: stopping server");
      break;
    }

    std::string message(length, ' ');
    if(!server->ReceiveMessage(length, &message[0])){
      Msg::Error("Did not receive message body: stopping server");
      break;
    }

    switch (type) {
    case GmshSocket::GMSH_START:
      _pid = atoi(message.c_str());
      _gmshServer = server;
      break;
    case GmshSocket::GMSH_STOP:
      _pid = -1;
      _gmshServer = 0;
      break;
    case GmshSocket::GMSH_PARAMETER:
      {
        std::string version, type, name;
        onelab::parameter::getInfoFromChar(message, version, type, name);
        if(type == "number"){
          onelab::number p;
          p.fromChar(message);
          set(p);
        }
        else if(type == "string"){
          onelab::string p;
          p.fromChar(message);
          set(p);
        }
        else
          Msg::Error("FIXME not done for this parameter type: %s", type.c_str());
      }
      break;
    case GmshSocket::GMSH_PARAMETER_QUERY:
      {
        std::string version, type, name, reply;
        onelab::parameter::getInfoFromChar(message, version, type, name);
        if(type == "number"){
          std::vector<onelab::number> par;
          get(par, name);
          if(par.size() == 1) reply = par[0].toChar();
        }
        else if(type == "string"){
          std::vector<onelab::string> par;
          get(par, name);
          if(par.size() == 1) reply = par[0].toChar();
        }
        else
          Msg::Error("FIXME query not done for this parameter type");
        if(reply.size()){
          server->SendMessage(GmshSocket::GMSH_PARAMETER, reply.size(), &reply[0]);
        }
        else{
          reply = "Parameter '" + name + "' not found";
          server->SendMessage(GmshSocket::GMSH_INFO, reply.size(), &reply[0]);
        }
      }
      break;
    case GmshSocket::GMSH_PARAM_QUERY_ALL:
      {
        std::string version, type, name, reply;
        onelab::parameter::getInfoFromChar(message, version, type, name);
	if(type == "number"){
	  std::vector<onelab::number> numbers;
	  get(numbers);
	  for(std::vector<onelab::number>::iterator it = numbers.begin(); 
              it != numbers.end(); it++){
	    reply = (*it).toChar();
	    server->SendMessage(GmshSocket::GMSH_PARAM_QUERY_ALL, reply.size(), &reply[0]);
	  }
	  reply = "OneLab: sent all numbers";
	  server->SendMessage(GmshSocket::GMSH_PARAM_QUERY_END, reply.size(), &reply[0]);
	}
	else if(type == "string"){
	  std::vector<onelab::string> strings;
	  get(strings);
	  for(std::vector<onelab::string>::iterator it = strings.begin();
              it != strings.end(); it++){
	    reply = (*it).toChar();
	    server->SendMessage(GmshSocket::GMSH_PARAM_QUERY_ALL, reply.size(), &reply[0]);
	  }
	  reply = "OneLab: sent all strings";
	  server->SendMessage(GmshSocket::GMSH_PARAM_QUERY_END, reply.size(), &reply[0]);
	}
        else
          Msg::Error("FIXME query not done for this parameter type: %s", type.c_str());
      }
      break;
    case GmshSocket::GMSH_PROGRESS:
      Msg::StatusBar(2, false, "%s %s", _name.c_str(), message.c_str());
      break;
    case GmshSocket::GMSH_INFO:
      Msg::Direct("%-8.8s: %s", _name.c_str(), message.c_str());
      break;
    case GmshSocket::GMSH_WARNING:
      Msg::Direct(2, "%-8.8s: %s", _name.c_str(), message.c_str());
      break;
    case GmshSocket::GMSH_ERROR:
      Msg::Direct(1, "%-8.8s: %s", _name.c_str(), message.c_str());
      break;
    case GmshSocket::GMSH_MERGE_FILE:
      {
        int n = PView::list.size();
        for(int i = 0; i < n; i++)
          PView::list[i]->getOptions()->visible = 0;
        MergeFile(message);
        if(FlGui::instance()->onelab->hideNewViews()){
          for(int i = n; i < PView::list.size(); i++) 
            PView::list[i]->getOptions()->visible = 0;
        }
        drawContext::global()->draw();
        if(n != (int)PView::list.size()) 
          FlGui::instance()->menu->setContext(menu_post, 0);
      }
      break;
    case GmshSocket::GMSH_PARSE_STRING:
      ParseString(message);
      drawContext::global()->draw();
      break;
    case GmshSocket::GMSH_SPEED_TEST:
      Msg::Info("got %d Mb message in %g seconds",
                length / 1024 / 1024, GetTimeInSeconds() - timer);
      break;
    case GmshSocket::GMSH_VERTEX_ARRAY:
      {
        int n = PView::list.size();
        PView::fillVertexArray(this, length, &message[0], swap);
        FlGui::instance()->updateViews(n != (int)PView::list.size());
        drawContext::global()->draw();
      }
      break;
    default:
      Msg::Warning("Received unknown message type (%d)", type);
      break;
    } 

    FlGui::instance()->check();
  }

  _gmshServer = 0;
  server->Shutdown();
  delete server;

  Msg::StatusBar(2, true, "Done running '%s'", _name.c_str());

  if(command.empty()){
    Msg::Info("Client disconnected: starting new connection");
    goto new_connection;
  }

  return true;
}

bool onelab::localNetworkClient::kill()
{
  if(_pid > 0) {
    if(KillProcess(_pid)){
      Msg::Info("Killed '%s' (pid %d)", _name.c_str(), _pid);
      _pid = -1;
      return true; 
    }
  }
  _pid = -1;
  return false;
}

static std::string getMshFileName(onelab::client *c)
{
  std::vector<onelab::string> ps;
  c->get(ps, "Gmsh/MshFileName");
  if(ps.size()){
    return ps[0].getValue();
  }
  else{
    std::string name = CTX::instance()->outputFileName;
    if(name.empty()){
      if(CTX::instance()->mesh.fileFormat == FORMAT_AUTO)
        name = GetDefaultFileName(FORMAT_MSH);
      else
        name = GetDefaultFileName(CTX::instance()->mesh.fileFormat);
    }
    onelab::string o("Gmsh/MshFileName", name, "Mesh name");
    o.setKind("file");
    c->set(o);
    return name;
  }
}

static std::string getModelName(onelab::client *c)
{
  std::vector<onelab::string> ps;
  c->get(ps, c->getName() + "/1ModelName");
  if(ps.size()){
    return ps[0].getValue();
  }
  else{
    std::vector<std::string> split = SplitFileName(GModel::current()->getFileName());
    std::string ext = FlGui::instance()->onelab->getModelExtension();
    std::string name(split[0] + split[1] + ext);
    onelab::string o(c->getName() + "/1ModelName", name, "Model name");
    o.setKind("file");
    c->set(o);
    return name;
  }
}

static void initializeLoop(std::string level)
{
  bool changed = false;
  std::vector<onelab::number> numbers;
  onelab::server::instance()->get(numbers);
  for(unsigned int i = 0; i < numbers.size(); i++){
    if(numbers[i].getAttribute("Loop") == level){
      if(numbers[i].getChoices().size() > 1){
        numbers[i].setValue(numbers[i].getChoices()[0]);
        onelab::server::instance()->set(numbers[i]);
        changed = true;
      }
      else if(numbers[i].getMin() != -onelab::parameter::maxNumber() &&
              numbers[i].getStep()){
        numbers[i].setValue(numbers[i].getMin());
        onelab::server::instance()->set(numbers[i]);
        changed = true;
      }
    }
  }

  // force this to make sure that we remesh, even if a mesh exists and
  // we did not actually change a Gmsh parameter
  if(changed)
    onelab::server::instance()->setChanged(true, "Gmsh");
}

static bool incrementLoop(std::string level)
{
  bool recompute = false, loop = false;
  std::vector<onelab::number> numbers;
  onelab::server::instance()->get(numbers);
  for(unsigned int i = 0; i < numbers.size(); i++){
    if(numbers[i].getAttribute("Loop") == level){
      loop = true;
      if(numbers[i].getChoices().size() > 1){
        // FIXME should store loopVariable attribute in the parameter
        // -- the following test will loop forever if 2 values are
        // identical in the list of choices
        std::vector<double> choices(numbers[i].getChoices());
        for(unsigned int j = 0; j < choices.size() - 1; j++){
          if(numbers[i].getValue() == choices[j]){
            numbers[i].setValue(choices[j + 1]);
            onelab::server::instance()->set(numbers[i]);
            Msg::Info("Recomputing with new choice %s=%g", 
                      numbers[i].getName().c_str(), numbers[i].getValue());
            recompute = true;
            break;
          }
        }
      }
      else if(numbers[i].getMax() != onelab::parameter::maxNumber() &&
              numbers[i].getValue() < numbers[i].getMax() && 
              numbers[i].getStep()){
        numbers[i].setValue(numbers[i].getValue() + numbers[i].getStep());
        onelab::server::instance()->set(numbers[i]);
        Msg::Info("Recomputing with new step %s=%g",
                  numbers[i].getName().c_str(), numbers[i].getValue());
        recompute = true;
      }
    }
  }

  if(loop && !recompute) // end of this loop level
    initializeLoop(level);

  return recompute;
}

static void initializeLoop()
{
  initializeLoop("1");
  initializeLoop("2");
  initializeLoop("3");

  if(onelab::server::instance()->getChanged())
    FlGui::instance()->onelab->rebuildTree();
}

static bool incrementLoop()
{
  bool ret = false;
  if(incrementLoop("3"))      ret = true;
  else if(incrementLoop("2")) ret = true;
  else if(incrementLoop("1")) ret = true;

  if(onelab::server::instance()->getChanged())
    FlGui::instance()->onelab->rebuildTree();
  
  return ret;
}

static std::vector<double> getRange(onelab::number &p)
{
  std::vector<double> v;
  if(p.getChoices().size()){
    v = p.getChoices();
  }
  else if(p.getMin() != -onelab::parameter::maxNumber() &&
          p.getMax() != onelab::parameter::maxNumber() && p.getStep()){
    for(double d = p.getMin(); d <= p.getMax(); d += p.getStep())
      v.push_back(d);
  }
  return v;
}

static std::string getShortName(const std::string &name, const std::string &ok="") 
{
  if(ok.size()) return ok;
  std::string s = name;
  // remove path
  std::string::size_type last = name.find_last_of('/');
  if(last != std::string::npos)
    s = name.substr(last + 1);
  // remove starting numbers
  while(s.size() && s[0] >= '0' && s[0] <= '9')
    s = s.substr(1);
  return s;
}

static bool updateOnelabGraph(int num)
{
  bool changed = false;
  for(unsigned int i = 0; i < PView::list.size(); i++){
    if(PView::list[i]->getData()->getFileName() == "OneLab" + num){
      delete PView::list[i];
      changed = true;
      break;
    }
  }
  
  std::vector<double> x, y;
  std::string xName, yName;
  std::vector<onelab::number> numbers;
  onelab::server::instance()->get(numbers);
  for(unsigned int i = 0; i < numbers.size(); i++){
    std::string v = numbers[i].getAttribute("Graph");
    v.resize(8, '0');
    if(v[2 * num] == '1'){
      x = getRange(numbers[i]);
      xName = getShortName(numbers[i].getName(), numbers[i].getShortHelp());
    }
    if(v[2 * num + 1] == '1'){
      y = getRange(numbers[i]);
      yName = getShortName(numbers[i].getName(), numbers[i].getShortHelp());
    }
  }
  if(x.empty()){
    xName.clear();
    for(unsigned int i = 0; i < y.size(); i++) x.push_back(i);
  }
  if(x.size() && y.size()){
    PView *v = new PView(xName, yName, x, y);
    v->getData()->setFileName("OneLab" + num);
    v->getOptions()->intervalsType = PViewOptions::Discrete;
    changed = true;
  }
  
  if(changed)
    FlGui::instance()->updateViews();
  return changed;
}

static void updateOnelabGraphs()
{
  bool redraw0 = updateOnelabGraph(0);
  bool redraw1 = updateOnelabGraph(1);
  bool redraw2 = updateOnelabGraph(2);
  bool redraw3 = updateOnelabGraph(3);
  if(redraw0 || redraw1 || redraw2 || redraw3)
    drawContext::global()->draw();
}

static void runGmshClient(const std::string &action)
{
  onelab::server::citer it = onelab::server::instance()->findClient("Gmsh");
  if(it == onelab::server::instance()->lastClient()) return;

  onelab::client *c = it->second;
  std::string mshFileName = getMshFileName(c);
  static std::string modelName = "";
  if(modelName.empty()){
    // first pass is special to prevent model reload, as well as
    // remeshing if a mesh file already exists on disk
    modelName = GModel::current()->getName();
    if(!StatFile(mshFileName))
      onelab::server::instance()->setChanged(false, "Gmsh");
  }

  if(action == "check"){
    if(onelab::server::instance()->getChanged("Gmsh") ||
       modelName != GModel::current()->getName()){
      // reload geometry if Gmsh parameters have been modified or if
      // the model name has changed
      modelName = GModel::current()->getName();
      geometry_reload_cb(0, 0);
    }
  }
  else if(action == "compute"){
    if(onelab::server::instance()->getChanged("Gmsh") ||
       modelName != GModel::current()->getName()){
      // reload the geometry, mesh it and save the mesh if Gmsh
      // parameters have been modified or if the model name has
      // changed
      modelName = GModel::current()->getName();
      geometry_reload_cb(0, 0);
      if(FlGui::instance()->onelab->meshAuto()){
        mesh_3d_cb(0, 0);
        CreateOutputFile(mshFileName, CTX::instance()->mesh.fileFormat);
      }
    }
    else if(StatFile(mshFileName)){
      // mesh+save if the mesh file does not exist
      if(FlGui::instance()->onelab->meshAuto()){
        mesh_3d_cb(0, 0);
        CreateOutputFile(mshFileName, CTX::instance()->mesh.fileFormat);
      }
    }
    onelab::server::instance()->setChanged(false, "Gmsh");
  }
}

void onelab_cb(Fl_Widget *w, void *data)
{
  if(!data) return;
  std::string action((const char*)data);

  if(action == "stop"){
    FlGui::instance()->onelab->stop(true);
    FlGui::instance()->onelab->setButtonMode("kill");
    return;
  }

  if(action == "kill"){
    FlGui::instance()->onelab->stop(true);
    for(onelab::server::citer it = onelab::server::instance()->firstClient();
        it != onelab::server::instance()->lastClient(); it++)
      it->second->kill();
    return;
  }

  if(action == "dump"){
    std::string db = onelab::server::instance()->toChar();
    Msg::Direct("OneLab database dump:");
    int start = 0;
    for(unsigned int i = 0; i < db.size(); i++){
      if(db[i] == onelab::parameter::charSep()) db[i] = '|';
      if(db[i] == '\n'){
        Msg::Direct("%s", db.substr(start, i - start).c_str());
        start = i + 1;
      }
    }
    return;
  }

  if(action == "reset"){
    // clear everything except model names
    std::vector<onelab::string> modelNames;
    for(onelab::server::citer it = onelab::server::instance()->firstClient();
      it != onelab::server::instance()->lastClient(); it++){
      onelab::client *c = it->second;
      std::vector<onelab::string> ps;
      c->get(ps, c->getName() + "/1ModelName");
      if(ps.size()) modelNames.push_back(ps[0]);
    }
    onelab::server::instance()->clear();
    if(onelab::server::instance()->findClient("Gmsh") != 
       onelab::server::instance()->lastClient())
      geometry_reload_cb(0, 0);
    for(unsigned int i = 0; i < modelNames.size(); i++)
      onelab::server::instance()->set(modelNames[i]);
    action = "check";
  }

  FlGui::instance()->onelab->setButtonMode("stop");

  if(action == "compute") initializeLoop();

  do{ // enter computation loop

    // the Gmsh client is special: it always gets executed first. (The
    // meta-model will allow more flexibility: but in the simple GUI
    // we can assume this)
    runGmshClient(action);

    if(action == "compute")
      FlGui::instance()->onelab->checkForErrors("Gmsh");
    if(FlGui::instance()->onelab->stop()) break;
    
    // iterate over all other clients
    for(onelab::server::citer it = onelab::server::instance()->firstClient();
        it != onelab::server::instance()->lastClient(); it++){
      onelab::client *c = it->second;
      if(c->getName() == "Gmsh" || // local Gmsh client
         c->getName() == "Listen" || // unknown client connecting through "-listen"
         c->getName() == "GmshRemote") // distant post-processing Gmsh client
        continue;
      std::string what = getModelName(c);

      // FIXME this should be uniformized (probably just be setting a onelab
      // variable to "check" or "compute", and letting the client decide what to
      // do)
      if(action == "check"){
	if(c->getName() == "GetDP")
	  c->run(what);
	else
	  c->run(what + " -a "); // '-a' for 'analyse only'
      }
      else if(action == "compute"){
	if(c->getName() == "GetDP"){
	  // get command line from the server
	  std::vector<onelab::string> ps;
	  onelab::server::instance()->get(ps, c->getName() + "/9Compute");
	  if(ps.size()) what += " " + ps[0].getValue();
	  c->run(what);
	}
	else
	  c->run(what);
      }
      
      if(action == "compute")
        FlGui::instance()->onelab->checkForErrors(c->getName());
      if(FlGui::instance()->onelab->stop()) break;
    }

    updateOnelabGraphs();
    FlGui::instance()->onelab->rebuildTree();

  } while(action == "compute" && !FlGui::instance()->onelab->stop() && 
          incrementLoop());

  FlGui::instance()->onelab->stop(false);
  FlGui::instance()->onelab->setButtonMode("compute");
  FlGui::instance()->onelab->show();
}

static void onelab_check_button_cb(Fl_Widget *w, void *data)
{
  if(!data) return;
  std::string name = FlGui::instance()->onelab->getPath((Fl_Tree_Item*)data);
  std::vector<onelab::number> numbers;
  onelab::server::instance()->get(numbers, name);
  if(numbers.size()){
    Fl_Check_Button *o = (Fl_Check_Button*)w;
    numbers[0].setValue(o->value());
    onelab::server::instance()->set(numbers[0]);
  }
}

static void onelab_input_range_cb(Fl_Widget *w, void *data)
{
  if(!data) return;
  std::string name = FlGui::instance()->onelab->getPath((Fl_Tree_Item*)data);
  std::vector<onelab::number> numbers;
  onelab::server::instance()->get(numbers, name);
  if(numbers.size()){
    inputRange *o = (inputRange*)w;
    numbers[0].setValue(o->value());
    numbers[0].setMin(o->minimum());
    numbers[0].setMax(o->maximum());
    numbers[0].setStep(o->step());
    numbers[0].setChoices(o->choices());
    numbers[0].setAttribute("Loop", o->loop());
    numbers[0].setAttribute("Graph", o->graph());
    onelab::server::instance()->set(numbers[0]);
    updateOnelabGraphs();
  }
}

static void onelab_input_choice_cb(Fl_Widget *w, void *data)
{
  if(!data) return;
  std::string name = FlGui::instance()->onelab->getPath((Fl_Tree_Item*)data);
  std::vector<onelab::string> strings;
  onelab::server::instance()->get(strings, name);
  if(strings.size()){
    Fl_Input_Choice *o = (Fl_Input_Choice*)w;
    strings[0].setValue(o->value());
    onelab::server::instance()->set(strings[0]);
  }
}

static void onelab_input_choice_file_chooser_cb(Fl_Widget *w, void *data)
{
  Fl_Input_Choice *but = (Fl_Input_Choice*)w->parent();
  if(fileChooser(FILE_CHOOSER_SINGLE, "Choose", "", but->value())){
    but->value(fileChooserGetName(1).c_str());
    but->do_callback(but, data);
  }
}

static void onelab_input_choice_file_edit_cb(Fl_Widget *w, void *data)
{
  Fl_Input_Choice *but = (Fl_Input_Choice*)w->parent();
  std::string prog = FixWindowsPath(CTX::instance()->editor);
  std::string file = FixWindowsPath(but->value());
  SystemCall(ReplaceSubString("%s", file, prog));
}

static void onelab_choose_executable_cb(Fl_Widget *w, void *data)
{
  onelab::localNetworkClient *c = (onelab::localNetworkClient*)data;
  std::string pattern = "*";
#if defined(WIN32)
  pattern += ".exe";
#endif
  const char *old = 0;
  if(!c->getCommandLine().empty()) old = c->getCommandLine().c_str();

  if(fileChooser(FILE_CHOOSER_SINGLE, "Choose executable", pattern.c_str(), old)){
    std::string exe = fileChooserGetName(1);
    c->setCommandLine(exe);
    if(c->getIndex() >= 0 && c->getIndex() < 5)
      opt_solver_executable(c->getIndex(), GMSH_SET, exe);
  }
}

static void onelab_remove_solver_cb(Fl_Widget *w, void *data)
{
  onelab::client *c = (onelab::client*)data;
  FlGui::instance()->onelab->removeSolver(c->getName());
}

static void onelab_add_solver_cb(Fl_Widget *w, void *data)
{
  for(int i = 0; i < 5; i++){
    if(opt_solver_name(i, GMSH_GET, "").empty()){
      const char *name = fl_input("Client name:", "");
      if(name){
        FlGui::instance()->onelab->addSolver(name, "", i);
      }
      return;
    }
  }
}

onelabWindow::onelabWindow(int deltaFontSize)
  : _deltaFontSize(deltaFontSize), _stop(false)
{
  FL_NORMAL_SIZE -= _deltaFontSize;

  int width = 29 * FL_NORMAL_SIZE;
  int height = 15 * BH + 3 * WB;
  
  _win = new paletteWindow
    (width, height, CTX::instance()->nonModalWindows ? true : false, "OneLab");
  _win->box(GMSH_WINDOW_BOX);

  _tree = new Fl_Tree(WB, WB, width - 2 * WB, height - 3 * WB - BH);
  _tree->connectorstyle(FL_TREE_CONNECTOR_SOLID);
  _tree->showroot(0);

  _butt[0] = new Fl_Button(width - WB - BB, height - WB - BH, BB, BH, "Compute");
  _butt[0]->callback(onelab_cb, (void*)"compute");
  _butt[1] = new Fl_Button(width - 2*WB - 2*BB, height - WB - BH, BB, BH, "Check");
  _butt[1]->callback(onelab_cb, (void*)"check");

  _gear = new Fl_Menu_Button
    (_butt[1]->x() - WB - BB/2, _butt[1]->y(), BB/2, BH, "@-1gmsh_gear");
  _gear->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
  _gear->add("Reset database", 0, onelab_cb, (void*)"reset");
  _gear->add("_Print database", 0, onelab_cb, (void*)"dump");
  _gear->add("Remesh automatically", 0, 0, 0, FL_MENU_TOGGLE);
  _gear->add("_Hide new views", 0, 0, 0, FL_MENU_TOGGLE);
  ((Fl_Menu_Item*)_gear->menu())[2].set();
  ((Fl_Menu_Item*)_gear->menu())[3].clear();
  _gearFrozenMenuSize = _gear->menu()->size();

  Fl_Box *resbox = new Fl_Box(WB, WB, 
                              width - 2 * BB - BB / 2 - 4 * WB,
                              height - 3 * WB - BH);
  _win->resizable(resbox);
  _win->size_range(2 * BB + BB / 2 + 4 * WB + 1, 2 * BH + 3 * WB);

  _win->position
    (CTX::instance()->solverPosition[0], CTX::instance()->solverPosition[1]);
  _win->end();

  FL_NORMAL_SIZE += _deltaFontSize;
}

void onelabWindow::rebuildTree()
{
  FL_NORMAL_SIZE -= _deltaFontSize;

  int width = (int)(0.5 * _tree->w());

  _tree->clear();
  _tree->sortorder(FL_TREE_SORT_ASCENDING);
  _tree->selectmode(FL_TREE_SELECT_NONE);

  for(unsigned int i = 0; i < _treeWidgets.size(); i++)
    Fl::delete_widget(_treeWidgets[i]);
  _treeWidgets.clear();

  std::vector<onelab::number> numbers;
  onelab::server::instance()->get(numbers);
  for(unsigned int i = 0; i < numbers.size(); i++){
    Fl_Tree_Item *n = _tree->add(numbers[i].getName().c_str());
    n->labelsize(FL_NORMAL_SIZE + 5);
    std::string label = getShortName(numbers[i].getName(), numbers[i].getShortHelp());
    _tree->begin();
    if(numbers[i].getChoices().size() == 2 &&
       numbers[i].getChoices()[0] == 0 && numbers[i].getChoices()[1] == 1){
      Fl_Check_Button *but = new Fl_Check_Button(1, 1, width, 1);
      _treeWidgets.push_back(but);
      but->copy_label(label.c_str());
      but->value(numbers[i].getValue());
      but->callback(onelab_check_button_cb, (void*)n);
      n->widget(but);
    }
    else{
      inputRange *but = new inputRange
        (1, 1, width, 1, onelab::parameter::maxNumber());
      _treeWidgets.push_back(but);
      but->copy_label(label.c_str());
      but->value(numbers[i].getValue());
      but->minimum(numbers[i].getMin());
      but->maximum(numbers[i].getMax());
      but->step(numbers[i].getStep());
      but->choices(numbers[i].getChoices());
      but->loop(numbers[i].getAttribute("Loop"));
      but->graph(numbers[i].getAttribute("Graph"));
      but->align(FL_ALIGN_RIGHT);
      but->callback(onelab_input_range_cb, (void*)n);
      but->when(FL_WHEN_RELEASE | FL_WHEN_ENTER_KEY);
      n->widget(but);
    }
    _tree->end();
  }

  std::vector<onelab::string> strings;
  onelab::server::instance()->get(strings);
  for(unsigned int i = 0; i < strings.size(); i++){
    Fl_Tree_Item *n = _tree->add(strings[i].getName().c_str());
    n->labelsize(FL_NORMAL_SIZE + 5);
    std::string label = getShortName(strings[i].getName(), strings[i].getShortHelp());
    _tree->begin();
    Fl_Input_Choice *but = new Fl_Input_Choice(1, 1, width, 1);
    _treeWidgets.push_back(but);
    but->copy_label(label.c_str());
    std::vector<Fl_Menu_Item> menu;
    for(unsigned int j = 0; j < strings[i].getChoices().size(); j++){
      Fl_Menu_Item it = {strings[i].getChoices()[j].c_str(), 0, 0, 0, 
                         (strings[i].getKind() == "file" && 
                          j == strings[i].getChoices().size() - 1) ? FL_MENU_DIVIDER : 0};
      menu.push_back(it);
    }
    if(strings[i].getKind() == "file"){
      Fl_Menu_Item it = {"Choose...", 0, onelab_input_choice_file_chooser_cb, (void*)n};
      menu.push_back(it);
      Fl_Menu_Item it2 = {"Edit...", 0, onelab_input_choice_file_edit_cb, (void*)n};
      menu.push_back(it2);
    }
    Fl_Menu_Item it = {0};
    menu.push_back(it);
    but->menubutton()->copy(&menu[0]);
    but->value(strings[i].getValue().c_str());
    but->align(FL_ALIGN_RIGHT);
    but->callback(onelab_input_choice_cb, (void*)n);
    but->when(FL_WHEN_RELEASE | FL_WHEN_ENTER_KEY);
    n->widget(but);
    _tree->end();
  }

  for(Fl_Tree_Item *n = _tree->first(); n; n = n->next()){
    if(n->has_children()){
      _tree->begin();
      Fl_Box *but = new Fl_Box(1, 1, width, 1);
      but->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
      _treeWidgets.push_back(but);
      but->copy_label(getShortName(n->label()).c_str());
      n->widget(but);
      _tree->end();
    }
  }
  
  _tree->redraw();

  FL_NORMAL_SIZE += _deltaFontSize;
}

void onelabWindow::checkForErrors(const std::string &client)
{
  if(Msg::GetErrorCount() > 0 && !CTX::instance()->expertMode){
    Msg::ResetErrorCounter();
    std::string msg
      (client + " reported an error: do you really want to continue?\n\n"
       "(To disable this warning in the future, select `Enable expert mode'\n"
       "in the option dialog.)");
    if(Msg::GetAnswer(msg.c_str(), 1, "Stop", "Continue") == 0)
      _stop = true;
  }
}

void onelabWindow::setButtonMode(const std::string &mode)
{
  if(mode == "compute"){
    _butt[0]->label("Compute");
    _butt[0]->callback(onelab_cb, (void*)"compute");
    _butt[1]->activate(); 
  }
  else if(mode == "stop"){
    _butt[0]->label("Stop");
    _butt[0]->callback(onelab_cb, (void*)"stop");
    _butt[1]->deactivate();
  }
  else{
    _butt[0]->label("Kill");
    _butt[0]->callback(onelab_cb, (void*)"kill");
    _butt[1]->deactivate();
  }
}

void onelabWindow::rebuildSolverList()
{
  // update OneLab window title and gear menu
  _title = "OneLab";
  for(int i = _gear->menu()->size(); i >= _gearFrozenMenuSize - 1; i--)
    _gear->remove(i);
  for(onelab::server::citer it = onelab::server::instance()->firstClient();
      it != onelab::server::instance()->lastClient(); it++){
    if(it == onelab::server::instance()->firstClient()) _title += " -";
    if(it->second->isNetworkClient()){
      onelab::localNetworkClient *c = (onelab::localNetworkClient*)it->second;
      char tmp[256];
      sprintf(tmp, "%s/Choose executable", c->getName().c_str());
      _gear->add(tmp, 0, onelab_choose_executable_cb, (void*)c);
      sprintf(tmp, "%s/Remove", c->getName().c_str());
      _gear->add(tmp, 0, onelab_remove_solver_cb, (void*)c);
    }
    _title += " " + it->second->getName();
  }
  _gear->add("Add new client...", 0, onelab_add_solver_cb);
  _win->label(_title.c_str());

  // update Gmsh solver menu
  std::vector<std::string> names, exes;
  for(int i = 0; i < 5; i++){
    if(opt_solver_name(i, GMSH_GET, "").size()){
      names.push_back(opt_solver_name(i, GMSH_GET, ""));
      exes.push_back(opt_solver_executable(i, GMSH_GET, ""));
    }
  }
  for(int i = 0; i < 5; i++){
    if(i < names.size()){
      onelab::server::citer it = onelab::server::instance()->findClient(names[i]);
      if(it != onelab::server::instance()->lastClient())
        it->second->setIndex(i);
      opt_solver_name(i, GMSH_SET, names[i]);
      opt_solver_executable(i, GMSH_SET, exes[i]);
    }
    else{
      opt_solver_name(i, GMSH_SET, "");
      opt_solver_executable(i, GMSH_SET, "");
    }
  }
  FlGui::instance()->menu->setContext(menu_solver, 0);
}

void onelabWindow::addSolver(const std::string &name, const std::string &commandLine,
                             int index)
{
  onelab::server::citer it = onelab::server::instance()->findClient(name);
  if(it == onelab::server::instance()->lastClient()){
    onelab::localNetworkClient *c = new onelab::localNetworkClient(name, commandLine);
    c->setIndex(index);
    opt_solver_name(index, GMSH_SET, name);
    if(commandLine.empty())
      onelab_choose_executable_cb(0, (void *)c);
    if(name == "GetDP")
      setModelExtension(".pro");
  }
  FlGui::instance()->onelab->rebuildSolverList();
}

void onelabWindow::removeSolver(const std::string &name)
{
  onelab::server::citer it = onelab::server::instance()->findClient(name);
  if(it != onelab::server::instance()->lastClient()){
    onelab::client *c = it->second;
    if(c->isNetworkClient()){
      onelab::server::instance()->unregisterClient(c);
      if(c->getIndex() >= 0 && c->getIndex() < 5){
        opt_solver_name(c->getIndex(), GMSH_SET, "");
        opt_solver_executable(c->getIndex(), GMSH_SET, "");
      }
      delete c;
    }
  }
  FlGui::instance()->onelab->rebuildSolverList();
}

void solver_cb(Fl_Widget *w, void *data)
{
  int num = (intptr_t)data;

  if(num >= 0){
    std::string name = opt_solver_name(num, GMSH_GET, "");
    std::string exe = opt_solver_executable(num, GMSH_GET, "");
    FlGui::instance()->onelab->addSolver(name, exe, num);
  }
  else
    FlGui::instance()->onelab->rebuildSolverList();

  onelab_cb(0, (void*)"check");
}

#else

#if defined(HAVE_ONELAB)
bool onelab::localNetworkClient::run(const std::string &what)
{
  Msg::Error("The solver interface requires OneLab and FLTK 1.3");
  return false;
}

bool onelab::localNetworkClient::kill()
{
  Msg::Error("The solver interface requires OneLab and FLTK 1.3");
  return false;
}
#endif

void solver_cb(Fl_Widget *w, void *data)
{
  Msg::Error("The solver interface requires OneLab and FLTK 1.3");
}

#endif
