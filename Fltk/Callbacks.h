// Gmsh - Copyright (C) 1997-2008 C. Geuzaine, J.-F. Remacle
//
// See the LICENSE.txt file for license information. Please report all
// bugs and problems to <gmsh@geuz.org>.

#ifndef _CALLBACKS_H_
#define _CALLBACKS_H_

#include <FL/Fl_Widget.H>

#define CALLBACK_ARGS   Fl_Widget* w, void* data

void ManualPlay(int time, int step);
int SetGlobalShortcut(int event);

// Common callbacks

void cancel_cb(CALLBACK_ARGS);
void color_cb(CALLBACK_ARGS);
void view_color_cb(CALLBACK_ARGS);
void redraw_cb(CALLBACK_ARGS);
void window_cb(CALLBACK_ARGS);
void activate_cb(CALLBACK_ARGS);

// Graphical window

void status_xyz1p_cb(CALLBACK_ARGS);
void status_play_cb(CALLBACK_ARGS);
void status_pause_cb(CALLBACK_ARGS);
void status_rewind_cb(CALLBACK_ARGS);
void status_stepbackward_cb(CALLBACK_ARGS);
void status_stepforward_cb(CALLBACK_ARGS);

// File Menu

void file_new_cb(CALLBACK_ARGS);
void file_open_cb(CALLBACK_ARGS);
void file_merge_cb(CALLBACK_ARGS);
void file_rename_cb(CALLBACK_ARGS);
void file_save_as_cb(CALLBACK_ARGS);
void file_save_as_auto_cb(CALLBACK_ARGS);
void file_save_as_geo_cb(CALLBACK_ARGS);
void file_save_as_geo_options_cb(CALLBACK_ARGS);
void file_save_as_msh_cb(CALLBACK_ARGS);
void file_save_as_msh_all_cb(CALLBACK_ARGS);
void file_save_as_unv_cb(CALLBACK_ARGS);
void file_save_as_gref_cb(CALLBACK_ARGS);
void file_save_as_vrml_cb(CALLBACK_ARGS);
void file_save_as_ps_simple_cb(CALLBACK_ARGS);
void file_save_as_ps_accurate_cb(CALLBACK_ARGS);
void file_save_as_epstex_simple_cb(CALLBACK_ARGS);
void file_save_as_epstex_accurate_cb(CALLBACK_ARGS);
void file_save_as_jpegtex_cb(CALLBACK_ARGS);
void file_save_as_pngtex_cb(CALLBACK_ARGS);
void file_save_as_tex_cb(CALLBACK_ARGS);
void file_save_as_jpeg_cb(CALLBACK_ARGS);
void file_save_as_png_cb(CALLBACK_ARGS);
void file_save_as_gif_cb(CALLBACK_ARGS);
void file_save_as_gif_dithered_cb(CALLBACK_ARGS);
void file_save_as_gif_transparent_cb(CALLBACK_ARGS);
void file_save_as_ppm_cb(CALLBACK_ARGS);
void file_save_as_yuv_cb(CALLBACK_ARGS);
void file_quit_cb(CALLBACK_ARGS);

// Option Menu

void options_cb(CALLBACK_ARGS);
void options_browser_cb(CALLBACK_ARGS);
void options_save_cb(CALLBACK_ARGS);
void options_restore_defaults_cb(CALLBACK_ARGS);

void general_options_cb(CALLBACK_ARGS);
void general_options_ok_cb(CALLBACK_ARGS);
void general_options_color_scheme_cb(CALLBACK_ARGS);
void general_options_rotation_center_select_cb(CALLBACK_ARGS);
void general_arrow_param_cb(CALLBACK_ARGS);

void geometry_options_cb(CALLBACK_ARGS);
void geometry_options_ok_cb(CALLBACK_ARGS);

void mesh_options_cb(CALLBACK_ARGS);
void mesh_options_ok_cb(CALLBACK_ARGS);

void solver_options_cb(CALLBACK_ARGS);
void solver_options_ok_cb(CALLBACK_ARGS);

void post_options_cb(CALLBACK_ARGS);
void post_options_ok_cb(CALLBACK_ARGS);

void view_options_cb(CALLBACK_ARGS);
void view_options_ok_cb(CALLBACK_ARGS);
void view_options_timestep_cb(CALLBACK_ARGS);
void view_options_timestep_decr_cb(CALLBACK_ARGS);
void view_options_timestep_incr_cb(CALLBACK_ARGS);
void view_arrow_param_cb(CALLBACK_ARGS);
void view_toggle_cb(CALLBACK_ARGS);
void view_reload_cb(CALLBACK_ARGS);
void view_reload_all_cb(CALLBACK_ARGS);
void view_reload_visible_cb(CALLBACK_ARGS);
void view_remove_cb(CALLBACK_ARGS);
void view_remove_other_cb(CALLBACK_ARGS);
void view_remove_all_cb(CALLBACK_ARGS);
void view_remove_visible_cb(CALLBACK_ARGS);
void view_remove_invisible_cb(CALLBACK_ARGS);
void view_remove_empty_cb(CALLBACK_ARGS);
void view_save_ascii_cb(CALLBACK_ARGS);
void view_save_binary_cb(CALLBACK_ARGS);
void view_save_parsed_cb(CALLBACK_ARGS);
void view_save_stl_cb(CALLBACK_ARGS);
void view_save_txt_cb(CALLBACK_ARGS);
void view_save_msh_cb(CALLBACK_ARGS);
void view_save_med_cb(CALLBACK_ARGS);
void view_alias_cb(CALLBACK_ARGS);
void view_alias_with_options_cb(CALLBACK_ARGS);
void view_combine_space_all_cb(CALLBACK_ARGS);
void view_combine_space_visible_cb(CALLBACK_ARGS);
void view_combine_space_by_name_cb(CALLBACK_ARGS);
void view_combine_time_all_cb(CALLBACK_ARGS);
void view_combine_time_visible_cb(CALLBACK_ARGS);
void view_combine_time_by_name_cb(CALLBACK_ARGS);
void view_all_visible_cb(CALLBACK_ARGS);
void view_applybgmesh_cb(CALLBACK_ARGS);
void view_field_cb(CALLBACK_ARGS);
void view_field_new_cb(CALLBACK_ARGS);
void view_field_apply_cb(CALLBACK_ARGS);
void view_field_revert_cb(CALLBACK_ARGS);
void view_field_cancel_cb(CALLBACK_ARGS);
void view_field_browser_cb(CALLBACK_ARGS);
void view_field_delete_cb(CALLBACK_ARGS);
void view_field_set_size_btn_cb(CALLBACK_ARGS);
void view_field_put_on_view_cb(CALLBACK_ARGS);
void view_field_select_node_cb(CALLBACK_ARGS);
void view_plugin_cb(CALLBACK_ARGS);
void view_plugin_browser_cb(CALLBACK_ARGS);
void view_plugin_run_cb(CALLBACK_ARGS);
void view_plugin_cancel_cb(CALLBACK_ARGS);

// Statistics Menu

void statistics_cb(CALLBACK_ARGS);
void statistics_update_cb(CALLBACK_ARGS);
void statistics_histogram_cb(CALLBACK_ARGS);

// Message Menu

void message_cb(CALLBACK_ARGS);
void message_copy_cb(CALLBACK_ARGS);
void message_clear_cb(CALLBACK_ARGS);
void message_save_cb(CALLBACK_ARGS);

// Visibility Menu

void visibility_cb(CALLBACK_ARGS);
void visibility_sort_cb(CALLBACK_ARGS);
void visibility_number_cb(CALLBACK_ARGS);
void visibility_interactive_cb(CALLBACK_ARGS);
void visibility_ok_cb(CALLBACK_ARGS);
void visibility_save_cb(CALLBACK_ARGS);
void visibility_delete_cb(CALLBACK_ARGS);

// Clipping planes Menu

void clip_cb(CALLBACK_ARGS);
void clip_update_cb(CALLBACK_ARGS);
void clip_invert_cb(CALLBACK_ARGS);
void clip_num_cb(CALLBACK_ARGS);
void clip_reset_cb(CALLBACK_ARGS);

// Manipulator Menu

void manip_cb(CALLBACK_ARGS);
void manip_update_cb(CALLBACK_ARGS);

// Help Menu

void help_short_cb(CALLBACK_ARGS);
void help_mouse_cb(CALLBACK_ARGS);
void help_command_line_cb(CALLBACK_ARGS);
void help_online_cb(CALLBACK_ARGS);
void help_license_cb(CALLBACK_ARGS);
void help_credits_cb(CALLBACK_ARGS);
void help_about_cb(CALLBACK_ARGS);

// Module Menu

void mod_geometry_cb(CALLBACK_ARGS);
void mod_mesh_cb(CALLBACK_ARGS);
void mod_solver_cb(CALLBACK_ARGS);
void mod_post_cb(CALLBACK_ARGS);
void mod_back_cb(CALLBACK_ARGS);
void mod_forward_cb(CALLBACK_ARGS);

// Dynamic Geometry Menus

void geometry_elementary_cb(CALLBACK_ARGS);
void   geometry_elementary_add_cb(CALLBACK_ARGS);
void     geometry_elementary_add_new_cb(CALLBACK_ARGS);
void     geometry_elementary_add_translate_cb(CALLBACK_ARGS);
void     geometry_elementary_add_rotate_cb(CALLBACK_ARGS);
void     geometry_elementary_add_scale_cb(CALLBACK_ARGS);
void     geometry_elementary_add_symmetry_cb(CALLBACK_ARGS);
void   geometry_elementary_delete_cb(CALLBACK_ARGS);
void   geometry_elementary_translate_cb(CALLBACK_ARGS);
void   geometry_elementary_rotate_cb(CALLBACK_ARGS);
void   geometry_elementary_scale_cb(CALLBACK_ARGS);
void   geometry_elementary_symmetry_cb(CALLBACK_ARGS);
void   geometry_elementary_extrude_cb(CALLBACK_ARGS);
void     geometry_elementary_extrude_translate_cb(CALLBACK_ARGS);
void     geometry_elementary_extrude_rotate_cb(CALLBACK_ARGS);
void   geometry_elementary_coherence_cb(CALLBACK_ARGS);
void geometry_physical_cb(CALLBACK_ARGS);
void   geometry_physical_add_cb(CALLBACK_ARGS);
void geometry_edit_cb(CALLBACK_ARGS);
void geometry_reload_cb(CALLBACK_ARGS); 

void con_geometry_define_parameter_cb(CALLBACK_ARGS);
void con_geometry_define_point_cb(CALLBACK_ARGS);
void con_geometry_snap_cb(CALLBACK_ARGS);

// Dynamic Mesh Menus

void mesh_save_cb(CALLBACK_ARGS);
void mesh_define_cb(CALLBACK_ARGS);
void mesh_1d_cb(CALLBACK_ARGS);
void mesh_2d_cb(CALLBACK_ARGS); 
void mesh_3d_cb(CALLBACK_ARGS); 
void mesh_delete_cb(CALLBACK_ARGS);
void mesh_delete_parts_cb(CALLBACK_ARGS);
void mesh_inspect_cb(CALLBACK_ARGS);
void mesh_remesh_cb(CALLBACK_ARGS); 
void mesh_update_edges_cb(CALLBACK_ARGS); 
void mesh_parameterize_cb(CALLBACK_ARGS);
void mesh_degree_cb(CALLBACK_ARGS); 
void mesh_optimize_cb(CALLBACK_ARGS); 
void mesh_optimize_netgen_cb(CALLBACK_ARGS); 
void mesh_classify_cb(CALLBACK_ARGS); 
void mesh_define_length_cb (CALLBACK_ARGS);
void mesh_define_recombine_cb (CALLBACK_ARGS);
void mesh_define_transfinite_cb (CALLBACK_ARGS); 
void mesh_define_transfinite_line_cb(CALLBACK_ARGS);
void mesh_define_transfinite_surface_cb(CALLBACK_ARGS);
void mesh_define_transfinite_volume_cb(CALLBACK_ARGS); 

// Dynamic Solver Menus

void solver_cb(CALLBACK_ARGS);
void solver_file_open_cb(CALLBACK_ARGS);
void solver_file_edit_cb(CALLBACK_ARGS);
void solver_choose_mesh_cb(CALLBACK_ARGS);
void solver_command_cb(CALLBACK_ARGS);
void solver_kill_cb(CALLBACK_ARGS);
void solver_choose_executable_cb(CALLBACK_ARGS);
void solver_ok_cb(CALLBACK_ARGS);

#endif

