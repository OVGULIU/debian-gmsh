#include "OnelabClients.h"
#include <algorithm>
#include "mathex.h"

// reserved keywords for the onelab parser

namespace olkey{ 
  static std::string deflabel("onelab.tags");
  static std::string label("OL."), comment("%"), separator(";");
  static std::string line(label+"line");
  static std::string begin(label+"begin");
  static std::string end(label+"end");
  static std::string include(label+"include");
  static std::string ifcond(label+"if");
  static std::string ifequal(label+"ifequal");
  static std::string iftrue(label+"iftrue"), ifntrue(label+"ifntrue");
  static std::string olelse(label+"else"), olendif(label+"endif");
  static std::string getValue(label+"get");
  static std::string mathex(label+"eval");
  static std::string getRegion(label+"region");
  static std::string number("number"), string("string");
  static std::string arguments("args"), inFiles("in"), outFiles("out");
  static std::string upload("up"), merge("merge");
}

int enclosed(const std::string &in, std::vector<std::string> &arguments,
	     int &end){
  // syntax: (arguments[Ø], arguments[1], ... , arguments[n])
  // arguments[i] may contain parenthesis
  int pos, cursor;
  arguments.resize(0);
  cursor=0;
  if ( (pos=in.find("(",cursor)) == std::string::npos )
     Msg::Fatal("Syntax error: <%s>",in.c_str());

  if (pos>0){
    std::cout << pos << in << std::endl;
     Msg::Fatal("Syntax error: <%s>",in.c_str());
  }
  unsigned int count=1;
  pos++; // skips '('
  cursor = pos; 
  do{
    if(in[pos]=='(') count++;
    else if(in[pos]==')') count--;
    else if(in[pos]==',') {
      arguments.push_back(removeBlanks(in.substr(cursor,pos-cursor)));
      if(count!=1)
	Msg::Fatal("Syntax error: mismatched parenthesis <%s>",in.c_str());
      cursor=pos+1; // skips ','
    }
    else if(in[pos]==';') 
	Msg::Fatal("Syntax error: unterminated sentence <%s>",in.c_str());

    pos++;
  } while( count && (pos!=std::string::npos) );
  // count is 0 when the closing brace is found. 

  if(count)
     Msg::Fatal("Syntax error: <%s>",in.c_str());
  else
    arguments.push_back(removeBlanks(in.substr(cursor,pos-1-cursor)));
  end=pos;
  return arguments.size();
}

int extractLogic(const std::string &in, std::vector<std::string> &arguments){
  // syntax: ( argument[0], argument[1]\in{<,>,<=,>=,==,!=}, arguments[2])
  int pos, cursor;
  arguments.resize(0);
  cursor=0;
  if ( (pos=in.find("(",cursor)) == std::string::npos )
     Msg::Fatal("Syntax error: <%s>",in.c_str());

  unsigned int count=1;
  pos++; // skips '('
  cursor=pos; 
  do{
    if(in[pos]=='(') count++;
    if(in[pos]==')') count--;
    if( (in[pos]=='<') || (in[pos]=='=') || (in[pos]=='>') ){
      arguments.push_back(removeBlanks(in.substr(cursor,pos-cursor)));
      if(count!=1)
	Msg::Fatal("Syntax error: <%s>",in.c_str());
      cursor=pos;
      if(in[pos+1]=='='){
	arguments.push_back(in.substr(cursor,2));
	pos++;
      }
      else{
      	arguments.push_back(in.substr(cursor,1));
      }
      cursor=pos+1;
    }
    pos++;
  } while( count && (pos!=std::string::npos) );
  // count is 0 when the closing brace is found. 

  if(count)
     Msg::Fatal("Syntax error: mismatched parenthesis in <%s>",in.c_str());
  else
    arguments.push_back(removeBlanks(in.substr(cursor,pos-1-cursor)));

  if((arguments.size()!=1) && (arguments.size()!=3))
    Msg::Fatal("Syntax error: <%s>",in.c_str());
  return arguments.size();
}

int extract(const std::string &in, std::string &paramName, 
	    std::string &action, std::vector<std::string> &arguments){
  // syntax: paramName.action( arg1, arg2, ... )
  int pos, cursor;
  cursor=0;
  if ( (pos=in.find(".",cursor)) == std::string::npos )
     Msg::Fatal("Syntax error: <%s>",in.c_str());
  else
    paramName.assign(sanitize(in.substr(cursor,pos-cursor)));
  cursor = pos+1; // skips '.'
  if ( (pos=in.find("(",cursor)) == std::string::npos )
     Msg::Fatal("Syntax error: <%s>",in.c_str());
  else
    action.assign(sanitize(in.substr(cursor,pos-cursor)));
  cursor = pos;
  return enclosed(in.substr(cursor),arguments,pos);
}

bool extractRange(const std::string &in, std::vector<double> &arguments){
  // syntax: a:b:c or a:b#n
  int pos, cursor;
  arguments.resize(0);
  cursor=0;
  if ( (pos=in.find(":",cursor)) == std::string::npos )
     Msg::Fatal("Syntax error in range <%s>",in.c_str());
  else{
    arguments.push_back(atof(in.substr(cursor,pos-cursor).c_str()));
  }
  cursor = pos+1; // skips ':'
  if ( (pos=in.find(":",cursor)) != std::string::npos ){
    arguments.push_back(atof(in.substr(cursor,pos-cursor).c_str()));
    arguments.push_back(atof(in.substr(pos+1).c_str()));
  }
  else if ( (pos=in.find("#",cursor)) != std::string::npos ){
    arguments.push_back(atof(in.substr(cursor,pos-cursor).c_str()));
    double NumStep = atof(in.substr(pos+1).c_str());
    arguments.push_back((arguments[1]-arguments[0])/((NumStep==0)?1:NumStep));
  }
  else
     Msg::Fatal("Syntax error in range <%s>",in.c_str());
  return (arguments.size()==3);
}

std::string extractExpandPattern(const std::string& str){
  int posa,posb;
  posa=str.find_first_of("\"\'<");
  posb=str.find_last_of("\"\'>");
  std::string pattern=str.substr(posa+1,posb-posa-1);
  posa=pattern.find("comma");
  if(posa!=std::string::npos) 
    pattern.replace(posa,5,",");
  if(pattern.size()!=3)
    Msg::Fatal("Incorrect expand pattern <%s>",
	       str.c_str());
  return pattern; 
}

std::string localSolverClient::longName(const std::string name){
  std::set<std::string>::iterator it;
  std::string fullName;
  if((it = _parameters.find(name)) != _parameters.end())
    fullName.assign(Msg::obtainFullName(*it));
  else
    fullName.assign(Msg::obtainFullName(name));
  //std::cout << "Full name=<" << name << "> => <" << fullName << ">" << std::endl;
  return fullName;
}

std::string localSolverClient::resolveGetVal(std::string line) {
  std::vector<onelab::number> numbers;
  std::vector<onelab::string> strings;
  std::vector<std::string> arguments;
  std::string buff;
  int pos,pos0,cursor;

  cursor=0;
  while ( (pos=line.find(olkey::getValue,cursor)) != std::string::npos){
    pos0=pos; // for further use
    cursor = pos+olkey::getValue.length();
    int NumArg=enclosed(line.substr(cursor),arguments,pos);
    if(NumArg<1)
      Msg::Fatal("Misformed %s statement: <%s>",
		 olkey::getValue.c_str(),line.c_str());
    std::string paramName=longName(arguments[0]);
    get(numbers,paramName);
    if (numbers.size()){
      std::stringstream Num;
      if(NumArg==1){
	Num << numbers[0].getValue();
	buff.assign(Num.str());
      }
      else if(NumArg==2){
	std::string name, action;
	std::vector<std::string> args;
	extract(arguments[1],name,action,args);
	if(!name.compare("choices")) { 
	  std::vector<double> choices=numbers[0].getChoices();
	  if(!action.compare("size")) {
	    buff.assign(ftoa(choices.size()));
	  }
	  else if(!action.compare("comp")) {
	    int i=atoi(args[0].c_str());
	    if( (i>=0) && (i<choices.size()) )
	      Num << choices[i];
	    buff.assign(ftoa(choices[i]));
	  }
	  else if(!action.compare("expand")) {
	    std::string pattern;
	    pattern.assign(extractExpandPattern(args[0]));
	    Msg::Info("Expand parameter <%s> with pattern <%s>",
		      paramName.c_str(),pattern.c_str());
	    buff.assign(1,pattern[0]);
	    for(std::vector<double>::iterator it = choices.begin();
		it != choices.end(); it++){
	      if(it != choices.begin())
		buff.append(1,pattern[1]);
	      buff.append(ftoa(*it));
	    }
	    buff.append(1,pattern[2]);	  
	  }
	  else
	    Msg::Fatal("Unknown action <%s> in %s statement",
		       action.c_str(),olkey::getValue.c_str());
	}
	else if(!name.compare("range")) {
	  double stp, min, max;
	  if( ((stp=numbers[0].getStep()) == 0) ||
	      ((min=numbers[0].getMin()) ==-onelab::parameter::maxNumber()) ||
	      ((max=numbers[0].getMax()) ==onelab::parameter::maxNumber()) )
	    Msg::Fatal("Invalid range description for parameter <%s>",
		       paramName.c_str());
	  if(!action.compare("size")) {
	    buff.assign(ftoa(fabs((max-min)/stp)));
	  }
	  else if(!action.compare("comp")) {
	    int i= atof(args[0].c_str());
	    if(stp > 0)
		Num << min+i*stp;
	    else if(stp < 0)
	      Num << max-i*stp;
	  }
	  else if(!action.compare("expand")) {
	  }
	  else
	    Msg::Fatal("Unknown action <%s> in %s statement",
		       action.c_str(),olkey::getValue.c_str());
	}
      }
    }
    else{
      get(strings,longName(paramName));
      if (strings.size())
	buff.assign(strings[0].getValue());
      else
	Msg::Fatal("resolveGetVal: unknown variable: <%s>",paramName.c_str());
    }
    line.replace(pos0,cursor+pos-pos0,buff); 
    cursor=pos0+buff.length();
  }

  // Check now wheter the line contains OL.mathex and resolve them
  cursor=0;
  while ( (pos=line.find(olkey::mathex,cursor)) != std::string::npos){
    int pos0=pos;
    cursor=pos+olkey::mathex.length();
    if(enclosed(line.substr(cursor),arguments,pos) != 1)
      Msg::Fatal("Misformed %s statement: <%s>",
		 olkey::mathex.c_str(),line.c_str());
    smlib::mathex* mathExp = new smlib::mathex();
    mathExp->expression(arguments[0]); 
    double val=mathExp->eval();
    //std::cout << "MathEx <" << arguments[0] << "> ="<< val << std::endl;
    line.replace(pos0,cursor+pos-pos0,ftoa(val));
  }

  // Check now wheter the line still contains OL.
  if ( (pos=line.find(olkey::label)) != std::string::npos)
    Msg::Fatal("Unidentified onelab command in <%s>",line.c_str());

  return line;
}

bool localSolverClient::resolveLogicExpr(std::vector<std::string> arguments) {
  std::vector<onelab::number> numbers;
  double val1, val2;
  bool condition;

  val1= atof( resolveGetVal(arguments[0]).c_str() );
  if(arguments.size()==1)
    return (bool)val1;

  if(arguments.size()==3){
    val2=atof( resolveGetVal(arguments[2]).c_str() );
    if(!arguments[1].compare("<"))
      condition = (val1<val2);
    else if (!arguments[1].compare("<="))
      condition = (val1<=val2);
    else if (!arguments[1].compare(">"))
      condition = (val1>val2);
    else if (!arguments[1].compare(">="))
      condition = (val1>=val2);
    else if (!arguments[1].compare("=="))
      condition = (val1==val2);   
    else if (!arguments[1].compare("!="))
      condition = (val1!=val2);
  }

  return condition;
}

void localSolverClient::parse_sentence(std::string line) { 
  int pos,cursor;
  std::string name,action,path;
  std::vector<std::string> arguments;
  std::vector<onelab::number> numbers;
  std::vector<onelab::string> strings;

  cursor = 0;
  while ( (pos=line.find(olkey::separator,cursor)) != std::string::npos){
    std::string name, action;
    extract(line.substr(cursor,pos-cursor),name,action,arguments);

    if(!action.compare("number")) { 
      double val;
      // syntax: paramName.number(val,path,help,range(optional))
      if(arguments.size()>1){
	name.assign(arguments[1] + name);
      }
      _parameters.insert(name);
      Msg::recordFullName(name);
      get(numbers, name);
      if(numbers.empty()){ 
	numbers.resize(1);
	numbers[0].setName(name);
	if(arguments[0].empty()){
	  numbers[0].setReadOnly(1);
	  val=0;
	}
	else
	  val=atof(resolveGetVal(arguments[0]).c_str());
	numbers[0].setValue(val);
      }

      if(arguments.size()>2)
	numbers[0].setLabel(arguments[2]);
      if(arguments.size()>3){
	std::vector<double> bounds;
	if (extractRange(arguments[3],bounds)){
	  numbers[0].setMin(bounds[0]);
	  numbers[0].setMax(bounds[1]);
	  numbers[0].setStep(bounds[2]);
	}
      }
      set(numbers[0]);
    }
    else if(!action.compare("string")) { 
      // paramName.string(val,path,help)
      if(arguments[0].empty())
	Msg::Fatal("No value given for param <%s>",name.c_str());
      std::string val=resolveGetVal(arguments[0]);
      if(arguments.size()>1)
	name.assign(arguments[1] + name); // append path
      _parameters.insert(name);
      Msg::recordFullName(name);
      get(strings, name);
      if(strings.size()){
	if(!strings[0].getReadOnly())
	  val.assign(strings[0].getValue()); // use value from server
	else
	  strings[0].setValue(val);
      }
      else{
	strings.resize(1);
	strings[0].setName(name);
	strings[0].setValue(val);
      }
      if(arguments.size()>2)
	strings[0].setLabel(arguments[2]);
      set(strings[0]);
    }
    else if(!action.compare("radioButton")) { 
      // syntax: paramName.radioButton(val,path,label)
      if(arguments[0].empty())
	Msg::Fatal("No value given for param <%s>",name.c_str());
      double val=atof(arguments[0].c_str());
      if(arguments.size()>1){
	name.assign(arguments[1] + name);
      }
      _parameters.insert(name);
      Msg::recordFullName(name);
      get(numbers, name);
      if(numbers.size()){ 
	val = numbers[0].getValue(); // use value from server
      }
      else{
	numbers.resize(1);
	numbers[0].setName(name);
	numbers[0].setValue(val);
      }
      if(arguments.size()>2)
	numbers[0].setLabel(arguments[2]);
      std::vector<double> choices;
      choices.push_back(0);
      choices.push_back(1);
      numbers[0].setChoices(choices);
      set(numbers[0]);
    }
    else if(!action.compare("range")){ 
      // set the range of an existing number
      if(arguments[0].empty())
	Msg::Fatal("No argument given for MinMax <%s>",name.c_str());
      name.assign(longName(name));
      get(numbers,name);
      if(numbers.size()){ // parameter must exist
	if(arguments.size()==1){
	  std::vector<double> bounds;
	  if (extractRange(arguments[1],bounds)){
	    numbers[0].setMin(bounds[0]);
	    numbers[0].setMax(bounds[1]);
	    numbers[0].setStep(bounds[2]);
	  }
	}
	else if(arguments.size()==3){
	  numbers[0].setMin(atof(arguments[0].c_str()));
	  numbers[0].setMax(atof(arguments[1].c_str()));
	  numbers[0].setStep(atof(arguments[2].c_str()));
	}
	else
	  Msg::Fatal("Wrong argument number for MinMax <%s>",name.c_str());
      }
      set(numbers[0]);
    }
    else if(!action.compare("addChoices")){
      if(arguments.size()){
	name.assign(longName(name));
	get(numbers,name);
	if(numbers.size()){ // parameter must exist
	  std::vector<double> choices=numbers[0].getChoices();
	  for(unsigned int i = 0; i < arguments.size(); i++){
	    double val=atof(resolveGetVal(arguments[i]).c_str());
	    if(std::find(choices.begin(),choices.end(),val)==choices.end())
	      choices.push_back(val);
	  }
	  numbers[0].setChoices(choices);
	  set(numbers[0]);
	}
	else{
	  get(strings,name);
	  if(strings.size()){
	    std::vector<std::string> choices=strings[0].getChoices();
	    for(unsigned int i = 0; i < arguments.size(); i++)
	      if(std::find(choices.begin(),choices.end(),
			   arguments[i])==choices.end())
		choices.push_back(arguments[i]);
	    strings[0].setChoices(choices);
	    set(strings[0]);
	  }
	  else{
	    Msg::Fatal("The parameter <%s> does not exist",name.c_str());
	  }
	}
      }
    }
    else if(!action.compare("addLabels")){
      if(arguments.size()){
	name.assign(longName(name));
	get(numbers,name);
	if(numbers.size()){ // parameter must exist
	  std::vector<double> choices=numbers[0].getChoices();
	  if(choices.size() != arguments.size())
	    Msg::Fatal("Nb of labels does not match nb of choices <%s>",
		       name.c_str());
	  std::vector<std::string> labels;
	  for(unsigned int i = 0; i < arguments.size(); i++){
	      labels.push_back(arguments[i]);
	  }
	  numbers[0].setChoiceLabels(labels);
	  set(numbers[0]);
	}
	else
	  Msg::Fatal("The number <%s> does not exist",name.c_str());
      }
    }
    else if(!action.compare("setValue")){ // force change on server
      if(arguments[0].empty())
	Msg::Fatal("Missing argument SetValue <%s>",name.c_str());
      name.assign(longName(name));
      get(numbers,name); 
      if(numbers.size()){ 
	numbers[0].setValue(atof(resolveGetVal(arguments[0]).c_str()));
	set(numbers[0]);
      }
      else{
	get(strings,name); 
	if(strings.size()){
	  strings[0].setValue(arguments[0]);
	  set(strings[0]);
	}
	else{
	  Msg::Fatal("The parameter <%s> does not exist",name.c_str());
	}
      }
    }
    else if(!action.compare("setVisible")){
      if(arguments[0].empty())
	Msg::Fatal("Missing argument SetVisible <%s>",name.c_str());
      name.assign(longName(name));
      get(numbers,name); 
      if(numbers.size()){ 
	numbers[0].setVisible(atof(resolveGetVal(arguments[0]).c_str()));
	set(numbers[0]);
      }
      else{
	get(strings,name); 
	if(strings.size()){
	  strings[0].setVisible(atof(resolveGetVal(arguments[0]).c_str()));
	  set(strings[0]);
	}
	else{
	  Msg::Fatal("The parameter <%s> does not exist",name.c_str());
	}
      }
    }
    else if(!action.compare("setReadOnly")){
      if(arguments[0].empty())
	Msg::Fatal("Missing argument SetReadOnly <%s>",name.c_str());
      name.assign(longName(name));
      get(numbers,name); 
      if(numbers.size()){ 
	numbers[0].setReadOnly(atof(resolveGetVal(arguments[0]).c_str()));
	set(numbers[0]);
      }
      else{
	get(strings,name); 
	if(strings.size()){
	  strings[0].setReadOnly(atof(resolveGetVal(arguments[0]).c_str()));
	  set(strings[0]);
	}
	else{
	  Msg::Fatal("The parameter <%s> does not exist",name.c_str());
	}
      }
    }
    else
      client_sentence(name,action,arguments);
    cursor=pos+1;
  }
}

void localSolverClient::modify_tags(const std::string lab, const std::string com, const std::string sep){
  bool changed=false;
  if(lab.compare(olkey::label) && lab.size()){
    changed=true;
    olkey::label.assign(lab);
    olkey::line.assign(olkey::label+"line");
    olkey::begin.assign(olkey::label+"begin");
    olkey::end.assign(olkey::label+"end");
    olkey::include.assign(olkey::label+"include");
    olkey::ifcond.assign(olkey::label+"if");
    olkey::ifequal.assign(olkey::label+"ifequal");
    olkey::iftrue.assign(olkey::label+"iftrue");
    olkey::ifntrue.assign(olkey::label+"ifntrue"); 
    olkey::olelse.assign(olkey::label+"else");
    olkey::olendif.assign(olkey::label+"endif");
    olkey::getValue.assign(olkey::label+"get");
    olkey::mathex.assign(olkey::label+"eval");
    olkey::getRegion.assign(olkey::label+"region");
  }
  if(com.compare(olkey::comment) && com.size()){
    changed=true;
    olkey::comment.assign(com);
  }
  if(sep.compare(olkey::separator) && sep.size()){
    changed=true;
    olkey::separator.assign(sep);
  }

  if(changed)
    Msg::Info("Using now onelab tags <%s,%s,%s>",
	      olkey::label.c_str(), olkey::comment.c_str(),
	      olkey::separator.c_str());
}

void localSolverClient::parse_oneline(std::string line, std::ifstream &infile) { 
  int pos,cursor;
  std::vector<std::string> arguments;
  std::vector<onelab::number> numbers;
  std::vector<onelab::string> strings;

  if((pos=line.find_first_not_of(" \t"))==std::string::npos){
    // empty line, skip
  }
  else if(!line.compare(pos,olkey::comment.size(),olkey::comment)){
    // commented out line, skip
  }
  else if ( (pos=line.find(olkey::deflabel)) != std::string::npos){
    // onelab.tags(label{,comment{,separator}})
    cursor = pos+olkey::deflabel.length();
    int NumArg=enclosed(line.substr(cursor),arguments,pos);
    modify_tags(((NumArg>0)?arguments[0]:""), ((NumArg>1)?arguments[1]:""),
		((NumArg>2)?arguments[2]:""));
  }
  else if( (pos=line.find(olkey::begin)) != std::string::npos) {
    // onelab.begin
    if (!parse_block(infile))
      Msg::Fatal("Misformed <%s> block <%s>",
		 olkey::begin.c_str(),olkey::end.c_str());
  }
  else if ( (pos=line.find(olkey::iftrue)) != std::string::npos) {
    // onelab.iftrue
    cursor = pos+olkey::iftrue.length();
    if(enclosed(line.substr(cursor),arguments,pos)<1)
      Msg::Fatal("Misformed <%s> statement: (%s)",
		 olkey::iftrue.c_str(),line.c_str());
    bool condition = false;
    get(strings,longName(arguments[0]));
    if (strings.size())
      condition= true;
    else{
      get(numbers,longName(arguments[0]));
      if (numbers.size())
	condition = (bool) numbers[0].getValue();
      else
	Msg::Fatal("Unknown parameter <%s> in <%s> statement",
		   arguments[0].c_str(),olkey::iftrue.c_str());
      if (!parse_ifstatement(infile,condition))
	Msg::Fatal("Misformed <%s> statement: <%s>",
		   olkey::iftrue.c_str(),arguments[0].c_str());
    }
  }
  else if ( (pos=line.find(olkey::ifntrue)) != std::string::npos) {
    // onelab.ifntrue
    cursor = pos+olkey::ifntrue.length();
    // pos=line.find_first_of(')',cursor)+1;
    // if(enclosed(line.substr(cursor,pos-cursor),arguments)<1)
    if(enclosed(line.substr(cursor),arguments,pos)<1)
      Msg::Fatal("Misformed <%s> statement: (%s)",
		 olkey::ifntrue.c_str(),line.c_str());
    bool condition = false;
    get(strings,longName(arguments[0]));
    if (strings.size())
      condition= true;
    else{
      get(numbers,longName(arguments[0]));
      if (numbers.size())
	condition = (bool) numbers[0].getValue();
      else
	Msg::Fatal("Unknown parameter <%s> in <%s> statement",
		   arguments[0].c_str(),olkey::ifntrue.c_str());
      if (!parse_ifstatement(infile,!condition))
	Msg::Fatal("Misformed <%s> statement: <%s>",
		   olkey::ifntrue.c_str(),arguments[0].c_str());
    }
  }
  else if ( (pos=line.find(olkey::ifcond)) != std::string::npos) {
    // onelab.ifcond
    cursor = pos+olkey::ifcond.length();
    int NumArgs=extractLogic(line.substr(cursor),arguments);
    bool condition= resolveLogicExpr(arguments);
    if (!parse_ifstatement(infile,condition))
      Msg::Fatal("Misformed %s statement: <%s>", line.c_str());
  }
  else if ( (pos=line.find(olkey::ifequal)) != std::string::npos) {
    // onelab.ifequal
    cursor = pos+olkey::ifequal.length();
    //pos=line.find_first_of(')',cursor)+1;
    //if (enclosed(line.substr(cursor,pos-cursor),arguments) <2)
    if (enclosed(line.substr(cursor),arguments,pos) <2)
      Msg::Fatal("Misformed %s statement: <%s>",
		 olkey::ifequal.c_str(),line.c_str());
    bool condition= false;
    get(strings,longName(arguments[0]));
    if (strings.size())
      condition= !strings[0].getValue().compare(arguments[1]);
    else{
      get(numbers,longName(arguments[0]));
      if (numbers.size())
        condition= (numbers[0].getValue() == atof(arguments[1].c_str()));
      else
        Msg::Fatal("Unknown argument <%s> in <%s> statement",
		   arguments[0].c_str()),olkey::ifequal.c_str();
    }
    if (!parse_ifstatement(infile,condition))
      Msg::Fatal("Misformed <%s> statement: (%s,%s)",
                 olkey::ifequal.c_str(), arguments[0].c_str(),
		 arguments[1].c_str());
  }
  else if ( (pos=line.find(olkey::include)) != std::string::npos) { 
    // onelab.include
    cursor = pos+olkey::include.length();
    // pos=line.find_first_of(')',cursor)+1;
    // if(enclosed(line.substr(cursor,pos-cursor),arguments)<1)
    if(enclosed(line.substr(cursor),arguments,pos)<1)
      Msg::Fatal("Misformed <%s> statement: (%s)",
		 olkey::include.c_str(),line.c_str());
    parse_onefile(arguments[0]);
  }
  else if( isOnelabBlock() ||
	 ( !isOnelabBlock() &&
	   ((pos=line.find(olkey::line)) != std::string::npos)) ){
    // either any other line within a onelabBlock or a line 
    // introduced by a "onelab.line" tag not within a onelabBlock
    std::string cmds="",cmd;
    int posa, posb, NbLines=1;
    do{
      if( (pos=line.find(olkey::line)) != std::string::npos)
	posa=pos + olkey::line.size();
      else
	posa=0; // skip tag 'olkey::line' if any

      posb=line.find(olkey::comment); // skip trailing comments if any
      if(posb==std::string::npos)
	cmd.assign(line.substr(posa));
      else
	cmd.assign(line.substr(posa,posb-posa));
      cmds.append(cmd);

      //std::cout << "cmds=<" << cmds << ">" << std::endl;

      // check whether "cmd" ends now with "olkey::separator"
      posb=cmd.find_last_not_of(" \t")-olkey::separator.length()+1;
      if(posb<0) posb=0;
      if(cmd.compare(posb,olkey::separator.length(),olkey::separator)){
	// append the next line
	getline (infile,line);
	if((pos=line.find_first_not_of(" \t"))==std::string::npos){
	  Msg::Fatal("Empty line not allowed within a command <%s>",
		     cmds.c_str());
	}
	else if(!line.compare(pos,olkey::comment.size(),olkey::comment)){
	  Msg::Fatal("Comment lines not allowed within a command <%s>",
		     cmds.c_str());
	}
	NbLines++; // command should not span over more than 10 lines
      }
      else
	break;
    } while (infile.good() && NbLines<=10);
    if(NbLines>=10)
      Msg::Fatal("Command <%s> should not span over more than 10 lines",
		 cmds.c_str());
    parse_sentence(cmds);
  }
  else if ( (pos=line.find(olkey::getValue)) != std::string::npos) {
    // onelab.getValue: nothing to do
  }
  else if ( (pos=line.find(olkey::mathex)) != std::string::npos) {
    // onelab.mathex: nothing to do
  }
  else if ( (pos=line.find(olkey::getRegion)) != std::string::npos) {
    // onelab.getRegion: nothing to do
  }
  else if( (pos=line.find(olkey::label)) != std::string::npos) {
      Msg::Fatal("Unknown ONELAB keyword in <%s>",line.c_str());
  }
  else{
    // not a onelab line, skip
  }
}

bool localSolverClient::parse_block(std::ifstream  &infile) { 
  int pos;
  std::string line;
  openOnelabBlock();
  while (infile.good()){
    getline (infile,line);
    if ((pos=line.find(olkey::end)) != std::string::npos){
      closeOnelabBlock();
      return true;
    }
    parse_oneline(line,infile);
  }
  return false;
} 

bool localSolverClient::parse_ifstatement(std::ifstream &infile, 
					  bool condition) { 
  int level, pos;
  std::string line;

  bool trueclause=true; 
  level=1;
  while ( infile.good() && level) {
    getline (infile,line);
    if ( ((pos=line.find(olkey::olelse)) != std::string::npos) && (level==1) ) 
      trueclause=false;
    else if ( (pos=line.find(olkey::olendif)) != std::string::npos) 
      level--;
    else if ( !(trueclause ^ condition) ) // xor bitwise operator
      parse_oneline(line,infile);
    else { // check for opening if statements
      if ( (pos=line.find(olkey::iftrue)) != std::string::npos) 
	level++; 
      else if ( (pos=line.find(olkey::ifntrue)) != std::string::npos) 
	level++; 
      else if ( (pos=line.find(olkey::ifcond)) != std::string::npos) 
	level++; 
    }
  }
  return level?false:true ;
} 

void localSolverClient::parse_onefile(std::string fileName, bool mandatory) { 
  int pos;
  std::string fullName=getWorkingDir()+fileName;
  std::ifstream infile(fullName.c_str());
  if (infile.is_open()){
    Msg::Info("Parse file <%s>",fullName.c_str());
    while (infile.good()){
      std::string line;
      getline(infile,line);
      parse_oneline(line,infile);
    }
    infile.close();
  }
  else
    if(mandatory)
      Msg::Fatal("The file <%s> does not exist",fullName.c_str());
    else
      Msg::Info("The file <%s> does not exist",fullName.c_str());
  modify_tags("OL.","%",";");
} 

bool localSolverClient::convert_ifstatement(std::ifstream &infile, std::ofstream &outfile, bool condition) { 
  int level, pos;
  std::string line;

  bool trueclause=true; 
  level=1;
  while ( infile.good() && level) {
    getline (infile,line);
    if ( ((pos=line.find(olkey::olelse)) != std::string::npos) && (level==1) ) 
      trueclause=false;
    else if ( (pos=line.find(olkey::olendif)) != std::string::npos) 
     level--;
    else if ( !(trueclause ^ condition) ) // xor bitwise operator
      convert_oneline(line,infile,outfile);
    else { // check for opening if statements
      if ( (pos=line.find(olkey::iftrue)) != std::string::npos) 
	level++; 
      else if ( (pos=line.find(olkey::ifntrue)) != std::string::npos) 
	level++; 
      else if ( (pos=line.find(olkey::ifcond)) != std::string::npos) 
	level++; 
    }
  }
  return level?false:true ;
} 

void localSolverClient::convert_oneline(std::string line, std::ifstream &infile, std::ofstream &outfile) { 
  int pos,cursor;
  std::vector<std::string> arguments;
  std::vector<onelab::number> numbers;
  std::vector<onelab::string> strings;
  std::vector<onelab::region> regions;

  if((pos=line.find_first_not_of(" \t"))==std::string::npos){
    // empty line
    outfile << line << std::endl;  
  }
  else if(!line.compare(pos,olkey::comment.size(),olkey::comment)){
    // commented out, skip the line
  }
  else if ( (pos=line.find(olkey::deflabel)) != std::string::npos){
    // onelab.tags(label,comment,separator)
    cursor = pos+olkey::deflabel.length();
    int NumArg=enclosed(line.substr(cursor),arguments,pos);
    modify_tags(((NumArg>0)?arguments[0]:""), ((NumArg>1)?arguments[1]:""),
		((NumArg>2)?arguments[2]:""));
  }
  else if( (pos=line.find(olkey::begin)) != std::string::npos) {
    // onelab.begin
    while (infile.good()){
      getline (infile,line);
      if( (pos=line.find(olkey::end)) != std::string::npos) return;
    }
    Msg::Fatal("Misformed <%s> block <%s>",
	       olkey::begin.c_str(),olkey::end.c_str());
  }
  else if ( (pos=line.find(olkey::iftrue)) != std::string::npos) {
    // onelab.iftrue
    cursor = pos+olkey::iftrue.length();
    // pos=line.find_first_of(')',cursor)+1;
    // if(enclosed(line.substr(cursor,pos-cursor),arguments)<1)
    if(enclosed(line.substr(cursor),arguments,pos)<1)
      Msg::Fatal("Misformed <%s> statement: (%s)",
		 olkey::iftrue.c_str(),line.c_str());
    bool condition = false;
    get(strings,longName(arguments[0]));
    if (strings.size())
      condition= true;
    else{
      get(numbers,longName(arguments[0]));
      if (numbers.size())
	condition = (bool) numbers[0].getValue();
      else
	Msg::Fatal("Unknown parameter <%s> in <%s> statement",
		   arguments[0].c_str(),olkey::iftrue.c_str());
    }
    if (!convert_ifstatement(infile,outfile,condition))
      Msg::Fatal("Misformed <%s> statement: %s",
		 olkey::iftrue.c_str(),arguments[0].c_str());     
  }
  else if ( (pos=line.find(olkey::ifntrue)) != std::string::npos) {
    // onelab.ifntrue
    cursor = pos+olkey::ifntrue.length();
    // pos=line.find_first_of(')',cursor)+1;
    // if(enclosed(line.substr(cursor,pos-cursor),arguments)<1)    
    if(enclosed(line.substr(cursor),arguments,pos)<1)
      Msg::Fatal("Misformed <%s> statement: (%s)",
		 olkey::ifntrue.c_str(),line.c_str());
    bool condition = false;
    get(strings,longName(arguments[0]));
    if (strings.size())
      condition= true;
    else{
      get(numbers,longName(arguments[0]));
      if (numbers.size())
	condition = (bool) numbers[0].getValue();
      else
	Msg::Fatal("Unknown parameter <%s> in <%s> statement",
		   arguments[0].c_str(),olkey::ifntrue.c_str());
    }
    if (!convert_ifstatement(infile,outfile,!condition))
      Msg::Fatal("Misformed <%s> statement: %s",
		 olkey::ifntrue.c_str(),arguments[0].c_str());  
  }
  else if ( (pos=line.find(olkey::ifcond)) != std::string::npos) {
    // onelab.ifcond
    cursor = pos+olkey::ifcond.length();
    int NumArgs=extractLogic(line.substr(cursor),arguments);
    bool condition= resolveLogicExpr(arguments);
    if (!convert_ifstatement(infile,outfile,condition))
      Msg::Fatal("Misformed %s statement: <%s>", line.c_str());
  }
  else if ( (pos=line.find(olkey::ifequal)) != std::string::npos) {
    // onelab.ifequal
    cursor = pos+olkey::ifequal.length();
    if(enclosed(line.substr(cursor),arguments,pos)<2)
      Msg::Fatal("Misformed <%s> statement: (%s)",
		 olkey::ifequal.c_str(),line.c_str());;
    bool condition= false;
    get(strings,longName(arguments[0]));
    if (strings.size())
      condition =  !strings[0].getValue().compare(arguments[1]);
    if (!convert_ifstatement(infile,outfile,condition))
      Msg::Fatal("Misformed <%s> statement: (%s)",
		 olkey::ifequal.c_str(),line.c_str());
  }
  else if ( (pos=line.find(olkey::include)) != std::string::npos) {
    // onelab.include
    cursor = pos+olkey::include.length();
    if(enclosed(line.substr(cursor),arguments,pos)<1)
      Msg::Fatal("Misformed <%s> statement: (%s)",
		 olkey::include.c_str(),line.c_str());
    convert_onefile(arguments[0],outfile);
  }
  else if ( (pos=line.find(olkey::getValue)) != std::string::npos) {
    outfile << resolveGetVal(line) << std::endl;
  }
  else if ( (pos=line.find(olkey::getRegion)) != std::string::npos) {
    // onelab.getRegion, possibly several times on the line
    cursor=0;
    std::string buff,action;
    while ( (pos=line.find(olkey::getRegion,cursor)) != std::string::npos){
      int pos0=pos;
      cursor = pos+olkey::getRegion.length();
      int NumArg=enclosed(line.substr(cursor),arguments,pos);

      if(NumArg>0){
	std::string paramName;
	paramName.assign("Gmsh/Physical groups/"+arguments[0]);
	get(regions,paramName);
	if (regions.size()){
	  std::set<std::string> region;
	  region=regions[0].getValue();

	  if(NumArg>1)
	    action.assign(arguments[1]);
	  else
	    action.assign("expand");

	  if(!action.compare("size"))
	    buff.assign(ftoa(region.size()));
	  else if(!action.compare("expand")){
	    std::string pattern;
	    if(NumArg>=3)
	      pattern.assign(extractExpandPattern(arguments[2]));
	    else
	      pattern.assign("   ");

	    buff.assign(1,pattern[0]);
	    for(std::set<std::string>::const_iterator it = region.begin();
		it != region.end(); it++){
	      if(it != region.begin())
		buff.append(1,pattern[1]);
	      buff.append((*it));
	    }
	    buff.append(1,pattern[2]);
	  }
	  else
	    Msg::Fatal("Unknown %s action: <%s>",
		       olkey::getRegion.c_str(), arguments[1].c_str());
	}
	else
	  Msg::Fatal("Unknown region: <%s>",paramName.c_str());
      }
      else
	Msg::Fatal("Misformed <%s> statement: (%s)",
		   olkey::getRegion.c_str(),line.c_str());
      line.replace(pos0,cursor+pos-pos0,buff);
      cursor=pos0+buff.length();
    }
    outfile << line << std::endl; 
  }
  else if ( (pos=line.find(olkey::label)) != std::string::npos){
    Msg::Fatal("Unidentified onelab command in <%s>",line.c_str());
  }
  else{
    outfile << line << std::endl; 
  }
}

void localSolverClient::convert_onefile(std::string fileName, std::ofstream &outfile) {
  int pos;
  std::string fullName=getWorkingDir()+fileName;
  std::ifstream infile(fullName.c_str());
  if (infile.is_open()){
    Msg::Info("Convert file <%s>",fullName.c_str());
    while ( infile.good() ) {
      std::string line;
      getline (infile,line);
      convert_oneline(line,infile,outfile);
    }
    infile.close();
  }
  else
    Msg::Fatal("The file %s cannot be opened",fullName.c_str());
  modify_tags("OL.","%",";");
}


void localSolverClient::client_sentence(const std::string &name, const std::string &action, 
		       const std::vector<std::string> &arguments) {
  Msg::Fatal("The action <%s> is unknown in this ccontext",action.c_str());
}

void MetaModel::client_sentence(const std::string &name, const std::string &action, 
		 const std::vector<std::string> &arguments){
  //std::vector<onelab::number> numbers;
  std::vector<onelab::string> strings;

  if(!action.compare("register")){
    // syntax name.Register([interf...|encaps...]{,cmdl{,wdir,{host{,rdir}}}}) ;
    if(!findClientByName(name)){
      Msg::Info("Define client <%s>", name.c_str());

      std::string cmdl="",wdir="",host="",rdir="";
      if(arguments.size()>=2) cmdl.assign(resolveGetVal(arguments[1]));
      if(arguments.size()>=3) wdir.assign(resolveGetVal(arguments[2]));
      if(arguments.size()>=4) host.assign(resolveGetVal(arguments[3]));
      if(arguments.size()>=5) rdir.assign(resolveGetVal(arguments[4]));

      // check if one has a saved command line on the server 
      // (prealably read from file .ol.save)
      if(cmdl.empty())
	cmdl=Msg::GetOnelabString(name + "/CommandLine");
      registerClient(name,resolveGetVal(arguments[0]),cmdl,
		     getWorkingDir()+wdir,host,rdir);
      onelab::string o(name + "/CommandLine","");
      o.setValue(cmdl);
      o.setKind("file");
      o.setVisible(cmdl.empty());
      o.setAttribute("Highlight","Ivory");
      set(o);
    }
    else
      Msg::Error("Redefinition of client <%s>", name.c_str());
  }
  else if(!action.compare("commandLine")){
    if(findClientByName(name)){
      if(arguments.size()) {
	if(arguments[0].size()){
	  onelab::string o(name + "/CommandLine",arguments[0]);
	  o.setKind("file");
	  o.setVisible(false);
	  set(o);
	}
	else
	  Msg::Error("No pathname given for client <%s>", name.c_str());
      }
    }
    else
      Msg::Error("Unknown client <%s>", name.c_str());
  }
  else if(!action.compare("active")){
    localSolverClient *c;
    if(c=findClientByName(name)){
      if(arguments.size()) {
	if(arguments[0].size())
	  c->setActive(atof( resolveGetVal(arguments[0]).c_str() ));
	else
	  Msg::Error("No argument for <%s.Active> statement", name.c_str());
      }
    }
    else
      Msg::Fatal("Unknown client <%s>", name.c_str());
  }
  else if(!action.compare(olkey::arguments)){
    if(arguments[0].size()){
      strings.resize(1);
      strings[0].setName(name+"/Arguments");
      strings[0].setValue(resolveGetVal(arguments[0]));
      strings[0].setVisible(false);
      set(strings[0]);
    }
  }
  else if(!action.compare(olkey::inFiles)){
    if(arguments[0].size()){
      strings.resize(1);
      strings[0].setName(name+"/InputFiles");
      strings[0].setValue(resolveGetVal(arguments[0]));
      strings[0].setKind("file");
      strings[0].setVisible(false);
      std::vector<std::string> choices;
      for(unsigned int i = 0; i < arguments.size(); i++)
	//if(std::find(choices.begin(),choices.end(),arguments[i])
	//==choices.end())
	choices.push_back(resolveGetVal(arguments[i]));
      strings[0].setChoices(choices);
      set(strings[0]);
    }
  }
  else if(!action.compare(olkey::outFiles)){
    if(arguments[0].size()){
      strings.resize(1);
      strings[0].setName(name+"/OutputFiles");
      strings[0].setValue(resolveGetVal(arguments[0]));
      strings[0].setKind("file");
      strings[0].setVisible(false);
      std::vector<std::string> choices;
      for(unsigned int i = 0; i < arguments.size(); i++)
	//if(std::find(choices.begin(),choices.end(),arguments[i])
	//==choices.end())
	choices.push_back(resolveGetVal(arguments[i]));
      strings[0].setChoices(choices);
      set(strings[0]);
    }
  }
  else if(!action.compare(olkey::upload)){
    if(arguments[0].size()){
      strings.resize(1);
      strings[0].setName(name+"/PostArray");
      strings[0].setValue(resolveGetVal(arguments[0]));
      std::vector<std::string> choices;
      for(unsigned int i = 0; i < arguments.size(); i++){
	std::string str=resolveGetVal(arguments[i]);
	choices.push_back(str);
	if(i%4==3){ // predefine the parameters to upload
	  Msg::recordFullName(str);
	  std::vector<onelab::number> numbers;
	  get(numbers, str);
	  if(numbers.empty()){ 
	    numbers.resize(1);
	    numbers[0].setName(str);
	    numbers[0].setValue(0);
	    numbers[0].setVisible(false);
	    set(numbers[0]);
	  }
	}
      }
      strings[0].setChoices(choices);
      strings[0].setVisible(false);
      set(strings[0]);
    }
  }
  else if(!action.compare(olkey::merge)){
    if(arguments[0].size()){
      strings.resize(1);
      strings[0].setName(name+"/Merge");
      strings[0].setValue(resolveGetVal(arguments[0]));
      strings[0].setKind("file");
      strings[0].setVisible(false);
      std::vector<std::string> choices;
      for(unsigned int i = 0; i < arguments.size(); i++)
	choices.push_back(resolveGetVal(arguments[i]));
      strings[0].setChoices(choices);
      set(strings[0]);
    }
  }
  // else if(!action.compare(olkey::checkCmd)){
  //   if(arguments[0].size()){
  //     strings.resize(1);
  //     strings[0].setName(name+"/9CheckCommand");
  //     strings[0].setValue(resolveGetVal(arguments[0]));
  //     strings[0].setVisible(false);
  //     set(strings[0]);
  //   }
  // }
  // else if(!action.compare(olkey::computeCmd)){
  //   if(arguments[0].size()){
  //     strings.resize(1);
  //     strings[0].setName(name+"/9ComputeCommand");
  //     strings[0].setValue(resolveGetVal(arguments[0]));
  //     strings[0].setVisible(false);
  //     set(strings[0]);
  //   }
  // }
  // else if(!action.compare("Set")){
  //   if(arguments[0].size()){
  //     strings.resize(1);
  //     strings[0].setName(name);
  //     strings[0].setValue(resolveGetVal(arguments[0]));
  //     strings[0].setVisible(false);
  //     if( (arguments[0].find(".geo") != std::string::npos) || 
  // 	  (arguments[0].find(".sif") != std::string::npos) ||
  // 	  (arguments[0].find(".pro") != std::string::npos)) {
  // 	strings[0].setKind("file");
  // 	strings[0].setVisible(true);
  //     }
  //     std::vector<std::string> choices;
  //     for(unsigned int i = 0; i < arguments.size(); i++)
  // 	if(std::find(choices.begin(),choices.end(),arguments[i])
  // 	   ==choices.end())
  // 	  choices.push_back(resolveGetVal(arguments[i]));
  //     strings[0].setChoices(choices);
  //     set(strings[0]);
  //   }
  // }
  // else if(!action.compare("List")){
  //   //no check whether choices[i] already inserted
  //   if(arguments[0].size()){
  //     strings.resize(1);
  //     strings[0].setName(name);
  //     strings[0].setValue(resolveGetVal(arguments[0]));
  //     strings[0].setVisible(false);
  //     std::vector<std::string> choices;
  //     for(unsigned int i = 0; i < arguments.size(); i++)
  // 	choices.push_back(resolveGetVal(arguments[i]));
  //     strings[0].setChoices(choices);
  //     set(strings[0]);
  //   }
  // }
  else
    Msg::Fatal("Unknown action <%s>",action.c_str());
}
