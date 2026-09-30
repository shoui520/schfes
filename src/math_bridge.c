#include <math.h>
#include <stdlib.h>
#include <time.h>
#include "boundary.h"
SOFT_ENTRY double sif_acos(double x) { return acos(x); }
SOFT_ENTRY double sif_asin(double x) { return asin(x); }
SOFT_ENTRY double sif_atan(double x) { return atan(x); }
SOFT_ENTRY double sif_ceil(double x) { return ceil(x); }
SOFT_ENTRY double sif_cos(double x) { return cos(x); }
SOFT_ENTRY double sif_cosh(double x) { return cosh(x); }
SOFT_ENTRY double sif_exp(double x) { return exp(x); }
SOFT_ENTRY double sif_floor(double x) { return floor(x); }
SOFT_ENTRY double sif_math_log(double x) { return log(x); }
SOFT_ENTRY double sif_log10(double x) { return log10(x); }
SOFT_ENTRY double sif_sin(double x) { return sin(x); }
SOFT_ENTRY double sif_sinh(double x) { return sinh(x); }
SOFT_ENTRY double sif_sqrt(double x) { return sqrt(x); }
SOFT_ENTRY double sif_tan(double x) { return tan(x); }
SOFT_ENTRY double sif_tanh(double x) { return tanh(x); }
SOFT_ENTRY float sif_acosf(float x) { return acosf(x); }
SOFT_ENTRY float sif_ceilf(float x) { return ceilf(x); }
SOFT_ENTRY float sif_cosf(float x) { return cosf(x); }
SOFT_ENTRY float sif_floorf(float x) { return floorf(x); }
SOFT_ENTRY float sif_logf(float x) { return logf(x); }
SOFT_ENTRY float sif_roundf(float x) { return roundf(x); }
SOFT_ENTRY float sif_sinf(float x) { return sinf(x); }
SOFT_ENTRY float sif_sqrtf(float x) { return sqrtf(x); }
SOFT_ENTRY float sif_truncf(float x) { return truncf(x); }
SOFT_ENTRY double sif_atan2(double x,double y) { return atan2(x,y); }
SOFT_ENTRY double sif_fmax(double x,double y) { return fmax(x,y); }
SOFT_ENTRY double sif_fmin(double x,double y) { return fmin(x,y); }
SOFT_ENTRY double sif_fmod(double x,double y) { return fmod(x,y); }
SOFT_ENTRY double sif_pow(double x,double y) { return pow(x,y); }
SOFT_ENTRY float sif_atan2f(float x,float y) { return atan2f(x,y); }
SOFT_ENTRY float sif_fmaxf(float x,float y) { return fmaxf(x,y); }
SOFT_ENTRY float sif_fmodf(float x,float y) { return fmodf(x,y); }
SOFT_ENTRY double sif_frexp(double x,int *y) { return frexp(x,y); }
SOFT_ENTRY double sif_ldexp(double x,int y) { return ldexp(x,y); }
SOFT_ENTRY double sif_modf(double x,double *y) { return modf(x,y); }
SOFT_ENTRY double sif_strtod(const char *x,char **y) { return strtod(x,y); }
SOFT_ENTRY double sif_difftime(int x,int y) { return difftime(x,y); }
SOFT_ENTRY int sif___fpclassifyd(double x) { return fpclassify(x); }
SOFT_ENTRY int sif___isinf(double x) { return isinf(x); }
SOFT_ENTRY int sif_isnan(double x) { return isnan(x); }
SOFT_ENTRY int sif_isnanf(float x) { return isnan(x); }
