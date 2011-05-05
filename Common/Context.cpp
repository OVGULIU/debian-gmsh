// Gmsh - Copyright (C) 1997-2009 C. Geuzaine, J.-F. Remacle
//
// See the LICENSE.txt file for license information. Please report all
// bugs and problems to <gmsh@geuz.org>.

#include <stdlib.h>
#include <string.h>
#include "GmshConfig.h"
#include "Context.h"

#if defined(HAVE_FLTK)
#include <FL/Fl.H>
#endif

static const char *getEnvironmentVariable(const char *var)
{
#if !defined(WIN32)
  return getenv(var);
#else
  const char *tmp = getenv(var);
  // Don't accept top dir or anything partially expanded like
  // c:\Documents and Settings\%USERPROFILE%, etc.
  if(!tmp || !strcmp(tmp, "/") || strstr(tmp, "%") || strstr(tmp, "$"))
    return 0;
  else
    return tmp;
#endif
}

CTX::CTX()
{
  // Initialize everything that has no default value in
  // DefaultOptions.h

  short int word = 0x0001;
  char *byte = (char*)&word;
  bigEndian = (byte[0] ? 0 : 1);

  const char *tmp;
  if((tmp = getEnvironmentVariable("GMSH_HOME")))
    homeDir = tmp;
  else if((tmp = getEnvironmentVariable("HOME")))
    homeDir = tmp;
  else if((tmp = getEnvironmentVariable("TMP")))
    homeDir = tmp;
  else if((tmp = getEnvironmentVariable("TEMP")))
    homeDir = tmp;
  else
    homeDir = "";
  int len = homeDir.size();
  if(len && homeDir[len - 1] != '/')
    homeDir += "/";

  batch = batchAfterMesh = 0;
  outputFileName = "";
  bgmFileName = "";
  createAppendMeshStatReport = 0;
  lc = 1.;
  min[0] = min[1] = min[2] = max[2] = 0.; 
  max[0] = max[1] = 1.; // for nice view when adding point in new model
  cg[0] = cg[1] = cg[2] = 0.;
  polygonOffset = 0;
  printing = 0;
  meshTimer[0] = meshTimer[1] = meshTimer[2] = 0.;
  drawRotationCenter = 0;
  pickElements = 0;
  geom.draw = 1;
  mesh.draw = 1;
  post.draw = 1;
  lock = 0; // very primitive locking
  mesh.changed = 0;
  post.combineTime = 0; // try to combineTime views at startup
#if defined(HAVE_FLTK)
  glFontEnum = FL_HELVETICA;
#else
  glFontEnum = -1;
#endif
  forcedBBox = 0;
  hideUnselected = 0;
  numWindows = numTiles = 1;
  deltaFontSize = 0;
}

CTX *CTX::_instance = 0;

CTX *CTX::instance()
{
  if(!_instance) _instance = new CTX();
  return _instance;
}

unsigned int CTX::packColor(int R, int G, int B, int A)
{
  if(bigEndian)
    return ( (unsigned int)((R)<<24 | (G)<<16 | (B)<<8 | (A)) );
  else
    return ( (unsigned int)((A)<<24 | (B)<<16 | (G)<<8 | (R)) );
}

int CTX::unpackRed(unsigned int X)
{
  if(bigEndian)
    return ( ( (X) >> 24 ) & 0xff );
  else
    return ( (X) & 0xff );
}

int CTX::unpackGreen(unsigned int X)
{
  if(bigEndian)
    return ( ( (X) >> 16 ) & 0xff );
  else
    return ( ( (X) >> 8 ) & 0xff );
}

int CTX::unpackBlue(unsigned int X)
{
  if(bigEndian)
    return ( ( (X) >> 8 ) & 0xff );
  else
    return ( ( (X) >> 16 ) & 0xff );
}

int CTX::unpackAlpha(unsigned int X)
{
  if(bigEndian)
    return ( (X) & 0xff );
  else
    return ( ( (X) >> 24 ) & 0xff );
}
