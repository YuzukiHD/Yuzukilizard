
#ifndef _ISP_MATH_UTIL_H_
#define _ISP_MATH_UTIL_H_

#include "../include/isp_type.h"
#include "matrix.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define square(x)	((x) * (x))
#define clear(x)	memset (&(x), 0, sizeof (x))

#define array_size(array)	(sizeof(array) / sizeof((array)[0]))

#define min(a, b) ({				\
	typeof(a) __a = (a);			\
	typeof(b) __b = (b);			\
	__a < __b ? __a : __b;			\
})

#define min_t(type, a, b) ({			\
	type __a = (a);				\
	type __b = (b);				\
	__a < __b ? __a : __b;			\
})

#define max(a, b) ({				\
	typeof(a) __a = (a);			\
	typeof(b) __b = (b);			\
	__a > __b ? __a : __b;			\
})

#define max_t(type, a, b) ({			\
	type __a = (a);				\
	type __b = (b);				\
	__a > __b ? __a : __b;			\
})

#define clamp(val, min, max) ({			\
	typeof(val) __val = (val);		\
	typeof(min) __min = (min);		\
	typeof(max) __max = (max);		\
	__val = __val < __min ? __min : __val;	\
	__val > __max ? __max : __val;		\
})

#define clamp_t(type, val, min, max) ({		\
	type __val = (val);			\
	type __min = (min);			\
	type __max = (max);			\
	__val = __val < __min ? __min : __val;	\
	__val > __max ? __max : __val;		\
})

#define div_round_up(num, denom)	(((num) + (denom) - 1) / (denom))
#define div_round(num, denom)	(((num) + ((denom) >> 1)) / (denom))

#define Q4		16
#define Q8		256
#define Q10		1024

#define SHIFT_Q4	4
#define SHIFT_Q8	8
#define SHIFT_Q10	10

#define R_SHIFT_N(x,n)		(((x) + (1 << (n-1)))>>(n))
#define L_SHIFT_N(x,n)		((x)<<(n))
#define IS_BETWEEN(x,a,b)	(((x)>=(a)) && ((x)<=(b)))

#define N_ROUND(x,y)		(((x)/(y))*(y))

#define ABS(x) ({						\
		long ret;					\
		if (sizeof(x) == sizeof(long)) {		\
			long __x = (x);				\
			ret = (__x < 0) ? -__x : __x;		\
		} else {					\
			int __x = (x);				\
			ret = (__x < 0) ? -__x : __x;		\
		}						\
		ret;						\
	})

void HorizontalMirror(int arr[64]);
void VerticalMirror(int arr[64]);

int ValueInterp(int curr, int x_low,int x_high,int y_low,int y_high);
void Conv(int u[],int v[],int w[], int m, int n);

int ArrayFindMinIndex(int array[], int num);
int ArrayFindSecondMinIndex(int array[], int num, int exclude);
int ArrayFindMaxIndex(int array[],int num);
int ArrayFindSecondMaxIndex(int array[], int num, int exclude);

int ArraySum(int *array,int len);
int ArrayStdVar(int x[], int n);

void ShellSort(int *a,int *sub, int n);

int SqrtM(unsigned int a);

int RoundQ4(int x);

struct matrix {
	float value[3][3];
};

void matrix_zero(struct matrix *m);
void matrix_invert(struct matrix *m);
void matrix_multiply(struct matrix *a, const struct matrix *b);
void matrix_float_to_s2q8(HW_S8Q8 out [3][3], const struct matrix *in);
void matrix_float_to_s4q8(HW_S8Q8 out[3][3], const struct matrix *in);
void matrix_float_to_s8q8(HW_S8Q8 out[3][3], const struct matrix *in);

void spline(double x[], double y[], int n, double yp1,double ypn, double y2[]);
void splint(double xa[], double ya[], double y2a[], int n, double x, double *y);
void spline_interp_u16(unsigned short x[], unsigned short y[], int n,
				unsigned short x2[], unsigned short y2[], int n2);

#endif /*_ISP_MATH_UTIL_H_*/

