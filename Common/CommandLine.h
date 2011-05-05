// Gmsh - Copyright (C) 1997-2008 C. Geuzaine, J.-F. Remacle
//
// See the LICENSE.txt file for license information. Please report all
// bugs and problems to <gmsh@geuz.org>.

#ifndef _COMMAND_LINE_H_
#define _COMMAND_LINE_H_

extern char gmsh_progname[], gmsh_copyright[], gmsh_version[], gmsh_os[];
extern char gmsh_date[], gmsh_host[], gmsh_packager[], gmsh_url[];
extern char gmsh_email[], gmsh_gui[], gmsh_options[], gmsh_license[];

void Get_Options(int argc, char *argv[]);
void Print_Usage(const char *name);

char *Get_BuildOptions();

int Get_GmshMajorVersion();
int Get_GmshMinorVersion();
int Get_GmshPatchVersion();
const char *Get_GmshExtraVersion();
const char *Get_GmshVersion();
const char *Get_GmshBuildDate();
const char *Get_GmshBuildHost();
const char *Get_GmshPackager();
const char *Get_GmshBuildOS();
const char *Get_GmshShortLicense();

#endif
