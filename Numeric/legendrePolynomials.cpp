#include "legendrePolynomials.h"

LegendrePolynomials::LegendrePolynomials(int o): n(o) {}
LegendrePolynomials::~LegendrePolynomials() {;}

void LegendrePolynomials::f(double u, double *val) const {
  val[0] = 1;
  
  for (int i=0;i<n;i++) {
    
    double a1i = i+1;
    double a3i = 2.*i+1;
    double a4i = i;
    
    val[i+1] = a3i * u * val[i];
    if (i>0) val[i+1] -= a4i * val[i-1];
    val[i+1] /= a1i;
  }
}

void LegendrePolynomials::df(double u, double *val) const {
  double tmp[n+1];
  f(u,tmp);

  val[0] = 0;  
  double g2 = (1.-u*u);
  
  for (int i=1;i<=n;i++) {
    double g1 = - u*i;
    double g0 = (double) i;
  
    val[i] = (g1 * tmp[i] + g0 * tmp[i-1])/g2;
  }
}
