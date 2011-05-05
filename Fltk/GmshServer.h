// Gmsh - Copyright (C) 1997-2008 C. Geuzaine, J.-F. Remacle
//
// See the LICENSE.txt file for license information. Please report all
// bugs and problems to <gmsh@geuz.org>.

#ifndef _GMSH_SERVER_H_
#define _GMSH_SERVER_H_

void SystemCall(const char *str);
int WaitForData(int socket, int num, double waitint);

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(_AIX)
#include <strings.h>
#endif

#if !defined(WIN32) || defined(__CYGWIN__)

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/time.h>
#include <netinet/in.h>

#else // pure windows

#include <winsock.h>

#endif

static int myselect(int socket, int seconds)
{
  struct timeval tv;
  tv.tv_sec = seconds;
  tv.tv_usec = 0;
  fd_set rfds;
  FD_ZERO(&rfds);
  FD_SET(socket, &rfds);
  return select(socket + 1, &rfds, NULL, NULL, &tv);
}

class GmshServer {
 public:
  // This should match what's in GmshClient.h (Do not use values
  // greater that 65535: if we receive types > 65535 we assume that we
  // receive data from a machine with a different byte ordering, and
  // we just swap the bytes)
  typedef enum{ CLIENT_START        = 1,
                CLIENT_STOP         = 2,
                CLIENT_INFO         = 10,
                CLIENT_WARNING      = 11,
                CLIENT_ERROR        = 12,
                CLIENT_PROGRESS     = 13,
                CLIENT_MERGE_FILE   = 20, // old name: CLIENT_VIEW
                CLIENT_PARSE_STRING = 21,
                CLIENT_OPTION_1     = 100,
                CLIENT_OPTION_2     = 101,
                CLIENT_OPTION_3     = 102,
                CLIENT_OPTION_4     = 103,
                CLIENT_OPTION_5     = 104 } MessageType;
  static int init, s;

 private:
  int _maxdelay, _portno, _sock;
  char *_sockname;
  int _ReceiveData(void *buffer, int bytes)
  {
    char *buf = (char *)buffer;
    int sofar = 0;
    int remaining = bytes;
    do {
      int len = recv(_sock, buf + sofar, remaining, 0);
      if(len <= 0)
        return 0;
      sofar += len;
      remaining -= len;
    } while(remaining > 0);
    return bytes;
  }
  void _SwapBytes(char *array, int size, int n)
  {
    char *x = new char[size];
    for(int i = 0; i < n; i++) {
      char *a = &array[i * size];
      memcpy(x, a, size);
      for(int c = 0; c < size; c++)
        a[size - 1 - c] = x[c];
    }
    delete [] x;
  }
  int _AcceptConnection(int s)
  {
#if defined(HAVE_NO_SOCKLEN_T)
    int len;
#else
    socklen_t len;
#endif
    if(_portno < 0){
#if !defined(WIN32) || defined(__CYGWIN__)
      struct sockaddr_un from_un;
      len = sizeof(from_un);
      return accept(s, (struct sockaddr *)&from_un, &len);
#else
      return -7; // Unix sockets not available on Windows without Cygwin
#endif
    }
    else{
      struct sockaddr_in from_in;
      len = sizeof(from_in);
      return accept(s, (struct sockaddr *)&from_in, &len);
    }
  }

 public:
  GmshServer(int maxdelay = 4)
    : _maxdelay(maxdelay), _portno(-1), _sock(0), _sockname(NULL) {}
  ~GmshServer(){}
  int StartClient(char *command, char *sockname = NULL)
  {
    int justwait = 0;

    if(!command || !strlen(command))
      justwait = 1;

    _sockname = sockname;

    // no socket? launch the command directly
    if(!_sockname) {
      SystemCall(command);
      return 1;
    }

    if(strstr(_sockname, "/") || strstr(_sockname, "\\") || !strstr(_sockname, ":")){
      // UNIX socket (testing ":" is not enough with Windows paths)
      _portno = -1;
    }
    else{
      // TCP/IP socket
      char *port = strstr(_sockname, ":");
      _portno = atoi(port + 1);
    }
    
    if(_portno < 0){
      // delete the file if it already exists
#if !defined(WIN32) || defined(__CYGWIN__)
      unlink(_sockname);

      // make the socket
      s = socket(PF_UNIX, SOCK_STREAM, 0);
      if(s < 0)
        return -1;  // Error: Couldn't create socket
      
      // bind the socket to its name
      struct sockaddr_un addr_un;
      memset((char *) &addr_un, 0, sizeof(addr_un));
      strcpy(addr_un.sun_path, _sockname);
      addr_un.sun_family = AF_UNIX;
      if(bind(s, (struct sockaddr *)&addr_un, sizeof(addr_un)) < 0)
        return -2;  // Error: Couldn't bind socket to name

      // change permissions on the socket name in case it has to be rm'd later
      chmod(_sockname, 0666);
#else
      return -7; // Unix sockets not available on Windows without Cygwin
#endif
    }
    else{
#if defined(WIN32) && !defined(__CYGWIN__)
      if(!init){ 
        WSADATA wsaData;
        int iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
        if(iResult != NO_ERROR)
          return -8;  // Error: Couldn't initialize Windows sockets
      }
#endif
      if(init != _portno){ 
        // We need a better solution to deal with addresses that have
        // already been bound...
        init = _portno;
        
        // make the socket
        s = socket(AF_INET, SOCK_STREAM, 0);
#if !defined(WIN32) || defined(__CYGWIN__)
        if(s < 0)
#else
        if(s == INVALID_SOCKET)
#endif
          return -1;  // Error: Couldn't create socket
        
        // bind the socket to its name
        struct sockaddr_in addr_in;
        memset((char *) &addr_in, 0, sizeof(addr_in));
        addr_in.sin_family = AF_INET;
        addr_in.sin_addr.s_addr = INADDR_ANY;
        addr_in.sin_port = htons(_portno);
        if(bind(s, (struct sockaddr *)&addr_in, sizeof(addr_in)) < 0)
          return -2;  // Error: Couldn't bind socket to name
      }
    }

    if(!justwait)
      SystemCall(command); // Start the solver
    
    // listen on socket (queue up to 20 connections before having
    // them automatically rejected)
    if(listen(s, 20))
      return -3;  // Error: Socket listen failed
    
    if(justwait){
      // wait indefinitely until we get data
      if(WaitForData(s, -1, 0.5))
        return -6; // not an actual error: we just stopped listening
    }
    else{
      // Wait at most _maxdelay seconds for data, issue error if no
      // connection in that amount of time
      if(!myselect(s, _maxdelay))
        return -4;  // Error: Socket listening timeout
    }

    // accept connection request
    if((_sock = _AcceptConnection(s)) < 0)
      return -5;  // Error: Socket accept failed
    
    return _sock;
  }
  int ReceiveMessageHeader(int *type, int *len)
  {
    bool swap = false;
    if(_ReceiveData(type, sizeof(int))){
      if(*type < 0) return 0;
      if(*type > 65535){ 
        // the data comes from a machine with different endianness and
        // we must swap the bytes
        swap = true;
        _SwapBytes((char*)type, sizeof(int), 1);
      }
      if(_ReceiveData(len, sizeof(int))){
        if(*len < 0) return 0;
        if(swap) _SwapBytes((char*)len, sizeof(int), 1);
        return 1;
      }
    }
    return 0;
  }
  int ReceiveMessageBody(int len, char *str)
  {
    if(_ReceiveData(str, len) == len) {
      str[len] = '\0';
      return 1;
    }
    return 0;
  }
  int StopClient()
  {
#if !defined(WIN32) || defined(__CYGWIN__)
    if(_portno < 0){
      // UNIX socket
      if(unlink(_sockname) == -1)
        return -1;  // Impossible to unlink the socket
    }
    close(_sock);
#else
    closesocket(_sock);
#endif
    return 0;
  }
};

#endif
