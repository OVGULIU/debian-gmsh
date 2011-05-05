// Gmsh - Copyright (C) 1997-2008 C. Geuzaine, J.-F. Remacle
//
// See the LICENSE.txt file for license information. Please report all
// bugs and problems to <gmsh@geuz.org>.

#ifndef _GMSH_UI_H_
#define _GMSH_UI_H_

#if defined(HAVE_FLTK)
#  include <FL/Fl.H>
#  if (FL_MAJOR_VERSION == 1) && (FL_MINOR_VERSION == 1)
#    if (FL_PATCH_VERSION >= 6)
#      define HAVE_FLTK_1_1_6_OR_ABOVE
#    endif
#    if (FL_PATCH_VERSION >= 5)
#      define HAVE_FLTK_1_1_5_OR_ABOVE
#    endif
#  else
#    error "Gmsh requires FLTK 1.1.x"
#  endif
#  include <FL/gl.h>
#  if defined(__APPLE__)
#    include <OpenGL/glu.h>
#  else
#    include <GL/glu.h>
#  endif
# define GMSH_WINDOW_BOX FL_FLAT_BOX
#endif

// some Windows versions define min/max macros! 
#if defined(WIN32)
#undef min
#undef max
#endif

#endif
