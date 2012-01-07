// Gmsh - Copyright (C) 1997-2010 C. Geuzaine, J.-F. Remacle
//
// See the LICENSE.txt file for license information. Please report all
// bugs and problems to <gmsh@geuz.org>.

#ifndef _MESH_GFACE_LLOYD_H_
#define _MESH_GFACE_LLOYD_H_

#include "GmshConfig.h"

#if defined(HAVE_BFGS)

#include "fullMatrix.h"
#include "DivideAndConquer.h"
#include <queue>

class GFace;
class voronoi_vertex;
class voronoi_element;
class voronoi_cell;  
class segment;
class segment_list;
class metric;


class smoothing{
  int ITER_MAX;
  int NORM;
 public :
  smoothing(int,int);
  void optimize_face(GFace*);
  void optimize_model();
};

class lpcvt{
 private :
  std::vector<voronoi_element> clipped;
  std::queue<int> fifo;
  std::vector<segment_list> borders;
  std::vector<double> angles;
  std::vector<voronoi_cell> temp;
  fullMatrix<double> gauss_points;
  fullMatrix<double> gauss_weights;
  int gauss_num;
 public :
  lpcvt();
  ~lpcvt();
  double angle(SPoint2,SPoint2,SPoint2);
  SVector3 normal(SPoint2,SPoint2);
  SPoint2 mid(SPoint2,SPoint2);
  bool same_side(SPoint2,SPoint2,SPoint2,SPoint2);
  bool interior(DocRecord&,GFace*,int);
  bool interior(DocRecord&,segment,segment,double,SPoint2);
  bool invisible(DocRecord&,GFace*,int);
  bool real(DocRecord&,int,int,int);
  double triangle_area(SPoint2,SPoint2,SPoint2);
  bool sliver(SPoint2,SPoint2,SPoint2);
  SPoint2 intersection(DocRecord&,segment,segment,SPoint2,SPoint2,bool&,SVector3&,segment&);
  SPoint2 intersection(SPoint2,SPoint2,SPoint2,SPoint2,bool&);
  SPoint2 convert(DocRecord&,int);
  SPoint2 circumcircle(DocRecord&,int,int,int);
  SPoint2 seed(DocRecord&,GFace*);
  void step1(DocRecord&,GFace*);
  void step2(DocRecord&,GFace*);
  void step3(DocRecord&,GFace*);
  void step4(DocRecord&,GFace*);
  void step5(DocRecord&,GFace*);
  void clip_cells(DocRecord&,GFace*);
  void clear();
  double total_area();
  void print_voronoi1();
  void print_voronoi2();
  void print_delaunay(DocRecord&);
  void print_segment(SPoint2,SPoint2,std::ofstream&);

  void compute_parameters(GFace*,int);
  double get_ratio(GFace*,SPoint2);
  void write(DocRecord&,GFace*,int);
  void eval(DocRecord&,std::vector<SVector3>&,double&,int);
  void swap();
  void get_gauss();
  double F(voronoi_element,int);
  SVector3 simple(voronoi_element,int);
  SVector3 dF_dC1(voronoi_element,int);
  SVector3 dF_dC2(voronoi_element,int);
  double f(SPoint2,SPoint2,metric,int);
  double df_dx(SPoint2,SPoint2,metric,int);
  double df_dy(SPoint2,SPoint2,metric,int);
  double Tx(double,double,SPoint2,SPoint2,SPoint2);
  double Ty(double,double,SPoint2,SPoint2,SPoint2);
  double J(SPoint2,SPoint2,SPoint2);
  SVector3 inner_dFdx0(SVector3,SPoint2,SPoint2,SPoint2,SPoint2);
  SVector3 boundary_dFdx0(SVector3,SPoint2,SPoint2,SPoint2,SVector3);
};

class metric{
 private :
  double a,b,c,d;
 public :
  metric(double,double,double,double);
  metric();
  ~metric();
  void set_a(double);	
  void set_b(double);	
  void set_c(double);	
  void set_d(double);	
  double get_a();	
  double get_b();	
  double get_c();	
  double get_d();
};

class voronoi_vertex{
 private :
  SPoint2 point;
  int index1;
  int index2;
  int index3;
  SVector3 normal;
  bool duplicate;
  double rho;
 public :
  voronoi_vertex(SPoint2);
  voronoi_vertex();
  ~voronoi_vertex();
  SPoint2 get_point();
  int get_index1();
  int get_index2();
  int get_index3();
  SVector3 get_normal();
  bool get_duplicate();
  double get_rho();
  void set_point(SPoint2);
  void set_index1(int);
  void set_index2(int);
  void set_index3(int);
  void set_normal(SVector3);
  void set_duplicate(bool);
  void set_rho(double);
};

class voronoi_element{
 private :
  voronoi_vertex v1;
  voronoi_vertex v2;
  voronoi_vertex v3;
  double drho_dx;
  double drho_dy;
  metric m;
 public :
  voronoi_element(voronoi_vertex,voronoi_vertex,voronoi_vertex);
  voronoi_element();
  ~voronoi_element();
  voronoi_vertex get_v1();
  voronoi_vertex get_v2();
  voronoi_vertex get_v3();
  double get_rho(double,double);
  double get_drho_dx();
  double get_drho_dy();
  metric get_metric();
  void set_v1(voronoi_vertex);
  void set_v2(voronoi_vertex);
  void set_v3(voronoi_vertex);
  void set_metric(metric);
  void deriv_rho(int);
  double compute_rho(double,int);
};

class voronoi_cell{
 private :
  std::vector<voronoi_vertex> vertices;
 public :
  voronoi_cell();
  ~voronoi_cell();
  int get_number_vertices();
  voronoi_vertex get_vertex(int);
  void add_vertex(voronoi_vertex);
  void clear();
};

class segment{
 private :
  int index1;
  int index2;
  int reference;
 public :
  segment(int,int,int);
  segment();
  ~segment();
  int get_index1();
  int get_index2();
  int get_reference();
  void set_index1(int);
  void set_index2(int);
  void set_reference(int);
  bool equal(int,int);
};

class segment_list{
 private :
  std::vector<segment> segments;
 public :
  segment_list();
  ~segment_list();
  int get_number_segments();
  segment get_segment(int);
  bool add_segment(int,int,int);
  bool add_segment(segment);
};

#endif

#endif
