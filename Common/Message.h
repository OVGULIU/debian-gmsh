// Gmsh - Copyright (C) 1997-2008 C. Geuzaine, J.-F. Remacle
//
// See the LICENSE.txt file for license information. Please report all
// bugs and problems to <gmsh@geuz.org>.

#ifndef _MESSAGE_H_
#define _MESSAGE_H_

#include <map>
#include <vector>
#include <string>
#include <stdarg.h>

class GmshMessage;

// a class to manage messages
class Message {
 private:
  // current cpu number and total number of cpus
  static int _commRank, _commSize;
  // verbosity level
  static int _verbosity;
  // step (in %) of the progress meter and current progress %
  static int _progressMeterStep, _progressMeterCurrent;
  // timers
  static std::map<std::string, double> _timers;
  // counters
  static int _warningCount, _errorCount;
  // callback
  static GmshMessage *_callback;
 public:
  Message() {}
  static void Init(int argc, char **argv);
  static void Exit(int level);
  static int GetCommRank(){ return _commRank; }
  static int GetCommSize(){ return _commSize; }
  static void SetCommRank(int val){ _commRank = val; }
  static void SetCommSize(int val){ _commSize = val; }
  static void SetCallback(GmshMessage *callback){ _callback = callback; }
  static void Barrier();
  static int GetNumThreads();
  static int GetMaxThreads();
  static int GetThreadNum();
  static void SetVerbosity(int val){ _verbosity = val; }
  static int GetVerbosity(){ return _verbosity; }
  static void Fatal(const char *fmt, ...);
  static void Error(const char *fmt, ...);
  static void Warning(const char *fmt, ...);
  static void Info(const char *fmt, ...);
  static void Direct(const char *fmt, ...);
  static void Direct(int level, const char *fmt, ...);
  static void StatusBar(int num, bool log, const char *fmt, ...);
  static void Debug(const char *fmt, ...);
  static void ProgressMeter(int n, int N, const char *fmt, ...);
  static void ProgressMeter(int n, int N){ ProgressMeter(n, N, ""); }
  static void SetProgressMeterStep(int step){ _progressMeterStep = step; }
  static void ResetProgressMeter(){ if(!_commRank) _progressMeterCurrent = 0; }
  static double &Timer(std::string str){ return _timers[str]; }
  static void PrintTimers();
  static void ResetErrorCounter(){ _warningCount = 0; _errorCount = 0; }
  static void PrintErrorCounter(const char *title);
  static double GetValue(const char *text, double defaultval);
  static bool GetBinaryAnswer(const char *question, const char *yes, 
			      const char *no,  bool defaultval=true);
};

typedef Message Msg;

#endif
