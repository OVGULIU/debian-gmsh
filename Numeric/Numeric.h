// Gmsh - Copyright (C) 1997-2008 C. Geuzaine, J.-F. Remacle
//
// See the LICENSE.txt file for license information. Please report all
// bugs and problems to <gmsh@geuz.org>.

#ifndef _NUMERIC_H_
#define _NUMERIC_H_

#include "NumericEmbedded.h"

int check_gsl();
void invert_singular_matrix3x3(double MM[3][3], double II[3][3]);

// Numerical routines implemented using either Numerical Recipes or
// the GSL
double brent(double ax, double bx, double cx,
             double (*f)(double), double tol, double *xmin);
void mnbrak(double *ax, double *bx, double *cx, double *fa, double *fb,
            double *fc, double (*func)(double));
void newt(double x[], int n, int *check,
          void (*vecfunc)(int, double [], double []));
void minimize_2 (double (*f) (double, double, void *data), 
                 void (*df) (double, double, double &, double &, double &, void *data) ,
                 void *data,int niter,
                 double &U, double &V, double &res);
void minimize_3 (double (*f) (double, double, double, void *data), 
                 void (*df) (double, double, double , double &, double &, 
                             double &, double &, void *data),
                 void *data,int niter,
                 double &U, double &V, double &W, double &res);
void minimize_N (int N, 
                 double (*f) (double*, void *data), 
                 void (*df) (double*, double*, double &, void *data) ,
                 void *data,int niter,
                 double *, double &res);

// Robust geometrical predicates
namespace gmsh{
double exactinit();
double incircle(double *pa, double *pb, double *pc, double *pd);
double insphere(double *pa, double *pb, double *pc, double *pd, double *pe);
double orient2d(double *pa, double *pb, double *pc);
double orient3d(double *pa, double *pb, double *pc, double *pd);
}

#endif
