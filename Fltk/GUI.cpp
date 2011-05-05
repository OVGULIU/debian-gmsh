// Gmsh - Copyright (C) 1997-2008 C. Geuzaine, J.-F. Remacle
//
// See the LICENSE.txt file for license information. Please report all
// bugs and problems to <gmsh@geuz.org>.

#include <string>
#include <sstream>
#include "GmshUI.h"
#include "GmshDefines.h"
#include "Message.h"
#include "Numeric.h"
#include "Context.h"
#include "Options.h"
#include "Draw.h"
#include "GUI.h"
#include "Callbacks.h"
#include "Win32Icon.h"
#include "OpenFile.h"
#include "CommandLine.h"
#include "Generator.h"
#include "Solvers.h"
#include "PluginManager.h"
#include "Shortcut_Window.h"
#include "PView.h"
#include "PViewOptions.h"
#include "PViewData.h"
#include "Field.h"
#include "GModel.h"
#include "GeoStringInterface.h"

#define NB_BUTT_SCROLL 25
#define NB_HISTORY_MAX 1000

#define IW (10*fontsize)  // input field width
#define BB (7*fontsize)   // width of a button with internal label
#define BH (2*fontsize+1) // button height
#define WB (7)            // window border

extern Context_T CTX;

extern StringXColor GeneralOptions_Color[] ;
extern StringXColor GeometryOptions_Color[] ;
extern StringXColor MeshOptions_Color[] ;
extern StringXColor SolverOptions_Color[] ;
extern StringXColor PostProcessingOptions_Color[] ;
extern StringXColor ViewOptions_Color[] ;
extern StringXColor PrintOptions_Color[] ;

// Definition of the static menus (we cannot use the 'g', 'm' 's' and
// 'p' mnemonics since they are already defined as global shortcuts)

Fl_Menu_Item m_menubar_table[] = {
  {"&File", 0, 0, 0, FL_SUBMENU},
    {"&New...",     FL_CTRL+'n', (Fl_Callback *)file_new_cb, 0},
    {"&Open...",    FL_CTRL+'o', (Fl_Callback *)file_open_cb, 0},
    {"M&erge...",   FL_CTRL+FL_SHIFT+'o', (Fl_Callback *)file_merge_cb, 0, FL_MENU_DIVIDER},
    {"&Rename...",  FL_CTRL+'r', (Fl_Callback *)file_rename_cb, 0},
    {"Save &As...", FL_CTRL+'s', (Fl_Callback *)file_save_as_cb, 0},
    {"Sa&ve Mesh",  FL_CTRL+FL_SHIFT+'s', (Fl_Callback *)mesh_save_cb, 0, FL_MENU_DIVIDER},
    {"&Quit",       FL_CTRL+'q', (Fl_Callback *)file_quit_cb, 0},
    {0},
  {"&Tools", 0, 0, 0, FL_SUBMENU},
    {"&Options...",      FL_CTRL+FL_SHIFT+'n', (Fl_Callback *)options_cb, 0},
    {"Pl&ugins...",      FL_CTRL+FL_SHIFT+'u', (Fl_Callback *)view_plugin_cb, (void*)(-1)},
    {"&Visibility",      FL_CTRL+FL_SHIFT+'v', (Fl_Callback *)visibility_cb, 0},
    {"&Clipping",        FL_CTRL+FL_SHIFT+'c', (Fl_Callback *)clip_cb, 0},
    {"&Manipulator",     FL_CTRL+FL_SHIFT+'m', (Fl_Callback *)manip_cb, 0, FL_MENU_DIVIDER},
    {"S&tatistics",      FL_CTRL+'i', (Fl_Callback *)statistics_cb, 0},
    {"M&essage Console", FL_CTRL+'l', (Fl_Callback *)message_cb, 0},
    {0},
  {"&Help", 0, 0, 0, FL_SUBMENU},
    {"On&line Documentation", 0, (Fl_Callback *)help_online_cb, 0, FL_MENU_DIVIDER},
    {"M&ouse Actions",        0, (Fl_Callback *)help_mouse_cb, 0},
    {"&Keyboard Shortcuts",   0, (Fl_Callback *)help_short_cb, 0},
    {"C&ommand Line Options", 0, (Fl_Callback *)help_command_line_cb, 0},
    {"&Current Options",      0, (Fl_Callback *)status_xyz1p_cb, (void*)"?", FL_MENU_DIVIDER},
    {"&About Gmsh...",        0, (Fl_Callback *)help_about_cb, 0},
    {0},
  {0}
};

// Alternative items for the MacOS system menu bar (removed '&'
// shortcuts: they would cause spurious menu items to appear on the
// menu window; removed File->Quit)

#if defined(__APPLE__) && defined(HAVE_FLTK_1_1_5_OR_ABOVE)

// random changes in fltk are driving me nuts sometimes
#if (FL_MAJOR_VERSION == 1) && (FL_MINOR_VERSION == 1) && (FL_PATCH_VERSION <= 6)
#undef FL_META
#define FL_META FL_CTRL
#endif

Fl_Menu_Item m_sys_menubar_table[] = {
  {"File", 0, 0, 0, FL_SUBMENU},
    {"New...",     FL_META+'n', (Fl_Callback *)file_new_cb, 0},
    {"Open...",    FL_META+'o', (Fl_Callback *)file_open_cb, 0},
    {"Merge...",   FL_META+FL_SHIFT+'o', (Fl_Callback *)file_merge_cb, 0, FL_MENU_DIVIDER},
    {"Rename...",  FL_META+'r', (Fl_Callback *)file_rename_cb, 0},
    {"Save As...", FL_META+'s', (Fl_Callback *)file_save_as_cb, 0},
    {"Save Mesh",  FL_META+FL_SHIFT+'s', (Fl_Callback *)mesh_save_cb, 0},
    {0},
  {"Tools", 0, 0, 0, FL_SUBMENU},
    {"Options...",      FL_META+FL_SHIFT+'n', (Fl_Callback *)options_cb, 0},
    {"Plugins...",      FL_META+FL_SHIFT+'u', (Fl_Callback *)view_plugin_cb, (void*)(-1)},
    {"Visibility",      FL_META+FL_SHIFT+'v', (Fl_Callback *)visibility_cb, 0},
    {"Clipping",        FL_META+FL_SHIFT+'c', (Fl_Callback *)clip_cb, 0},
    {"Manipulator",     FL_META+FL_SHIFT+'m', (Fl_Callback *)manip_cb, 0, FL_MENU_DIVIDER},
    {"Statistics",      FL_META+'i', (Fl_Callback *)statistics_cb, 0},
    {"Message Console", FL_META+'l', (Fl_Callback *)message_cb, 0},
    {0},
  {"Window", 0, 0, 0, FL_SUBMENU},
    {"Minimize",           FL_META+'m', (Fl_Callback *)window_cb, (void*)"minimize"},
    {"Zoom",               0, (Fl_Callback *)window_cb, (void*)"zoom", FL_MENU_DIVIDER},
    {"Bring All to Front", 0, (Fl_Callback *)window_cb, (void*)"front"},
    {0},
  {"Help", 0, 0, 0, FL_SUBMENU},
    {"Online Documentation", 0, (Fl_Callback *)help_online_cb, 0, FL_MENU_DIVIDER},
    {"Mouse Actions",        0, (Fl_Callback *)help_mouse_cb, 0},
    {"Keyboard Shortcuts",   0, (Fl_Callback *)help_short_cb, 0},
    {"Command Line Options", 0, (Fl_Callback *)help_command_line_cb, 0},
    {"Current Options",      0, (Fl_Callback *)status_xyz1p_cb, (void*)"?", FL_MENU_DIVIDER},
    {"About Gmsh...",        0, (Fl_Callback *)help_about_cb, 0},
    {0},
  {0}
};

#endif

Fl_Menu_Item m_module_table[] = {
  {"Geometry",        'g', (Fl_Callback *)mod_geometry_cb, 0},
  {"Mesh",            'm', (Fl_Callback *)mod_mesh_cb, 0},
  {"Solver",          's', (Fl_Callback *)mod_solver_cb, 0},
  {"Post-processing", 'p', (Fl_Callback *)mod_post_cb, 0},
  {0}
};

// Definition of the dynamic contexts

Context_Item menu_geometry[] = {
  {"0Geometry", NULL} ,
  {"Elementary entities", (Fl_Callback *)geometry_elementary_cb} ,
  {"Physical groups",     (Fl_Callback *)geometry_physical_cb} ,
  {"Edit",                (Fl_Callback *)geometry_edit_cb} , 
  {"Reload",              (Fl_Callback *)geometry_reload_cb} , 
  {0}
};  
    Context_Item menu_geometry_elementary[] = {
      {"0Geometry>Elementary", NULL} ,
      {"Add",       (Fl_Callback *)geometry_elementary_add_cb} ,
      {"Delete",    (Fl_Callback *)geometry_elementary_delete_cb, (void*)0} ,
      {"Translate", (Fl_Callback *)geometry_elementary_translate_cb, (void*)0} ,
      {"Rotate",    (Fl_Callback *)geometry_elementary_rotate_cb, (void*)0} ,
      {"Scale",     (Fl_Callback *)geometry_elementary_scale_cb, (void*)0} ,
      {"Symmetry",  (Fl_Callback *)geometry_elementary_symmetry_cb, (void*)0} ,
      {"Extrude",   (Fl_Callback *)geometry_elementary_extrude_cb, (void*)0} ,
      {"Coherence", (Fl_Callback *)geometry_elementary_coherence_cb} ,
      {0} 
    };  
        Context_Item menu_geometry_elementary_add[] = {
          {"0Geometry>Elementary>Add", NULL} ,
          {"New",       (Fl_Callback *)geometry_elementary_add_new_cb, (void*)0} ,
          {"Translate", (Fl_Callback *)geometry_elementary_add_translate_cb, (void*)0} ,
          {"Rotate",    (Fl_Callback *)geometry_elementary_add_rotate_cb, (void*)0} ,
          {"Scale",     (Fl_Callback *)geometry_elementary_add_scale_cb, (void*)0} ,
          {"Symmetry",  (Fl_Callback *)geometry_elementary_add_symmetry_cb, (void*)0} ,
          {0} 
        };  
            Context_Item menu_geometry_elementary_add_new[] = {
              {"0Geometry>Elementary>Add>New", NULL} ,
              {"Parameter",     (Fl_Callback *)geometry_elementary_add_new_cb, (void*)"Parameter"} ,
              {"Point",         (Fl_Callback *)geometry_elementary_add_new_cb, (void*)"Point"} ,
              {"Straight line", (Fl_Callback *)geometry_elementary_add_new_cb, (void*)"Line"} ,
              {"Spline",        (Fl_Callback *)geometry_elementary_add_new_cb, (void*)"Spline"} ,
              {"B-Spline",      (Fl_Callback *)geometry_elementary_add_new_cb, (void*)"BSpline"} ,
              {"Circle arc",    (Fl_Callback *)geometry_elementary_add_new_cb, (void*)"Circle"} ,
              {"Ellipse arc",   (Fl_Callback *)geometry_elementary_add_new_cb, (void*)"Ellipse"} ,
              {"Plane surface", (Fl_Callback *)geometry_elementary_add_new_cb, (void*)"Plane Surface"} ,
              {"Ruled surface", (Fl_Callback *)geometry_elementary_add_new_cb, (void*)"Ruled Surface"} ,
              {"Volume",        (Fl_Callback *)geometry_elementary_add_new_cb, (void*)"Volume"} ,
              {0} 
            };  
            Context_Item menu_geometry_elementary_add_translate[] = {
              {"0Geometry>Elementary>Add>Translate", NULL} ,
              {"Point",   (Fl_Callback *)geometry_elementary_add_translate_cb, (void*)"Point"} ,  
              {"Line",    (Fl_Callback *)geometry_elementary_add_translate_cb, (void*)"Line"} ,	  
              {"Surface", (Fl_Callback *)geometry_elementary_add_translate_cb, (void*)"Surface"} ,
              {"Volume",  (Fl_Callback *)geometry_elementary_add_translate_cb, (void*)"Volume"} , 
              {0} 
            };  
            Context_Item menu_geometry_elementary_add_rotate[] = {
              {"0Geometry>Elementary>Add>Rotate", NULL} ,
              {"Point",   (Fl_Callback *)geometry_elementary_add_rotate_cb, (void*)"Point"} ,  
              {"Line",    (Fl_Callback *)geometry_elementary_add_rotate_cb, (void*)"Line"} ,	  
              {"Surface", (Fl_Callback *)geometry_elementary_add_rotate_cb, (void*)"Surface"} ,
              {"Volume",  (Fl_Callback *)geometry_elementary_add_rotate_cb, (void*)"Volume"} , 
              {0} 
            };  
            Context_Item menu_geometry_elementary_add_scale[] = {
              {"0Geometry>Elementary>Add>Scale", NULL} ,
              {"Point",   (Fl_Callback *)geometry_elementary_add_scale_cb, (void*)"Point"} ,  
              {"Line",    (Fl_Callback *)geometry_elementary_add_scale_cb, (void*)"Line"} ,	  
              {"Surface", (Fl_Callback *)geometry_elementary_add_scale_cb, (void*)"Surface"} ,
              {"Volume",  (Fl_Callback *)geometry_elementary_add_scale_cb, (void*)"Volume"} , 
              {0} 
            };  
            Context_Item menu_geometry_elementary_add_symmetry[] = {
              {"0Geometry>Elementary>Add>Symmetry", NULL} ,
              {"Point",   (Fl_Callback *)geometry_elementary_add_symmetry_cb, (void*)"Point"} ,  
              {"Line",    (Fl_Callback *)geometry_elementary_add_symmetry_cb, (void*)"Line"} ,	  
              {"Surface", (Fl_Callback *)geometry_elementary_add_symmetry_cb, (void*)"Surface"} ,
              {"Volume",  (Fl_Callback *)geometry_elementary_add_symmetry_cb, (void*)"Volume"} , 
              {0} 
            };  
        Context_Item menu_geometry_elementary_delete[] = {
          {"0Geometry>Elementary>Delete", NULL} ,
          {"Point",   (Fl_Callback *)geometry_elementary_delete_cb, (void*)"Point"} ,
          {"Line",    (Fl_Callback *)geometry_elementary_delete_cb, (void*)"Line"} ,
          {"Surface", (Fl_Callback *)geometry_elementary_delete_cb, (void*)"Surface"} ,
          {"Volume",  (Fl_Callback *)geometry_elementary_delete_cb, (void*)"Volume"} ,
          {0} 
        };  
        Context_Item menu_geometry_elementary_translate[] = {
          {"0Geometry>Elementary>Translate", NULL} ,
          {"Point",   (Fl_Callback *)geometry_elementary_translate_cb, (void*)"Point"} ,  
          {"Line",    (Fl_Callback *)geometry_elementary_translate_cb, (void*)"Line"} ,	  
          {"Surface", (Fl_Callback *)geometry_elementary_translate_cb, (void*)"Surface"} ,
          {"Volume",  (Fl_Callback *)geometry_elementary_translate_cb, (void*)"Volume"} , 
          {0} 
        };  
        Context_Item menu_geometry_elementary_rotate[] = {
          {"0Geometry>Elementary>Rotate", NULL} ,
          {"Point",   (Fl_Callback *)geometry_elementary_rotate_cb, (void*)"Point"} ,  
          {"Line",    (Fl_Callback *)geometry_elementary_rotate_cb, (void*)"Line"} ,	  
          {"Surface", (Fl_Callback *)geometry_elementary_rotate_cb, (void*)"Surface"} ,
          {"Volume",  (Fl_Callback *)geometry_elementary_rotate_cb, (void*)"Volume"} , 
          {0} 
        };  
        Context_Item menu_geometry_elementary_scale[] = {
          {"0Geometry>Elementary>Scale", NULL} ,
          {"Point",   (Fl_Callback *)geometry_elementary_scale_cb, (void*)"Point"} ,  
          {"Line",    (Fl_Callback *)geometry_elementary_scale_cb, (void*)"Line"} ,	  
          {"Surface", (Fl_Callback *)geometry_elementary_scale_cb, (void*)"Surface"} ,
          {"Volume",  (Fl_Callback *)geometry_elementary_scale_cb, (void*)"Volume"} , 
          {0} 
        };  
        Context_Item menu_geometry_elementary_symmetry[] = {
          {"0Geometry>Elementary>Symmetry", NULL} ,
          {"Point",   (Fl_Callback *)geometry_elementary_symmetry_cb, (void*)"Point"} ,  
          {"Line",    (Fl_Callback *)geometry_elementary_symmetry_cb, (void*)"Line"} ,	  
          {"Surface", (Fl_Callback *)geometry_elementary_symmetry_cb, (void*)"Surface"} ,
          {"Volume",  (Fl_Callback *)geometry_elementary_symmetry_cb, (void*)"Volume"} , 
          {0} 
        };  
        Context_Item menu_geometry_elementary_extrude[] = {
          {"0Geometry>Elementary>Extrude", NULL} ,
          {"Translate", (Fl_Callback *)geometry_elementary_extrude_translate_cb, (void*)0} ,
          {"Rotate",    (Fl_Callback *)geometry_elementary_extrude_rotate_cb, (void*)0} ,
          {0} 
        };  
            Context_Item menu_geometry_elementary_extrude_translate[] = {
              {"0Geometry>Elementary>Extrude>Translate", NULL} ,
              {"Point",   (Fl_Callback *)geometry_elementary_extrude_translate_cb, (void*)"Point"} ,
              {"Line",    (Fl_Callback *)geometry_elementary_extrude_translate_cb, (void*)"Line"} ,
              {"Surface", (Fl_Callback *)geometry_elementary_extrude_translate_cb, (void*)"Surface"} ,
              {0} 
            };  
            Context_Item menu_geometry_elementary_extrude_rotate[] = {
              {"0Geometry>Elementary>Extrude>Rotate", NULL} ,
              {"Point",   (Fl_Callback *)geometry_elementary_extrude_rotate_cb, (void*)"Point"} ,
              {"Line",    (Fl_Callback *)geometry_elementary_extrude_rotate_cb, (void*)"Line"} ,
              {"Surface", (Fl_Callback *)geometry_elementary_extrude_rotate_cb, (void*)"Surface"} ,
              {0} 
            };  
    Context_Item menu_geometry_physical[] = {
      {"0Geometry>Physical", NULL} ,
      {"Add",    (Fl_Callback *)geometry_physical_add_cb, (void*)0} ,
      {0} 
    };  
        Context_Item menu_geometry_physical_add[] = {
          {"0Geometry>Physical>Add", NULL} ,
          {"Point",   (Fl_Callback *)geometry_physical_add_cb, (void*)"Point" } ,
          {"Line",    (Fl_Callback *)geometry_physical_add_cb, (void*)"Line" } ,
          {"Surface", (Fl_Callback *)geometry_physical_add_cb, (void*)"Surface" } ,
          {"Volume",  (Fl_Callback *)geometry_physical_add_cb, (void*)"Volume" } ,
          {0} 
        };  

Context_Item menu_mesh[] = {
  {"1Mesh", NULL} ,
  {"Define",       (Fl_Callback *)mesh_define_cb} ,
  {"Inspect",      (Fl_Callback *)mesh_inspect_cb} , 
  {"Delete",       (Fl_Callback *)mesh_delete_cb} , 
  {"1D",           (Fl_Callback *)mesh_1d_cb} ,
  {"2D",           (Fl_Callback *)mesh_2d_cb} , 
  {"3D",           (Fl_Callback *)mesh_3d_cb} , 
  {"First order",  (Fl_Callback *)mesh_degree_cb, (void*)1 } , 
  {"Second order", (Fl_Callback *)mesh_degree_cb, (void*)2 } , 
  {"Optimize",     (Fl_Callback *)mesh_optimize_cb} , 
#if defined(HAVE_NETGEN)
  {"Optimize (Netgen)", (Fl_Callback *)mesh_optimize_netgen_cb} , 
#endif
#if defined(HAVE_FOURIER_MODEL)
  {"Reparameterize",   (Fl_Callback *)mesh_parameterize_cb} , 
#endif
  //{"Reclassify",   (Fl_Callback *)mesh_classify_cb} , 
  {"Save",         (Fl_Callback *)mesh_save_cb} ,
  {0} 
};  
    Context_Item menu_mesh_define[] = {
      {"1Mesh>Define", NULL} ,
      {"Fields",      (Fl_Callback *)view_field_cb, (void*)(-1) },
      {"Characteristic length", (Fl_Callback *)mesh_define_length_cb  } ,
      {"Recombine",   (Fl_Callback *)mesh_define_recombine_cb  } ,
      {"Transfinite", (Fl_Callback *)mesh_define_transfinite_cb  } , 
      {0} 
    };  
        Context_Item menu_mesh_define_transfinite[] = {
          {"1Mesh>Define>Transfinite", NULL} ,
          {"Line",    (Fl_Callback *)mesh_define_transfinite_line_cb} ,
          {"Surface", (Fl_Callback *)mesh_define_transfinite_surface_cb} ,
          {"Volume",  (Fl_Callback *)mesh_define_transfinite_volume_cb} , 
          {0} 
        };  
    Context_Item menu_mesh_delete[] = {
      {"1Mesh>Edit>Delete", NULL} ,
      {"Elements", (Fl_Callback *)mesh_delete_parts_cb, (void*)"elements"} ,
      {"Lines",    (Fl_Callback *)mesh_delete_parts_cb, (void*)"lines"} ,
      {"Surfaces", (Fl_Callback *)mesh_delete_parts_cb, (void*)"surfaces"} ,
      {"Volumes",  (Fl_Callback *)mesh_delete_parts_cb, (void*)"volumes"} ,
      {0} 
    };  

// FIXME: should create MAXSOLVERS items...
Context_Item menu_solver[] = {
  {"2Solver", NULL} ,
  {"Solver 0", (Fl_Callback *)solver_cb , (void*)0} ,
  {"Solver 1", (Fl_Callback *)solver_cb , (void*)1} ,
  {"Solver 2", (Fl_Callback *)solver_cb , (void*)2} ,
  {"Solver 3", (Fl_Callback *)solver_cb , (void*)3} ,
  {"Solver 4", (Fl_Callback *)solver_cb , (void*)4} ,
  {0} 
};

Context_Item menu_post[] = {
  {"3Post-processing", NULL} ,
  {0} 
};

// some other reusable menus

static Fl_Menu_Item menu_point_display[] = {
  {"Color dot",   0, 0, 0},
  {"3D sphere",   0, 0, 0},
  {0}
};

static Fl_Menu_Item menu_point_display_post[] = {
  {"Color dot",      0, 0, 0},
  {"3D sphere",      0, 0, 0},
  {"Scaled dot",     0, 0, 0},
  {"Scaled sphere",  0, 0, 0},
  {0}
};

static Fl_Menu_Item menu_line_display[] = {
  {"Color segment", 0, 0, 0},
  {"3D cylinder",   0, 0, 0},
  {0}
};

static Fl_Menu_Item menu_line_display_post[] = {
  {"Color segment",    0, 0, 0},
  {"3D cylinder",      0, 0, 0},
  {"Tapered cylinder", 0, 0, 0},
  {0}
};

static Fl_Menu_Item menu_surface_display[] = {
  {"Cross",     0, 0, 0},
  {"Wireframe", 0, 0, 0},
  {"Solid",     0, 0, 0},
  {0}
};

static Fl_Menu_Item menu_axes_mode[] = {
  {"None",        0, 0, 0},
  {"Simple axes", 0, 0, 0},
  {"Box",         0, 0, 0},
  {"Full grid",   0, 0, 0},
  {"Open grid",   0, 0, 0},
  {"Ruler",       0, 0, 0},
  {0}
};


#define NUM_FONTS 14

Fl_Menu_Item menu_font_names[] = {
  {"Times-Roman",           0, 0, (void*)FL_TIMES},
  {"Times-Bold",            0, 0, (void*)FL_TIMES_BOLD},
  {"Times-Italic",          0, 0, (void*)FL_TIMES_ITALIC},
  {"Times-BoldItalic",      0, 0, (void*)FL_TIMES_BOLD_ITALIC},
  {"Helvetica",             0, 0, (void*)FL_HELVETICA},
  {"Helvetica-Bold",        0, 0, (void*)FL_HELVETICA_BOLD},
  {"Helvetica-Oblique",     0, 0, (void*)FL_HELVETICA_ITALIC},
  {"Helvetica-BoldOblique", 0, 0, (void*)FL_HELVETICA_BOLD_ITALIC},
  {"Courier",               0, 0, (void*)FL_COURIER},
  {"Courier-Bold",          0, 0, (void*)FL_COURIER_BOLD},
  {"Courier-Oblique",       0, 0, (void*)FL_COURIER_ITALIC},
  {"Courier-BoldOblique",   0, 0, (void*)FL_COURIER_BOLD_ITALIC},
  {"Symbol",                0, 0, (void*)FL_SYMBOL},
  {"ZapfDingbats",          0, 0, (void*)FL_ZAPF_DINGBATS},
  {0}
};

int GetFontIndex(const char *fontname)
{
  if(fontname){
    for(int i = 0; i < NUM_FONTS; i++)
      if(!strcmp(menu_font_names[i].label(), fontname))
        return i;
  }
  Msg::Error("Unknown font \"%s\" (using \"Helvetica\" instead)", fontname);
  Msg::Info("Available fonts:");
  for(int i = 0; i < NUM_FONTS; i++)
    Msg::Info("  \"%s\"", menu_font_names[i].label());
  return 4;
}

int GetFontEnum(int index)
{
  if(index >= 0 && index < NUM_FONTS)
    return (long)menu_font_names[index].user_data();
  return FL_HELVETICA;
}

const char *GetFontName(int index)
{
  if(index >= 0 && index < NUM_FONTS)
    return menu_font_names[index].label();
  return "Helvetica";
}

int GetFontAlign(const char *alignstr)
{
  if(alignstr){
    if(!strcmp(alignstr, "BottomLeft") || !strcmp(alignstr, "Left") ||
       !strcmp(alignstr, "left"))
      return 0;
    else if(!strcmp(alignstr, "BottomCenter") || !strcmp(alignstr, "Center") ||
            !strcmp(alignstr, "center"))
      return 1;
    else if(!strcmp(alignstr, "BottomRight") || !strcmp(alignstr, "Right") ||
            !strcmp(alignstr, "right"))
      return 2;
    else if(!strcmp(alignstr, "TopLeft"))
      return 3;
    else if(!strcmp(alignstr, "TopCenter"))
      return 4;
    else if(!strcmp(alignstr, "TopRight"))
      return 5;
    else if(!strcmp(alignstr, "CenterLeft"))
      return 6;
    else if(!strcmp(alignstr, "CenterCenter"))
      return 7;
    else if(!strcmp(alignstr, "CenterRight"))
      return 8;
  }
  Msg::Error("Unknown font alignment \"%s\" (using \"Left\" instead)", alignstr);
  Msg::Info("Available font alignments:");
  Msg::Info("  \"Left\" (or \"BottomLeft\")");
  Msg::Info("  \"Center\" (or \"BottomCenter\")");
  Msg::Info("  \"Right\" (or \"BottomRight\")");
  Msg::Info("  \"TopLeft\"");
  Msg::Info("  \"TopCenter\"");
  Msg::Info("  \"TopRight\"");
  Msg::Info("  \"CenterLeft\"");
  Msg::Info("  \"CenterCenter\"");
  Msg::Info("  \"CenterRight\"");
  return 0;
}

int GetFontSize()
{
  if(CTX.fontsize > 0){
    return CTX.fontsize;
  }
  else{
    int w = Fl::w();
    if(w <= 1024)      return 11;
    else if(w <= 1280) return 12;
    else if(w <= 1680) return 13;
    else if(w <= 1920) return 14;
    else               return 15;
  }
}

// Definition of global shortcuts

int GUI::global_shortcuts(int event)
{
  // we only handle shortcuts here
  if(event != FL_SHORTCUT)
    return 0;

  if(Fl::test_shortcut('0')) {
    geometry_reload_cb(0, 0);
    mod_geometry_cb(0, 0);
    return 1;
  }
  else if(Fl::test_shortcut('1') || Fl::test_shortcut(FL_F + 1)) {
    mesh_1d_cb(0, 0);
    mod_mesh_cb(0, 0);
    return 1;
  }
  else if(Fl::test_shortcut('2') || Fl::test_shortcut(FL_F + 2)) {
    mesh_2d_cb(0, 0);
    mod_mesh_cb(0, 0);
    return 1;
  }
  else if(Fl::test_shortcut('3') || Fl::test_shortcut(FL_F + 3)) {
    mesh_3d_cb(0, 0);
    mod_mesh_cb(0, 0);
    return 1;
  }
  else if(Fl::test_shortcut(FL_CTRL + 'q') || Fl::test_shortcut(FL_META + 'q')){
    // only necessary when using the system menu bar, but hey, it
    // cannot hurt...
    file_quit_cb(0, 0);
    return 1;
  }
  else if(Fl::test_shortcut('g')) {
    mod_geometry_cb(0, 0);
    return 1;
  }
  else if(Fl::test_shortcut('m')) {
    mod_mesh_cb(0, 0);
    return 1;
  }
  else if(Fl::test_shortcut('s')) {
    mod_solver_cb(0, 0);
    return 1;
  }
  else if(Fl::test_shortcut('p')) {
    mod_post_cb(0, 0);
    return 1;
  }
  else if(Fl::test_shortcut('<')) {
    mod_back_cb(0, 0);
    return 1;
  }
  else if(Fl::test_shortcut('>')) {
    mod_forward_cb(0, 0);
    return 1;
  }
  else if(Fl::test_shortcut('e')) {
    end_selection = 1;
    return 0;   // trick: do as if we didn't use it
  }
  else if(Fl::test_shortcut('u')) {
    undo_selection = 1;
    return 0;   // trick: do as if we didn't use it
  }
  else if(Fl::test_shortcut('i')) {
    invert_selection = 1;
    return 0;   // trick: do as if we didn't use it
  }
  else if(Fl::test_shortcut('q')) {
    quit_selection = 1;
    return 0;   // trick: do as if we didn't use it
  }
  else if(Fl::test_shortcut('-')) {
    invert_selection = 1;
    return 0;   // trick: do as if we didn't use it
  }
  else if(Fl::test_shortcut(FL_Escape) ||
          Fl::test_shortcut(FL_META + FL_Escape) ||
          Fl::test_shortcut(FL_SHIFT + FL_Escape) ||
          Fl::test_shortcut(FL_CTRL + FL_Escape) ||
          Fl::test_shortcut(FL_ALT + FL_Escape)) {
    if(g_opengl_window->LassoMode){
      g_opengl_window->LassoMode = false;
      redraw_opengl();
    }
    else{
      status_xyz1p_cb(0, (void *)"S");
    }
    return 1;
  }
  else if(Fl::test_shortcut(FL_SHIFT + 'a')) { 
    window_cb(0, (void*)"front");
    return 1;
  }
  else if(Fl::test_shortcut(FL_SHIFT + 'o')) {
    general_options_cb(0, 0);
    return 1;
  }
  else if(Fl::test_shortcut(FL_SHIFT + 'g')) {
    geometry_options_cb(0, 0);
    return 1;
  }
  else if(Fl::test_shortcut(FL_SHIFT + 'm')) {
    mesh_options_cb(0, 0);
    return 1;
  }
  else if(Fl::test_shortcut(FL_SHIFT + 's')) {
    solver_options_cb(0, 0);
    return 1;
  }
  else if(Fl::test_shortcut(FL_SHIFT + 'p')) {
    post_options_cb(0, 0);
    return 1;
  }
  else if(Fl::test_shortcut(FL_SHIFT + 'w')) {
    if(PView::list.size()){
      if(view_number >= 0 && view_number < (int)PView::list.size())
        create_view_options_window(view_number);
      else
        create_view_options_window(0);
    }
    return 1;
  }
  else if(Fl::test_shortcut(FL_SHIFT + 'u')) {
    if(PView::list.size()){
      if(view_number >= 0 && view_number < (int)PView::list.size())
        create_plugin_window(view_number);
      else
        create_plugin_window(0);
    }
    return 1;
  }
  else if(Fl::test_shortcut(FL_ALT + 'f')) {
    opt_general_fast_redraw(0, GMSH_SET | GMSH_GUI,
                            !opt_general_fast_redraw(0, GMSH_GET, 0));
    redraw_opengl();
    return 1;
  }
  else if(Fl::test_shortcut(FL_ALT + 'b')) {
    opt_general_draw_bounding_box(0, GMSH_SET | GMSH_GUI,
                                  !opt_general_draw_bounding_box(0, GMSH_GET, 0));
    redraw_opengl();
    return 1;
  }
  else if(Fl::test_shortcut(FL_ALT + 'i')) {
    for(unsigned int i = 0; i < PView::list.size(); i++)
      if(opt_view_visible(i, GMSH_GET, 0))
        opt_view_show_scale(i, GMSH_SET | GMSH_GUI,
                            !opt_view_show_scale(i, GMSH_GET, 0));
    redraw_opengl();
    return 1;
  }
  else if(Fl::test_shortcut(FL_ALT + 'c')) {
    opt_general_color_scheme(0, GMSH_SET | GMSH_GUI,
                             opt_general_color_scheme(0, GMSH_GET, 0) + 1);
    redraw_opengl();
    return 1;
  }
  else if(Fl::test_shortcut(FL_ALT + 'w')) {
    opt_geometry_light(0, GMSH_SET | GMSH_GUI,
                       !opt_geometry_light(0, GMSH_GET, 0));
    opt_mesh_light(0, GMSH_SET | GMSH_GUI,
                   !opt_mesh_light(0, GMSH_GET, 0));
    for(unsigned int i = 0; i < PView::list.size(); i++)
      if(opt_view_visible(i, GMSH_GET, 0))
        opt_view_light(i, GMSH_SET | GMSH_GUI,
                       !opt_view_light(i, GMSH_GET, 0));
    redraw_opengl();
    return 1;
  }
  else if(Fl::test_shortcut(FL_ALT + FL_SHIFT + 'w')) {
    opt_mesh_reverse_all_normals(0, GMSH_SET | GMSH_GUI,
                                 !opt_mesh_reverse_all_normals(0, GMSH_GET, 0));
    redraw_opengl();
    return 1;
  }
  else if(Fl::test_shortcut(FL_ALT + 'x') || 
          Fl::test_shortcut(FL_ALT + FL_SHIFT + 'x')) {
    status_xyz1p_cb(0, (void *)"x");
    return 1;
  }
  else if(Fl::test_shortcut(FL_ALT + 'y') ||
          Fl::test_shortcut(FL_ALT + FL_SHIFT + 'y')) {
    status_xyz1p_cb(0, (void *)"y");
    return 1;
  }
  else if(Fl::test_shortcut(FL_ALT + 'z') ||
          Fl::test_shortcut(FL_ALT + FL_SHIFT + 'z')) {
    status_xyz1p_cb(0, (void *)"z");
    return 1;
  }
  else if(Fl::test_shortcut(FL_ALT + 'o') ||
          Fl::test_shortcut(FL_ALT + FL_SHIFT + 'o')) {
    status_xyz1p_cb(0, (void *)"p");
    return 1;
  }
  else if(Fl::test_shortcut(FL_ALT + 'a')) {
    opt_general_axes(0, GMSH_SET | GMSH_GUI, 
                     opt_general_axes(0, GMSH_GET, 0) + 1);
    for(unsigned int i = 0; i < PView::list.size(); i++)
      if(opt_view_visible(i, GMSH_GET, 0))
        opt_view_axes(i, GMSH_SET | GMSH_GUI, opt_view_axes(i, GMSH_GET, 0)+1);
    redraw_opengl();
    return 1;
  }
  else if(Fl::test_shortcut(FL_ALT + FL_SHIFT + 'a')) {
    opt_general_small_axes(0, GMSH_SET | GMSH_GUI,
                           !opt_general_small_axes(0, GMSH_GET, 0));
    redraw_opengl();
    return 1;
  }
  else if(Fl::test_shortcut(FL_ALT + 'p')) {
    opt_geometry_points(0, GMSH_SET | GMSH_GUI,
                        !opt_geometry_points(0, GMSH_GET, 0));
    redraw_opengl();
    return 1;
  }
  else if(Fl::test_shortcut(FL_ALT + 'l')) {
    opt_geometry_lines(0, GMSH_SET | GMSH_GUI,
                       !opt_geometry_lines(0, GMSH_GET, 0));
    redraw_opengl();
    return 1;
  }
  else if(Fl::test_shortcut(FL_ALT + 's')) {
    opt_geometry_surfaces(0, GMSH_SET | GMSH_GUI,
                          !opt_geometry_surfaces(0, GMSH_GET, 0));
    redraw_opengl();
    return 1;
  }
  else if(Fl::test_shortcut(FL_ALT + 'v')) {
    opt_geometry_volumes(0, GMSH_SET | GMSH_GUI,
                         !opt_geometry_volumes(0, GMSH_GET, 0));
    redraw_opengl();
    return 1;
  }
  else if(Fl::test_shortcut(FL_ALT + FL_SHIFT + 'p')) {
    opt_mesh_points(0, GMSH_SET | GMSH_GUI, !opt_mesh_points(0, GMSH_GET, 0));
    redraw_opengl();
    return 1;
  }
  else if(Fl::test_shortcut(FL_ALT + FL_SHIFT + 'l')) {
    opt_mesh_lines(0, GMSH_SET | GMSH_GUI, 
                   !opt_mesh_lines(0, GMSH_GET, 0));
    redraw_opengl();
    return 1;
  }
  else if(Fl::test_shortcut(FL_ALT + FL_SHIFT + 's')) {
    opt_mesh_surfaces_edges(0, GMSH_SET | GMSH_GUI,
                            !opt_mesh_surfaces_edges(0, GMSH_GET, 0));
    redraw_opengl();
    return 1;
  }
  else if(Fl::test_shortcut(FL_ALT + FL_SHIFT + 'v')) {
    opt_mesh_volumes_edges(0, GMSH_SET | GMSH_GUI,
                           !opt_mesh_volumes_edges(0, GMSH_GET, 0));
    redraw_opengl();
    return 1;
  }
  else if(Fl::test_shortcut(FL_ALT + 'd')){
    opt_geometry_surface_type(0, GMSH_SET | GMSH_GUI,
                              opt_geometry_surface_type(0, GMSH_GET, 0) + 1);
    redraw_opengl();
    return 1;
  }
  else if(Fl::test_shortcut(FL_ALT + FL_SHIFT + 'd')) {
    opt_mesh_surfaces_faces(0, GMSH_SET | GMSH_GUI,
                            !opt_mesh_surfaces_faces(0, GMSH_GET, 0));
    redraw_opengl();
    return 1;
  }
  else if(Fl::test_shortcut(FL_ALT + FL_SHIFT + 'b')) {
    opt_mesh_volumes_faces(0, GMSH_SET | GMSH_GUI,
                           !opt_mesh_volumes_faces(0, GMSH_GET, 0));
    redraw_opengl();
    return 1;
  }
  else if(Fl::test_shortcut(FL_ALT + 'm')) {
    int old = opt_mesh_points(0, GMSH_GET, 0) || 
      opt_mesh_lines(0, GMSH_GET, 0) ||
      opt_mesh_surfaces_edges(0, GMSH_GET, 0) ||
      opt_mesh_surfaces_faces(0, GMSH_GET, 0);
    opt_mesh_points(0, GMSH_SET | GMSH_GUI, !old);
    opt_mesh_lines(0, GMSH_SET | GMSH_GUI, !old);
    opt_mesh_surfaces_edges(0, GMSH_SET | GMSH_GUI, !old);
    opt_mesh_surfaces_faces(0, GMSH_SET | GMSH_GUI, !old);
    opt_mesh_volumes_edges(0, GMSH_SET | GMSH_GUI, !old);
    opt_mesh_volumes_faces(0, GMSH_SET | GMSH_GUI, !old);
    redraw_opengl();
    return 1;
  }
  else if(Fl::test_shortcut(FL_ALT + 't')) {
    for(unsigned int i = 0; i < PView::list.size(); i++)
      if(opt_view_visible(i, GMSH_GET, 0)){
	double t = opt_view_intervals_type(i, GMSH_GET, 0) + 1;
	if(t == 4) t++; // skip numeric
        opt_view_intervals_type(i, GMSH_SET | GMSH_GUI, t);
      }
    redraw_opengl();
    return 1;
  }
  else if(Fl::test_shortcut(FL_ALT + 'r')) {
    for(unsigned int i = 0; i < PView::list.size(); i++)
      if(opt_view_visible(i, GMSH_GET, 0))
        opt_view_range_type(i, GMSH_SET | GMSH_GUI,
                            opt_view_range_type(i, GMSH_GET, 0) + 1);
    redraw_opengl();
    return 1;
  }
  else if(Fl::test_shortcut(FL_ALT + 'n')) {
    for(unsigned int i = 0; i < PView::list.size(); i++)
      if(opt_view_visible(i, GMSH_GET, 0))
        opt_view_draw_strings(i, GMSH_SET | GMSH_GUI,
                              !opt_view_draw_strings(i, GMSH_GET, 0));
    redraw_opengl();
    return 1;
  }
  else if(Fl::test_shortcut(FL_ALT + 'e')) {
    for(unsigned int i = 0; i < PView::list.size(); i++)
      if(opt_view_visible(i, GMSH_GET, 0))
        opt_view_show_element(i, GMSH_SET | GMSH_GUI,
                              !opt_view_show_element(i, GMSH_GET, 0));
    redraw_opengl();
    return 1;
  }
  else if(Fl::test_shortcut(FL_ALT + 'h')) {
    static int show = 0;
    for(unsigned int i = 0; i < PView::list.size(); i++)
      opt_view_visible(i, GMSH_SET | GMSH_GUI, show);
    redraw_opengl();
    show = !show;
    return 1;
  }
  else if(arrow_shortcuts()) {
    return 1;
  }

  return 0;
}

// Test the arrow shortcuts (this is not in the global_shortcuts)
// since it is used elsewhere (where we want to override widget
// navigation: necessary since FLTK>=1.1)

int GUI::arrow_shortcuts()
{
  if(Fl::test_shortcut(FL_Left)) {
    ManualPlay(1, -1);
    return 1;
  }
  else if(Fl::test_shortcut(FL_Right)) {
    ManualPlay(1, 1);
    return 1;
  }
  else if(Fl::test_shortcut(FL_Up)) {
    ManualPlay(0, -1);
    return 1;
  }
  else if(Fl::test_shortcut(FL_Down)) {
    ManualPlay(0, 1);
    return 1;
  }
  return 0;
}

// The GUI constructor

GUI::GUI(int argc, char **argv)
{
  // initialize static windows
  g_window = NULL;
  m_window = NULL;
  opt_window = NULL;
  plugin_window = NULL;
  field_window = NULL;
  stat_window = NULL;
  msg_window = NULL;
  vis_window = NULL;
  clip_window = NULL;
  manip_window = NULL;
  about_window = NULL;
  context_geometry_window = NULL;
  context_mesh_window = NULL;

  // initialize on-screen message buffer
  onscreen_buffer[0][0] = '\0';
  onscreen_buffer[1][0] = '\0';

  // initialize selection bits
  selection = ENT_NONE;
  try_selection = quit_selection = end_selection = 0;
  undo_selection = invert_selection = 0;
  for(int i = 0; i < 4; i++) try_selection_xywh[i] = 0;

  // set X display
  if(strlen(CTX.display))
    Fl::display(CTX.display);

  // add global shortcuts
  Fl::add_handler(SetGlobalShortcut);

  // store fontsize now: we don't want any subsequent change
  // (e.g. when doing a 'restore options') to be taken into account
  // in the dynamic GUI features (set_context, plugin, etc.)
  fontsize = GetFontSize();

  // set default font size
  FL_NORMAL_SIZE = fontsize;

  // handle themes and tooltip font size
  if(strlen(CTX.gui_theme))
    Fl::scheme(CTX.gui_theme);
  Fl_Tooltip::size(fontsize);

  // register image formats not in core fltk library (jpeg/png)
  fl_register_images();

  // load default system icons (for file browser)
  Fl_File_Icon::load_system_icons();
  
  // add callback to respond to the Mac Finder (when you click on a
  // document)
#if defined(__APPLE__) && defined(HAVE_FLTK_1_1_5_OR_ABOVE)
  fl_open_callback(OpenProjectMacFinder);
#endif

  // All static windows are contructed (even if some are not
  // displayed) since the shortcuts should be valid even for hidden
  // windows, and we don't want to test for widget existence every time

  create_menu_window();
  create_graphic_window();

#if defined(WIN32)
  g_window->icon((char *)LoadImage(fl_display, MAKEINTRESOURCE(IDI_ICON),
                                   IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR));
#elif defined(__APPLE__)
  // Nothing to do here
#else
  fl_open_display();
  static char gmsh32x32[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x01, 0x00, 0x00, 0x40, 0x03, 0x00,
    0x00, 0x40, 0x03, 0x00, 0x00, 0x20, 0x07, 0x00, 0x00, 0x20, 0x07, 0x00,
    0x00, 0x10, 0x0f, 0x00, 0x00, 0x10, 0x0f, 0x00, 0x00, 0x08, 0x1f, 0x00,
    0x00, 0x08, 0x1f, 0x00, 0x00, 0x04, 0x3f, 0x00, 0x00, 0x04, 0x3f, 0x00,
    0x00, 0x02, 0x7f, 0x00, 0x00, 0x02, 0x7f, 0x00, 0x00, 0x01, 0xff, 0x00,
    0x00, 0x01, 0xff, 0x00, 0x80, 0x00, 0xff, 0x01, 0x80, 0x00, 0xff, 0x01,
    0x40, 0x00, 0xff, 0x03, 0x40, 0x00, 0xff, 0x03, 0x20, 0x00, 0xff, 0x07,
    0x20, 0x00, 0xff, 0x07, 0x10, 0x00, 0xff, 0x0f, 0x10, 0x00, 0xff, 0x0f,
    0x08, 0x00, 0xff, 0x1f, 0x08, 0x00, 0xff, 0x1f, 0x04, 0x40, 0xfd, 0x3f,
    0x04, 0xa8, 0xea, 0x3f, 0x02, 0x55, 0x55, 0x7f, 0xa2, 0xaa, 0xaa, 0x7a,
    0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00 };
  g_window->icon((char*)XCreateBitmapFromData(fl_display, DefaultRootWindow(fl_display),
                                              gmsh32x32, 32, 32));
  m_window->icon((char*)XCreateBitmapFromData(fl_display, DefaultRootWindow(fl_display),
                                              gmsh32x32, 32, 32));
#endif

  // open graphics window first for correct non-modal behaviour on windows
  g_window->show(1, argv);
  m_window->show();

  // graphic window should have the initial focus (so we can
  // e.g. directly loop through time steps with the keyboard)
  g_opengl_window->take_focus();
  
  create_option_window();
  create_plugin_window(0);
  create_field_window(0);
  create_message_window();
  create_statistics_window();
  create_visibility_window();
  create_clip_window();
  create_manip_window();
  create_about_window();
  create_geometry_context_window(0);
  create_mesh_context_window(0);
  for(int i = 0; i < MAXSOLVERS; i++) {
    solver[i].window = NULL;
    create_solver_window(i);
  }
  call_for_solver_plugin(-1);

  // Draw the scene
  g_opengl_window->redraw();
}

// Run the GUI until no window is left

int GUI::run()
{
  return Fl::run();
}

// Check (now) if any pending events and run them

void GUI::check()
{
  Fl::check();
}

// Wait for any events and run them

void GUI::wait()
{
  Fl::wait();
}

void GUI::wait(double time)
{
  Fl::wait(time);
}

// Create the menu window

void GUI::create_menu_window()
{
  int y;

  if(m_window) {
    m_window->show();
    return;
  }

  int width = 14 * fontsize;

  // this is the initial height: no dynamic button is shown!
#if defined(__APPLE__) && defined(HAVE_FLTK_1_1_5_OR_ABOVE)
  if(CTX.system_menu_bar){
    MH = BH + 6;  // the menu bar is not in the application!
  }
  else{
#endif
    MH = BH + BH + 6;
#if defined(__APPLE__) && defined(HAVE_FLTK_1_1_5_OR_ABOVE)
  }
#endif

  m_window = new Main_Window(width, MH + NB_BUTT_SCROLL * BH, CTX.non_modal_windows, "Gmsh");
  m_window->box(GMSH_WINDOW_BOX);
  m_window->callback(file_quit_cb);

#if defined(__APPLE__) && defined(HAVE_FLTK_1_1_5_OR_ABOVE)
  if(CTX.system_menu_bar){
    // the system menubar is kind of a hack in fltk < 1.1.7: it still
    // creates a real (invisible) menubar. To avoid spurious mouse
    // click events we make it a 1x1 pixel rectangle, 1 pixel off the
    // edge (so it falls behind the navigation buttons)
    m_sys_menu_bar = new Fl_Sys_Menu_Bar(1, 1, 1, 1);
    m_sys_menu_bar->menu(m_sys_menubar_table);
    m_sys_menu_bar->global();
    Fl_Box *o = new Fl_Box(0, 0, width, BH + 6);
    o->box(FL_UP_BOX);
    y = 3;
  }
  else{
#endif
    m_menu_bar = new Fl_Menu_Bar(0, 0, width, BH);
    m_menu_bar->menu(m_menubar_table);
    m_menu_bar->box(FL_UP_BOX);
    m_menu_bar->global();
    Fl_Box *o = new Fl_Box(0, BH, width, BH + 6);
    o->box(FL_UP_BOX);
    y = BH + 3;
#if defined(__APPLE__) && defined(HAVE_FLTK_1_1_5_OR_ABOVE)
  }
#endif

  m_navig_butt[0] = new Fl_Button(1, y, 18, BH / 2, "@#-1<");
  m_navig_butt[0]->labeltype(FL_SYMBOL_LABEL);
  m_navig_butt[0]->box(FL_FLAT_BOX);
  m_navig_butt[0]->selection_color(FL_WHITE);
  m_navig_butt[0]->callback(mod_back_cb);
  m_navig_butt[0]->tooltip("Go back one in the menu history (<)");

  m_navig_butt[1] = new Fl_Button(1, y + BH / 2, 18, BH / 2, "@#-1>");
  m_navig_butt[1]->labeltype(FL_SYMBOL_LABEL);
  m_navig_butt[1]->box(FL_FLAT_BOX);
  m_navig_butt[1]->selection_color(FL_WHITE);
  m_navig_butt[1]->callback(mod_forward_cb);
  m_navig_butt[1]->tooltip("Go forward one in the menu history (>)");

  m_module_butt = new Fl_Choice(19, y, width - 24, BH);
  m_module_butt->menu(m_module_table);
  m_module_butt->box(FL_THIN_DOWN_BOX);
  // force the executation of the callback even if we didn't change
  // the selection (we want to go back to the top-level menu every
  // time we select one of the categories, even if the category is not
  // changed):
  m_module_butt->when(FL_WHEN_RELEASE_ALWAYS);

  // create an empty scroll area that will get populated dynamically
  // in set_context()
  m_scroll = new Fl_Scroll(0, MH, width, NB_BUTT_SCROLL * BH); 
  m_scroll->type(Fl_Scroll::VERTICAL);
  m_scroll->end();

  m_window->size(width, MH);
  m_window->position(CTX.position[0], CTX.position[1]);
  
  m_window->end();
}

// Dynamically set the context

void GUI::set_context(Context_Item * menu_asked, int flag)
{
  static int nb_back = 0, nb_forward = 0, init_context = 0;
  static Context_Item *menu_history[NB_HISTORY_MAX];
  Context_Item *menu;

  if(!init_context) {
    init_context = 1;
    for(int i = 0; i < NB_HISTORY_MAX; i++) {
      menu_history[i] = NULL;
    }
  }

  if(nb_back > NB_HISTORY_MAX - 2)
    nb_back = 1; // we should do a circular list

  if(flag == -1) {
    if(nb_back > 1) {
      nb_back--;
      nb_forward++;
      menu = menu_history[nb_back - 1];
    }
    else
      return;
  }
  else if(flag == 1) {
    if(nb_forward > 0) {
      nb_back++;
      nb_forward--;
      menu = menu_history[nb_back - 1];
    }
    else
      return;
  }
  else {
    menu = menu_asked;
    if(!nb_back || menu_history[nb_back - 1] != menu) {
      menu_history[nb_back++] = menu;
    }
    nb_forward = 0;
  }

  if(menu[0].label[0] == '0'){
    m_module_butt->value(0);
  }
  else if(menu[0].label[0] == '1'){
    m_module_butt->value(1);
  }
  else if(menu[0].label[0] == '2'){
    m_module_butt->value(2);
    menu[1].label = opt_solver_name0(0, GMSH_GET, 0);
    menu[2].label = opt_solver_name1(0, GMSH_GET, 0);
    menu[3].label = opt_solver_name2(0, GMSH_GET, 0);
    menu[4].label = opt_solver_name3(0, GMSH_GET, 0);
    menu[5].label = opt_solver_name4(0, GMSH_GET, 0);
    for(int i = 0; i < MAXSOLVERS; i++) {
      if(!strlen(menu[i + 1].label))
        menu[i + 1].label = NULL;
    }
  }
  else if(menu[0].label[0] == '3'){
    m_module_butt->value(3);
  }
  else {
    Msg::Warning("Something is wrong in your dynamic context definition");
    return;
  }

  Msg::StatusBar(1, false, menu[0].label + 1);

  // Remove all the children (m_push*, m_toggle*, m_pop*). FLTK <=
  // 1.1.4 should be OK with this. FLTK 1.1.5 may crash as it may
  // access a widget's data after its callback is executed (we call
  // set_context in the button callbacks!). FLTK 1.1.6 introduced a
  // fix (Fl::delete_widget) to delay the deletion until the next
  // Fl::wait call. In any case, we cannot use m_scroll->clear()
  // (broken in < 1.1.5, potential crasher in >= 1.1.5).
  for(unsigned int i = 0; i < m_push_butt.size(); i++){
    m_scroll->remove(m_push_butt[i]);
#if defined(HAVE_FLTK_1_1_6_OR_ABOVE)
    Fl::delete_widget(m_push_butt[i]);
#else
    delete m_push_butt[i];
#endif
  }
  for(unsigned int i = 0; i < m_toggle_butt.size(); i++){
    m_scroll->remove(m_toggle_butt[i]);
#if defined(HAVE_FLTK_1_1_6_OR_ABOVE)
    Fl::delete_widget(m_toggle_butt[i]);
#else
    delete m_toggle_butt[i];
#endif
  }
  for(unsigned int i = 0; i < m_toggle2_butt.size(); i++){
    m_scroll->remove(m_toggle2_butt[i]);
#if defined(HAVE_FLTK_1_1_6_OR_ABOVE)
    Fl::delete_widget(m_toggle2_butt[i]);
#else
    delete m_toggle2_butt[i];
#endif
  }
  for(unsigned int i = 0; i < m_popup_butt.size(); i++){
    m_scroll->remove(m_popup_butt[i]);
#if defined(HAVE_FLTK_1_1_6_OR_ABOVE)
    Fl::delete_widget(m_popup_butt[i]);
#else
    delete m_popup_butt[i];
#endif
  }
  for(unsigned int i = 0; i < m_popup2_butt.size(); i++){
    m_scroll->remove(m_popup2_butt[i]);
#if defined(HAVE_FLTK_1_1_6_OR_ABOVE)
    Fl::delete_widget(m_popup2_butt[i]);
#else
    delete m_popup2_butt[i];
#endif
  }

  // reset the vectors
  m_push_butt.clear();
  m_toggle_butt.clear();
  m_toggle2_butt.clear();
  m_popup_butt.clear();
  m_popup2_butt.clear();
  for(unsigned int i = 0; i < m_pop_label.size(); i++)
    delete [] m_pop_label[i];
  m_pop_label.clear();

  int width = m_window->w();
  int popw = 4 * fontsize + 3;

  // construct the dynamic menu
  int nb = 0;
  if(m_module_butt->value() == 3){ // post-processing context
    for(nb = 0; nb < (int)PView::list.size(); nb++) {
      PViewData *data = PView::list[nb]->getData();
      PViewOptions *opt = PView::list[nb]->getOptions();
      
      Fl_Light_Button *b1 = new Fl_Light_Button(0, MH + nb * BH, width - popw, BH);
      b1->callback(view_toggle_cb, (void *)nb);
      b1->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE | FL_ALIGN_CLIP);
      b1->value(opt->Visible);
      b1->label(data->getName().c_str());
      b1->tooltip(data->getFileName().c_str());
      
      char *tmp = new char[32];
      sprintf(tmp, "[%d]@#-1>", nb);
      Fl_Button *b2 = new Fl_Button(width - popw, MH + nb * BH, popw, BH, tmp);
      m_pop_label.push_back(tmp);
      b2->align(FL_ALIGN_RIGHT | FL_ALIGN_INSIDE | FL_ALIGN_CLIP);
      b2->tooltip("Show view option menu (Shift+w)");
  
      Popup_Button *p[2];
      p[0] = new Popup_Button(width - popw, MH + nb * BH, popw, BH);
      p[0]->type(Fl_Menu_Button::POPUP123);
      p[1] = new Popup_Button(0, MH + nb * BH, width - popw, BH);
      p[1]->type(Fl_Menu_Button::POPUP3);
  
      for(int j = 0; j < 2; j++) {
        p[j]->add("Reload/View", 'r', 
                  (Fl_Callback *) view_reload_cb, (void *)nb, 0);
        p[j]->add("Reload/Visible Views", 0, 
                  (Fl_Callback *) view_reload_visible_cb, (void *)nb, 0);
        p[j]->add("Reload/All Views", 0, 
                  (Fl_Callback *) view_reload_all_cb, (void *)nb, 0);
        p[j]->add("Remove/View", FL_Delete, 
                  (Fl_Callback *) view_remove_cb, (void *)nb, 0);
        p[j]->add("Remove/Other Views", 0, 
                  (Fl_Callback *) view_remove_other_cb, (void *)nb, 0);
        p[j]->add("Remove/Visible Views", 0, 
                  (Fl_Callback *) view_remove_visible_cb, (void *)nb, 0);
        p[j]->add("Remove/Invisible Views", 0, 
                  (Fl_Callback *) view_remove_invisible_cb, (void *)nb, 0);
        p[j]->add("Remove/Empty Views", 0, 
                  (Fl_Callback *) view_remove_empty_cb, (void *)nb, 0);
        p[j]->add("Remove/All Views", 0, 
                  (Fl_Callback *) view_remove_all_cb, (void *)nb, 0);
        p[j]->add("Alias/View without Options", 0, 
                  (Fl_Callback *) view_alias_cb, (void *)nb, 0);
        p[j]->add("Alias/View with Options", 0, 
                  (Fl_Callback *) view_alias_with_options_cb, (void *)nb, 0);
        p[j]->add("Combine Elements/From Visible Views", 0, 
                  (Fl_Callback *) view_combine_space_visible_cb, (void *)nb, 0);
        p[j]->add("Combine Elements/From All Views", 0, 
                  (Fl_Callback *) view_combine_space_all_cb, (void *)nb, 0);
        p[j]->add("Combine Elements/By View Name", 0, 
                  (Fl_Callback *) view_combine_space_by_name_cb, (void *)nb, 0);
        p[j]->add("Combine Time Steps/From Visible Views", 0, 
                  (Fl_Callback *) view_combine_time_visible_cb, (void *)nb, 0);
        p[j]->add("Combine Time Steps/From All Views", 0, 
                  (Fl_Callback *) view_combine_time_all_cb, (void *)nb, 0);
        p[j]->add("Combine Time Steps/By View Name", 0, 
                 (Fl_Callback *) view_combine_time_by_name_cb, (void *)nb, 0);
        p[j]->add("Set Visibility/All On", 0, 
                  (Fl_Callback *) view_all_visible_cb, (void *)1, 0);
        p[j]->add("Set Visibility/All Off", 0, 
                  (Fl_Callback *) view_all_visible_cb, (void *)0, 0);
        p[j]->add("Set Visibility/Invert", 0, 
                  (Fl_Callback *) view_all_visible_cb, (void *)-1, 0);
        p[j]->add("Save As/Parsed View...", 0, 
                  (Fl_Callback *) view_save_parsed_cb, (void *)nb, 0);
        p[j]->add("Save As/ASCII View...", 0, 
                  (Fl_Callback *) view_save_ascii_cb, (void *)nb, 0);
        p[j]->add("Save As/Binary View...", 0, 
                  (Fl_Callback *) view_save_binary_cb, (void *)nb, 0);
        p[j]->add("Save As/STL Triangulation...", 0, 
                  (Fl_Callback *) view_save_stl_cb, (void *)nb, 0);
        p[j]->add("Save As/Raw Text...", 0, 
                  (Fl_Callback *) view_save_txt_cb, (void *)nb, 0);
        p[j]->add("Save As/Gmsh Mesh...", 0, 
                  (Fl_Callback *) view_save_msh_cb, (void *)nb, 0);
#if defined(HAVE_MED)
        p[j]->add("Save As/MED file...", 0, 
                  (Fl_Callback *) view_save_med_cb, (void *)nb, 0);
#endif
        p[j]->add("Apply As Background Mesh", 0, 
                  (Fl_Callback *) view_applybgmesh_cb, (void *)nb, FL_MENU_DIVIDER);
        p[j]->add("Options...", 'o', 
                  (Fl_Callback *) view_options_cb, (void *)nb, 0);
        p[j]->add("Plugins...", 'p', 
                  (Fl_Callback *) view_plugin_cb, (void *)nb, 0);
      }

      m_toggle_butt.push_back(b1);
      m_toggle2_butt.push_back(b2);
      m_popup_butt.push_back(p[0]);
      m_popup2_butt.push_back(p[1]);
      m_scroll->add(b1);
      m_scroll->add(b2);
      m_scroll->add(p[0]);
      m_scroll->add(p[1]);
    }
  }
  else{ // geometry, mesh and solver contexts
    while(menu[nb + 1].label) {
      Fl_Button *b = new Fl_Button(0, MH + nb * BH, width, BH);
      b->label(menu[nb + 1].label);
      b->callback(menu[nb + 1].callback, menu[nb + 1].arg);
      m_push_butt.push_back(b);
      m_scroll->add(b);
      nb++;
    }
  }

  m_scroll->redraw();

  if(nb <= NB_BUTT_SCROLL)
    m_window->size(width, MH + nb * BH);
  else
    m_window->size(width, MH + NB_BUTT_SCROLL * BH);
}

int GUI::get_context()
{
  return m_module_butt->value();
}

// Create the graphic window

#define vv(x,y) fl_vertex(x,y)
#define bl fl_begin_loop()
#define el fl_end_loop()

void gmsh_play(Fl_Color c)
{
  fl_color(c);
  bl; vv(-0.3,0.8); vv(0.5,0.0); vv(-0.3,-0.8); el;
}

void gmsh_pause(Fl_Color c)
{
  fl_color(c);
  bl; vv(-0.8,-0.8); vv(-0.3,-0.8); vv(-0.3,0.8); vv(-0.8,0.8); el;
  bl; vv(0.0,-0.8); vv(0.5,-0.8); vv(0.5,0.8); vv(0.0,0.8); el;
}

void gmsh_rewind(Fl_Color c)
{
  fl_color(c);
  bl; vv(-0.8,-0.8); vv(-0.3,-0.8); vv(-0.3,0.8); vv(-0.8,0.8); el;
  bl; vv(-0.3,0.0); vv(0.5,-0.8); vv(0.5,0.8); el;
}

void gmsh_forward(Fl_Color c)
{
  fl_color(c);
  bl; vv(0.0,0.8); vv(0.8,0.0); vv(0.0,-0.8); el;
  bl; vv(-0.8,0.8); vv(-0.3,0.8); vv(-0.3,-0.8); vv(-0.8,-0.8); el;
}

void gmsh_back(Fl_Color c)
{
  fl_rotate(180);  
  gmsh_forward(c);
}

void gmsh_ortho(Fl_Color c)
{
  fl_color(c);
  bl; vv(-0.8,0.8); vv(0.3,0.8); vv(0.3,-0.3); vv(-0.8,-0.3); el;
  bl; vv(-0.3,0.3); vv(0.8,0.3); vv(0.8,-0.8); vv(-0.3,-0.8); el;
  fl_begin_line(); vv(-0.8,0.8); vv(-0.3,0.3); fl_end_line();
  fl_begin_line(); vv(0.3,0.8); vv(0.8,0.3); fl_end_line();
  fl_begin_line(); vv(0.3,-0.3); vv(0.8,-0.8); fl_end_line();
  fl_begin_line(); vv(-0.8,-0.3); vv(-0.3,-0.8); fl_end_line();
}

void gmsh_rotate(Fl_Color c)
{
  fl_color(c);
  fl_begin_line(); fl_arc(0.0, -0.1, 0.7, 0.0, 270.0); fl_end_line();
  fl_begin_polygon(); vv(0.5,0.6); vv(-0.1,0.9); vv(-0.1,0.3); fl_end_polygon();
}

void gmsh_models(Fl_Color c)
{
  fl_color(c);
  bl; vv(-0.8,-0.8); vv(-0.3,-0.8); vv(-0.3,-0.3); vv(-0.8,-0.3); el;
  bl; vv(0.3,-0.8); vv(0.8,-0.8); vv(0.8,-0.3); vv(0.3,-0.3); el;
  bl; vv(-0.8,0.3); vv(-0.3,0.3); vv(-0.3,0.8); vv(-0.8,0.8); el;
  bl; vv(0.3,0.3); vv(0.8,0.3); vv(0.8,0.8); vv(0.3,0.8); el;
}

#undef vv
#undef bl
#undef el

void GUI::create_graphic_window()
{
  if(g_window) {
    g_window->show();
    return;
  }

  fl_add_symbol("gmsh_rewind", gmsh_rewind, 1);
  fl_add_symbol("gmsh_back", gmsh_back, 1);
  fl_add_symbol("gmsh_play", gmsh_play, 1);
  fl_add_symbol("gmsh_pause", gmsh_pause, 1);
  fl_add_symbol("gmsh_forward", gmsh_forward, 1);
  fl_add_symbol("gmsh_ortho", gmsh_ortho, 1);
  fl_add_symbol("gmsh_rotate", gmsh_rotate, 1);
  fl_add_symbol("gmsh_models", gmsh_models, 1);

  int sh = 2 * fontsize - 4;    // status bar height
  int sw = fontsize + 3;        // status button width
  int width = CTX.viewport[2] - CTX.viewport[0];
  int glheight = CTX.viewport[3] - CTX.viewport[1];
  int height = glheight + sh;

  // the graphic window is the main window: it should be a normal
  // window (neither modal nor non-modal)
  g_window = new Main_Window(width, height, false);
  g_window->callback(file_quit_cb);

  // bottom button bar

  Fl_Box *bottom = new Fl_Box(0, glheight, width, sh);
  bottom->box(FL_FLAT_BOX);

  int x = 2;
  int sht = sh - 4; // leave a 2 pixel border at the bottom

  g_status_butt[5] = new Fl_Button(x, glheight + 2, sw, sht, "@-1gmsh_models");
  x += sw;
  g_status_butt[5]->callback(status_xyz1p_cb, (void *)"model");
  g_status_butt[5]->tooltip("Switch current model");

  g_status_butt[0] = new Fl_Button(x, glheight + 2, sw, sht, "X");
  x += sw;
  g_status_butt[0]->callback(status_xyz1p_cb, (void *)"x");
  g_status_butt[0]->tooltip("Set +X or -X view (Alt+x or Alt+Shift+x)");

  g_status_butt[1] = new Fl_Button(x, glheight + 2, sw, sht, "Y");
  x += sw;
  g_status_butt[1]->callback(status_xyz1p_cb, (void *)"y");
  g_status_butt[1]->tooltip("Set +Y or -Y view (Alt+y or Alt+Shift+y)");

  g_status_butt[2] = new Fl_Button(x, glheight + 2, sw, sht, "Z");
  x += sw;
  g_status_butt[2]->callback(status_xyz1p_cb, (void *)"z");
  g_status_butt[2]->tooltip("Set +Z or -Z view (Alt+z or Alt+Shift+z)");

  g_status_butt[4] = new Fl_Button(x, glheight + 2, sw, sht, "@-1gmsh_rotate");
  x += sw;
  g_status_butt[4]->callback(status_xyz1p_cb, (void *)"r");
  g_status_butt[4]->tooltip("Rotate +90 or -90 degrees");

  g_status_butt[3] = new Fl_Button(x, glheight + 2, 2 * fontsize, sht, "1:1");
  x += 2 * fontsize;
  g_status_butt[3]->callback(status_xyz1p_cb, (void *)"1:1");
  g_status_butt[3]->tooltip("Set unit scale");

  g_status_butt[8] = new Fl_Button(x, glheight + 2, sw, sht, "@-1gmsh_ortho");
  x += sw;
  g_status_butt[8]->callback(status_xyz1p_cb, (void *)"p");
  g_status_butt[8]->tooltip("Toggle projection mode (Alt+o or Alt+Shift+o)");

  g_status_butt[9] = new Fl_Button(x, glheight + 2, sw, sht, "S");
  x += sw;
  g_status_butt[9]->callback(status_xyz1p_cb, (void *)"S");
  g_status_butt[9]->tooltip("Toggle mouse selection ON/OFF (Escape)");

  g_status_butt[6] = new Fl_Button(x, glheight + 2, sw, sht, "@-1gmsh_rewind");
  x += sw;
  g_status_butt[6]->callback(status_rewind_cb);
  g_status_butt[6]->tooltip("Rewind animation");
  g_status_butt[6]->deactivate();

  g_status_butt[10] = new Fl_Button(x, glheight + 2, sw, sht, "@-1gmsh_back");
  x += sw;
  g_status_butt[10]->callback(status_stepbackward_cb);
  g_status_butt[10]->tooltip("Step backward");
  g_status_butt[10]->deactivate();

  g_status_butt[7] = new Fl_Button(x, glheight + 2, sw, sht, "@-1gmsh_play");
  x += sw;
  g_status_butt[7]->callback(status_play_cb);
  g_status_butt[7]->tooltip("Play/pause animation");
  g_status_butt[7]->deactivate();

  g_status_butt[11] = new Fl_Button(x, glheight + 2, sw, sht, "@-1gmsh_forward");
  x += sw;
  g_status_butt[11]->callback(status_stepforward_cb);
  g_status_butt[11]->tooltip("Step forward");
  g_status_butt[11]->deactivate();

  for(int i = 0; i < 12; i++) {
    g_status_butt[i]->box(FL_FLAT_BOX);
    g_status_butt[i]->selection_color(FL_WHITE);
    g_status_butt[i]->align(FL_ALIGN_CENTER | FL_ALIGN_INSIDE | FL_ALIGN_CLIP);
  }

  x += 2;
  int wleft = (width - x) / 3 - 1;
  int wright = (width - x) - (width - x) / 3 - 1;

  g_status_label[0] = new Fl_Box(x, glheight + 2, wleft, sht);
  g_status_label[1] = new Fl_Box(x + (width - x) / 3, glheight + 2, wright, sht);
  for(int i = 0; i < 2; i++) {
    g_status_label[i]->box(FL_THIN_DOWN_BOX);
    g_status_label[i]->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE | FL_ALIGN_CLIP);
  }

  // dummy resizable box
  Dummy_Box *resize_box = new Dummy_Box(x, 0, width - x, glheight);
  g_window->resizable(resize_box);

  // opengl window
  g_opengl_window = new Opengl_Window(0, 0, width, glheight);
  int mode = FL_RGB | FL_DEPTH | (CTX.db ? FL_DOUBLE : FL_SINGLE);
  if(CTX.antialiasing) mode |= FL_MULTISAMPLE;
  g_opengl_window->mode(mode);
  g_opengl_window->end();

  g_window->position(CTX.gl_position[0], CTX.gl_position[1]);
  g_window->end();
}

// Set the size of the graphical window

void GUI::set_size(int new_w, int new_h)
{
  g_window->size(new_w, new_h + g_window->h() - g_opengl_window->h());
}

// Set graphic window title

void GUI::set_title(const char *str)
{
  g_window->label(str);
}

// Update GUI when views get added or deleted

void GUI::update_views()
{
  check_anim_buttons();
  if(get_context() == 3)
    set_context(menu_post, 0);
  reset_option_browser();
  reset_plugin_view_browser();
  reset_clip_browser();
  reset_external_view_list();
}

// Set animation button

void GUI::set_anim_buttons(int mode)
{
  if(mode) {
    g_status_butt[7]->callback(status_play_cb);
    g_status_butt[7]->label("@-1gmsh_play");
  }
  else {
    g_status_butt[7]->callback(status_pause_cb);
    g_status_butt[7]->label("@-1gmsh_pause");
  }
}

void GUI::check_anim_buttons()
{
  bool play = false;
  if(CTX.post.anim_cycle){
    play = true;
  }
  else{
    for(unsigned int i = 0; i < PView::list.size(); i++){
      if(PView::list[i]->getData()->getNumTimeSteps() > 1){
        play = true;
        break;
      }
    }
  }
  if(play){
    g_status_butt[6]->activate();
    g_status_butt[7]->activate();
    g_status_butt[10]->activate();
    g_status_butt[11]->activate();
  }
  else{
    g_status_butt[6]->deactivate();
    g_status_butt[7]->deactivate();
    g_status_butt[10]->deactivate();
    g_status_butt[11]->deactivate();
  }
}

// Set the status messages

void GUI::set_status(const char *msg, int num)
{
  if(num == 0 || num == 1){
    static char buff[2][1024];
    strncpy(buff[num], msg, sizeof(buff[num]) - 1);
    buff[num][sizeof(buff[num]) - 1] = '\0';
    g_status_label[num]->label(buff[num]);
    g_status_label[num]->redraw();
  }
  else if(num == 2){
    int n = strlen(msg);
    int i = 0;
    while(i < n) if(msg[i++] == '\n') break;
    strncpy(onscreen_buffer[0], msg, sizeof(onscreen_buffer[0]) - 1);
    if(i < n) 
      strncpy(onscreen_buffer[1], &msg[i], sizeof(onscreen_buffer[1]) - 1);
    else
      onscreen_buffer[1][0] = '\0';
    onscreen_buffer[0][i-1] = '\0';
    redraw_opengl();
  }
}

void GUI::add_multiline_in_browser(Fl_Browser * o, const char *prefix, const char *str)
{
  int start = 0, len;
  char *buff;
  if(!str || !strlen(str) || !strcmp(str, "\n")) {
    o->add(" ");
    return;
  }
  for(unsigned int i = 0; i < strlen(str); i++) {
    if(i == strlen(str) - 1 || str[i] == '\n') {
      len = i - start + (str[i] == '\n' ? 0 : 1);
      buff = new char[len + strlen(prefix) + 2];
      strcpy(buff, prefix);
      strncat(buff, &str[start], len);
      buff[len + strlen(prefix)] = '\0';
      o->add(buff);
      start = i + 1;
    }
  }
}

// set the current drawing context 

void GUI::make_opengl_current()
{
  g_opengl_window->make_current();
}

// Draw the opengl window

void GUI::redraw_opengl()
{
  g_opengl_window->make_current();
  g_opengl_window->redraw();
  check();
}

// Create the option window

void GUI::hide_all_option_groups()
{
  gen_group->hide();
  geo_group->hide();
  mesh_group->hide();
  solver_group->hide();
  post_group->hide();
  view_group->hide();
}

void GUI::create_general_options_window()
{
  create_option_window();
  hide_all_option_groups();
  gen_group->show();
  opt_browser->value(1);
  opt_window->label("Options - General");
}

void GUI::create_geometry_options_window()
{
  create_option_window();
  hide_all_option_groups();
  geo_group->show();
  opt_browser->value(2);
  opt_window->label("Options - Geometry");
}

void GUI::create_mesh_options_window()
{
  create_option_window();
  hide_all_option_groups();
  mesh_group->show();
  opt_browser->value(3);
  opt_window->label("Options - Mesh");
}

void GUI::create_solver_options_window()
{
  create_option_window();
  hide_all_option_groups();
  solver_group->show();
  opt_browser->value(4);
  opt_window->label("Options - Solver");
}

void GUI::create_post_options_window()
{
  create_option_window();
  hide_all_option_groups();
  post_group->show();
  opt_browser->value(5);
  opt_window->label("Options - Post-processing");
}

void GUI::create_view_options_window(int num)
{
  create_option_window();
  hide_all_option_groups();
  update_view_window(num);
  view_group->show();
  opt_browser->value(6 + num);
  static char str[128];
  sprintf(str, "Options - View [%d]", num);
  opt_window->label(str);
}

void GUI::reset_option_browser()
{
  char str[128];
  int select = opt_browser->value();
  opt_browser->clear();
  opt_browser->add("General");
  opt_browser->add("Geometry");
  opt_browser->add("Mesh");
  opt_browser->add("Solver");
  opt_browser->add("Post-pro");
  for(unsigned int i = 0; i < PView::list.size(); i++) {
    sprintf(str, "View [%d]", i);
    opt_browser->add(str);
  }
  int item = (select <= opt_browser->size()) ? select : opt_browser->size();

  opt_browser->value(item);
  hide_all_option_groups();
  switch(item){
  case 0: case 1: gen_group->show(); break;
  case 2: geo_group->show(); break;
  case 3: mesh_group->show(); break;
  case 4: solver_group->show(); break;
  case 5: post_group->show(); break;
  default: update_view_window(item - 6); view_group->show(); break;
  }
}

void GUI::reset_external_view_list()
{
  char str[32];
  view_choice[10]->clear();
  view_choice[11]->clear();
  view_choice[10]->add("Self");
  view_choice[11]->add("Self");
  for(unsigned int i = 0; i < PView::list.size(); i++) {
    sprintf(str, "View [%d]", i);
    view_choice[10]->add(str, 0, NULL);
    view_choice[11]->add(str, 0, NULL);
  }
  if(view_number >= 0){
    opt_view_external_view(view_number, GMSH_GUI, 0);
    opt_view_gen_raise_view(view_number, GMSH_GUI, 0);
  }
}

void GUI::create_option_window()
{
  int width = 34 * fontsize + WB;
  int height = 13 * BH + 5 * WB;
  int L = 7 * fontsize;

  if(opt_window) {
    opt_window->show();
    return;
  }

  opt_window = new Dialog_Window(width, height, CTX.non_modal_windows);
  opt_window->box(GMSH_WINDOW_BOX);

  // Buttons

  {
    Fl_Button *o = new Fl_Button(width - BB - WB, height - BH - WB, BB, BH, "Cancel");
    o->callback(cancel_cb, (void *)opt_window);
  }
  {
    Fl_Button *o = new Fl_Button((int)(width - 2.5 * BB - 2 * WB), height - BH - WB, (int)(1.5 * BB), BH, "Save as defaults");
    o->callback(options_save_cb);
  }
  {
    opt_redraw = new Fl_Return_Button((int)(width - 3.5 * BB - 3 * WB), height - BH - WB, BB, BH, "Redraw");
    opt_redraw->callback(redraw_cb);
  }

  // Selection browser

  opt_browser = new Fl_Hold_Browser(WB, WB, L - WB, height - 3 * WB - BH);
  opt_browser->has_scrollbar(Fl_Browser_::VERTICAL);
  opt_browser->add("General");
  opt_browser->add("Geometry");
  opt_browser->add("Mesh");
  opt_browser->add("Solver");
  opt_browser->add("Post-pro");
  opt_browser->callback(options_browser_cb);
  opt_browser->value(1);
  opt_window->label("Options - General");

  width -= L;
  int BW = width - 4 * WB;
  height -= WB + BH;

  // General options

  gen_group = new Fl_Group(L, 0, width, height, "General Options");
  {
    Fl_Tabs *o = new Fl_Tabs(L + WB, WB, width - 2 * WB, height - 2 * WB);
    {
      Fl_Group *o = new Fl_Group(L + WB, WB + BH, width - 2 * WB, height - 2 * WB - BH, "General");

      gen_butt[10] = new Fl_Check_Button(L + 2 * WB, 2 * WB + 1 * BH, BW, BH, "Enable expert mode");
      gen_butt[10]->type(FL_TOGGLE_BUTTON);
      gen_butt[10]->callback(general_options_ok_cb);

      gen_butt[13] = new Fl_Check_Button(L + 2 * WB, 2 * WB + 2 * BH, BW, BH, "Show tooltips");
      gen_butt[13]->type(FL_TOGGLE_BUTTON);
      gen_butt[13]->callback(general_options_ok_cb);

      gen_butt[6] = new Fl_Check_Button(L + 2 * WB, 2 * WB + 3 * BH, BW, BH, "Show bounding boxes");
      gen_butt[6]->tooltip("(Alt+b)");
      gen_butt[6]->type(FL_TOGGLE_BUTTON);
      gen_butt[6]->callback(general_options_ok_cb);

      gen_butt[2] = new Fl_Check_Button(L + 2 * WB, 2 * WB + 4 * BH, BW, BH, "Draw simplified model during user interaction");
      gen_butt[2]->tooltip("(Alt+f)");
      gen_butt[2]->type(FL_TOGGLE_BUTTON);
      gen_butt[2]->callback(general_options_ok_cb, (void*)"fast_redraw");

      gen_butt[11] = new Fl_Check_Button(L + 2 * WB, 2 * WB + 5 * BH, BW, BH, "Enable mouse hover over meshes");
      gen_butt[11]->type(FL_TOGGLE_BUTTON);
      gen_butt[11]->callback(general_options_ok_cb);

      gen_butt[3] = new Fl_Check_Button(L + 2 * WB, 2 * WB + 6 * BH, BW, BH, "Enable double buffering");
      gen_butt[3]->type(FL_TOGGLE_BUTTON);
      gen_butt[3]->callback(general_options_ok_cb);

      gen_butt[12] = new Fl_Check_Button(L + 2 * WB, 2 * WB + 7 * BH, BW, BH, "Enable antialiasing");
      gen_butt[12]->type(FL_TOGGLE_BUTTON);
      gen_butt[12]->callback(general_options_ok_cb);

      gen_butt[5] = new Fl_Check_Button(L + 2 * WB, 2 * WB + 8 * BH, BW, BH, "Use trackball rotation instead of Euler angles");
      gen_butt[5]->type(FL_TOGGLE_BUTTON);
      gen_butt[5]->callback(general_options_ok_cb);

      gen_butt[15] = new Fl_Check_Button(L + 2 * WB, 2 * WB + 9 * BH, BW, BH, "Rotate around pseudo center of mass");
      gen_butt[15]->type(FL_TOGGLE_BUTTON);
      gen_butt[15]->callback(general_options_ok_cb, (void*)"rotation_center");

      gen_push_butt[0] = new Fl_Button(L + 2 * IW - 2 * WB, 2 * WB + 10 * BH, BB, BH, "Select");
      gen_push_butt[0]->callback(general_options_rotation_center_select_cb);

      gen_value[8] = new Fl_Value_Input(L + 2 * WB, 2 * WB + 10 * BH, IW / 3, BH);
      gen_value[8]->callback(general_options_ok_cb, (void*)"rotation_center_coord");
      gen_value[9] = new Fl_Value_Input(L + 2 * WB + IW / 3, 2 * WB + 10 * BH, IW / 3, BH);
      gen_value[9]->callback(general_options_ok_cb, (void*)"rotation_center_coord");
      gen_value[10] = new Fl_Value_Input(L + 2 * WB + 2 * IW / 3, 2 * WB + 10 * BH, IW / 3, BH, "Rotation center");
      gen_value[10]->align(FL_ALIGN_RIGHT);
      gen_value[10]->callback(general_options_ok_cb, (void*)"rotation_center_coord");

      o->end();
    }
    {
      Fl_Group *o = new Fl_Group(L + WB, WB + BH, width - 2 * WB, height - 2 * WB - BH, "Axes");

      gen_choice[4] = new Fl_Choice(L + 2 * WB, 2 * WB + 1 * BH, IW, BH, "Axes mode");
      gen_choice[4]->menu(menu_axes_mode);
      gen_choice[4]->align(FL_ALIGN_RIGHT);
      gen_choice[4]->tooltip("(Alt+a)");
      gen_choice[4]->callback(general_options_ok_cb, (void*)"general_axes");

      gen_butt[16] = new Fl_Check_Button(L + width - (int)(0.85*IW) - 2 * WB, 2 * WB + 1 * BH, (int)(0.85*IW), BH, "Mikado style");
      gen_butt[16]->type(FL_TOGGLE_BUTTON);
      gen_butt[16]->callback(general_options_ok_cb);

      gen_value[17] = new Fl_Value_Input(L + 2 * WB, 2 * WB + 2 * BH, IW / 3, BH);
      gen_value[17]->minimum(0.);
      gen_value[17]->step(1);
      gen_value[17]->maximum(100);
      gen_value[17]->callback(general_options_ok_cb);
      gen_value[18] = new Fl_Value_Input(L + 2 * WB + 1*IW/3, 2 * WB + 2 * BH, IW / 3, BH);
      gen_value[18]->minimum(0.);
      gen_value[18]->step(1);
      gen_value[18]->maximum(100);
      gen_value[18]->callback(general_options_ok_cb);
      gen_value[19] = new Fl_Value_Input(L + 2 * WB + 2*IW/3, 2 * WB + 2 * BH, IW / 3, BH, "Axes tics");
      gen_value[19]->minimum(0.);
      gen_value[19]->step(1);
      gen_value[19]->maximum(100);
      gen_value[19]->align(FL_ALIGN_RIGHT);
      gen_value[19]->callback(general_options_ok_cb);

      gen_input[3] = new Fl_Input(L + 2 * WB, 2 * WB + 3 * BH, IW/3, BH);
      gen_input[3]->callback(general_options_ok_cb);
      gen_input[4] = new Fl_Input(L + 2 * WB + 1*IW/3, 2 * WB + 3 * BH, IW/3, BH);
      gen_input[4]->callback(general_options_ok_cb);
      gen_input[5] = new Fl_Input(L + 2 * WB + 2*IW/3, 2 * WB + 3 * BH, IW/3, BH, "Axes format");
      gen_input[5]->align(FL_ALIGN_RIGHT);
      gen_input[5]->callback(general_options_ok_cb);
      
      gen_input[6] = new Fl_Input(L + 2 * WB, 2 * WB + 4 * BH, IW/3, BH);
      gen_input[6]->callback(general_options_ok_cb);
      gen_input[7] = new Fl_Input(L + 2 * WB + 1*IW/3, 2 * WB + 4 * BH, IW/3, BH);
      gen_input[7]->callback(general_options_ok_cb);
      gen_input[8] = new Fl_Input(L + 2 * WB + 2*IW/3, 2 * WB + 4 * BH, IW/3, BH, "Axes labels");
      gen_input[8]->align(FL_ALIGN_RIGHT);
      gen_input[8]->callback(general_options_ok_cb);

      gen_butt[0] = new Fl_Check_Button(L + 2 * WB, 2 * WB + 5 * BH, BW, BH, "Set position and size of axes automatically");
      gen_butt[0]->type(FL_TOGGLE_BUTTON);
      gen_butt[0]->callback(general_options_ok_cb, (void*)"general_axes_auto");
      
      gen_value[20] = new Fl_Value_Input(L + 2 * WB, 2 * WB + 6 * BH, IW / 3, BH);
      gen_value[20]->callback(general_options_ok_cb);
      gen_value[21] = new Fl_Value_Input(L + 2 * WB + IW / 3, 2 * WB + 6 * BH, IW / 3, BH);
      gen_value[21]->callback(general_options_ok_cb);
      gen_value[22] = new Fl_Value_Input(L + 2 * WB + 2 * IW / 3, 2 * WB + 6 * BH, IW / 3, BH, "Axes minimum");
      gen_value[22]->align(FL_ALIGN_RIGHT);
      gen_value[22]->callback(general_options_ok_cb);

      gen_value[23] = new Fl_Value_Input(L + 2 * WB, 2 * WB + 7 * BH, IW / 3, BH);
      gen_value[23]->callback(general_options_ok_cb);
      gen_value[24] = new Fl_Value_Input(L + 2 * WB + IW / 3, 2 * WB + 7 * BH, IW / 3, BH);
      gen_value[24]->callback(general_options_ok_cb);
      gen_value[25] = new Fl_Value_Input(L + 2 * WB + 2 * IW / 3, 2 * WB + 7 * BH, IW / 3, BH, "Axes maximum");
      gen_value[25]->align(FL_ALIGN_RIGHT);
      gen_value[25]->callback(general_options_ok_cb);

      gen_butt[1] = new Fl_Check_Button(L + 2 * WB, 2 * WB + 8 * BH, BW, BH, "Show small axes");
      gen_butt[1]->tooltip("(Alt+Shift+a)");
      gen_butt[1]->type(FL_TOGGLE_BUTTON);
      gen_butt[1]->callback(general_options_ok_cb, (void*)"general_small_axes");

      gen_value[26] = new Fl_Value_Input(L + 2 * WB, 2 * WB + 9 * BH, IW / 2, BH);
      gen_value[26]->minimum(-2000);
      gen_value[26]->maximum(2000);
      gen_value[26]->step(1);
      gen_value[26]->callback(general_options_ok_cb);
      gen_value[27] = new Fl_Value_Input(L + 2 * WB + IW / 2, 2 * WB + 9 * BH, IW / 2, BH, "Small axes position");
      gen_value[27]->align(FL_ALIGN_RIGHT);
      gen_value[27]->minimum(-2000);
      gen_value[27]->maximum(2000);
      gen_value[27]->step(1);
      gen_value[27]->callback(general_options_ok_cb);

      o->end();
    }
    {
      Fl_Group *o = new Fl_Group(L + WB, WB + BH, width - 2 * WB, height - 2 * WB - BH, "Output");
      gen_butt[7] = new Fl_Check_Button(L + 2 * WB, 2 * WB + 1 * BH, BW, BH, "Print messages on terminal");
      gen_butt[7]->type(FL_TOGGLE_BUTTON);
      gen_butt[7]->callback(general_options_ok_cb);

      gen_butt[8] = new Fl_Check_Button(L + 2 * WB, 2 * WB + 2 * BH, BW, BH, "Save session information on exit");
      gen_butt[8]->type(FL_TOGGLE_BUTTON);
      gen_butt[8]->callback(general_options_ok_cb);

      gen_butt[9] = new Fl_Check_Button(L + 2 * WB, 2 * WB + 3 * BH, BW/2-WB, BH, "Save options on exit");
      gen_butt[9]->type(FL_TOGGLE_BUTTON);
      gen_butt[9]->callback(general_options_ok_cb);

      Fl_Button *b0 = new Fl_Button(L + width / 2, 2 * WB + 3 * BH, (int)(1.75*BB), BH, "Restore default options");
      b0->callback(options_restore_defaults_cb);

      gen_butt[14] = new Fl_Check_Button(L + 2 * WB, 2 * WB + 4 * BH, BW, BH, "Ask confirmation before overwriting files");
      gen_butt[14]->type(FL_TOGGLE_BUTTON);
      gen_butt[14]->callback(general_options_ok_cb);

      gen_value[5] = new Fl_Value_Input(L + 2 * WB, 2 * WB + 5 * BH, IW, BH, "Message verbosity");
      gen_value[5]->minimum(0);
      gen_value[5]->maximum(10);
      gen_value[5]->step(1);
      gen_value[5]->align(FL_ALIGN_RIGHT);
      gen_value[5]->callback(general_options_ok_cb);

      gen_input[0] = new Fl_Input(L + 2 * WB, 2 * WB + 6 * BH, IW, BH, "Default file name");
      gen_input[0]->align(FL_ALIGN_RIGHT);
      gen_input[0]->callback(general_options_ok_cb);

      o->end();
    }
    {
      Fl_Group *o = new Fl_Group(L + WB, WB + BH, width - 2 * WB, height - 2 * WB - BH, "Helpers");

      gen_input[1] = new Fl_Input(L + 2 * WB, 2 * WB + 1 * BH, IW, BH, "Text editor command");
      gen_input[1]->align(FL_ALIGN_RIGHT);
      gen_input[1]->callback(general_options_ok_cb);

      gen_input[2] = new Fl_Input(L + 2 * WB, 2 * WB + 2 * BH, IW, BH, "Web browser command");
      gen_input[2]->align(FL_ALIGN_RIGHT);
      gen_input[2]->callback(general_options_ok_cb);

      o->end();
    }
    {
      Fl_Group *o = new Fl_Group(L + WB, WB + BH, width - 2 * WB, height - 2 * WB - BH, "Aspect");
      o->hide();

      static Fl_Menu_Item menu_projection[] = {
        {"Orthographic", 0, 0, 0},
        {"Perspective", 0, 0, 0},
        {0}
      };
      gen_choice[2] = new Fl_Choice(L + 2 * WB, 2 * WB + 1 * BH, IW, BH, "Projection mode");
      gen_choice[2]->menu(menu_projection);
      gen_choice[2]->align(FL_ALIGN_RIGHT);
      gen_choice[2]->tooltip("(Alt+o)");
      gen_choice[2]->callback(general_options_ok_cb);

      gen_value[14] = new Fl_Value_Input(L + 2 * WB, 2 * WB + 2 * BH, IW, BH, "Z-clipping distance factor");
      gen_value[14]->minimum(0.1);
      gen_value[14]->maximum(10.);
      gen_value[14]->step(0.1);
      gen_value[14]->align(FL_ALIGN_RIGHT);
      gen_value[14]->callback(general_options_ok_cb);

      gen_value[15] = new Fl_Value_Input(L + 2 * WB, 2 * WB + 3 * BH, IW/2, BH);
      gen_value[15]->minimum(0.);
      gen_value[15]->maximum(10.);
      gen_value[15]->step(0.01);
      gen_value[15]->align(FL_ALIGN_RIGHT);
      gen_value[15]->callback(general_options_ok_cb);

      gen_value[16] = new Fl_Value_Input(L + 2 * WB + IW/2, 2 * WB + 3 * BH, IW/2, BH, "Polygon offset factor/units");
      gen_value[16]->minimum(0.);
      gen_value[16]->maximum(10.);
      gen_value[16]->step(0.01);
      gen_value[16]->align(FL_ALIGN_RIGHT);
      gen_value[16]->callback(general_options_ok_cb);

      gen_butt[4] = new Fl_Check_Button(L + 2 * WB, 2 * WB + 4 * BH, BW, BH, "Always apply polygon offset");
      gen_butt[4]->type(FL_TOGGLE_BUTTON);
      gen_butt[4]->callback(general_options_ok_cb);

      gen_value[11] = new Fl_Value_Input(L + 2 * WB, 2 * WB + 5 * BH, IW, BH, "Quadric subdivisions");
      gen_value[11]->minimum(3);
      gen_value[11]->maximum(30);
      gen_value[11]->step(1);
      gen_value[11]->align(FL_ALIGN_RIGHT);
      gen_value[11]->callback(general_options_ok_cb);

      gen_value[6] = new Fl_Value_Input(L + 2 * WB, 2 * WB + 6 * BH, IW, BH, "Point size");
      gen_value[6]->minimum(0.1);
      gen_value[6]->maximum(50);
      gen_value[6]->step(0.1);
      gen_value[6]->align(FL_ALIGN_RIGHT);
      gen_value[6]->callback(general_options_ok_cb);

      gen_value[7] = new Fl_Value_Input(L + 2 * WB, 2 * WB + 7 * BH, IW, BH, "Line width");
      gen_value[7]->minimum(0.1);
      gen_value[7]->maximum(50);
      gen_value[7]->step(0.1);
      gen_value[7]->align(FL_ALIGN_RIGHT);
      gen_value[7]->callback(general_options_ok_cb);
      
      static Fl_Menu_Item menu_genvectype[] = {
        {"Line", 0, 0, 0},
        {"Arrow", 0, 0, 0},
        {"Pyramid", 0, 0, 0},
        {"3D arrow", 0, 0, 0},
        {0}
      };
      gen_choice[0] = new Fl_Choice(L + 2 * WB, 2 * WB + 8 * BH, IW, BH, "Vector display");
      gen_choice[0]->menu(menu_genvectype);
      gen_choice[0]->align(FL_ALIGN_RIGHT);
      gen_choice[0]->callback(general_options_ok_cb);

      Fl_Button *b = new Fl_Button(L + 2 * IW - 2 * WB, 2 * WB + 8 * BH, BB, BH, "Edit arrow");
      b->callback(general_arrow_param_cb);

      gen_choice[1] = new Fl_Choice(L + 2 * WB, 2 * WB + 9 * BH, IW, BH, "Font");
      gen_choice[1]->menu(menu_font_names);
      gen_choice[1]->align(FL_ALIGN_RIGHT);
      gen_choice[1]->callback(general_options_ok_cb);

      gen_value[12] = new Fl_Value_Input(L + 2 * WB, 2 * WB + 10 * BH, IW, BH, "Font size");
      gen_value[12]->minimum(5);
      gen_value[12]->maximum(40);
      gen_value[12]->step(1);
      gen_value[12]->align(FL_ALIGN_RIGHT);
      gen_value[12]->callback(general_options_ok_cb);

      o->end();
    }
    {
      Fl_Group *o = new Fl_Group(L + WB, WB + BH, width - 2 * WB, height - 2 * WB - BH, "Color");
      o->hide();

      gen_value[2] = new Fl_Value_Input(L + 2 * WB, 2 * WB + 1 * BH, IW/3, BH);
      gen_value[2]->minimum(-1.);
      gen_value[2]->maximum(1.);
      gen_value[2]->step(0.01);
      gen_value[2]->callback(general_options_ok_cb, (void*)"light_value");

      gen_value[3] = new Fl_Value_Input(L + 2 * WB + IW / 3, 2 * WB + 1 * BH, IW/3, BH);
      gen_value[3]->minimum(-1.);
      gen_value[3]->maximum(1.);
      gen_value[3]->step(0.01);
      gen_value[3]->callback(general_options_ok_cb, (void*)"light_value");

      gen_value[4] = new Fl_Value_Input(L + 2 * WB + 2 * IW / 3, 2 * WB + 1 * BH, IW/3, BH, "Light position");
      gen_value[4]->minimum(-1.);
      gen_value[4]->maximum(1.);
      gen_value[4]->step(0.01);
      gen_value[4]->align(FL_ALIGN_RIGHT);
      gen_value[4]->callback(general_options_ok_cb, (void*)"light_value");

      gen_value[13] = new Fl_Value_Input(L + 2 * WB, 2 * WB + 2 * BH, IW, BH, "Light position divisor");
      gen_value[13]->minimum(0.);
      gen_value[13]->maximum(1.);
      gen_value[13]->step(0.01);
      gen_value[13]->align(FL_ALIGN_RIGHT);
      gen_value[13]->callback(general_options_ok_cb);

      gen_sphere = new SpherePosition_Widget(L + width - 2 * BH - 2 * WB, 2 * WB + 1 * BH, 2 * BH);
      gen_sphere->callback(general_options_ok_cb, (void*)"light_sphere");

      gen_value[1] = new Fl_Value_Input(L + 2 * WB, 2 * WB + 3 * BH, IW/2, BH);
      gen_value[1]->minimum(0);
      gen_value[1]->maximum(10);
      gen_value[1]->step(0.1);
      gen_value[1]->callback(general_options_ok_cb);
      gen_value[0] = new Fl_Value_Input(L + 2 * WB + IW/2, 2 * WB + 3 * BH, IW/2, BH, "Material shininess/exponent");
      gen_value[0]->minimum(0);
      gen_value[0]->maximum(128);
      gen_value[0]->step(1);
      gen_value[0]->align(FL_ALIGN_RIGHT);
      gen_value[0]->callback(general_options_ok_cb);

      static Fl_Menu_Item menu_color_scheme[] = {
        {"Dark", 0, 0, 0},
        {"Light", 0, 0, 0},
        {"Grayscale", 0, 0, 0},
        {0}
      };

      gen_choice[3] = new Fl_Choice(L + 2 * WB, 2 * WB + 4 * BH, IW, BH, "Predefined color scheme");
      gen_choice[3]->menu(menu_color_scheme);
      gen_choice[3]->align(FL_ALIGN_RIGHT);
      gen_choice[3]->tooltip("(Alt+c)");
      gen_choice[3]->callback(general_options_color_scheme_cb);

      static Fl_Menu_Item menu_bg_grad[] = {
        {"None", 0, 0, 0},
        {"Vertical", 0, 0, 0},
        {"Horizontal", 0, 0, 0},
        {"Radial", 0, 0, 0},
        {0}
      };

      gen_choice[5] = new Fl_Choice(L + 2 * WB, 2 * WB + 5 * BH, IW, BH, "Background gradient");
      gen_choice[5]->menu(menu_bg_grad);
      gen_choice[5]->align(FL_ALIGN_RIGHT);
      gen_choice[5]->callback(general_options_ok_cb);

      Fl_Scroll *s = new Fl_Scroll(L + 2 * WB, 3 * WB + 6 * BH, IW + 20, height - 5 * WB - 6 * BH);
      int i = 0;
      while(GeneralOptions_Color[i].str) {
        gen_col[i] = new Fl_Button(L + 2 * WB, 3 * WB + (6 + i) * BH, IW, BH, GeneralOptions_Color[i].str);
        gen_col[i]->callback(color_cb, (void *)GeneralOptions_Color[i].function);
        i++;
      }
      s->end();

      o->end();
    }
    o->end();
  }
  gen_group->end();

  // Geometry options

  geo_group = new Fl_Group(L, 0, width, height, "Geometry Options");
  geo_group->hide();
  {
    Fl_Tabs *o = new Fl_Tabs(L + WB, WB, width - 2 * WB, height - 2 * WB);
    {
      Fl_Group *o = new Fl_Group(L + WB, WB + BH, width - 2 * WB, height - 2 * WB - BH, "General");
      o->hide();

      geo_value[2] = new Fl_Value_Input(L + 2 * WB, 2 * WB + 1 * BH, IW, BH, "Geometry tolerance");
      geo_value[2]->align(FL_ALIGN_RIGHT);
      geo_value[2]->callback(geometry_options_ok_cb);

      geo_butt[8] = new Fl_Check_Button(L + 2 * WB, 2 * WB + 2 * BH, BW, BH, "Remove duplicate entities in GEO models");
      geo_butt[8]->type(FL_TOGGLE_BUTTON);
      geo_butt[8]->callback(geometry_options_ok_cb);

      geo_butt[11] = new Fl_Check_Button(L + 2 * WB, 2 * WB + 3 * BH, BW, BH, "Remove small edges in OpenCascade models");
      geo_butt[11]->type(FL_TOGGLE_BUTTON);
      geo_butt[11]->callback(geometry_options_ok_cb);

      geo_butt[12] = new Fl_Check_Button(L + 2 * WB, 2 * WB + 4 * BH, BW, BH, "Remove small faces in OpenCascade models");
      geo_butt[12]->type(FL_TOGGLE_BUTTON);
      geo_butt[12]->callback(geometry_options_ok_cb);

      geo_butt[13] = new Fl_Check_Button(L + 2 * WB, 2 * WB + 5 * BH, BW, BH, "Sew faces in OpenCascade models");
      geo_butt[13]->type(FL_TOGGLE_BUTTON);
      geo_butt[13]->callback(geometry_options_ok_cb);

#if !defined(HAVE_OCC)
      geo_butt[11]->deactivate();
      geo_butt[12]->deactivate();
      geo_butt[13]->deactivate();
#endif
      o->end();
    }
    {
      Fl_Group *o = new Fl_Group(L + WB, WB + BH, width - 2 * WB, height - 2 * WB - BH, "Visibility");

      geo_butt[0] = new Fl_Check_Button(L + 2 * WB, 2 * WB + 1 * BH, BW / 2 - WB, BH, "Points");
      geo_butt[0]->tooltip("(Alt+p)");
      geo_butt[0]->type(FL_TOGGLE_BUTTON);
      geo_butt[0]->callback(geometry_options_ok_cb);
      
      geo_butt[1] = new Fl_Check_Button(L + 2 * WB, 2 * WB + 2 * BH, BW / 2 - WB, BH, "Lines");
      geo_butt[1]->tooltip("(Alt+l)");
      geo_butt[1]->type(FL_TOGGLE_BUTTON);
      geo_butt[1]->callback(geometry_options_ok_cb);

      geo_butt[2] = new Fl_Check_Button(L + 2 * WB, 2 * WB + 3 * BH, BW / 2 - WB, BH, "Surfaces");
      geo_butt[2]->tooltip("(Alt+s)");
      geo_butt[2]->type(FL_TOGGLE_BUTTON);
      geo_butt[2]->callback(geometry_options_ok_cb);

      geo_butt[3] = new Fl_Check_Button(L + 2 * WB, 2 * WB + 4 * BH, BW / 2 - WB, BH, "Volumes");
      geo_butt[3]->tooltip("(Alt+v)");
      geo_butt[3]->type(FL_TOGGLE_BUTTON);
      geo_butt[3]->callback(geometry_options_ok_cb);

      geo_butt[4] = new Fl_Check_Button(L + width / 2, 2 * WB + 1 * BH, BW / 2 - WB, BH, "Point numbers");
      geo_butt[4]->type(FL_TOGGLE_BUTTON);
      geo_butt[4]->callback(geometry_options_ok_cb);

      geo_butt[5] = new Fl_Check_Button(L + width / 2, 2 * WB + 2 * BH, BW / 2 - WB, BH, "Line numbers");
      geo_butt[5]->type(FL_TOGGLE_BUTTON);
      geo_butt[5]->callback(geometry_options_ok_cb);

      geo_butt[6] = new Fl_Check_Button(L + width / 2, 2 * WB + 3 * BH, BW / 2 - WB, BH, "Surface numbers");
      geo_butt[6]->type(FL_TOGGLE_BUTTON);
      geo_butt[6]->callback(geometry_options_ok_cb);

      geo_butt[7] = new Fl_Check_Button(L + width / 2, 2 * WB + 4 * BH, BW / 2 - WB, BH, "Volume numbers");
      geo_butt[7]->type(FL_TOGGLE_BUTTON);
      geo_butt[7]->callback(geometry_options_ok_cb);

      geo_value[0] = new Fl_Value_Input(L + 2 * WB, 2 * WB + 5 * BH, IW, BH, "Normals");
      geo_value[0]->minimum(0);
      geo_value[0]->maximum(500);
      geo_value[0]->step(1);
      geo_value[0]->align(FL_ALIGN_RIGHT);
      geo_value[0]->when(FL_WHEN_RELEASE);
      geo_value[0]->callback(geometry_options_ok_cb);

      geo_value[1] = new Fl_Value_Input(L + 2 * WB, 2 * WB + 6 * BH, IW, BH, "Tangents");
      geo_value[1]->minimum(0);
      geo_value[1]->maximum(500);
      geo_value[1]->step(1);
      geo_value[1]->align(FL_ALIGN_RIGHT);
      geo_value[1]->when(FL_WHEN_RELEASE);
      geo_value[1]->callback(geometry_options_ok_cb);

      o->end();
    }
    {
      Fl_Group *o = new Fl_Group(L + WB, WB + BH, width - 2 * WB, height - 2 * WB - BH, "Aspect");
      o->hide();

      geo_choice[0] = new Fl_Choice(L + 2 * WB, 2 * WB + 1 * BH, IW, BH, "Point display");
      geo_choice[0]->menu(menu_point_display);
      geo_choice[0]->align(FL_ALIGN_RIGHT);
      geo_choice[0]->callback(geometry_options_ok_cb);

      geo_value[3] = new Fl_Value_Input(L + 2 * WB, 2 * WB + 2 * BH, IW, BH, "Point size");
      geo_value[3]->minimum(0.1);
      geo_value[3]->maximum(50);
      geo_value[3]->step(0.1);
      geo_value[3]->align(FL_ALIGN_RIGHT);
      geo_value[3]->callback(geometry_options_ok_cb);

      geo_value[5] = new Fl_Value_Input(L + 2 * WB, 2 * WB + 3 * BH, IW, BH, "Selected point size");
      geo_value[5]->minimum(0.1);
      geo_value[5]->maximum(50);
      geo_value[5]->step(0.1);
      geo_value[5]->align(FL_ALIGN_RIGHT);
      geo_value[5]->callback(geometry_options_ok_cb);

      geo_choice[1] = new Fl_Choice(L + 2 * WB, 2 * WB + 4 * BH, IW, BH, "Line display");
      geo_choice[1]->menu(menu_line_display);
      geo_choice[1]->align(FL_ALIGN_RIGHT);     
      geo_choice[1]->callback(geometry_options_ok_cb);

      geo_value[4] = new Fl_Value_Input(L + 2 * WB, 2 * WB + 5 * BH, IW, BH, "Line width");
      geo_value[4]->minimum(0.1);
      geo_value[4]->maximum(50);
      geo_value[4]->step(0.1);
      geo_value[4]->align(FL_ALIGN_RIGHT);
      geo_value[4]->callback(geometry_options_ok_cb);

      geo_value[6] = new Fl_Value_Input(L + 2 * WB, 2 * WB + 6 * BH, IW, BH, "Selected line width");
      geo_value[6]->minimum(0.1);
      geo_value[6]->maximum(50);
      geo_value[6]->step(0.1);
      geo_value[6]->align(FL_ALIGN_RIGHT);
      geo_value[6]->callback(geometry_options_ok_cb);

      geo_choice[2] = new Fl_Choice(L + 2 * WB, 2 * WB + 7 * BH, IW, BH, "Surface display");
      geo_choice[2]->menu(menu_surface_display);
      geo_choice[2]->align(FL_ALIGN_RIGHT);     
      geo_choice[2]->callback(geometry_options_ok_cb);
      geo_choice[2]->tooltip("(Alt+d)");

      o->end();
    }
    {
      Fl_Group *o = new Fl_Group(L + WB, WB + BH, width - 2 * WB, height - 2 * WB - BH, "Color");
      o->hide();

      geo_butt[9] = new Fl_Check_Button(L + 2 * WB, 2 * WB + 1 * BH, BW, BH, "Enable lighting");
      geo_butt[9]->type(FL_TOGGLE_BUTTON);
      geo_butt[9]->tooltip("(Alt+w)");
      geo_butt[9]->callback(geometry_options_ok_cb);

      geo_butt[14] = new Fl_Check_Button(L + 2 * WB, 2 * WB + 2 * BH, BW, BH, "Use two-side lighting");
      geo_butt[14]->type(FL_TOGGLE_BUTTON);
      geo_butt[14]->callback(geometry_options_ok_cb);

      geo_butt[10] = new Fl_Check_Button(L + 2 * WB, 2 * WB + 3 * BH, BW, BH, "Highlight orphan entities");
      geo_butt[10]->type(FL_TOGGLE_BUTTON);
      geo_butt[10]->callback(geometry_options_ok_cb);

      Fl_Scroll *s = new Fl_Scroll(L + 2 * WB, 2 * WB + 4 * BH, IW + 20, height - 4 * WB - 4 * BH);
      int i = 0;
      while(GeometryOptions_Color[i].str) {
        geo_col[i] = new Fl_Button(L + 2 * WB, 2 * WB + (4 + i) * BH, IW, BH, GeometryOptions_Color[i].str);
        geo_col[i]->callback(color_cb, (void *)GeometryOptions_Color[i].function);
        i++;
      }
      s->end();

      o->end();
    }
    o->end();
  }
  geo_group->end();

  // Mesh options

  mesh_group = new Fl_Group(L, 0, width, height, "Mesh Options");
  mesh_group->hide();
  {
    Fl_Tabs *o = new Fl_Tabs(L + WB, WB, width - 2 * WB, height - 2 * WB);
    {
      Fl_Group *o = new Fl_Group(L + WB, WB + BH, width - 2 * WB, height - 2 * WB - BH, "General");
      o->hide();

      static Fl_Menu_Item menu_2d_algo[] = {
        {"Frontal", 0, 0, 0},
        {"Delaunay", 0, 0, 0},
        {"MeshAdapt+Delaunay", 0, 0, 0},
        {0}
      };
      static Fl_Menu_Item menu_3d_algo[] = {
        {"Tetgen+Delaunay", 0, 0, 0},
        {"Netgen", 0, 0, 0},
        {0}
      };
      static Fl_Menu_Item menu_recombine_algo[] = {
        {"Mixed Tri-Quads", 0, 0, 0},
        {"All Quads", 0, 0, 0},
        {0}
      };

      mesh_choice[2] = new Fl_Choice(L + 2 * WB, 2 * WB + 1 * BH, IW, BH, "2D algorithm");
      mesh_choice[2]->menu(menu_2d_algo);
      mesh_choice[2]->align(FL_ALIGN_RIGHT);
      mesh_choice[2]->callback(mesh_options_ok_cb);

      mesh_choice[3] = new Fl_Choice(L + 2 * WB, 2 * WB + 2 * BH, IW, BH, "3D algorithm");
      mesh_choice[3]->menu(menu_3d_algo);
      mesh_choice[3]->align(FL_ALIGN_RIGHT);
      mesh_choice[3]->callback(mesh_options_ok_cb);

      mesh_choice[5] = new Fl_Choice(L + 2 * WB, 2 * WB + 3 * BH, IW, BH, "Recombine algorithm");
      mesh_choice[5]->menu(menu_recombine_algo);
      mesh_choice[5]->align(FL_ALIGN_RIGHT);
      mesh_choice[5]->callback(mesh_options_ok_cb);
      // not reimplemented yet
      ((Fl_Menu_Item*)mesh_choice[5]->menu())[1].deactivate();

      mesh_value[0] = new Fl_Value_Input(L + 2 * WB, 2 * WB + 4 * BH, IW, BH, "Smoothing steps");
      mesh_value[0]->minimum(0);
      mesh_value[0]->maximum(100);
      mesh_value[0]->step(1);
      mesh_value[0]->align(FL_ALIGN_RIGHT);
      mesh_value[0]->callback(mesh_options_ok_cb);

      mesh_value[2] = new Fl_Value_Input(L + 2 * WB, 2 * WB + 5 * BH, IW, BH, "Element size factor");
      mesh_value[2]->minimum(0.001);
      mesh_value[2]->maximum(1000);
      mesh_value[2]->step(0.01);
      mesh_value[2]->align(FL_ALIGN_RIGHT);
      mesh_value[2]->callback(mesh_options_ok_cb);

      mesh_value[25] = new Fl_Value_Input(L + 2 * WB, 2 * WB + 6 * BH, IW, BH, "Minimum element size");
      mesh_value[25]->align(FL_ALIGN_RIGHT);
      mesh_value[25]->callback(mesh_options_ok_cb);

      mesh_value[26] = new Fl_Value_Input(L + 2 * WB, 2 * WB + 7 * BH, IW, BH, "Maximum element size");
      mesh_value[26]->align(FL_ALIGN_RIGHT);
      mesh_value[26]->callback(mesh_options_ok_cb);

      mesh_value[3] = new Fl_Value_Input(L + 2 * WB, 2 * WB + 8 * BH, IW, BH, "Element order");
      mesh_value[3]->minimum(1);
      // FIXME: this makes it possible to set > 2 by hand, but not by
      // dragging (>2 is too buggy for general use)
      mesh_value[3]->maximum(2);
      mesh_value[3]->step(1);
      mesh_value[3]->align(FL_ALIGN_RIGHT);
      mesh_value[3]->callback(mesh_options_ok_cb);

      mesh_butt[4] = new Fl_Check_Button(L + 2 * WB, 2 * WB + 9 * BH, BW, BH, "Use incomplete second order elements");
      mesh_butt[4]->type(FL_TOGGLE_BUTTON);
      mesh_butt[4]->callback(mesh_options_ok_cb);

      o->end();
    }

    {
      Fl_Group *o = new Fl_Group(L + WB, WB + BH, width - 2 * WB, height - 2 * WB - BH, "Advanced");
      o->hide();

      mesh_butt[1] = new Fl_Check_Button(L + 2 * WB, 2 * WB + 1 * BH, BW, BH, "Compute element sizes from curvature" );
      mesh_butt[1]->type(FL_TOGGLE_BUTTON);
      mesh_butt[1]->callback(mesh_options_ok_cb);

      mesh_butt[5] = new Fl_Check_Button(L + 2 * WB, 2 * WB + 2 * BH, BW, BH, "Compute element sizes using point values");
      mesh_butt[5]->type(FL_TOGGLE_BUTTON);
      mesh_butt[5]->callback(mesh_options_ok_cb);

      mesh_butt[2] = new Fl_Check_Button(L + 2 * WB, 2 * WB + 3 * BH, BW, BH, "Optimize quality of tetrahedra");
      mesh_butt[2]->type(FL_TOGGLE_BUTTON);
      mesh_butt[2]->callback(mesh_options_ok_cb);

      mesh_butt[24] = new Fl_Check_Button(L + 2 * WB, 2 * WB + 4 * BH, BW, BH, "Optimize quality of tetrahedra with Netgen");
      mesh_butt[24]->type(FL_TOGGLE_BUTTON);
#if !defined(HAVE_NETGEN)
      mesh_butt[24]->deactivate();
#endif
      mesh_butt[24]->callback(mesh_options_ok_cb);

      mesh_butt[3] = new Fl_Check_Button(L + 2 * WB, 2 * WB + 5 * BH, BW, BH, "Optimize high order mesh (2D-plane only)");
      mesh_butt[3]->type(FL_TOGGLE_BUTTON);
      mesh_butt[3]->callback(mesh_options_ok_cb);

      mesh_butt[21] = new Fl_Check_Button(L + 2 * WB, 2 * WB + 6 * BH, BW, BH, "Impose C1 continuity (2D-plane only)");
      mesh_butt[21]->type(FL_TOGGLE_BUTTON);
      mesh_butt[21]->callback(mesh_options_ok_cb);

      o->end();
    }

    {
      Fl_Group *o = new Fl_Group(L + WB, WB + BH, width - 2 * WB, height - 2 * WB - BH, "Visibility");

      mesh_butt[6] = new Fl_Check_Button(L + 2 * WB, 2 * WB + 1 * BH, BW / 2 - WB, BH, "Nodes");
      mesh_butt[6]->tooltip("(Alt+Shift+p)");
      mesh_butt[6]->type(FL_TOGGLE_BUTTON);
      mesh_butt[6]->callback(mesh_options_ok_cb);

      mesh_butt[7] = new Fl_Check_Button(L + 2 * WB, 2 * WB + 2 * BH, BW / 2 - WB, BH, "Lines");
      mesh_butt[7]->tooltip("(Alt+Shift+l)");
      mesh_butt[7]->type(FL_TOGGLE_BUTTON);
      mesh_butt[7]->callback(mesh_options_ok_cb);

      mesh_butt[8] = new Fl_Check_Button(L + 2 * WB, 2 * WB + 3 * BH, BW / 2 - WB, BH, "Surface edges");
      mesh_butt[8]->tooltip("(Alt+Shift+s)");
      mesh_butt[8]->type(FL_TOGGLE_BUTTON);
      mesh_butt[8]->callback(mesh_options_ok_cb);

      mesh_butt[9] = new Fl_Check_Button(L + 2 * WB, 2 * WB + 4 * BH, BW / 2 - WB, BH, "Surface faces");
      mesh_butt[9]->tooltip("(Alt+Shift+d)");
      mesh_butt[9]->type(FL_TOGGLE_BUTTON);
      mesh_butt[9]->callback(mesh_options_ok_cb);

      mesh_butt[10] = new Fl_Check_Button(L + 2 * WB, 2 * WB + 5 * BH, BW / 2 - WB, BH, "Volume edges");
      mesh_butt[10]->tooltip("(Alt+Shift+v)");
      mesh_butt[10]->type(FL_TOGGLE_BUTTON);
      mesh_butt[10]->callback(mesh_options_ok_cb);

      mesh_butt[11] = new Fl_Check_Button(L + 2 * WB, 2 * WB + 6 * BH, BW / 2 - WB, BH, "Volume faces");
      mesh_butt[11]->tooltip("(Alt+Shift+b)");
      mesh_butt[11]->type(FL_TOGGLE_BUTTON);
      mesh_butt[11]->callback(mesh_options_ok_cb);

      mesh_butt[12] = new Fl_Check_Button(L + width / 2, 2 * WB + 1 * BH, BW / 2 - WB, BH, "Node labels");
      mesh_butt[12]->type(FL_TOGGLE_BUTTON);
      mesh_butt[12]->callback(mesh_options_ok_cb);

      mesh_butt[13] = new Fl_Check_Button(L + width / 2, 2 * WB + 2 * BH, BW / 2 - WB, BH, "Line labels");
      mesh_butt[13]->type(FL_TOGGLE_BUTTON);
      mesh_butt[13]->callback(mesh_options_ok_cb);

      mesh_butt[14] = new Fl_Check_Button(L + width / 2, 2 * WB + 3 * BH, BW / 2 - WB, BH, "Surface labels");
      mesh_butt[14]->type(FL_TOGGLE_BUTTON);
      mesh_butt[14]->callback(mesh_options_ok_cb);

      mesh_butt[15] = new Fl_Check_Button(L + width / 2, 2 * WB + 4 * BH, BW / 2 - WB, BH, "Volume labels");
      mesh_butt[15]->type(FL_TOGGLE_BUTTON);
      mesh_butt[15]->callback(mesh_options_ok_cb);

      static Fl_Menu_Item menu_label_type[] = {
        {"Number", 0, 0, 0},
        {"Elementary entity", 0, 0, 0},
        {"Physical group", 0, 0, 0},
        {"Mesh partition", 0, 0, 0},
        {"Coordinates", 0, 0, 0},
        {0}
      };
      mesh_choice[7] = new Fl_Choice(L + width / 2, 2 * WB + 5 * BH, width / 4 - 2 * WB, BH, "Label type");
      mesh_choice[7]->menu(menu_label_type);
      mesh_choice[7]->align(FL_ALIGN_RIGHT);
      mesh_choice[7]->callback(mesh_options_ok_cb);

      mesh_value[12] = new Fl_Value_Input(L + width / 2, 2 * WB + 6 * BH, width / 4 - 2 * WB, BH, "Frequency");
      mesh_value[12]->minimum(0);
      mesh_value[12]->maximum(100);
      mesh_value[12]->step(1);
      mesh_value[12]->align(FL_ALIGN_RIGHT);
      mesh_value[12]->when(FL_WHEN_RELEASE);
      mesh_value[12]->callback(mesh_options_ok_cb);

      static Fl_Menu_Item menu_mesh_element_types[] = {
        {"Triangles",   0, 0, 0, FL_MENU_TOGGLE},
        {"Quadrangles", 0, 0, 0, FL_MENU_TOGGLE},
        {"Tetrahedra",  0, 0, 0, FL_MENU_TOGGLE},
        {"Hexahedra",   0, 0, 0, FL_MENU_TOGGLE},
        {"Prisms",      0, 0, 0, FL_MENU_TOGGLE},
        {"Pyramids",    0, 0, 0, FL_MENU_TOGGLE},
        {0}
      };

      mesh_menu_butt = new Fl_Menu_Button(L + 2 * WB, 2 * WB + 7 * BH, IW, BH, "Elements");
      mesh_menu_butt->menu(menu_mesh_element_types);
      mesh_menu_butt->callback(mesh_options_ok_cb);

      mesh_value[4] = new Fl_Value_Input(L + 2 * WB, 2 * WB + 8 * BH, IW / 4, BH);
      mesh_value[4]->minimum(0);
      mesh_value[4]->maximum(1);
      mesh_value[4]->step(0.01);
      mesh_value[4]->align(FL_ALIGN_RIGHT);
      mesh_value[4]->when(FL_WHEN_RELEASE);
      mesh_value[4]->callback(mesh_options_ok_cb);

      mesh_value[5] = new Fl_Value_Input(L + 2 * WB + IW / 4, 2 * WB + 8 * BH, IW / 2 - IW / 4, BH);
      mesh_value[5]->minimum(0);
      mesh_value[5]->maximum(1);
      mesh_value[5]->step(0.01);
      mesh_value[5]->align(FL_ALIGN_RIGHT);
      mesh_value[5]->when(FL_WHEN_RELEASE);
      mesh_value[5]->callback(mesh_options_ok_cb);

      static Fl_Menu_Item menu_quality_type[] = {
        {"Gamma", 0, 0, 0},
        {"Eta", 0, 0, 0},
        {"Rho", 0, 0, 0},
        {0}
      };
      mesh_choice[6] = new Fl_Choice(L + 2 * WB + IW / 2, 2 * WB + 8 * BH, IW/2, BH, "Quality range");
      mesh_choice[6]->menu(menu_quality_type);
      mesh_choice[6]->align(FL_ALIGN_RIGHT);
      mesh_choice[6]->callback(mesh_options_ok_cb);

      mesh_value[6] = new Fl_Value_Input(L + 2 * WB, 2 * WB + 9 * BH, IW / 2, BH);
      mesh_value[6]->align(FL_ALIGN_RIGHT);
      mesh_value[6]->when(FL_WHEN_RELEASE);
      mesh_value[6]->callback(mesh_options_ok_cb);

      mesh_value[7] = new Fl_Value_Input(L + 2 * WB + IW / 2, 2 * WB + 9 * BH, IW / 2, BH, "Size range");
      mesh_value[7]->align(FL_ALIGN_RIGHT);
      mesh_value[7]->when(FL_WHEN_RELEASE);
      mesh_value[7]->callback(mesh_options_ok_cb);

      mesh_value[8] = new Fl_Value_Input(L + 2 * WB, 2 * WB + 10 * BH, IW, BH, "Normals");
      mesh_value[8]->minimum(0);
      mesh_value[8]->maximum(500);
      mesh_value[8]->step(1);
      mesh_value[8]->align(FL_ALIGN_RIGHT);
      mesh_value[8]->when(FL_WHEN_RELEASE);
      mesh_value[8]->callback(mesh_options_ok_cb);

      mesh_value[13] = new Fl_Value_Input(L + 2 * WB, 2 * WB + 11 * BH, IW, BH, "Tangents");
      mesh_value[13]->minimum(0);
      mesh_value[13]->maximum(200);
      mesh_value[13]->step(1.0);
      mesh_value[13]->align(FL_ALIGN_RIGHT);
      mesh_value[13]->when(FL_WHEN_RELEASE);
      mesh_value[13]->callback(mesh_options_ok_cb);

      o->end();
    }
    {
      Fl_Group *o = new Fl_Group(L + WB, WB + BH, width - 2 * WB, height - 2 * WB - BH, "Aspect");
      o->hide();

      mesh_value[9] = new Fl_Value_Input(L + 2 * WB, 2 * WB + 1 * BH, IW, BH, "Element shrinking factor");
      mesh_value[9]->minimum(0);
      mesh_value[9]->maximum(1);
      mesh_value[9]->step(0.01);
      mesh_value[9]->align(FL_ALIGN_RIGHT);
      mesh_value[9]->when(FL_WHEN_RELEASE);
      mesh_value[9]->callback(mesh_options_ok_cb);

      mesh_choice[0] = new Fl_Choice(L + 2 * WB, 2 * WB + 2 * BH, IW, BH, "Point display");
      mesh_choice[0]->menu(menu_point_display);
      mesh_choice[0]->align(FL_ALIGN_RIGHT);
      mesh_choice[0]->callback(mesh_options_ok_cb);

      mesh_value[10] = new Fl_Value_Input(L + 2 * WB, 2 * WB + 3 * BH, IW, BH, "Point size");
      mesh_value[10]->minimum(0.1);
      mesh_value[10]->maximum(50);
      mesh_value[10]->step(0.1);
      mesh_value[10]->align(FL_ALIGN_RIGHT);
      mesh_value[10]->callback(mesh_options_ok_cb);

      mesh_value[11] = new Fl_Value_Input(L + 2 * WB, 2 * WB + 4 * BH, IW, BH, "Line width");
      mesh_value[11]->minimum(0.1);
      mesh_value[11]->maximum(50);
      mesh_value[11]->step(0.1);
      mesh_value[11]->align(FL_ALIGN_RIGHT);
      mesh_value[11]->callback(mesh_options_ok_cb);

      o->end();
    }
    {
      Fl_Group *o = new Fl_Group(L + WB, WB + BH, width - 2 * WB, height - 2 * WB - BH, "Color");
      o->hide();

      mesh_butt[17] = new Fl_Check_Button(L + 2 * WB, 2 * WB + 1 * BH, BW, BH, "Enable lighting");
      mesh_butt[17]->tooltip("(Alt+w)");
      mesh_butt[17]->type(FL_TOGGLE_BUTTON);
      mesh_butt[17]->callback(mesh_options_ok_cb, (void*)"mesh_light");

      mesh_butt[20] = new Fl_Check_Button(L + 2 * WB, 2 * WB + 2 * BH, BW, BH, "Enable lighting of lines");
      mesh_butt[20]->type(FL_TOGGLE_BUTTON);
      mesh_butt[20]->callback(mesh_options_ok_cb);

      mesh_butt[18] = new Fl_Check_Button(L + 2 * WB, 2 * WB + 3 * BH, BW, BH, "Use two-side lighting");
      mesh_butt[18]->type(FL_TOGGLE_BUTTON);
      mesh_butt[18]->callback(mesh_options_ok_cb);

      mesh_butt[0] = new Fl_Check_Button(L + 2 * WB, 2 * WB + 4 * BH, BW, BH, "Reverse all normals");
      mesh_butt[0]->tooltip("(Alt+Shift+w)");
      mesh_butt[0]->type(FL_TOGGLE_BUTTON);
      mesh_butt[0]->callback(mesh_options_ok_cb);

      mesh_butt[19] = new Fl_Check_Button(L + 2 * WB, 2 * WB + 5 * BH, BW, BH, "Smooth normals");
      mesh_butt[19]->type(FL_TOGGLE_BUTTON);
      mesh_butt[19]->callback(mesh_options_ok_cb);

      mesh_value[18] = new Fl_Value_Input(L + 2 * WB, 2 * WB + 6 * BH, IW, BH, "Smoothing threshold angle");
      mesh_value[18]->minimum(0.);
      mesh_value[18]->maximum(180.);
      mesh_value[18]->step(1.);
      mesh_value[18]->align(FL_ALIGN_RIGHT);
      mesh_value[18]->when(FL_WHEN_RELEASE);
      mesh_value[18]->callback(mesh_options_ok_cb);

      static Fl_Menu_Item menu_mesh_color[] = {
        {"By element type", 0, 0, 0},
        {"By elementary entity", 0, 0, 0},
        {"By physical group", 0, 0, 0},
        {"By mesh partition", 0, 0, 0},
        {0}
      };
      mesh_choice[4] = new Fl_Choice(L + 2 * WB, 2 * WB + 7 * BH, IW, BH, "Coloring mode");
      mesh_choice[4]->menu(menu_mesh_color);
      mesh_choice[4]->align(FL_ALIGN_RIGHT);
      mesh_choice[4]->callback(mesh_options_ok_cb);

      Fl_Scroll *s = new Fl_Scroll(L + 2 * WB, 3 * WB + 8 * BH, IW + 20, height - 5 * WB - 8 * BH);
      int i = 0;
      while(MeshOptions_Color[i].str) {
        mesh_col[i] = new Fl_Button(L + 2 * WB, 3 * WB + (8 + i) * BH, IW, BH, MeshOptions_Color[i].str);
        mesh_col[i]->callback(color_cb, (void *)MeshOptions_Color[i].function);
        i++;
      }
      s->end();

      o->end();
    }
    o->end();
  }
  mesh_group->end();

  // Solver options

  solver_group = new Fl_Group(L, 0, width, height, "Solver Options");
  solver_group->hide();
  {
    Fl_Tabs *o = new Fl_Tabs(L + WB, WB, width - 2 * WB, height - 2 * WB);
    {
      {
        Fl_Group *o = new Fl_Group(L + WB, WB + BH, width - 2 * WB, height - 2 * WB - BH, "General");

        solver_value[0] = new Fl_Value_Input(L + 2 * WB, 2 * WB + 1 * BH, IW, BH, "Maximum solver delay");
        solver_value[0]->minimum(0);
        solver_value[0]->maximum(10);
        solver_value[0]->step(1);
        solver_value[0]->align(FL_ALIGN_RIGHT);
        solver_value[0]->callback(solver_options_ok_cb);

        solver_input[0] = new Fl_Input(L + 2 * WB, 2 * WB + 2 * BH, IW, BH, "Socket name");
        solver_input[0]->align(FL_ALIGN_RIGHT);
        solver_input[0]->callback(solver_options_ok_cb);

        solver_butt[0] = new Fl_Check_Button(L + 2 * WB, 2 * WB + 3 * BH, BW, BH, "Always listen to incoming connection requests");
        solver_butt[0]->type(FL_TOGGLE_BUTTON);
        solver_butt[0]->callback(solver_options_ok_cb);
        
        o->end();
      }
    }
    o->end();
  }
  solver_group->end();

  // Post-processing options

  post_group = new Fl_Group(L, 0, width, height, "Post-processing Options");
  post_group->hide();
  {
    Fl_Tabs *o = new Fl_Tabs(L + WB, WB, width - 2 * WB, height - 2 * WB);
    {
      Fl_Group *o = new Fl_Group(L + WB, WB + BH, width - 2 * WB, height - 2 * WB - BH, "General");

      static Fl_Menu_Item menu_links[] = {
        {"None", 0, 0, 0},
        {"Apply next changes to all visible views", 0, 0, 0},
        {"Apply next changes to all views", 0, 0, 0},
        {"Force same options for all visible views", 0, 0, 0},
        {"Force same options for all views", 0, 0, 0},
        {0}
      };

      post_choice[0] = new Fl_Choice(L + 2 * WB, 2 * WB + 1 * BH, IW, BH, "View links");
      post_choice[0]->menu(menu_links);
      post_choice[0]->align(FL_ALIGN_RIGHT);
      post_choice[0]->callback(post_options_ok_cb);

      post_value[0] = new Fl_Value_Input(L + 2 * WB, 2 * WB + 2 * BH, IW, BH, "Animation delay");
      post_value[0]->minimum(0);
      post_value[0]->maximum(10);
      post_value[0]->step(0.01);
      post_value[0]->align(FL_ALIGN_RIGHT);
      post_value[0]->callback(post_options_ok_cb);

      post_butt[0] = new Fl_Check_Button(L + 2 * WB, 2 * WB + 3 * BH, BW, BH, "Cycle through views instead of time steps");
      post_butt[0]->type(FL_TOGGLE_BUTTON);
      post_butt[0]->callback(post_options_ok_cb);

      post_butt[1] = new Fl_Check_Button(L + 2 * WB, 2 * WB + 4 * BH, BW, BH, "Remove original views after combination");
      post_butt[1]->type(FL_TOGGLE_BUTTON);
      post_butt[1]->callback(post_options_ok_cb);

      post_butt[2] = new Fl_Check_Button(L + 2 * WB, 2 * WB + 5 * BH, BW, BH, "Draw value scales horizontally");
      post_butt[2]->type(FL_TOGGLE_BUTTON);
      post_butt[2]->callback(post_options_ok_cb);

      o->end();
    }
    o->end();
  }
  post_group->end();

  // View options

  view_number = -1;
  view_group = new Fl_Group(L, 0, width, height, "View Options");
  view_group->hide();
  {
    Fl_Tabs *o = new Fl_Tabs(L + WB, WB, width - 2 * WB, height - 2 * WB);
    {
      Fl_Group *o = new Fl_Group(L + WB, WB + BH, width - 2 * WB, height - 2 * WB - BH, "General");

      static Fl_Menu_Item menu_plot_type[] = {
        {"3D", 0, 0, 0},
        {"2D space", 0, 0, 0},
        {"2D time", 0, 0, 0},
        {0}
      };
      view_choice[13] = new Fl_Choice(L + 2 * WB, 2 * WB + 1 * BH, IW, BH, "Plot type");
      view_choice[13]->menu(menu_plot_type);
      view_choice[13]->align(FL_ALIGN_RIGHT);
      view_choice[13]->callback(view_options_ok_cb);

      view_input[0] = new Fl_Input(L + 2 * WB, 2 * WB + 2 * BH, IW, BH, "View name");
      view_input[0]->align(FL_ALIGN_RIGHT);
      view_input[0]->callback(view_options_ok_cb);

      int sw = (int)(1.5 * fontsize);
      view_butt_rep[0] = new Fl_Repeat_Button(L + 2 * WB, 2 * WB + 3 * BH, sw, BH, "-");
      view_butt_rep[0]->callback(view_options_timestep_decr_cb);
      view_butt_rep[1] = new Fl_Repeat_Button(L + 2 * WB + IW - sw, 2 * WB + 3 * BH, sw, BH, "+");
      view_butt_rep[1]->callback(view_options_timestep_incr_cb);
      view_value[50] = new Fl_Value_Input(L + 2 * WB + sw, 2 * WB + 3 * BH, IW - 2 * sw, BH);
      view_value[50]->callback(view_options_timestep_cb);
      view_value[50]->align(FL_ALIGN_RIGHT);
      view_value[50]->minimum(0);
      view_value[50]->maximum(0);
      view_value[50]->step(1);
      Fl_Box *a = new Fl_Box(L + 2 * WB + IW, 2 * WB + 3 * BH, IW / 2, BH, "Time step");
      a->box(FL_NO_BOX);
      a->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);

      view_range = new Fl_Group(L + 2 * WB, 2 * WB + 4 * BH, width - 4 * WB, 8 * BH);

      view_value[30] = new Fl_Value_Input(L + 2 * WB, 2 * WB + 4 * BH, IW, BH, "Intervals");
      view_value[30]->align(FL_ALIGN_RIGHT);
      view_value[30]->minimum(1);
      view_value[30]->maximum(256);
      view_value[30]->step(1);
      view_value[30]->when(FL_WHEN_RELEASE);
      view_value[30]->callback(view_options_ok_cb);
      
      static Fl_Menu_Item menu_iso[] = {
        {"Iso-values", 0, 0, 0},
        {"Continuous map", 0, 0, 0},
        {"Filled iso-values", 0, 0, 0},
        {"Numeric values", 0, 0, 0},
        {0}
      };
      view_choice[0] = new Fl_Choice(L + 2 * WB, 2 * WB + 5 * BH, IW, BH, "Intervals type");
      view_choice[0]->menu(menu_iso);
      view_choice[0]->align(FL_ALIGN_RIGHT);
      view_choice[0]->tooltip("(Alt+t)");
      view_choice[0]->callback(view_options_ok_cb);

      static Fl_Menu_Item menu_range[] = {
        {"Default", 0, 0, 0},
        {"Custom", 0, 0, 0},
        {"Per time step", 0, 0, 0},
        {0}
      };
      view_choice[7] = new Fl_Choice(L + 2 * WB, 2 * WB + 6 * BH, IW, BH, "Range mode");
      view_choice[7]->menu(menu_range);
      view_choice[7]->align(FL_ALIGN_RIGHT);
      view_choice[7]->tooltip("(Alt+r)");
      view_choice[7]->callback(view_options_ok_cb, (void*)"custom_range");

      int sw2 = (int)(2.5 * fontsize);
      view_push_butt[1] = new Fl_Button(L + 2 * WB, 2 * WB + 7 * BH, sw2, BH, "Min");
      view_push_butt[1]->callback(view_options_ok_cb, (void*)"range_min");
      view_value[31] = new Fl_Value_Input(L + 2 * WB + sw2, 2 * WB + 7 * BH, IW - sw2, BH, "Custom minimum");
      view_value[31]->align(FL_ALIGN_RIGHT);
      view_value[31]->when(FL_WHEN_RELEASE);
      view_value[31]->callback(view_options_ok_cb);

      view_push_butt[2] = new Fl_Button(L + 2 * WB, 2 * WB + 8 * BH, sw2, BH, "Max");
      view_push_butt[2]->callback(view_options_ok_cb, (void*)"range_max");
      view_value[32] = new Fl_Value_Input(L + 2 * WB + sw2, 2 * WB + 8 * BH, IW - sw2, BH, "Custom maximum");
      view_value[32]->align(FL_ALIGN_RIGHT);
      view_value[32]->when(FL_WHEN_RELEASE);
      view_value[32]->callback(view_options_ok_cb);

      view_butt[38] = new Fl_Check_Button(L + 2 * WB, 2 * WB + 9 * BH, BW, BH, "Saturate out-of-range values");
      view_butt[38]->type(FL_TOGGLE_BUTTON);
      view_butt[38]->callback(view_options_ok_cb);

      static Fl_Menu_Item menu_scale[] = {
        {"Linear", 0, 0, 0},
        {"Logarithmic", 0, 0, 0},
        {"Double logarithmic", 0, 0, 0},
        {0}
      };
      view_choice[1] = new Fl_Choice(L + 2 * WB, 2 * WB + 10 * BH, IW, BH, "Value scale mode");
      view_choice[1]->menu(menu_scale);
      view_choice[1]->align(FL_ALIGN_RIGHT);
      view_choice[1]->callback(view_options_ok_cb);

      view_input[1] = new Fl_Input(L + 2 * WB, 2 * WB + 11 * BH, IW, BH, "Number display format");
      view_input[1]->align(FL_ALIGN_RIGHT);
      view_input[1]->callback(view_options_ok_cb);

      view_range->end();

      o->end();
    }
    {
      Fl_Group *o = new Fl_Group(L + WB, WB + BH, width - 2 * WB, height - 2 * WB - BH, "Axes");
      o->hide();

      view_choice[8] = new Fl_Choice(L + 2 * WB, 2 * WB + 1 * BH, IW, BH, "Axes mode");
      view_choice[8]->menu(menu_axes_mode);
      view_choice[8]->align(FL_ALIGN_RIGHT);
      view_choice[8]->tooltip("(Alt+a)");
      view_choice[8]->callback(view_options_ok_cb, (void*)"view_axes");

      view_butt[3] = new Fl_Check_Button(L + width - (int)(0.85*IW) - 2 * WB, 2 * WB + 1 * BH, (int)(0.85*IW), BH, "Mikado style");
      view_butt[3]->type(FL_TOGGLE_BUTTON);
      view_butt[3]->callback(view_options_ok_cb);

      view_value[3] = new Fl_Value_Input(L + 2 * WB, 2 * WB + 2 * BH, IW/3, BH);
      view_value[3]->minimum(0.);
      view_value[3]->step(1);
      view_value[3]->maximum(100);
      view_value[3]->callback(view_options_ok_cb);
      view_value[4] = new Fl_Value_Input(L + 2 * WB + 1*IW/3, 2 * WB + 2 * BH, IW/3, BH);
      view_value[4]->minimum(0.);
      view_value[4]->step(1);
      view_value[4]->maximum(100);
      view_value[4]->callback(view_options_ok_cb);
      view_value[5] = new Fl_Value_Input(L + 2 * WB + 2*IW/3, 2 * WB + 2 * BH, IW/3, BH, "Axes tics");
      view_value[5]->minimum(0.);
      view_value[5]->step(1);
      view_value[5]->maximum(100);
      view_value[5]->align(FL_ALIGN_RIGHT);
      view_value[5]->callback(view_options_ok_cb);

      view_input[7] = new Fl_Input(L + 2 * WB, 2 * WB + 3 * BH, IW/3, BH);
      view_input[7]->callback(view_options_ok_cb);
      view_input[8] = new Fl_Input(L + 2 * WB + 1*IW/3, 2 * WB + 3 * BH, IW/3, BH);
      view_input[8]->callback(view_options_ok_cb);
      view_input[9] = new Fl_Input(L + 2 * WB + 2*IW/3, 2 * WB + 3 * BH, IW/3, BH, "Axes format");
      view_input[9]->align(FL_ALIGN_RIGHT);
      view_input[9]->callback(view_options_ok_cb);
      
      view_input[10] = new Fl_Input(L + 2 * WB, 2 * WB + 4 * BH, IW/3, BH);
      view_input[10]->callback(view_options_ok_cb);
      view_input[11] = new Fl_Input(L + 2 * WB + 1*IW/3, 2 * WB + 4 * BH, IW/3, BH);
      view_input[11]->callback(view_options_ok_cb);
      view_input[12] = new Fl_Input(L + 2 * WB + 2*IW/3, 2 * WB + 4 * BH, IW/3, BH, "Axes labels");
      view_input[12]->align(FL_ALIGN_RIGHT);
      view_input[12]->callback(view_options_ok_cb);
      
      view_butt[25] = new Fl_Check_Button(L + 2 * WB, 2 * WB + 5 * BH, BW, BH, "Position 3D axes automatically");
      view_butt[25]->type(FL_TOGGLE_BUTTON);
      view_butt[25]->callback(view_options_ok_cb, (void*)"view_axes_auto_3d");
      
      view_value[13] = new Fl_Value_Input(L + 2 * WB, 2 * WB + 6 * BH, IW / 3, BH);
      view_value[13]->callback(view_options_ok_cb);
      view_value[14] = new Fl_Value_Input(L + 2 * WB + IW / 3, 2 * WB + 6 * BH, IW / 3, BH);
      view_value[14]->callback(view_options_ok_cb);
      view_value[15] = new Fl_Value_Input(L + 2 * WB + 2 * IW / 3, 2 * WB + 6 * BH, IW / 3, BH, "3D axes minimum");
      view_value[15]->align(FL_ALIGN_RIGHT);
      view_value[15]->callback(view_options_ok_cb);

      view_value[16] = new Fl_Value_Input(L + 2 * WB, 2 * WB + 7 * BH, IW / 3, BH);
      view_value[16]->callback(view_options_ok_cb);
      view_value[17] = new Fl_Value_Input(L + 2 * WB + IW / 3, 2 * WB + 7 * BH, IW / 3, BH);
      view_value[17]->callback(view_options_ok_cb);
      view_value[18] = new Fl_Value_Input(L + 2 * WB + 2 * IW / 3, 2 * WB + 7 * BH, IW / 3, BH, "3D axes maximum");
      view_value[18]->align(FL_ALIGN_RIGHT);
      view_value[18]->callback(view_options_ok_cb);

      view_butt[7] = new Fl_Check_Button(L + 2 * WB, 2 * WB + 8 * BH, BW, BH, "Position 2D axes/value scale automatically");
      view_butt[7]->type(FL_TOGGLE_BUTTON);
      view_butt[7]->callback(view_options_ok_cb, (void*)"view_axes_auto_2d");
      
      view_value[20] = new Fl_Value_Input(L + 2 * WB, 2 * WB + 9 * BH, IW / 2, BH);
      view_value[20]->minimum(-2000);
      view_value[20]->maximum(2000);
      view_value[20]->step(1);
      view_value[20]->callback(view_options_ok_cb);
      view_value[21] = new Fl_Value_Input(L + 2 * WB + IW / 2, 2 * WB + 9 * BH, IW / 2, BH, "2D axes/value scale position");
      view_value[21]->align(FL_ALIGN_RIGHT);
      view_value[21]->minimum(-2000);
      view_value[21]->maximum(2000);
      view_value[21]->step(1);
      view_value[21]->callback(view_options_ok_cb);

      view_value[22] = new Fl_Value_Input(L + 2 * WB, 2 * WB + 10 * BH, IW / 2, BH);
      view_value[22]->minimum(0);
      view_value[22]->maximum(2000);
      view_value[22]->step(1);
      view_value[22]->callback(view_options_ok_cb);
      view_value[23] = new Fl_Value_Input(L + 2 * WB + IW / 2, 2 * WB + 10 * BH, IW / 2, BH, "2D axes/value scale size");
      view_value[23]->align(FL_ALIGN_RIGHT);
      view_value[23]->minimum(0);
      view_value[23]->maximum(2000);
      view_value[23]->step(1);
      view_value[23]->callback(view_options_ok_cb);

      o->end();
    }
    {
      Fl_Group *o = new Fl_Group(L + WB, WB + BH, width - 2 * WB, height - 2 * WB - BH, "Visibility");
      o->hide();

      view_butt[4] = new Fl_Check_Button(L + 2 * WB, 2 * WB + 1 * BH, BW, BH, "Show value scale");
      view_butt[4]->tooltip("(Alt+i)");
      view_butt[4]->type(FL_TOGGLE_BUTTON);
      view_butt[4]->callback(view_options_ok_cb);

      static Fl_Menu_Item time_display[] = {
        {"None", 0, 0, 0},
        {"Value if multi-step", 0, 0, 0},
        {"Value", 0, 0, 0},
        {"Step if multi-step", 0, 0, 0},
        {"Step", 0, 0, 0},
        {0}
      };
      view_choice[12] = new Fl_Choice(L + 2 * WB, 2 * WB + 2 * BH, IW, BH, "Time display mode");
      view_choice[12]->menu(time_display);
      view_choice[12]->align(FL_ALIGN_RIGHT);
      view_choice[12]->callback(view_options_ok_cb);

      view_butt[5] = new Fl_Check_Button(L + 2 * WB, 2 * WB + 3 * BH, BW, BH, "Show annotations");
      view_butt[5]->tooltip("(Alt+n)");
      view_butt[5]->type(FL_TOGGLE_BUTTON);
      view_butt[5]->callback(view_options_ok_cb);

      view_butt[10] = new Fl_Check_Button(L + 2 * WB, 2 * WB + 4 * BH, BW / 2, BH, "Draw element outlines");
      view_butt[10]->tooltip("(Alt+e)");
      view_butt[10]->type(FL_TOGGLE_BUTTON);
      view_butt[10]->callback(view_options_ok_cb);

      view_butt[2] = new Fl_Check_Button(L + 2 * WB + BW / 2, 2 * WB + 4 * BH, BW / 2, BH, "Draw 3D skin only");
      view_butt[2]->type(FL_TOGGLE_BUTTON);
      view_butt[2]->callback(view_options_ok_cb);

      static Fl_Menu_Item menu_view_element_types[] = {
        {"Points",      0, 0, 0, FL_MENU_TOGGLE},
        {"Lines",       0, 0, 0, FL_MENU_TOGGLE},
        {"Triangles",   0, 0, 0, FL_MENU_TOGGLE},
        {"Quadrangles", 0, 0, 0, FL_MENU_TOGGLE},
        {"Tetrahedra",  0, 0, 0, FL_MENU_TOGGLE},
        {"Hexahedra",   0, 0, 0, FL_MENU_TOGGLE},
        {"Prisms",      0, 0, 0, FL_MENU_TOGGLE},
        {"Pyramids",    0, 0, 0, FL_MENU_TOGGLE},
        {0}
      };

      view_menu_butt[1] = new Fl_Menu_Button(L + 2 * WB, 2 * WB + 5 * BH, IW, BH, "Elements");
      view_menu_butt[1]->menu(menu_view_element_types);
      view_menu_butt[1]->callback(view_options_ok_cb);
      
      static Fl_Menu_Item menu_boundary[] = {
        {"None", 0, 0, 0},
        {"Dimension - 1", 0, 0, 0},
        {"Dimension - 2", 0, 0, 0},
        {"Dimension - 3", 0, 0, 0},
        {0}
      };
      view_choice[9] = new Fl_Choice(L + 2 * WB, 2 * WB + 6 * BH, IW, BH, "Element boundary mode");
      view_choice[9]->menu(menu_boundary);
      view_choice[9]->align(FL_ALIGN_RIGHT);
      view_choice[9]->callback(view_options_ok_cb);

      view_value[0] = new Fl_Value_Input(L + 2 * WB, 2 * WB + 7 * BH, IW, BH, "Normals");
      view_value[0]->minimum(0);
      view_value[0]->maximum(500);
      view_value[0]->step(1);
      view_value[0]->align(FL_ALIGN_RIGHT);
      view_value[0]->when(FL_WHEN_RELEASE);
      view_value[0]->callback(view_options_ok_cb);

      view_value[1] = new Fl_Value_Input(L + 2 * WB, 2 * WB + 8 * BH, IW, BH, "Tangents");
      view_value[1]->minimum(0);
      view_value[1]->maximum(500);
      view_value[1]->step(1);
      view_value[1]->align(FL_ALIGN_RIGHT);
      view_value[1]->when(FL_WHEN_RELEASE);
      view_value[1]->callback(view_options_ok_cb);

      static Fl_Menu_Item menu_view_field_types[] = {
        {"Scalar", 0, 0, 0, FL_MENU_TOGGLE},
        {"Vector", 0, 0, 0, FL_MENU_TOGGLE},
        {"Tensor", 0, 0, 0, FL_MENU_TOGGLE},
        {0}
      };

      view_menu_butt[0] = new Fl_Menu_Button(L + 2 * WB, 2 * WB + 9 * BH, IW, BH, "Fields");
      view_menu_butt[0]->menu(menu_view_field_types);
      view_menu_butt[0]->callback(view_options_ok_cb);

      view_value[33] = new Fl_Value_Input(L + 2 * WB, 2 * WB + 10 * BH, IW, BH, "Maximum recursion level");
      view_value[33]->align(FL_ALIGN_RIGHT);
      view_value[33]->minimum(0);
      view_value[33]->maximum(8);
      view_value[33]->step(1);
      view_value[33]->when(FL_WHEN_RELEASE);
      view_value[33]->callback(view_options_ok_cb);

      view_value[34] = new Fl_Value_Input(L + 2 * WB, 2 * WB + 11 * BH, IW, BH, "Target error");
      view_value[34]->align(FL_ALIGN_RIGHT);
      view_value[34]->minimum(0.);
      view_value[34]->maximum(1.);
      view_value[34]->step(1.e-3);
      view_value[34]->when(FL_WHEN_RELEASE);
      view_value[34]->callback(view_options_ok_cb);

      o->end();
    }
    {
      Fl_Group *o = new Fl_Group(L + WB, WB + BH, width - 2 * WB, height - 2 * WB - BH, "Offset");
      o->hide();

      Fl_Box *b = new Fl_Box(FL_NO_BOX, L + 2 * WB, 2 * WB + 1 * BH, IW, BH, "Coordinate transformation:");
      b->align(FL_ALIGN_INSIDE|FL_ALIGN_LEFT);

      int ss = 2*IW/3/3+4;
      view_value[51] = new Fl_Value_Input(L + 2 * WB       , 2 * WB + 2 * BH, ss, BH);
      view_value[52] = new Fl_Value_Input(L + 2 * WB + ss  , 2 * WB + 2 * BH, ss, BH);
      view_value[53] = new Fl_Value_Input(L + 2 * WB + 2*ss, 2 * WB + 2 * BH, ss, BH, " X");
      view_value[40] = new Fl_Value_Input(L + 2 * WB + IW  , 2 * WB + 2 * BH, 7*IW/10, BH);

      view_value[54] = new Fl_Value_Input(L + 2 * WB       , 2 * WB + 3 * BH, ss, BH);
      view_value[55] = new Fl_Value_Input(L + 2 * WB + ss  , 2 * WB + 3 * BH, ss, BH);
      view_value[56] = new Fl_Value_Input(L + 2 * WB + 2*ss, 2 * WB + 3 * BH, ss, BH, " Y +");
      view_value[41] = new Fl_Value_Input(L + 2 * WB + IW  , 2 * WB + 3 * BH, 7*IW/10, BH);

      view_value[57] = new Fl_Value_Input(L + 2 * WB       , 2 * WB + 4 * BH, ss, BH);
      view_value[58] = new Fl_Value_Input(L + 2 * WB + ss  , 2 * WB + 4 * BH, ss, BH);
      view_value[59] = new Fl_Value_Input(L + 2 * WB + 2*ss, 2 * WB + 4 * BH, ss, BH, " Z");
      view_value[42] = new Fl_Value_Input(L + 2 * WB + IW  , 2 * WB + 4 * BH, 7*IW/10, BH);

      Fl_Box *b2 = new Fl_Box(FL_NO_BOX, L + 2 * WB + 2 * IW-3*WB, 2 * WB + 1 * BH, 7*IW/10, BH, "Raise:");
      b2->align(FL_ALIGN_INSIDE|FL_ALIGN_LEFT);

      view_value[43] = new Fl_Value_Input(L + 2 * WB + 2 * IW-3*WB, 2 * WB + 2 * BH, 7*IW/10, BH);
      view_value[44] = new Fl_Value_Input(L + 2 * WB + 2 * IW-3*WB, 2 * WB + 3 * BH, 7*IW/10, BH);
      view_value[45] = new Fl_Value_Input(L + 2 * WB + 2 * IW-3*WB, 2 * WB + 4 * BH, 7*IW/10, BH);

      view_value[46] = new Fl_Value_Input(L + 2 * WB, 2 * WB + 5 * BH, 3*ss, BH, "Normal raise");

      for(int i = 40; i <= 46; i++){
        view_value[i]->align(FL_ALIGN_RIGHT);
        view_value[i]->when(FL_WHEN_RELEASE);
        view_value[i]->callback(view_options_ok_cb);
      }
      for(int i = 51; i <= 59; i++){
        view_value[i]->minimum(-1.);
        view_value[i]->maximum(1.);
        view_value[i]->step(0.1);
        view_value[i]->align(FL_ALIGN_RIGHT);
        view_value[i]->when(FL_WHEN_RELEASE);
        view_value[i]->callback(view_options_ok_cb);
      }

      view_butt[6] = new Fl_Check_Button(L + 2 * WB, 2 * WB + 6 * BH, BW, BH, "Use general transformation expressions");
      view_butt[6]->type(FL_TOGGLE_BUTTON);
      view_butt[6]->callback(view_options_ok_cb, (void*)"general_transform");

      view_choice[11] = new Fl_Choice(L + 2 * WB, 2 * WB + 7 * BH, IW, BH, "Data source");
      view_choice[11]->align(FL_ALIGN_RIGHT);
      view_choice[11]->add("Self");
      view_choice[11]->callback(view_options_ok_cb);

      view_value[2] = new Fl_Value_Input(L + 2 * WB, 2 * WB + 8 * BH, IW, BH, "Factor");
      view_value[2]->align(FL_ALIGN_RIGHT);
      view_value[2]->when(FL_WHEN_RELEASE);
      view_value[2]->callback(view_options_ok_cb);

      view_input[4] = new Fl_Input(L + 2 * WB, 2 * WB + 9 * BH, IW, BH, "X expression");
      view_input[4]->align(FL_ALIGN_RIGHT);
      view_input[4]->callback(view_options_ok_cb);

      view_input[5] = new Fl_Input(L + 2 * WB, 2 * WB + 10 * BH, IW, BH, "Y expression");
      view_input[5]->align(FL_ALIGN_RIGHT);
      view_input[5]->callback(view_options_ok_cb);

      view_input[6] = new Fl_Input(L + 2 * WB, 2 * WB + 11 * BH, IW, BH, "Z expression");
      view_input[6]->align(FL_ALIGN_RIGHT);
      view_input[6]->callback(view_options_ok_cb);

      o->end();
    }
    {
      Fl_Group *o = new Fl_Group(L + WB, WB + BH, width - 2 * WB, height - 2 * WB - BH, "Aspect");
      o->hide();

      view_value[12] = new Fl_Value_Input(L + 2 * WB, 2 * WB + 1 * BH, IW, BH, "Element shrinking factor");
      view_value[12]->minimum(0.);
      view_value[12]->step(0.01);
      view_value[12]->maximum(1.);
      view_value[12]->align(FL_ALIGN_RIGHT);
      view_value[12]->when(FL_WHEN_RELEASE);
      view_value[12]->callback(view_options_ok_cb);

      view_choice[5] = new Fl_Choice(L + 2 * WB, 2 * WB + 2 * BH, IW, BH, "Point display");
      view_choice[5]->menu(menu_point_display_post);
      view_choice[5]->align(FL_ALIGN_RIGHT);
      view_choice[5]->callback(view_options_ok_cb);

      view_value[61] = new Fl_Value_Input(L + 2 * WB, 2 * WB + 3 * BH, IW, BH, "Point size");
      view_value[61]->minimum(0.1);
      view_value[61]->maximum(50);
      view_value[61]->step(0.1);
      view_value[61]->align(FL_ALIGN_RIGHT);
      view_value[61]->callback(view_options_ok_cb);

      view_choice[6] = new Fl_Choice(L + 2 * WB, 2 * WB + 4 * BH, IW, BH, "Line display");
      view_choice[6]->menu(menu_line_display_post);
      view_choice[6]->align(FL_ALIGN_RIGHT);
      view_choice[6]->callback(view_options_ok_cb);

      view_butt[26] = new Fl_Check_Button(L + width - (int)(1.15*BB) - 2 * WB, 2 * WB + 4 * BH, (int)(1.15*BB), BH, "Stipple in 2D");
      view_butt[26]->type(FL_TOGGLE_BUTTON);
      view_butt[26]->callback(view_options_ok_cb);

      view_value[62] = new Fl_Value_Input(L + 2 * WB, 2 * WB + 5 * BH, IW, BH, "Line width");
      view_value[62]->minimum(0.1);
      view_value[62]->maximum(50);
      view_value[62]->step(0.1);
      view_value[62]->align(FL_ALIGN_RIGHT);
      view_value[62]->callback(view_options_ok_cb);

      {
        view_vector = new Fl_Group(L + 2 * WB, 2 * WB + 6 * BH, width - 2 * WB, 4 * BH, 0);

        static Fl_Menu_Item menu_vectype[] = {
          {"Line", 0, 0, 0},
          {"Arrow", 0, 0, 0},
          {"Pyramid", 0, 0, 0},
          {"3D arrow", 0, 0, 0},
          {"Displacement", 0, 0, 0},
          {"Comet", 0, 0, 0},
          {0}
        };
        view_choice[2] = new Fl_Choice(L + 2 * WB, 2 * WB + 6 * BH, IW, BH, "Vector display");
        view_choice[2]->menu(menu_vectype);
        view_choice[2]->align(FL_ALIGN_RIGHT);
        view_choice[2]->callback(view_options_ok_cb);

        view_push_butt[0] = new Fl_Button(L + width - (int)(1.15*BB) - 2 * WB, 2 * WB + 6 * BH, (int)(1.15*BB), BH, "Edit arrow");
        view_push_butt[0]->callback(view_arrow_param_cb);

        view_value[60] = new Fl_Value_Input(L + 2 * WB, 2 * WB + 7 * BH, IW, BH, "Arrow size");
        view_value[60]->minimum(0);
        view_value[60]->maximum(500);
        view_value[60]->step(1);
        view_value[60]->align(FL_ALIGN_RIGHT);
        view_value[60]->callback(view_options_ok_cb);

        view_butt[0] = new Fl_Check_Button(L + width - (int)(1.15*BB) - 2 * WB, 2 * WB + 7 * BH, (int)(1.15*BB), BH, "Proportional");
        view_butt[0]->type(FL_TOGGLE_BUTTON);
        view_butt[0]->callback(view_options_ok_cb);

        view_value[63] = new Fl_Value_Input(L + 2 * WB, 2 * WB + 8 * BH, IW, BH, "Displacement factor");
        view_value[63]->minimum(0.);
        view_value[63]->maximum(1.);
        view_value[63]->step(0.01);
        view_value[63]->align(FL_ALIGN_RIGHT);
        view_value[63]->when(FL_WHEN_RELEASE);
        view_value[63]->callback(view_options_ok_cb);

        view_choice[10] = new Fl_Choice(L + 2 * WB, 2 * WB + 9 * BH, IW, BH, "Data source");
        view_choice[10]->align(FL_ALIGN_RIGHT);
        view_choice[10]->add("Self");
        view_choice[10]->callback(view_options_ok_cb);

        view_vector->end();
      }

      static Fl_Menu_Item menu_vecloc[] = {
        {"Barycenter", 0, 0, 0},
        {"Vertex", 0, 0, 0},
        {0}
      };
      view_choice[3] = new Fl_Choice(L + 2 * WB, 2 * WB + 10 * BH, IW, BH, "Glyph location");
      view_choice[3]->menu(menu_vecloc);
      view_choice[3]->align(FL_ALIGN_RIGHT);
      view_choice[3]->callback(view_options_ok_cb);

      view_butt[1] = new Fl_Check_Button(L + width - (int)(1.15*BB) - 2 * WB, 2 * WB + 10 * BH, (int)(1.15*BB), BH, "Center glyph");
      view_butt[1]->type(FL_TOGGLE_BUTTON);
      view_butt[1]->callback(view_options_ok_cb);
      
      static Fl_Menu_Item menu_tensor[] = {
        {"Von-Mises", 0, 0, 0},
        {0}
      };
      view_choice[4] = new Fl_Choice(L + 2 * WB, 2 * WB + 11 * BH, IW, BH, "Tensor display");
      view_choice[4]->menu(menu_tensor);
      view_choice[4]->align(FL_ALIGN_RIGHT);
      view_choice[4]->callback(view_options_ok_cb);

      o->end();
    }
    {
      Fl_Group *o = new Fl_Group(L + WB, WB + BH, width - 2 * WB, height - 2 * WB - BH, "Color");
      o->hide();

      view_butt[11] = new Fl_Check_Button(L + 2 * WB, 2 * WB + 1 * BH, BW, BH, "Enable lighting");
      view_butt[11]->tooltip("(Alt+w)");
      view_butt[11]->type(FL_TOGGLE_BUTTON);
      view_butt[11]->callback(view_options_ok_cb, (void*)"view_light");

      view_butt[8] = new Fl_Check_Button(L + 2 * WB, 2 * WB + 2 * BH, BW, BH, "Enable lighting of lines");
      view_butt[8]->type(FL_TOGGLE_BUTTON);
      view_butt[8]->callback(view_options_ok_cb);

      view_butt[9] = new Fl_Check_Button(L + 2 * WB, 2 * WB + 3 * BH, BW, BH, "Use two-side lighting");
      view_butt[9]->type(FL_TOGGLE_BUTTON);
      view_butt[9]->callback(view_options_ok_cb);

      view_butt[12] = new Fl_Check_Button(L + 2 * WB, 2 * WB + 4 * BH, BW, BH, "Smooth normals");
      view_butt[12]->type(FL_TOGGLE_BUTTON);
      view_butt[12]->callback(view_options_ok_cb);

      view_value[10] = new Fl_Value_Input(L + 2 * WB, 2 * WB + 5 * BH, IW, BH, "Smoothing threshold angle");
      view_value[10]->minimum(0.);
      view_value[10]->step(1.);
      view_value[10]->maximum(180.);
      view_value[10]->align(FL_ALIGN_RIGHT);
      view_value[10]->when(FL_WHEN_RELEASE);
      view_value[10]->callback(view_options_ok_cb);

      view_butt[24] = new Fl_Check_Button(L + 2 * WB, 2 * WB + 6 * BH, BW, BH, "Use fake transparency mode");
      view_butt[24]->type(FL_TOGGLE_BUTTON);
      view_butt[24]->callback(view_options_ok_cb);

      Fl_Scroll *s = new Fl_Scroll(L + 2 * WB, 3 * WB + 7 * BH, IW + 20, height - 5 * WB - 7 * BH);
      int i = 0;
      while(ViewOptions_Color[i].str) {
        view_col[i] = new Fl_Button(L + 2 * WB, 3 * WB + (7 + i) * BH, IW, BH, ViewOptions_Color[i].str);
        view_col[i]->callback(view_color_cb, (void *)ViewOptions_Color[i].function);
        i++;
      }
      s->end();

      o->end();
    }
    {
      Fl_Group *o = new Fl_Group(L + WB, WB + BH, width - 2 * WB, height - 2 * WB - BH, "Map");
      o->hide();

      view_colorbar_window = new Colorbar_Window(L + 2 * WB, 2 * WB + BH, width - 4 * WB, height - 4 * WB - BH);
      view_colorbar_window->end();
      view_colorbar_window->callback(view_options_ok_cb);

      o->end();
    }
    o->end();
  }
  view_group->end();

  opt_window->position(CTX.opt_position[0], CTX.opt_position[1]);
  opt_window->end();
}

void GUI::update_view_window(int num)
{
  if(num < 0 || num >= (int)PView::list.size()) return;

  view_number = num;

  PView *view = PView::list[num];
  PViewData *data = view->getData();
  PViewOptions *opt = view->getOptions();

  double maxval = std::max(fabs(data->getMin()), fabs(data->getMax()));
  if(!maxval) maxval = 1.;
  double val1 = 10. * CTX.lc;
  double val2 = 2. * CTX.lc / maxval;

  opt_view_name(num, GMSH_GUI, NULL);
  opt_view_format(num, GMSH_GUI, NULL);
  opt_view_type(num, GMSH_GUI, 0);
  opt_view_show_scale(num, GMSH_GUI, 0);
  opt_view_draw_strings(num, GMSH_GUI, 0);

  opt_view_max_recursion_level(num, GMSH_GUI, 0);
  opt_view_target_error(num, GMSH_GUI, 0);
  if(data->isAdaptive()){
    view_value[33]->activate();
    view_value[34]->activate();
  }
  else{
    view_value[33]->deactivate();
    view_value[34]->deactivate();
  }

  if(data->getNumPoints() || data->getNumLines()){
    ((Fl_Menu_Item*)view_choice[13]->menu())[1].activate();
    ((Fl_Menu_Item*)view_choice[13]->menu())[2].activate();
  }
  else {
    ((Fl_Menu_Item*)view_choice[13]->menu())[1].deactivate();
    ((Fl_Menu_Item*)view_choice[13]->menu())[2].deactivate();
  }

  opt_view_auto_position(num, GMSH_GUI, 0);
  opt_view_position0(num, GMSH_GUI, 0);
  opt_view_position1(num, GMSH_GUI, 0);
  opt_view_size0(num, GMSH_GUI, 0);
  opt_view_size1(num, GMSH_GUI, 0);

  opt_view_axes(num, GMSH_GUI, 0);
  opt_view_axes_mikado(num, GMSH_GUI, 0);
  opt_view_axes_format0(num, GMSH_GUI, NULL);
  opt_view_axes_format1(num, GMSH_GUI, NULL);
  opt_view_axes_format2(num, GMSH_GUI, NULL);
  opt_view_axes_tics0(num, GMSH_GUI, 0);
  opt_view_axes_tics1(num, GMSH_GUI, 0);
  opt_view_axes_tics2(num, GMSH_GUI, 0);
  opt_view_axes_label0(num, GMSH_GUI, NULL);
  opt_view_axes_label1(num, GMSH_GUI, NULL);
  opt_view_axes_label2(num, GMSH_GUI, NULL);
  opt_view_axes_auto_position(num, GMSH_GUI, 0);
  opt_view_axes_xmin(num, GMSH_GUI, 0);
  opt_view_axes_xmax(num, GMSH_GUI, 0);
  opt_view_axes_ymin(num, GMSH_GUI, 0);
  opt_view_axes_ymax(num, GMSH_GUI, 0);
  opt_view_axes_zmin(num, GMSH_GUI, 0);
  opt_view_axes_zmax(num, GMSH_GUI, 0);
  for(int i = 13; i <= 18; i++){
    view_value[i]->step(CTX.lc/200.);
    view_value[i]->minimum(-CTX.lc);
    view_value[i]->maximum(CTX.lc);
  }

  if(data->getNumElements()) {
    view_range->activate();
    ((Fl_Menu_Item*)view_choice[13]->menu())[0].activate();
  }
  else {
    view_range->deactivate();
    ((Fl_Menu_Item*)view_choice[13]->menu())[0].deactivate();
  }
  opt_view_show_element(num, GMSH_GUI, 0);
  opt_view_draw_skin_only(num, GMSH_GUI, 0);
  opt_view_light(num, GMSH_GUI, 0);
  opt_view_light_two_side(num, GMSH_GUI, 0);
  opt_view_light_lines(num, GMSH_GUI, 0);
  opt_view_smooth_normals(num, GMSH_GUI, 0);
  opt_view_angle_smooth_normals(num, GMSH_GUI, 0);
  opt_view_boundary(num, GMSH_GUI, 0);
  opt_view_explode(num, GMSH_GUI, 0);
  opt_view_draw_points(num, GMSH_GUI, 0);
  opt_view_draw_lines(num, GMSH_GUI, 0);
  opt_view_draw_triangles(num, GMSH_GUI, 0);
  opt_view_draw_quadrangles(num, GMSH_GUI, 0);
  opt_view_draw_tetrahedra(num, GMSH_GUI, 0);
  opt_view_draw_hexahedra(num, GMSH_GUI, 0);
  opt_view_draw_prisms(num, GMSH_GUI, 0);
  opt_view_draw_pyramids(num, GMSH_GUI, 0);
  opt_view_draw_scalars(num, GMSH_GUI, 0);
  opt_view_draw_vectors(num, GMSH_GUI, 0);
  opt_view_draw_tensors(num, GMSH_GUI, 0);
  opt_view_normals(num, GMSH_GUI, 0);
  opt_view_tangents(num, GMSH_GUI, 0);

  opt_view_nb_iso(num, GMSH_GUI, 0);
  opt_view_intervals_type(num, GMSH_GUI, 0);
  opt_view_range_type(num, GMSH_GUI, 0);
  opt_view_custom_min(num, GMSH_GUI, 0);
  opt_view_custom_max(num, GMSH_GUI, 0);
  opt_view_scale_type(num, GMSH_GUI, 0);
  opt_view_saturate_values(num, GMSH_GUI, 0);

  opt_view_offset0(num, GMSH_GUI, 0);
  opt_view_offset1(num, GMSH_GUI, 0);
  opt_view_offset2(num, GMSH_GUI, 0);
  for(int i = 40; i <= 42; i++) {
    view_value[i]->step(val1/100.);
    view_value[i]->minimum(-val1);
    view_value[i]->maximum(val1);
  }
  opt_view_transform00(num, GMSH_GUI, 0);
  opt_view_transform01(num, GMSH_GUI, 0);
  opt_view_transform02(num, GMSH_GUI, 0);
  opt_view_transform10(num, GMSH_GUI, 0);
  opt_view_transform11(num, GMSH_GUI, 0);
  opt_view_transform12(num, GMSH_GUI, 0);
  opt_view_transform20(num, GMSH_GUI, 0);
  opt_view_transform21(num, GMSH_GUI, 0);
  opt_view_transform22(num, GMSH_GUI, 0);
  opt_view_raise0(num, GMSH_GUI, 0);
  opt_view_raise1(num, GMSH_GUI, 0);
  opt_view_raise2(num, GMSH_GUI, 0);
  opt_view_normal_raise(num, GMSH_GUI, 0);
  for(int i = 43; i <= 46; i++) {
    view_value[i]->step(val2/100.);
    view_value[i]->minimum(-val2);
    view_value[i]->maximum(val2);
  }
  opt_view_use_gen_raise(num, GMSH_GUI, 0);
  opt_view_gen_raise_view(num, GMSH_GUI, 0);
  opt_view_gen_raise_factor(num, GMSH_GUI, 0);
  opt_view_gen_raise0(num, GMSH_GUI, 0);
  opt_view_gen_raise1(num, GMSH_GUI, 0);
  opt_view_gen_raise2(num, GMSH_GUI, 0);
  view_value[2]->step(val2/100.);
  view_value[2]->minimum(-val2);
  view_value[2]->maximum(val2);

  if(data->getNumTimeSteps() == 1) {
    view_value[50]->deactivate();
    view_butt_rep[0]->deactivate();
    view_butt_rep[1]->deactivate();
  }
  else {
    view_value[50]->activate();
    view_butt_rep[0]->activate();
    view_butt_rep[1]->activate();
  }
  view_value[50]->maximum(data->getNumTimeSteps() - 1);
  opt_view_timestep(num, GMSH_GUI, 0);
  opt_view_show_time(num, GMSH_GUI, 0);

  if(data->getNumVectors() || data->getNumTensors())
    view_vector->activate();
  else
    view_vector->deactivate();

  opt_view_point_size(num, GMSH_GUI, 0);
  opt_view_point_type(num, GMSH_GUI, 0);
  opt_view_line_width(num, GMSH_GUI, 0);
  opt_view_line_type(num, GMSH_GUI, 0);
  opt_view_vector_type(num, GMSH_GUI, 0);
  opt_view_arrow_size(num, GMSH_GUI, 0);
  opt_view_arrow_size_proportional(num, GMSH_GUI, 0);

  opt_view_displacement_factor(num, GMSH_GUI, 0);
  double val3 = 2. * CTX.lc / maxval;
  view_value[63]->step(val3/100.);
  view_value[63]->maximum(val3);

  opt_view_external_view(num, GMSH_GUI, 0);
  opt_view_glyph_location(num, GMSH_GUI, 0);
  opt_view_center_glyphs(num, GMSH_GUI, 0);
  opt_view_tensor_type(num, GMSH_GUI, 0);

  opt_view_fake_transparency(num, GMSH_GUI, 0);
  opt_view_use_stipple(num, GMSH_GUI, 0);
  opt_view_color_points(num, GMSH_GUI, 0);
  opt_view_color_lines(num, GMSH_GUI, 0);
  opt_view_color_triangles(num, GMSH_GUI, 0);
  opt_view_color_quadrangles(num, GMSH_GUI, 0);
  opt_view_color_tetrahedra(num, GMSH_GUI, 0);
  opt_view_color_hexahedra(num, GMSH_GUI, 0);
  opt_view_color_prisms(num, GMSH_GUI, 0);
  opt_view_color_pyramids(num, GMSH_GUI, 0);
  opt_view_color_tangents(num, GMSH_GUI, 0);
  opt_view_color_normals(num, GMSH_GUI, 0);
  opt_view_color_text2d(num, GMSH_GUI, 0);
  opt_view_color_text3d(num, GMSH_GUI, 0);
  opt_view_color_axes(num, GMSH_GUI, 0);

  view_colorbar_window->update(data->getName().c_str(), data->getMin(), 
                               data->getMax(), &opt->CT, &view->getChanged());
}

// Create the plugin manager window

void GUI::create_plugin_dialog_box(GMSH_Plugin *p, int x, int y, int width, int height)
{
  p->dialogBox = new PluginDialogBox;
  p->dialogBox->group = new Fl_Group(x, y, width, height);

  {
    Fl_Tabs *o = new Fl_Tabs(x, y, width, height);
    {
      Fl_Group *g = new Fl_Group(x, y + BH, width, height - BH, "Options");
      Fl_Scroll *s = new Fl_Scroll(x + WB, y + WB + BH, width - 2 * WB, height - BH - 2 * WB);

      int m = p->getNbOptionsStr();
      if(m > MAX_PLUGIN_OPTIONS) m = MAX_PLUGIN_OPTIONS;

      int n = p->getNbOptions();
      if(n > MAX_PLUGIN_OPTIONS) n = MAX_PLUGIN_OPTIONS;

      int k = 0;
      for(int i = 0; i < m; i++) {
        StringXString *sxs = p->getOptionStr(i);
        p->dialogBox->input[i] = new Fl_Input(x + WB, y + WB + (k + 1) * BH, IW, BH, sxs->str);
        p->dialogBox->input[i]->align(FL_ALIGN_RIGHT);
        p->dialogBox->input[i]->value(sxs->def);
        k++;
      }
      for(int i = 0; i < n; i++) {
        StringXNumber *sxn = p->getOption(i);
        p->dialogBox->value[i] = new Fl_Value_Input(x + WB, y + WB + (k + 1) * BH, IW, BH, sxn->str);
        p->dialogBox->value[i]->align(FL_ALIGN_RIGHT);
        p->dialogBox->value[i]->value(sxn->def);
        k++;
      }

      s->end();
      g->end();
      o->resizable(g); // to avoid ugly resizing of tab labels
    }
    {
      Fl_Group *g = new Fl_Group(x, y + BH, width, height - BH, "About");

      Fl_Browser *o = new Fl_Browser(x + WB, y + WB + BH, width - 2 * WB, height - 2 * WB - BH);

      char name[1024], copyright[256], author[256], help[4096];
      p->getName(name);
      p->getInfos(author, copyright, help);

      o->add(" ");
      add_multiline_in_browser(o, "@c@b@.", name);
      o->add(" ");
      add_multiline_in_browser(o, "", help);
      o->add(" ");
      add_multiline_in_browser(o, "Author: ", author);
      add_multiline_in_browser(o, "Copyright (C) ", copyright);
      o->add(" ");

      g->end();
    }
    o->end();
  }

  p->dialogBox->group->end();
  p->dialogBox->group->hide();
}

void GUI::reset_plugin_view_browser()
{
  // save selected state
  std::vector<int> state;
  for(int i = 0; i < plugin_view_browser->size(); i++){
    if(plugin_view_browser->selected(i + 1))
      state.push_back(1);
    else
      state.push_back(0);
  }

  char str[128];
  plugin_view_browser->clear();

  if(PView::list.size()){
    plugin_view_browser->activate();
    for(unsigned int i = 0; i < PView::list.size(); i++) {
      sprintf(str, "View [%d]", i);
      plugin_view_browser->add(str);
    }
    for(int i = 0; i < plugin_view_browser->size(); i++){
      if(i < (int)state.size() && state[i])
        plugin_view_browser->select(i + 1);
    }
  }
  else{
    plugin_view_browser->add("No Views");
    plugin_view_browser->deactivate();
  }

  view_plugin_browser_cb(NULL, NULL);
}

void GUI::create_plugin_window(int numview)
{
  int width0 = 34 * fontsize + WB;
  int height0 = 13 * BH + 5 * WB;

  int width = (CTX.plugin_size[0] < width0) ? width0 : CTX.plugin_size[0];
  int height = (CTX.plugin_size[1] < height0) ? height0 : CTX.plugin_size[1];

  if(plugin_window) {
    reset_plugin_view_browser();
    if(numview >= 0 && numview < (int)PView::list.size()){
      plugin_view_browser->deselect();
      plugin_view_browser->select(numview + 1);
      view_plugin_browser_cb(NULL, NULL);
    }
    plugin_window->show();
    return;
  }

  plugin_window = new Dialog_Window(width, height, CTX.non_modal_windows, "Plugins");
  plugin_window->box(GMSH_WINDOW_BOX);

  {
    Fl_Button *o = new Fl_Button(width - BB - WB, height - BH - WB, BB, BH, "Cancel");
    o->callback(view_plugin_cancel_cb);
  }
  {
    plugin_run = new Fl_Return_Button(width - 2 * BB - 2 * WB, height - BH - WB, BB, BH, "Run");
    plugin_run->callback(view_plugin_run_cb);
    plugin_run->deactivate();
  }

  int L1 = (int)(0.3 * width), L2 = (int)(0.6 * L1);
  plugin_browser = new Fl_Hold_Browser(WB, WB, L1, height - 3 * WB - BH);
  plugin_browser->callback(view_plugin_browser_cb);

  plugin_view_browser = new Fl_Multi_Browser(WB + L1, WB, L2, height - 3 * WB - BH);
  plugin_view_browser->has_scrollbar(Fl_Browser_::VERTICAL);
  plugin_view_browser->callback(view_plugin_browser_cb);

  for(GMSH_PluginManager::iter it = GMSH_PluginManager::instance()->begin();
      it != GMSH_PluginManager::instance()->end(); ++it) {
    GMSH_Plugin *p = (*it).second;
    if(p->getType() == GMSH_Plugin::GMSH_POST_PLUGIN) {
      char name[256];
      p->getName(name);
      plugin_browser->add(name, p);
      create_plugin_dialog_box(p, 2 * WB + L1 + L2, WB, width - L1 - L2 - 3 * WB, height - 3 * WB - BH);
      // select first plugin by default
      if(it == GMSH_PluginManager::instance()->begin()){
        plugin_browser->select(1);
        p->dialogBox->group->show();
      }
    }
  }

  Dummy_Box *resize_box = new Dummy_Box(3*WB + L1+L2, WB, WB, height - 3 * WB - BH);
  plugin_window->resizable(resize_box);
  plugin_window->size_range(width0, height0);

  plugin_window->position(CTX.plugin_position[0], CTX.plugin_position[1]);
  plugin_window->end();
}

void FieldDialogBox::save_values()
{
  std::list<Fl_Widget*>::iterator input=inputs.begin();
  Field *f=current_field;
  std::ostringstream sstream;
  std::istringstream istream;
  int i;
  char a;
  sstream.precision(16);
  for(std::map<std::string,FieldOption*>::iterator it=f->options.begin();it!=f->options.end();it++){
    FieldOption *option=it->second;
    sstream.str("");
    switch(option->get_type()){
    case FIELD_OPTION_STRING:
    case FIELD_OPTION_PATH:
      sstream<<"\""<<((Fl_Input*)*input)->value()<<"\"";
      break;
    case FIELD_OPTION_INT:
      sstream<<(int)((Fl_Value_Input*)*input)->value();
      break;
    case FIELD_OPTION_DOUBLE:
      sstream<<((Fl_Value_Input*)*input)->value();
      break;
    case FIELD_OPTION_BOOL:
      sstream<<(bool)((Fl_Check_Button*)*input)->value();
      break;
    case FIELD_OPTION_LIST:
      sstream<<"{";
      istream.str(((Fl_Input*)*input)->value());
      while(istream>>i){
	sstream<<i;
	if(istream>>a){
	  if(a!=',')
	    Msg::Error("Unexpected character \'%c\' while parsing option '%s' of field \'%s\'",a,it->first.c_str(),f->id);
	  sstream<<", ";
	}
      }
      sstream<<"}";
      break;
    }
    if((*input)->changed()){
      add_field_option(f->id,it->first.c_str(),sstream.str().c_str(),CTX.filename);
      (*input)->clear_changed();
    }
    input++;
  }
}

void FieldDialogBox::load_field(Field *f)
{
  current_field=f;
  std::list<Fl_Widget*>::iterator input=inputs.begin();
  for(std::map<std::string,FieldOption*>::iterator it=f->options.begin();it!=f->options.end();it++){
    FieldOption *option=it->second;
    std::ostringstream vstr;
    std::list<int>::iterator list_it;;
    switch(option->get_type()){
    case FIELD_OPTION_STRING:
    case FIELD_OPTION_PATH:
      ((Fl_Input*)(*input))->value(option->string().c_str());
      break;
    case FIELD_OPTION_INT:
    case FIELD_OPTION_DOUBLE:
      ((Fl_Value_Input*)(*input))->value(option->numerical_value());
      break;
    case FIELD_OPTION_BOOL:
      ((Fl_Check_Button*)(*input))->value((int)option->numerical_value());
      break;
    case FIELD_OPTION_LIST:
      vstr.str("");
      for(list_it=option->list().begin();list_it!=option->list().end();list_it++){
	if(list_it!=option->list().begin())
	  vstr<<", ";
	vstr<<*list_it;
      }
      ((Fl_Input*)(*input))->value(vstr.str().c_str());
      break;
    }
    input++;
  }
  if(PView::list.size()){
    put_on_view_btn->activate();
    for(unsigned int i = 0; i < PView::list.size(); i++) {
      std::ostringstream s;
      s<<"View ["<<i<<"]";
      put_on_view_btn->add(s.str().c_str());
    }
  }
  else{
    put_on_view_btn->deactivate();
  }
  set_size_btn->value(GModel::current()->getFields()->background_field==f->id);
}

FieldDialogBox::FieldDialogBox(Field *f, int x, int y, int width, int height,int fontsize)
{
  current_field=NULL;
  group=new Fl_Group(x, y, width, height);
  {
    Fl_Box *b = new Fl_Box(x, y, width, BH,f->get_name());
    b->labelfont(FL_BOLD);
    Fl_Tabs *o = new Fl_Tabs(x, y + BH + WB, width, height-2*BH-2*WB);
    group->resizable(o);
    {
      Fl_Group *g = new Fl_Group(x, y + 2*BH + WB, width, height - 2*BH-3*WB, "Options");
      apply_btn = new Fl_Return_Button(x+width - BB-WB ,y+ height - 2*BH -2*WB, BB, BH, "Apply");
      apply_btn->callback(view_field_apply_cb,this);
      revert_btn = new Fl_Button(x+width - 2*BB-2*WB ,y+ height - 2*BH -2*WB, BB, BH, "Revert");
      revert_btn->callback(view_field_revert_cb,this);
      Fl_Scroll *s = new Fl_Scroll(x + WB, y + 2*WB + 2*BH, width - 2 * WB, height - 4*BH - 5 * WB);
      int yy=y+3*WB+2*BH;
      for(std::map<std::string,FieldOption*>::iterator it=f->options.begin();it!=f->options.end();it++){
	Fl_Widget *input;
	switch(it->second->get_type()){
	case FIELD_OPTION_INT:
	case FIELD_OPTION_DOUBLE:
	  input=new Fl_Value_Input(x+WB,yy,IW,BH,it->first.c_str());
	  break;
	case FIELD_OPTION_BOOL:
	  input=new Fl_Check_Button(x+WB,yy,BH,BH,it->first.c_str());
	  break;
	case FIELD_OPTION_PATH:
	case FIELD_OPTION_STRING:
	  input=new Fl_Input(x+WB,yy,IW,BH,it->first.c_str());
	  break;
	case FIELD_OPTION_LIST:
	default:
	  /*{
	    Fl_Button *b=new Fl_Button(x+WB,yy,BH,BH);
	    b->label("@+");
	    b->callback(view_field_select_node_cb);
	    }
	    input=new Fl_Input(x+WB+2*BH,yy,IW-2*BH,BH,it->first.c_str());*/
	  input=new Fl_Input(x+WB,yy,IW,BH,it->first.c_str());
	  break;
	}
	input->align(FL_ALIGN_RIGHT);
	inputs.push_back(input);
	yy+=WB+BH;
      }
      o->resizable(g); // to avoid ugly resizing of tab labels
      g->resizable(s);
      s->end();
      g->end();
    }
    {
      Fl_Group *g = new Fl_Group(x, y + 2*BH + WB, width, height - 2*BH-3*WB, "Help");
      Fl_Browser *o = new Fl_Browser(x + WB, y + 2*WB + 2*BH, width - 2 * WB, height - 4 * WB - 3 * BH);
      
      //    char name[1024], copyright[256], author[256], help[4096];
      //    p->getName(name);
      //    p->getInfos(author, copyright, help);
      
      o->add(" ");
      //   add_multiline_in_browser(o, "@c@b@.", name);
      o->add(" ");
      //  add_multiline_in_browser(o, "", help);
      o->add(" ");
      //add_multiline_in_browser(o, "Author: ", author);
      //add_multiline_in_browser(o, "Copyright (C) ", copyright);
      o->add(" ");
      
      g->end();
    }
    o->end();
  }
  {
    Fl_Button *b = new Fl_Button(x+width - BB,y+ height - BH , BB, BH, "Delete");
    b->callback(view_field_delete_cb,this);
  }
  put_on_view_btn = new Fl_Menu_Button(x+width - BB-(int)(1.25*BB)-WB,y+ height - BH ,(int)(1.25*BB),BH,"Put on view");
  put_on_view_btn->callback(view_field_put_on_view_cb,this);
  
  set_size_btn = new Fl_Check_Button(x,y+ height - BH, (int)(1.3*BB),BH,"Background size");
  set_size_btn->callback(view_field_set_size_btn_cb,this);
  
  group->end();
  group->hide();
}

void GUI::create_field_window(int numfield)
{
  int width0 = 34 * fontsize + WB;
  int height0 = 13 * BH + 5 * WB;
  
  int width = (CTX.field_size[0] < width0) ? width0 : CTX.field_size[0];
  int height = (CTX.field_size[1] < height0) ? height0 : CTX.field_size[1];
  
  int L1 = BB;
  int i_entry=1;
  if(field_window) {
    width=field_window->w();
    height=field_window->h();
    FieldManager &fields=*GModel::current()->getFields();
    field_browser->clear();
    for(FieldManager::iterator it=fields.begin();it!=fields.end();it++){
      Field *field=it->second;
      std::ostringstream sstream;
      if(it->first==fields.background_field)
	sstream<<"*";
      sstream<<it->first;
      sstream<<" "<<field->get_name();
      field_browser->add(sstream.str().c_str(),field);
      if(!field->dialog_box()){
	field_window->begin();
	field->dialog_box()=new FieldDialogBox(field, 2 * WB + L1 , WB, width - L1 - 3 * WB, height - 2*WB  ,fontsize);
	field_window->end();
      }
      if(it->second->id==numfield){
	field_browser->select(i_entry);
	field_browser->do_callback();
      }
      i_entry++;
    }
    field_window->show();
    return;
  }
  
  selected_field_dialog_box=NULL;
  field_window = new Dialog_Window(width, height, CTX.non_modal_windows, "Fields");
  field_window->box(GMSH_WINDOW_BOX);
  Fl_Group *resize_box = new Fl_Group(2*WB+L1, 2*WB+BB,width-3*WB-L1, height - 3 * WB-BB);
  resize_box->end();
  {
    Fl_Menu_Button *b= new Fl_Menu_Button(WB,WB,L1,BH,"New");
    FieldManager &fields=*GModel::current()->getFields();
    std::map<std::string, FieldFactory*>::iterator it;
    for(it=fields.map_type_name.begin();it!=fields.map_type_name.end();it++)
      b->add(it->first.c_str());
    b->callback(view_field_new_cb);
  }
  field_browser = new Fl_Hold_Browser(WB, 2*WB+BH, L1, height - 3 * WB - BH);
  field_browser->callback(view_field_browser_cb);
  field_window->resizable(resize_box);
  field_window->size_range(width0, height0);
  field_window->position(CTX.field_position[0], CTX.field_position[1]);
  field_window->end();
}

// Create the window for the statistics

void GUI::create_statistics_window()
{
  int i, num = 0;
  static Fl_Group *g[10];

  if(stat_window) {
    if(!stat_window->shown())
      set_statistics(false);
    for(i = 0; i < 3; i++)
      g[i]->hide();
    switch(get_context()){
    case 0: g[0]->show(); break; // geometry
    case 1: g[1]->show(); break; // mesh
    case 3: g[2]->show(); break; // post-pro
    default: g[1]->show(); break; // mesh
    }
    stat_window->show();
    return;
  }

  int width = 26 * fontsize;
  int height = 5 * WB + 17 * BH;

  stat_window = new Dialog_Window(width, height, CTX.non_modal_windows, "Statistics");
  stat_window->box(GMSH_WINDOW_BOX);
  {
    Fl_Tabs *o = new Fl_Tabs(WB, WB, width - 2 * WB, height - 3 * WB - BH);
    {
      g[0] = new Fl_Group(WB, WB + BH, width - 2 * WB, height - 3 * WB - 2 * BH, "Geometry");
      stat_value[num++] = new Fl_Output(2 * WB, 2 * WB + 1 * BH, IW, BH, "Points");
      stat_value[num++] = new Fl_Output(2 * WB, 2 * WB + 2 * BH, IW, BH, "Lines");
      stat_value[num++] = new Fl_Output(2 * WB, 2 * WB + 3 * BH, IW, BH, "Surfaces");
      stat_value[num++] = new Fl_Output(2 * WB, 2 * WB + 4 * BH, IW, BH, "Volumes");
      stat_value[num++] = new Fl_Output(2 * WB, 2 * WB + 5 * BH, IW, BH, "Physical groups");
      g[0]->end();
    }
    {
      g[1] = new Fl_Group(WB, WB + BH, width - 2 * WB, height - 3 * WB - 2 * BH, "Mesh");
      stat_value[num++] = new Fl_Output(2 * WB, 2 * WB + 1 * BH, IW, BH, "Nodes on Lines");
      stat_value[num++] = new Fl_Output(2 * WB, 2 * WB + 2 * BH, IW, BH, "Nodes on surfaces");
      stat_value[num++] = new Fl_Output(2 * WB, 2 * WB + 3 * BH, IW, BH, "Nodes in volumes");
      stat_value[num++] = new Fl_Output(2 * WB, 2 * WB + 4 * BH, IW, BH, "Triangles");
      stat_value[num++] = new Fl_Output(2 * WB, 2 * WB + 5 * BH, IW, BH, "Quadrangles");
      stat_value[num++] = new Fl_Output(2 * WB, 2 * WB + 6 * BH, IW, BH, "Tetrahedra");
      stat_value[num++] = new Fl_Output(2 * WB, 2 * WB + 7 * BH, IW, BH, "Hexahedra");
      stat_value[num++] = new Fl_Output(2 * WB, 2 * WB + 8 * BH, IW, BH, "Prisms");
      stat_value[num++] = new Fl_Output(2 * WB, 2 * WB + 9 * BH, IW, BH, "Pyramids");

      stat_value[num++] = new Fl_Output(2 * WB, 2 * WB + 10 * BH, IW, BH, "Time for 1D mesh");
      stat_value[num++] = new Fl_Output(2 * WB, 2 * WB + 11 * BH, IW, BH, "Time for 2D mesh");
      stat_value[num++] = new Fl_Output(2 * WB, 2 * WB + 12 * BH, IW, BH, "Time for 3D mesh");

      stat_value[num] = new Fl_Output(2 * WB, 2 * WB + 13 * BH, IW, BH, "Gamma"); 
      stat_value[num]->tooltip("~ inscribed_radius / circumscribed_radius"); num++;
      stat_value[num] = new Fl_Output(2 * WB, 2 * WB + 14 * BH, IW, BH, "Eta");
      stat_value[num]->tooltip("~ volume^(2/3) / sum_edge_length^2"); num++;
      stat_value[num] = new Fl_Output(2 * WB, 2 * WB + 15 * BH, IW, BH, "Rho");
      stat_value[num]->tooltip("~ min_edge_length / max_edge_length"); num++;

      stat_butt[0] = new Fl_Button(width - BB - 5 * WB, 2 * WB + 13 * BH, BB, BH, "Graph");
      stat_butt[0]->callback(statistics_histogram_cb, (void *)"Gamma");
      stat_butt[1] = new Fl_Button(width - BB - 5 * WB, 2 * WB + 14 * BH, BB, BH, "Graph");
      stat_butt[1]->callback(statistics_histogram_cb, (void *)"Eta");
      stat_butt[2] = new Fl_Button(width - BB - 5 * WB, 2 * WB + 15 * BH, BB, BH, "Graph");
      stat_butt[2]->callback(statistics_histogram_cb, (void *)"Rho");

      g[1]->end();
    }
    {
      g[2] = new Fl_Group(WB, WB + BH, width - 2 * WB, height - 3 * WB - 2 * BH, "Post-processing");
      stat_value[num++] = new Fl_Output(2 * WB, 2 * WB + 1 * BH, IW, BH, "Views");
      stat_value[num++] = new Fl_Output(2 * WB, 2 * WB + 2 * BH, IW, BH, "Points");
      stat_value[num++] = new Fl_Output(2 * WB, 2 * WB + 3 * BH, IW, BH, "Lines");
      stat_value[num++] = new Fl_Output(2 * WB, 2 * WB + 4 * BH, IW, BH, "Triangles");
      stat_value[num++] = new Fl_Output(2 * WB, 2 * WB + 5 * BH, IW, BH, "Quadrangles");
      stat_value[num++] = new Fl_Output(2 * WB, 2 * WB + 6 * BH, IW, BH, "Tetrahedra");
      stat_value[num++] = new Fl_Output(2 * WB, 2 * WB + 7 * BH, IW, BH, "Hexahedra");
      stat_value[num++] = new Fl_Output(2 * WB, 2 * WB + 8 * BH, IW, BH, "Prisms");
      stat_value[num++] = new Fl_Output(2 * WB, 2 * WB + 9 * BH, IW, BH, "Pyramids");
      stat_value[num++] = new Fl_Output(2 * WB, 2 * WB + 10 * BH, IW, BH, "Strings");
      g[2]->end();
    }
    o->end();
  }

  for(i = 0; i < num; i++) {
    stat_value[i]->align(FL_ALIGN_RIGHT);
    stat_value[i]->value(0);
  }

  {
    Fl_Return_Button *o = new Fl_Return_Button(width - 2 * BB - 2 * WB, height - BH - WB, BB, BH, "Update");
    o->callback(statistics_update_cb);
  }
  {
    Fl_Button *o = new Fl_Button(width - BB - WB, height - BH - WB, BB, BH, "Cancel");
    o->callback(cancel_cb, (void *)stat_window);
  }

  stat_window->position(CTX.stat_position[0], CTX.stat_position[1]);
  stat_window->end();
}

void GUI::set_statistics(bool compute_quality)
{
  int num = 0;
  static double s[50];
  static char label[50][256];

  if(compute_quality)
    GetStatistics(s, quality);
  else
    GetStatistics(s);

  // geom
  sprintf(label[num], "%g", s[0]); stat_value[num]->value(label[num]); num++;
  sprintf(label[num], "%g", s[1]); stat_value[num]->value(label[num]); num++;
  sprintf(label[num], "%g", s[2]); stat_value[num]->value(label[num]); num++;
  sprintf(label[num], "%g", s[3]); stat_value[num]->value(label[num]); num++;
  sprintf(label[num], "%g", s[45]); stat_value[num]->value(label[num]); num++;

  // mesh
  sprintf(label[num], "%g", s[4]); stat_value[num]->value(label[num]); num++;
  sprintf(label[num], "%g", s[5]); stat_value[num]->value(label[num]); num++;
  sprintf(label[num], "%g", s[6]); stat_value[num]->value(label[num]); num++;
  sprintf(label[num], "%g", s[7]); stat_value[num]->value(label[num]); num++;
  sprintf(label[num], "%g", s[8]); stat_value[num]->value(label[num]); num++;
  sprintf(label[num], "%g", s[9]); stat_value[num]->value(label[num]); num++;
  sprintf(label[num], "%g", s[10]); stat_value[num]->value(label[num]); num++;
  sprintf(label[num], "%g", s[11]); stat_value[num]->value(label[num]); num++;
  sprintf(label[num], "%g", s[12]); stat_value[num]->value(label[num]); num++;

  sprintf(label[num], "%g", s[13]); stat_value[num]->value(label[num]); num++;
  sprintf(label[num], "%g", s[14]); stat_value[num]->value(label[num]); num++;
  sprintf(label[num], "%g", s[15]); stat_value[num]->value(label[num]); num++;

  if(!compute_quality){
    for(int i = 0; i < 3; i++) stat_butt[i]->deactivate();
    sprintf(label[num], "Press Update");
    stat_value[num]->deactivate();
    stat_value[num]->value(label[num]); num++;
    sprintf(label[num], "Press Update");
    stat_value[num]->deactivate();
    stat_value[num]->value(label[num]); num++;
    sprintf(label[num], "Press Update");
    stat_value[num]->deactivate();
    stat_value[num]->value(label[num]); num++;
  }
  else{
    for(int i = 0; i < 3; i++) stat_butt[i]->activate();
    sprintf(label[num], "%.4g (%.4g->%.4g)", s[17], s[18], s[19]);
    stat_value[num]->activate();
    stat_value[num]->value(label[num]); num++;
    sprintf(label[num], "%.4g (%.4g->%.4g)", s[20], s[21], s[22]);
    stat_value[num]->activate();
    stat_value[num]->value(label[num]); num++;
    sprintf(label[num], "%.4g (%.4g->%.4g)", s[23], s[24], s[25]);
    stat_value[num]->activate();
    stat_value[num]->value(label[num]); num++;
  }

  // post
  sprintf(label[num], "%g", s[26]); stat_value[num]->value(label[num]); num++;
  sprintf(label[num], "%g", s[27]); stat_value[num]->value(label[num]); num++;
  sprintf(label[num], "%g", s[28]); stat_value[num]->value(label[num]); num++;
  sprintf(label[num], "%g", s[29]); stat_value[num]->value(label[num]); num++;
  sprintf(label[num], "%g", s[30]); stat_value[num]->value(label[num]); num++;
  sprintf(label[num], "%g", s[31]); stat_value[num]->value(label[num]); num++;
  sprintf(label[num], "%g", s[32]); stat_value[num]->value(label[num]); num++;
  sprintf(label[num], "%g", s[33]); stat_value[num]->value(label[num]); num++;
  sprintf(label[num], "%g", s[34]); stat_value[num]->value(label[num]); num++;
  sprintf(label[num], "%g", s[35]); stat_value[num]->value(label[num]); num++;
}


// Create the window for the messages

void GUI::create_message_window(bool redraw_only)
{

  if(msg_window) {
    if(msg_window->shown() && redraw_only)
      msg_window->redraw();
    else
      msg_window->show();
    return;
  }

  int width = CTX.msg_size[0];
  int height = CTX.msg_size[1];

  msg_window = new Dialog_Window(width, height, CTX.non_modal_windows, "Message Console");
  msg_window->box(GMSH_WINDOW_BOX);

  msg_browser = new Fl_Browser(0, 0, width, height - 2 * WB - BH);
  msg_browser->box(FL_FLAT_BOX);
  msg_browser->textfont(FL_COURIER);
  msg_browser->textsize(fontsize - 1);
  msg_browser->type(FL_MULTI_BROWSER);
  msg_browser->callback(message_copy_cb);

  {
    Fl_Return_Button *o = new Fl_Return_Button(width - 3 * BB - 3 * WB, height - BH - WB, BB, BH, "Clear");
    o->callback(message_clear_cb);
  }
  {
    Fl_Button *o = new Fl_Button(width - 2 * BB - 2 * WB, height - BH - WB, BB, BH, "Save");
    o->callback(message_save_cb);
  }
  {
    Fl_Button *o = new Fl_Button(width - BB - WB, height - BH - WB, BB, BH, "Cancel");
    o->callback(cancel_cb, (void *)msg_window);
  }

  msg_window->resizable(new Fl_Box(WB, WB, 100, 10));
  msg_window->size_range(WB + 100 + 3 * BB + 4 * WB, 100);

  msg_window->position(CTX.msg_position[0], CTX.msg_position[1]);
  msg_window->end();
}

void GUI::add_message(const char *msg)
{
  msg_browser->add(msg, 0);
  msg_browser->bottomline(msg_browser->size());
}

void GUI::save_message(const char *filename)
{
  FILE *fp;

  if(!(fp = fopen(filename, "w"))) {
    Msg::Error("Unable to open file '%s'", filename);
    return;
  }

  Msg::StatusBar(2, true, "Writing '%s'", filename);
  for(int i = 1; i <= msg_browser->size(); i++) {
    const char *c = msg_browser->text(i);
    if(c[0] == '@')
      fprintf(fp, "%s\n", &c[5]);
    else
      fprintf(fp, "%s\n", c);
  }
  Msg::StatusBar(2, true, "Wrote '%s'", filename);
  fclose(fp);
}

void GUI::fatal_error(const char *filename)
{
  fl_alert("A fatal error has occurred which will force Gmsh to abort.\n"
           "The error messages have been saved in the following file:\n\n%s",
           filename);
}

// Create the visibility window

void GUI::reset_visibility()
{
  if(vis_window) {
    vis_browser->clear();
    if(vis_window->shown())
      visibility_cb(NULL, NULL);
  }
}

class Vis_Browser : public Fl_Browser{
  // special browser that reacts differently to Enter key
  int handle(int event)
  {
    if(event == FL_KEYBOARD){
      switch(Fl::event_key()) {
      case FL_Enter:
      case FL_KP_Enter:
        visibility_ok_cb(NULL, NULL);
        return 1;
      }
    }
    return Fl_Browser::handle(event);
  }
public:
  Vis_Browser(int x, int y, int w , int h, const char* c = 0)
    : Fl_Browser(x, y, w, h, c){}
};

void GUI::create_visibility_window(bool redraw_only)
{
  if(vis_window) {
    if(vis_window->shown() && redraw_only)
      vis_window->redraw();
    else
      vis_window->show();
    return;
  }

  static int cols[5] = { 15, 95, 95, 180, 0 };
  static Fl_Menu_Item type_table[] = {
    {"Elementary entities", 0, (Fl_Callback *) visibility_cb},
    {"Physical groups", 0, (Fl_Callback *) visibility_cb},
    {"Mesh partitions", 0, (Fl_Callback *) visibility_cb},
    {0}
  };

  int width = cols[0] + cols[1] + cols[2] + cols[3] + 6 * WB;
  int height = 18 * BH;
  int brw = width - 4 * WB;

  vis_window = new Dialog_Window(width, height, CTX.non_modal_windows, "Visibility");
  vis_window->box(GMSH_WINDOW_BOX);

  vis_type = new Fl_Choice(WB, WB, (width - 3 * WB) / 2, BH);
  vis_type->menu(type_table);
  
  vis_butt[0] = new Fl_Check_Button(WB + (width - 3 * WB) / 2 + WB, WB, (width - 3 * WB) / 2, BH, 
                                    "Set visibility recursively");
  vis_butt[0]->type(FL_TOGGLE_BUTTON);
  vis_butt[0]->value(1);

  Fl_Tabs *o = new Fl_Tabs(WB, 2 * WB + BH, width - 2 * WB, height - 4 * WB - 2 * BH);
  {
    vis_group[0] = new Fl_Group(WB, 2 * WB + 2 * BH, width - 2 * WB, height - 4 * WB - 3 * BH, "Browser");

    Fl_Button *o0 = new Fl_Button(2 * WB, 3 * WB + 2 * BH, cols[0], BH/2, "*");
    o0->align(FL_ALIGN_TOP | FL_ALIGN_INSIDE);
    o0->tooltip("Select/unselect all");
    o0->callback(visibility_sort_cb, (void *)"*");

    Fl_Button *o1 = new Fl_Button(2 * WB, 3 * WB + 2 * BH + BH/2, cols[0], BH - BH/2, "-");
    o1->tooltip("Invert selection");
    o1->callback(visibility_sort_cb, (void *)"-");

    Fl_Button *o2 = new Fl_Button(2 * WB + cols[0], 3 * WB + 2 * BH, cols[1], BH, "Type");
    o2->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    o2->tooltip("Sort by type");
    o2->callback(visibility_sort_cb, (void *)"type");

    Fl_Button *o3 = new Fl_Button(2 * WB + cols[0] + cols[1], 3 * WB + 2 * BH, cols[2], BH, "Number");
    o3->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    o3->tooltip("Sort by number");
    o3->callback(visibility_sort_cb, (void *)"number");

    Fl_Button *o4 = new Fl_Button(2 * WB + cols[0] + cols[1] + cols[2], 3 * WB + 2 * BH, cols[3], BH, "Name");
    o4->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    o4->tooltip("Sort by name");
    o4->callback(visibility_sort_cb, (void *)"name");

    Fl_Button *o5 = new Fl_Button(width - 4 * WB, 3 * WB + 2 * BH, 2 * WB, BH, "+");
    o5->tooltip("Add parameter name for first selected item");
    o5->callback(visibility_sort_cb, (void *)"+");

    {
      Fl_Group *o = new Fl_Group(2 * WB, 3 * WB + 3 * BH, brw, height - 7 * WB - 5 * BH);
      
      vis_browser = new Vis_Browser(2 * WB, 3 * WB + 3 * BH, brw, height - 7 * WB - 5 * BH);
      vis_browser->type(FL_MULTI_BROWSER);
      vis_browser->column_widths(cols);
      
      o->end();
      Fl_Group::current()->resizable(o);
    }

    vis_push_butt[0] = new Fl_Button(width - 2 * BB - 3 * WB, height - 2 * BH - 3 * WB, BB, BH, "Delete");
    vis_push_butt[0]->callback(visibility_delete_cb);

    Fl_Return_Button *b1 = new Fl_Return_Button(width - 1 * BB - 2 * WB, height - 2 * BH - 3 * WB, BB, BH, "Apply");
    b1->callback(visibility_ok_cb);

    vis_group[0]->end();
    Fl_Group::current()->resizable(vis_group[0]);
  }
  {
    vis_group[1] = new Fl_Group(WB, 2 * WB + 2 * BH, width - 2 * WB, height - 4 * WB - 2 * BH, "Numeric input");
    vis_group[1]->resizable(NULL);

    for(int i = 0; i < 6; i++){
      vis_input[i] = new Fl_Input(width/2-WB/2-IW, 3 * WB + (i+2) * BH, IW, BH);
      vis_input[i]->align(FL_ALIGN_LEFT);
      vis_input[i]->value("*");

      Fl_Button *o1 = new Fl_Button(width/2+WB/2, 3 * WB + (i+2) * BH, BB, BH, "Show");
      o1->callback(visibility_number_cb, (void *)(100+i));

      Fl_Button *o2 = new Fl_Button(width/2+WB/2+BB+WB, 3 * WB + (i+2) * BH, BB, BH, "Hide");
      o2->callback(visibility_number_cb, (void *)i);
    }

    vis_input[0]->label("Node");
    vis_input[0]->tooltip("Enter node number, or *");

    vis_input[1]->label("Element");
    vis_input[1]->tooltip("Enter element number, or *");

    vis_input[2]->label("Point");
    vis_input[2]->tooltip("Enter point number, or *");

    vis_input[3]->label("Line");
    vis_input[3]->tooltip("Enter line number, or *");

    vis_input[4]->label("Surface");
    vis_input[4]->tooltip("Enter surface number, or *");

    vis_input[5]->label("Volume");
    vis_input[5]->tooltip("Enter volume number, or *");

    vis_group[1]->end();
  }
  {
    vis_group[2] = new Fl_Group(WB, 2 * WB + 2 * BH, width - 2 * WB, height - 4 * WB - 2 * BH, "Interactive");
    vis_group[2]->resizable(NULL);

    int ll = width/2 - BH - WB - IW;

    Fl_Box *b2 = new Fl_Box(FL_NO_BOX, ll, 3 * WB + 2 * BH, IW, BH, 
                            "Hide with the mouse:");
    b2->align(FL_ALIGN_INSIDE|FL_ALIGN_CENTER);

    Fl_Button *butt1 = new Fl_Button(ll, 3 * WB + 3 * BH, IW, BH, "Elements");
    butt1->callback(visibility_interactive_cb, (void *)"hide_elements");
    Fl_Button *butt2 = new Fl_Button(ll, 3 * WB + 4 * BH, IW, BH, "Points");
    butt2->callback(visibility_interactive_cb, (void *)"hide_points");
    Fl_Button *butt3 = new Fl_Button(ll, 3 * WB + 5 * BH, IW, BH, "Lines");
    butt3->callback(visibility_interactive_cb, (void *)"hide_lines");
    Fl_Button *butt4 = new Fl_Button(ll, 3 * WB + 6 * BH, IW, BH, "Surfaces");
    butt4->callback(visibility_interactive_cb, (void *)"hide_surfaces");
    Fl_Button *butt5 = new Fl_Button(ll, 3 * WB + 7 * BH, IW, BH, "Volumes");
    butt5->callback(visibility_interactive_cb, (void *)"hide_volumes");

    Fl_Button *butt6 = new Fl_Button(ll + IW + WB, 3 * WB + 3 * BH, 2 * BH, 5*BH, "Show\nAll");
    butt6->callback(visibility_interactive_cb, (void *)"show_all");

    int ll2 = ll + IW + WB + 2*BH + WB;

    Fl_Box *b12 = new Fl_Box(FL_NO_BOX, ll2, 3 * WB + 2 * BH, IW, BH, 
                             "Show with the mouse:");
    b12->align(FL_ALIGN_INSIDE|FL_ALIGN_CENTER);

    Fl_Button *butt11 = new Fl_Button(ll2, 3 * WB + 3 * BH, IW, BH, "Elements");
    butt11->callback(visibility_interactive_cb, (void *)"show_elements");
    Fl_Button *butt12 = new Fl_Button(ll2, 3 * WB + 4 * BH, IW, BH, "Points");
    butt12->callback(visibility_interactive_cb, (void *)"show_points");
    Fl_Button *butt13 = new Fl_Button(ll2, 3 * WB + 5 * BH, IW, BH, "Lines");
    butt13->callback(visibility_interactive_cb, (void *)"show_lines");
    Fl_Button *butt14 = new Fl_Button(ll2, 3 * WB + 6 * BH, IW, BH, "Surfaces");
    butt14->callback(visibility_interactive_cb, (void *)"show_surfaces");
    Fl_Button *butt15 = new Fl_Button(ll2, 3 * WB + 7 * BH, IW, BH, "Volumes");
    butt15->callback(visibility_interactive_cb, (void *)"show_volumes");
    
    vis_group[2]->end();
  }
  o->end();

  vis_window->resizable(o);
  vis_window->size_range(width, 9 * BH + 6 * WB, width);

  {
    Fl_Button *o1 = new Fl_Button(width - 2 * BB - 2 * WB, height - BH - WB, BB, BH, "Save");
    o1->callback(visibility_save_cb);

    Fl_Button *o2 = new Fl_Button(width - BB - WB, height - BH - WB, BB, BH, "Cancel");
    o2->callback(cancel_cb, (void *)vis_window);
  }

  vis_window->position(CTX.vis_position[0], CTX.vis_position[1]);
  vis_window->end();
}

// Create the clipping planes window

void GUI::reset_clip_browser()
{
  char str[128];
  clip_browser->clear();
  clip_browser->add("Geometry");
  clip_browser->add("Mesh");
  for(unsigned int i = 0; i < PView::list.size(); i++) {
    sprintf(str, "View [%d]", i);
    clip_browser->add(str);
  }
  int idx = clip_choice->value();
  clip_browser->deselect();
  for(int i = 0; i < clip_browser->size(); i++)
    if(CTX.clip[idx] & (1<<i))
      clip_browser->select(i+1);
  for(int i = 0; i < 4; i++)
    clip_value[i]->value(CTX.clip_plane[idx][i]);
  for(int i = 4; i < 7; i++)
    clip_value[i]->value(0.);
  for(int i = 7; i < 10; i++)
    clip_value[i]->value(1.);

  for(int i = 0; i < 3; i++) {
    clip_value[i]->step(0.01);
    clip_value[i]->minimum(-1.0);
    clip_value[i]->maximum(1.0);
  }
  double val1 = 0;
  for(int i = 0; i < 3; i++)
    val1 = std::max(val1, std::max(fabs(CTX.min[i]), fabs(CTX.max[i])));
  val1 *= 1.5;
  for(int i = 3; i < 10; i++){
    clip_value[i]->step(val1/200.);
    clip_value[i]->minimum(-val1);
    clip_value[i]->maximum(val1);
  }
}

void GUI::create_clip_window()
{
  if(clip_window) {
    reset_clip_browser();
    clip_window->show();
    return;
  }

  static Fl_Menu_Item plane_number[] = {
    {"Plane 0", 0, 0},
    {"Plane 1", 0, 0},
    {"Plane 2", 0, 0},
    {"Plane 3", 0, 0},
    {"Plane 4", 0, 0},
    {"Plane 5", 0, 0},
    {0}
  };

  int width = 26 * fontsize;
  int height = 10 * BH + 5 * WB;
  int L = 7 * fontsize;

  clip_window = new Dialog_Window(width, height, CTX.non_modal_windows, "Clipping");
  clip_window->box(GMSH_WINDOW_BOX);

  clip_browser = new Fl_Multi_Browser(WB, WB, L - WB, height - BH - 3 * WB);
  clip_browser->callback(clip_update_cb);

  Fl_Tabs *o = new Fl_Tabs(L + WB, WB, width - L - 2 * WB, height - 3 * WB - 4 * BH);
  {
    clip_group[0] = new Fl_Group(L + WB, WB + BH, width - L - 2 * WB, height - 3 * WB - 5 * BH, "Planes");

    int BW = width - L - 4 * WB - 4 * fontsize;

    clip_choice = new Fl_Choice(L + 2 * WB, 2 * WB + 1 * BH, BW, BH);
    clip_choice->menu(plane_number);
    clip_choice->callback(clip_num_cb);

    Fl_Button *invert = new Fl_Button(L + 2 * WB, 2 * WB + 2 * BH, fontsize, 4 * BH, "-");
    invert->callback(clip_invert_cb);
    invert->tooltip("Invert orientation");
    
    clip_value[0] = new Fl_Value_Input(L + 2 * WB + fontsize, 2 * WB + 2 * BH, BW - fontsize, BH, "A");
    clip_value[1] = new Fl_Value_Input(L + 2 * WB + fontsize, 2 * WB + 3 * BH, BW - fontsize, BH, "B");
    clip_value[2] = new Fl_Value_Input(L + 2 * WB + fontsize, 2 * WB + 4 * BH, BW - fontsize, BH, "C");
    clip_value[3] = new Fl_Value_Input(L + 2 * WB + fontsize, 2 * WB + 5 * BH, BW - fontsize, BH, "D");
    for(int i = 0; i < 4; i++){
      clip_value[i]->align(FL_ALIGN_RIGHT);
      clip_value[i]->callback(clip_update_cb);
    }

    clip_group[0]->end();
  }
  {
    clip_group[1] = new Fl_Group(L + WB, WB + BH, width - L - 2 * WB, height - 3 * WB - 5 * BH, "Box");
    clip_group[1]->hide();

    int w2 = (width - L - 4 * WB) / 2;
    int BW = w2 - 2 * fontsize;
    clip_value[4] = new Fl_Value_Input(L + 2 * WB, 2 * WB + 1 * BH, BW, BH, "Cx");
    clip_value[5] = new Fl_Value_Input(L + 2 * WB, 2 * WB + 2 * BH, BW, BH, "Cy");
    clip_value[6] = new Fl_Value_Input(L + 2 * WB, 2 * WB + 3 * BH, BW, BH, "Cz");
    clip_value[7] = new Fl_Value_Input(L + 2 * WB + w2, 2 * WB + 1 * BH, BW, BH, "Wx");
    clip_value[8] = new Fl_Value_Input(L + 2 * WB + w2, 2 * WB + 2 * BH, BW, BH, "Wy");
    clip_value[9] = new Fl_Value_Input(L + 2 * WB + w2, 2 * WB + 3 * BH, BW, BH, "Wz");
    for(int i = 4; i < 10; i++){
      clip_value[i]->align(FL_ALIGN_RIGHT);
      clip_value[i]->callback(clip_update_cb);
    }

    clip_group[1]->end();
  }
  o->callback(clip_reset_cb);
  o->end();

  clip_butt[0] = new Fl_Check_Button(L + WB, 3 * WB + 6 * BH, width - L - 2 * WB, BH, "Keep whole elements");
  clip_butt[1] = new Fl_Check_Button(L + WB, 3 * WB + 7 * BH, width - L - 2 * WB, BH, "Only draw intersecting volume layer");
  clip_butt[2] = new Fl_Check_Button(L + WB, 3 * WB + 8 * BH, width - L - 2 * WB, BH, "Cut only volume elements");
  for(int i = 0; i < 3; i++){
    clip_butt[i]->type(FL_TOGGLE_BUTTON);
    clip_butt[i]->callback(clip_update_cb);
  }

  reset_clip_browser();

  {
    Fl_Return_Button *o = new Fl_Return_Button(width - 3 * BB - 3 * WB, height - BH - WB, BB, BH, "Redraw");
    o->callback(redraw_cb);
  }
  {
    Fl_Button *o = new Fl_Button(width - 2 * BB - 2 * WB, height - BH - WB, BB, BH, "Reset");
    o->callback(clip_reset_cb);
  }
  {
    Fl_Button *o = new Fl_Button(width - BB - WB, height - BH - WB, BB, BH, "Cancel");
    o->callback(cancel_cb, (void *)clip_window);
  }

  clip_window->position(CTX.clip_position[0], CTX.clip_position[1]);
  clip_window->end();
}

// create the manipulator

void GUI::update_manip_window(int force)
{
  if(force || manip_window->shown()){
    double val1 = CTX.lc;
    for(int i = 0; i < 3; i++){
      manip_value[i]->value(CTX.r[i]);
      manip_value[i]->minimum(-360.);
      manip_value[i]->maximum(360.);
      manip_value[i]->step(1.);

      manip_value[i+3]->value(CTX.t[i]);
      manip_value[i+3]->minimum(-val1);
      manip_value[i+3]->maximum(val1);
      manip_value[i+3]->step(val1/200.);

      manip_value[i+6]->value(CTX.s[i]);
      manip_value[i+6]->minimum(0.01);
      manip_value[i+6]->maximum(100.);
      manip_value[i+6]->step(0.01);
    }
  }
}

void GUI::create_manip_window()
{
  if(manip_window) {
    update_manip_window(1);
    manip_window->show();
    return;
  }

  int width = 4 * BB + 2 * WB;
  int height = 5 * BH + 3 * WB;

  manip_window = new Dialog_Window(width, height, CTX.non_modal_windows, "Manipulator");
  manip_window->box(GMSH_WINDOW_BOX);

  Fl_Box *top[3], *left[3];
  top[0] = new Fl_Box(WB + 1 * BB, 1 * WB + 0 * BH, BB, BH, "X");
  top[1] = new Fl_Box(WB + 2 * BB, 1 * WB + 0 * BH, BB, BH, "Y");
  top[2] = new Fl_Box(WB + 3 * BB, 1 * WB + 0 * BH, BB, BH, "Z");
  left[0] = new Fl_Box(WB + 0 * BB, 1 * WB + 1 * BH, BB, BH, "Rotation");
  left[1] = new Fl_Box(WB + 0 * BB, 1 * WB + 2 * BH, BB, BH, "Translation");
  left[2] = new Fl_Box(WB + 0 * BB, 1 * WB + 3 * BH, BB, BH, "Scale");
  for(int i = 0; i < 3; i++){  
    top[i]->align(FL_ALIGN_INSIDE|FL_ALIGN_CENTER);
    left[i]->align(FL_ALIGN_INSIDE|FL_ALIGN_CENTER);
  }

  manip_value[0] = new Fl_Value_Input(WB + 1 * BB, 1 * WB + 1 * BH, BB, BH);
  manip_value[1] = new Fl_Value_Input(WB + 2 * BB, 1 * WB + 1 * BH, BB, BH);
  manip_value[2] = new Fl_Value_Input(WB + 3 * BB, 1 * WB + 1 * BH, BB, BH);
  manip_value[3] = new Fl_Value_Input(WB + 1 * BB, 1 * WB + 2 * BH, BB, BH);
  manip_value[4] = new Fl_Value_Input(WB + 2 * BB, 1 * WB + 2 * BH, BB, BH);
  manip_value[5] = new Fl_Value_Input(WB + 3 * BB, 1 * WB + 2 * BH, BB, BH);
  manip_value[6] = new Fl_Value_Input(WB + 1 * BB, 1 * WB + 3 * BH, BB, BH);
  manip_value[7] = new Fl_Value_Input(WB + 2 * BB, 1 * WB + 3 * BH, BB, BH);
  manip_value[8] = new Fl_Value_Input(WB + 3 * BB, 1 * WB + 3 * BH, BB, BH);

  for(int i = 0; i < 9; i++){
    if(i < 3){
      manip_value[i]->minimum(0.);
      manip_value[i]->maximum(360.);
      manip_value[i]->step(1.);
    }
    else if(i > 5){
      manip_value[i]->minimum(0.1);
      manip_value[i]->maximum(100.);
      manip_value[i]->step(0.1);
    }
    manip_value[i]->align(FL_ALIGN_RIGHT);
    manip_value[i]->callback(manip_update_cb);
  }

  {
    Fl_Return_Button *o = new Fl_Return_Button(width - 2 * BB - 2 * WB, height - BH - WB, BB, BH, "Reset");
    o->callback(status_xyz1p_cb, (void *)"reset");
  }
  {
    Fl_Button *o = new Fl_Button(width - BB - WB, height - BH - WB, BB, BH, "Cancel");
    o->callback(cancel_cb, (void *)manip_window);
  }

  manip_window->position(CTX.manip_position[0], CTX.manip_position[1]);
  manip_window->end();
}

// Create the about window

void GUI::create_about_window()
{
  char buffer[1024];

  if(about_window) {
    about_window->show();
    return;
  }

  int width = 28 * fontsize;
  int height = 15 * BH + BH/2;

  about_window = new Dialog_Window(width, height, CTX.non_modal_windows, "About Gmsh");
  about_window->box(GMSH_WINDOW_BOX);

  {
    Fl_Browser *o = new Fl_Browser(0, 0, width, height - 2 * WB - BH);
    o->box(FL_FLAT_BOX);
    o->has_scrollbar(0); // no scrollbars
    o->add(" ");
    o->add("@c@b@.Gmsh");
    o->add("@c@.A three-dimensional finite element mesh generator");
    o->add("@c@.with built-in pre- and post-processing facilities");
    o->add(" ");
    o->add("@c@.Copyright (C) 1997-2008");
#if defined(__APPLE__)
    o->add("@c@.Christophe Geuzaine and Jean-Francois Remacle");
#else
    o->add("@c@.Christophe Geuzaine and Jean-Fran�ois Remacle");
#endif
    o->add(" ");
    o->add("@c@.Please send all questions and bug reports to");
    o->add("@c@b@.gmsh@geuz.org");
    o->add(" ");
    sprintf(buffer, "@c@.Version: %s", Get_GmshVersion());
    o->add(buffer);
    sprintf(buffer, "@c@.License: %s", Get_GmshShortLicense());
    o->add(buffer);
    sprintf(buffer, "@c@.Graphical user interface toolkit: FLTK %d.%d.%d", 
            FL_MAJOR_VERSION, FL_MINOR_VERSION, FL_PATCH_VERSION);
    o->add(buffer);
    sprintf(buffer, "@c@.Build OS: %s", Get_GmshBuildOS());
    o->add(buffer);
    sprintf(buffer, "@c@.Build date: %s", Get_GmshBuildDate());
    o->add(buffer);
    sprintf(buffer, "@c@.Build host: %s", Get_GmshBuildHost());
    o->add(buffer);
    {
      char str1[1024];
      strcpy(str1, Get_BuildOptions());
      unsigned int len = 30;
      if(strlen(str1) > len){
        int split;
        for(split = len - 1; split >= 0; split--){
          if(str1[split] == ' '){
            str1[split] = '\0';
            break;
          }
        }
        sprintf(buffer, "@c@.Build options: %s", str1);
        o->add(buffer);
        sprintf(buffer, "@c@.%s", &str1[split+1]);
        o->add(buffer);
      }
      else{
        sprintf(buffer, "@c@.Options: %s", str1);
        o->add(buffer);
      }
    }
    sprintf(buffer, "@c@.Packaged by: %s", Get_GmshPackager());
    o->add(buffer);
    o->add(" ");
    o->add("@c@.Visit http://www.geuz.org/gmsh/ for more information");
    o->add(" ");
    o->callback(cancel_cb, (void *)about_window);
  }

  {
    Fl_Button *o = new Fl_Button(width/2 - BB - WB/2, height - BH - WB, BB, BH, "License");
    o->callback(help_license_cb);
  }

  {
    Fl_Button *o = new Fl_Button(width/2 + WB/2, height - BH - WB, BB, BH, "Credits");
    o->callback(help_credits_cb);
  }

  about_window->position(Fl::x() + Fl::w()/2 - width / 2,
                         Fl::y() + Fl::h()/2 - height / 2);
  about_window->end();
}


// Create the window for geometry context dependant definitions

void GUI::create_geometry_context_window(int num)
{
  static Fl_Group *g[10];
  int i;

  if(context_geometry_window) {
    for(i = 0; i < 6; i++)
      g[i]->hide();
    g[num]->show();
    context_geometry_window->show();
    return;
  }

  int width = 31 * fontsize;
  int height = 5 * WB + 9 * BH;

  context_geometry_window = new Dialog_Window(width, height, CTX.non_modal_windows, "Contextual Geometry Definitions");
  context_geometry_window->box(GMSH_WINDOW_BOX);
  {
    Fl_Tabs *o = new Fl_Tabs(WB, WB, width - 2 * WB, height - 3 * WB - BH);
    // 0: Parameter
    {
      g[0] = new Fl_Group(WB, WB + BH, width - 2 * WB, height - 3 * WB - 2 * BH, "Parameter");
      context_geometry_input[0] = new Fl_Input(2 * WB, 2 * WB + 1 * BH, IW, BH, "Name");
      context_geometry_input[0]->value("lc");
      context_geometry_input[1] = new Fl_Input(2 * WB, 2 * WB + 2 * BH, IW, BH, "Value");
      context_geometry_input[1]->value("0.1");
      for(i = 0; i < 2; i++) {
        context_geometry_input[i]->align(FL_ALIGN_RIGHT);
      }
      {
        Fl_Return_Button *o = new Fl_Return_Button(width - BB - 2 * WB, 2 * WB + 7 * BH, BB, BH, "Add");
        o->callback(con_geometry_define_parameter_cb);
      }
      g[0]->end();
    }
    // 1: Point
    {
      g[1] = new Fl_Group(WB, WB + BH, width - 2 * WB, height - 3 * WB - 2 * BH, "Point");
      context_geometry_input[2] = new Fl_Input(2 * WB, 2 * WB + 1 * BH, IW, BH, "X coordinate");
      context_geometry_input[2]->value("0");
      context_geometry_input[3] = new Fl_Input(2 * WB, 2 * WB + 2 * BH, IW, BH, "Y coordinate");
      context_geometry_input[3]->value("0");
      context_geometry_input[4] = new Fl_Input(2 * WB, 2 * WB + 3 * BH, IW, BH, "Z coordinate");
      context_geometry_input[4]->value("0");
      context_geometry_input[5] = new Fl_Input(2 * WB, 2 * WB + 4 * BH, IW, BH, "Characteristic length");
      context_geometry_input[5]->value("0.1");
      for(i = 2; i < 6; i++) {
        context_geometry_input[i]->align(FL_ALIGN_RIGHT);
      }
      context_geometry_value[0] = new Fl_Value_Input(2 * WB, 2 * WB + 5 * BH, IW/3, BH);
      context_geometry_value[1] = new Fl_Value_Input(2 * WB + IW/3, 2 * WB + 5 * BH, IW/3, BH);
      context_geometry_value[2] = new Fl_Value_Input(2 * WB + 2*IW/3, 2 * WB + 5 * BH, IW/3, BH, "Snapping grid spacing");
      for(i = 0; i < 3; i++) {
        context_geometry_value[i]->align(FL_ALIGN_RIGHT);
        context_geometry_value[i]->callback(con_geometry_snap_cb);
      }
      {
        Fl_Return_Button *o = new Fl_Return_Button(width - BB - 2 * WB, 2 * WB + 7 * BH, BB, BH, "Add");
        o->callback(con_geometry_define_point_cb);
      }
      g[1]->end();
    }
    // 2: Translation
    {
      g[2] = new Fl_Group(WB, WB + BH, width - 2 * WB, height - 3 * WB - 2 * BH, "Translation");
      context_geometry_input[6] = new Fl_Input(2 * WB, 2 * WB + 1 * BH, IW, BH, "X component");
      context_geometry_input[6]->value("0");
      context_geometry_input[7] = new Fl_Input(2 * WB, 2 * WB + 2 * BH, IW, BH, "Y component");
      context_geometry_input[7]->value("0");
      context_geometry_input[8] = new Fl_Input(2 * WB, 2 * WB + 3 * BH, IW, BH, "Z component");
      context_geometry_input[8]->value("1");
      for(i = 6; i < 9; i++) {
        context_geometry_input[i]->align(FL_ALIGN_RIGHT);
      }
      g[2]->end();
    }
    // 3: Rotation
    {
      g[3] = new Fl_Group(WB, WB + BH, width - 2 * WB, height - 3 * WB - 2 * BH, "Rotation");
      context_geometry_input[9] = new Fl_Input(2 * WB, 2 * WB + 1 * BH, IW, BH, "X coordinate of an axis point");
      context_geometry_input[9]->value("0");
      context_geometry_input[10] = new Fl_Input(2 * WB, 2 * WB + 2 * BH, IW, BH, "Y coordinate of an axis point");
      context_geometry_input[10]->value("0");
      context_geometry_input[11] = new Fl_Input(2 * WB, 2 * WB + 3 * BH, IW, BH, "Z coordinate of an axis point");
      context_geometry_input[11]->value("0");
      context_geometry_input[12] = new Fl_Input(2 * WB, 2 * WB + 4 * BH, IW, BH, "X component of axis direction");
      context_geometry_input[12]->value("0");
      context_geometry_input[13] = new Fl_Input(2 * WB, 2 * WB + 5 * BH, IW, BH, "Y component of axis direction");
      context_geometry_input[13]->value("1");
      context_geometry_input[14] = new Fl_Input(2 * WB, 2 * WB + 6 * BH, IW, BH, "Z component of axis direction");
      context_geometry_input[14]->value("0");
      context_geometry_input[15] = new Fl_Input(2 * WB, 2 * WB + 7 * BH, IW, BH, "Angle in radians");
      context_geometry_input[15]->value("Pi/4");
      for(i = 9; i < 16; i++) {
        context_geometry_input[i]->align(FL_ALIGN_RIGHT);
      }
      g[3]->end();
    }
    // 4: Scale
    {
      g[4] = new Fl_Group(WB, WB + BH, width - 2 * WB, height - 3 * WB - 2 * BH, "Scale");
      context_geometry_input[16] = new Fl_Input(2 * WB, 2 * WB + 1 * BH, IW, BH, "X component of direction");
      context_geometry_input[16]->value("0");
      context_geometry_input[17] = new Fl_Input(2 * WB, 2 * WB + 2 * BH, IW, BH, "Y component of direction");
      context_geometry_input[17]->value("0");
      context_geometry_input[18] = new Fl_Input(2 * WB, 2 * WB + 3 * BH, IW, BH, "Z component of direction");
      context_geometry_input[18]->value("0");
      context_geometry_input[19] = new Fl_Input(2 * WB, 2 * WB + 4 * BH, IW, BH, "Factor");
      context_geometry_input[19]->value("0.5");
      for(i = 16; i < 20; i++) {
        context_geometry_input[i]->align(FL_ALIGN_RIGHT);
      }
      g[4]->end();
    }
    // 5: Symmetry
    {
      g[5] = new Fl_Group(WB, WB + BH, width - 2 * WB, height - 3 * WB - 2 * BH, "Symmetry");
      context_geometry_input[20] = new Fl_Input(2 * WB, 2 * WB + 1 * BH, IW, BH, "A");
      context_geometry_input[20]->value("1");
      context_geometry_input[21] = new Fl_Input(2 * WB, 2 * WB + 2 * BH, IW, BH, "B");
      context_geometry_input[21]->value("0");
      context_geometry_input[22] = new Fl_Input(2 * WB, 2 * WB + 3 * BH, IW, BH, "C");
      context_geometry_input[22]->value("0");
      context_geometry_input[23] = new Fl_Input(2 * WB, 2 * WB + 4 * BH, IW, BH, "D");
      context_geometry_input[23]->value("1");
      for(i = 20; i < 24; i++) {
        context_geometry_input[i]->align(FL_ALIGN_RIGHT);
      }
      g[5]->end();
    }
    o->end();
  }

  {
    Fl_Button *o = new Fl_Button(width - BB - WB, height - BH - WB, BB, BH, "Cancel");
    o->callback(cancel_cb, (void *)context_geometry_window);
  }

  context_geometry_window->position(CTX.ctx_position[0], CTX.ctx_position[1]);
  context_geometry_window->end();
}

// Create the window for physical context dependant definitions

void GUI::call_for_solver_plugin (int dim)
{ 
  GMSH_Solve_Plugin *sp = GMSH_PluginManager::instance()->findSolverPlugin();   
  if (sp) {
    sp->popupPropertiesForPhysicalEntity(dim);
  }
}

// Create the window for mesh context dependant definitions

void GUI::create_mesh_context_window(int num)
{
  static Fl_Group *g[10];
  static Fl_Menu menu_transfinite_dir[] = {
    {"Left", 0, 0, 0},
    {"Right", 0, 0, 0},
    {"Alternated", 0, 0, 0},
    {0}
  };

  if(context_mesh_window) {
    for(int i = 0; i < 3; i++)
      g[i]->hide();
    g[num]->show();
    context_mesh_window->show();
    return;
  }

  int width = 29 * fontsize;
  int height = 5 * WB + 5 * BH;

  context_mesh_window = new Dialog_Window(width, height, CTX.non_modal_windows, "Contextual Mesh Definitions");
  context_mesh_window->box(GMSH_WINDOW_BOX);
  {
    Fl_Tabs *o = new Fl_Tabs(WB, WB, width - 2 * WB, height - 3 * WB - BH);
    // 0: Characteristic length
    {
      g[0] = new Fl_Group(WB, WB + BH, width - 2 * WB, height - 3 * WB - 2 * BH, "Characteristic Length");
      context_mesh_input[0] = new Fl_Input(2 * WB, 2 * WB + 1 * BH, IW, BH, "Value");
      context_mesh_input[0]->value("0.1");
      context_mesh_input[0]->align(FL_ALIGN_RIGHT);
      g[0]->end();
    }
    // 1: Transfinite line
    {
      g[1] = new Fl_Group(WB, WB + BH, width - 2 * WB, height - 3 * WB - 2 * BH, "Transfinite Line");
      context_mesh_input[1] = new Fl_Input(2 * WB, 2 * WB + 1 * BH, IW, BH, "Number of points");
      context_mesh_input[1]->value("10");
      context_mesh_input[2] = new Fl_Input(2 * WB, 2 * WB + 3 * BH, IW, BH, "Parameter");
      context_mesh_input[2]->value("1");
      for(int i = 1; i < 3; i++) {
        context_mesh_input[i]->align(FL_ALIGN_RIGHT);
      }
      static Fl_Menu_Item menu_trsf_mesh[] = {
        {"Progression", 0, 0, 0},
        {"Bump", 0, 0, 0},
        {0}
      };
      context_mesh_choice[0] = new Fl_Choice(2 * WB, 2 * WB + 2 * BH, IW, BH, "Type");
      context_mesh_choice[0]->menu(menu_trsf_mesh);
      context_mesh_choice[0]->align(FL_ALIGN_RIGHT);
      g[1]->end();
    }
    
    // 2: Transfinite surface
    {
      g[2] = new Fl_Group(WB, WB + BH, width - 2 * WB, height - 3 * WB - 2 * BH, "Transfinite Surface");

      context_mesh_choice[1] = new Fl_Choice(2 * WB, 2 * WB + 1 * BH, IW, BH, "Transfinite Arrangement");
      context_mesh_choice[1]->menu(menu_transfinite_dir);
      context_mesh_choice[1]->align(FL_ALIGN_RIGHT);

      g[2]->end();
    }
    o->end();
  }

  {
    Fl_Button *o = new Fl_Button(width - BB - WB, height - BH - WB, BB, BH, "Cancel");
    o->callback(cancel_cb, (void *)context_mesh_window);
  }

  context_mesh_window->position(CTX.ctx_position[0], CTX.ctx_position[1]);
  context_mesh_window->end();
}


// Create the windows for the solvers

void GUI::create_solver_window(int num)
{
  if(solver[num].window) {
    solver[num].window->show();
    return;
  }

  for(int i = 0; i < MAXSOLVERS; i++)
    if(strlen(SINFO[num].option_name[i]))
      SINFO[num].nboptions = i + 1;

  int LL = 2 * IW;
  int width = LL + BB + BB / 3 + 4 * WB;
  int height = (8 + SINFO[num].nboptions) * BH + 6 * WB;
  int BBS = (width - 8 * WB) / 5;
  
  solver[num].window = new Dialog_Window(width, height, CTX.non_modal_windows);
  solver[num].window->box(GMSH_WINDOW_BOX);
  {
    Fl_Tabs *o = new Fl_Tabs(WB, WB, width - 2 * WB, height - 3 * WB - 1 * BH);
    {
      Fl_Group *g = new Fl_Group(WB, WB + BH, width - 2 * WB, height - 3 * WB - 2 * BH, "Controls");

      solver[num].input[2] = new Fl_Input(2 * WB, 2 * WB + 1 * BH, LL, BH, "Solver");
      solver[num].input[2]->callback(solver_ok_cb, (void *)num);

      Fl_Button *b1 = new Fl_Button(width - 2 * WB - BBS, 2 * WB + 1 * BH, BBS, BH, "Choose");
      b1->callback(solver_choose_executable_cb, (void *)num);
      Fl_Button *b2 = new Fl_Button(width - 2 * WB - BBS, 2 * WB + 2 * BH, BBS, BH, "Save");
      b2->callback(options_save_cb);

      int ww = (LL - WB) / 2;
      solver[num].butt[2] = new Fl_Check_Button(2 * WB, 2 * WB + 2 * BH, ww, BH, "Client/server");
      solver[num].butt[0] = new Fl_Check_Button(2 * WB, 2 * WB + 3 * BH, ww, BH, "Pop-up messages");
      solver[num].butt[1] = new Fl_Check_Button(3 * WB + ww, 2 * WB + 2 * BH, ww, BH, "Auto-load results");
      
      for(int i = 0; i < 3; i++){
        solver[num].butt[i]->type(FL_TOGGLE_BUTTON);
        solver[num].butt[i]->callback(solver_ok_cb, (void *)num);
      }

      solver[num].input[0] = new Fl_Input(2 * WB + BB / 2, 2 * WB + 4 * BH, LL - BB / 2, BH, "Input");
      Fl_Button *b3 = new Fl_Button(width - 2 * WB - BBS, 2 * WB + 4 * BH, BBS, BH, "Choose");
      b3->callback(solver_file_open_cb, (void *)num);

      Fl_Button *b4 = new Fl_Button(2 * WB, 2 * WB + 4 * BH, BB / 2, BH, "Edit");
      b4->callback(solver_file_edit_cb, (void *)num);

      solver[num].input[1] = new Fl_Input(2 * WB, 2 * WB + 5 * BH, LL, BH, "Mesh");
      Fl_Button *b5 = new Fl_Button(width - 2 * WB - BBS, 2 * WB + 5 * BH, BBS, BH, "Choose");
      b5->callback(solver_choose_mesh_cb, (void *)num);

      for(int i = 0; i < 3; i++) {
        solver[num].input[i]->align(FL_ALIGN_RIGHT);
      }

      for(int i = 0; i < SINFO[num].nboptions; i++) {
        solver[num].choice[i] = new Fl_Choice(2 * WB, 2 * WB + (6 + i) * BH, LL, BH,
                                              SINFO[num].option_name[i]);
        solver[num].choice[i]->align(FL_ALIGN_RIGHT);
      }

      static int arg[MAXSOLVERS][5][2];
      for(int i = 0; i < 5; i++) {
        if(strlen(SINFO[num].button_name[i])) {
          arg[num][i][0] = num;
          arg[num][i][1] = i;
          solver[num].command[i] = new Fl_Button((2 + i) * WB + i * BBS, 
                                                 3 * WB + (6 + SINFO[num].nboptions) * BH,
                                                 BBS, BH, SINFO[num].button_name[i]);
          solver[num].command[i]->callback(solver_command_cb, (void *)arg[num][i]);
        }
      }

      g->end();
    }
    {
      Fl_Group *g = new Fl_Group(WB, WB + BH, width - 2 * WB, height - 3 * WB - 2 * BH, "About");

      Fl_Browser *o = new Fl_Browser(2 * WB, 2 * WB + 1 * BH, width - 4 * WB,
                                     height - 5 * WB - 2 * BH);
      o->add(" ");
      add_multiline_in_browser(o, "@c@b@.", SINFO[num].name);
      o->add(" ");
      add_multiline_in_browser(o, "@c@. ", SINFO[num].help);

      g->end();
    }
    o->end();
  }

  {
    Fl_Button *b1 = new Fl_Button(width - 2 * BB - 2 * WB, height - BH - WB, BB, BH, "Kill solver");
    b1->callback(solver_kill_cb, (void *)num);
    Fl_Button *b2 = new Fl_Button(width - BB - WB, height - BH - WB, BB, BH, "Cancel");
    b2->callback(cancel_cb, (void *)solver[num].window);
  }

  solver[num].window->position(CTX.solver_position[0], CTX.solver_position[1]);
  solver[num].window->end();
}
