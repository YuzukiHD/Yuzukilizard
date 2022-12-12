
#include "isp_math_util.h"

#define N 8

void HorizontalMirror(int arr[64])
{
	int i, j, temp;

	for(i = 0; i < N; i++) {
		for (j = 0; j < N / 2; j++) {
			temp = arr[i*N+j];
			arr[i*N+j] = arr[i*N+N -j-1];
			arr[i*N+N -j-1] = temp;
		}
	}
}

void VerticalMirror(int arr[64])
{
	int i, j, temp;

	for(i = 0; i < N; i++) {
		for (j = 0; j < N / 2; j++) {
			temp = arr[j*N+i];
			arr[j*N+i]= arr[(N -j-1)*N+i];
			arr[(N -j-1)*N+i] = temp;
		}
	}
}

int ValueInterp(int curr, int x_low, int x_high, int y_low, int y_high)
{
	if ((x_high - x_low) == 0)
		return y_low;

	return (y_low + (y_high - y_low)*(curr - x_low)/(x_high - x_low));
}

void Conv(int u[],int v[],int w[], int m, int n)
{
	// w = u*v; length(u) = m; length(v) = n.
	int i, j;
	int k = m+n-1;

	for(i = 0; i < k; i++)
	{
		for(j=max(0,i+1-n); j<=min(i,m-1); j++)
		{
			w[i] += ((((HW_S64)u[j])*v[i-j])>>10);
		}
	}
}

int ArrayFindMinIndex(int array[], int num)
{
	int i, ind = 0;
	int tmp = array[0];

	for (i = 0; i < num; i++) {
		if (array[i] < tmp) {
			tmp = array[i];
			ind = i;
		}
	}
	return ind;
}

int ArrayFindSecondMinIndex(int array[], int num, int exclude)
{
	int i, ind = 0;
	int tmp = array[0];

	if (exclude == 0) {
		tmp = array[num - 1];
		ind = num - 1;
	}

	for (i = 0; i < num; i++)
	{
		if (array[i] < tmp && i != exclude)
		{
			tmp = array[i];
			ind = i;
		}
	}
	return ind;
}

int ArrayFindMaxIndex(int array[],int num)
{
	int i, ind = 0;
	int tmp = array[0];

	for (i = 0; i < num; i++)
	{
		if (array[i] > tmp)
		{
			tmp = array[i];
			ind = i;
		}
	}
	return ind;
}

int ArrayFindSecondMaxIndex(int array[], int num, int exclude)
{
	int i, ind = 0;
	int tmp = array[0];

	if (exclude == 0) {
		tmp = array[num - 1];
		ind = num - 1;
	}

	for (i = 0; i < num; i++)
	{
		if (array[i] > tmp && i != exclude)
		{
			tmp = array[i];
			ind = i;
		}
	}
	return ind;
}

void ShellSort(int *a,int *sub, int n)
{
	int i, j, k, x, y;
	k = n/2;

	while(k>=1) {
		for(i=k;i<n;i++) {
			x=a[i];
			y = sub[i];
			j=i-k;
			while(j>=0&&x>a[j]) {
				a[j+k]=a[j];
				sub[j+k]=sub[j];
				j-=k;
			}
			a[j+k]=x;
			sub[j+k]=y;
		}
		k/=2;
	}
}

int ArraySum(int *array, int len)
{
	int i, sum = 0;
	for(i = 0;i<len;i++)
	{
		sum += array[i];
	}
	return sum;
}

int SqrtM(unsigned int a)
{
	int x, i = 100, tmp = 0;

	if (a == 0)
		return 0;
	x = (a+1)/2;
	while((i--) && (ABS(tmp-x) > 1)){
		tmp = x;
		x = (x+a/x+1)/2;
	}
	return x;
}

int ArrayStdVar(int x[], int n)
{
	int i, xaver = 0, x2aver = 0;
	for(i=0;i<n;++i)
	{
		xaver+=x[i];
		x2aver+=x[i]*x[i];
	}
	xaver/=n;
	x2aver/=n;
	return (int)sqrt(x2aver-xaver*xaver);
}

int RoundQ4(int x)
{
	int ret;
	//printf("x&0xf = %d\n",x&0xf);
	if(0x8 > (x&0xf) )
	{
		ret = x>>4;
	}
	else
	{
		ret = (x>>4)+1;
	}
	return ret;
}

void matrix_zero(struct matrix *m)
{
	unsigned int i, j;

	for (i = 0; i < 3; ++i) {
		for (j = 0; j < 3; ++j)
			m->value[i][j] = 0.0;
	}
}

void matrix_invert(struct matrix *m)
{
	/* Invert the matrix using the transpose of the matrix of cofactors. The
	 * Gauss-Jordan elimination would be faster in the general case, but we
	 * know that the matrix is 3x3.
	 */
	const float eps = 1e-6;
	struct matrix out;
	unsigned int i, j;
	float det;

	out.value[0][0] = m->value[1][1] * m->value[2][2]
			- m->value[1][2] * m->value[2][1];
	out.value[0][1] = m->value[0][2] * m->value[2][1]
			- m->value[0][1] * m->value[2][2];
	out.value[0][2] = m->value[0][1] * m->value[1][2]
			- m->value[0][2] * m->value[1][1];
	out.value[1][0] = m->value[1][2] * m->value[2][0]
			- m->value[1][0] * m->value[2][2];
	out.value[1][1] = m->value[0][0] * m->value[2][2]
			- m->value[0][2] * m->value[2][0];
	out.value[1][2] = m->value[0][2] * m->value[1][0]
			- m->value[0][0] * m->value[1][2];
	out.value[2][0] = m->value[1][0] * m->value[2][1]
			- m->value[1][1] * m->value[2][0];
	out.value[2][1] = m->value[0][1] * m->value[2][0]
			- m->value[0][0] * m->value[2][1];
	out.value[2][2] = m->value[0][0] * m->value[1][1]
			- m->value[1][0] * m->value[0][1];

	det = m->value[0][0] * out.value[0][0] +
	      m->value[0][1] * out.value[1][0] +
	      m->value[0][2] * out.value[2][0];

	if (det < eps)
		return;

	det = 1/det;

	for (i = 0; i < 3; ++i)
		for (j = 0; j < 3; ++j)
			m->value[i][j] = out.value[i][j] * det;
}

void matrix_multiply(struct matrix *a, const struct matrix *b)
{
	struct matrix out;

	/* Compute a * b and return the result in a. */
	out.value[0][0] = a->value[0][0] * b->value[0][0]
			+ a->value[0][1] * b->value[1][0]
			+ a->value[0][2] * b->value[2][0];
	out.value[0][1] = a->value[0][0] * b->value[0][1]
			+ a->value[0][1] * b->value[1][1]
			+ a->value[0][2] * b->value[2][1];
	out.value[0][2] = a->value[0][0] * b->value[0][2]
			+ a->value[0][1] * b->value[1][2]
			+ a->value[0][2] * b->value[2][2];
	out.value[1][0] = a->value[1][0] * b->value[0][0]
			+ a->value[1][1] * b->value[1][0]
			+ a->value[1][2] * b->value[2][0];
	out.value[1][1] = a->value[1][0] * b->value[0][1]
			+ a->value[1][1] * b->value[1][1]
			+ a->value[1][2] * b->value[2][1];
	out.value[1][2] = a->value[1][0] * b->value[0][2]
			+ a->value[1][1] * b->value[1][2]
			+ a->value[1][2] * b->value[2][2];
	out.value[2][0] = a->value[2][0] * b->value[0][0]
			+ a->value[2][1] * b->value[1][0]
			+ a->value[2][2] * b->value[2][0];
	out.value[2][1] = a->value[2][0] * b->value[0][1]
			+ a->value[2][1] * b->value[1][1]
			+ a->value[2][2] * b->value[2][1];
	out.value[2][2] = a->value[2][0] * b->value[0][2]
			+ a->value[2][1] * b->value[1][2]
			+ a->value[2][2] * b->value[2][2];

	*a = out;
}

void matrix_float_to_s2q8(HW_S8Q8 out[3][3], const struct matrix *in)
{
	unsigned int i, j;

	for (i = 0; i < 3; ++i) {
		for (j = 0; j < 3; ++j)
			out[i][j] = ((HW_S8Q8)(in->value[i][j] * 256) & 0x3ff);
	}
}

void matrix_float_to_s4q8(HW_S8Q8 out[3][3], const struct matrix *in)
{
	unsigned int i, j;

	for (i = 0; i < 3; ++i) {
		for (j = 0; j < 3; ++j)
			out[i][j] = ((HW_S8Q8)(in->value[i][j] * 256) & 0xfff);
	}
}

void matrix_float_to_s8q8(HW_S8Q8 out[3][3], const struct matrix *in)
{
	unsigned int i, j;

	for (i = 0; i < 3; ++i) {
		for (j = 0; j < 3; ++j)
			out[i][j] = ((HW_S8Q8)(in->value[i][j] * 256));
	}
}

void spline(double x[], double y[], int n, double yp1,double ypn, double y2[])
{
	double u[100],aaa,sig,p,bbb,ccc,qn,un;
	int i,k;
	if( yp1 > 9.9e+29 )
	{
		y2[1] = 0;
		u[1] = 0;
	}
	else
	{
		y2[1] = -0.5;
		aaa = (y[2] - y[1]) / (x[2] - x[1]);
		u[1] = (3.0 / (x[2] - x[1])) * (aaa - yp1);
	}
	for (i = 2; i<=n-1; i++)
	{
		sig = (x[i] - x[i - 1]) / (x[i + 1] - x[i - 1]);
		p = sig * y2[i - 1] + 2.0;
		y2[i] = (sig - 1.0) / p;
		aaa = (y[i + 1] - y[i]) / (x[i + 1] - x[i]);
		bbb = (y[i] - y[i - 1]) / (x[i] - x[i - 1]);
		ccc = x[i + 1] - x[i - 1];
		u[i] = (6.0 * (aaa - bbb) / ccc - sig * u[i - 1]) / p;
	}
	if (ypn > 9.9e+29 )
	{
		qn = 0.0;
		un = 0.0;
	}
	else
	{
		qn = 0.5;
		aaa = ypn - (y[n] - y[n - 1]) / (x[n] - x[n - 1]);
		un = (3.0/ (x[n] - x[n - 1])) * aaa;
	}
	y2[n] = (un - qn * u[n - 1]) / (qn * y2[n - 1] + 1.0);
	for (k = n - 1;k>=1;k--)
		y2[k] = y2[k] * y2[k + 1] + u[k];
}

void splint(double xa[], double ya[], double y2a[], int n, double x, double *y)
{
	int klo,khi,k;
	double h,a,b,aaa,bbb;
	klo = 1;
	khi = n;
loop:
	if (khi - klo > 1 )
	{
		k = (khi + klo) / 2;
		if (xa[k] > x)
			khi = k;
		else
		{
			klo = k;
		}
		goto loop;
	}
	h = xa[khi] - xa[klo];
	if (h == 0 )
	{
		printf("  pause  'bad  xa  input'");
		return;
	}
	a = (xa[khi] - x) / h;
	b = (x - xa[klo]) / h;
	aaa = a * ya[klo] + b * ya[khi];
	bbb = (a*a*a - a) * y2a[klo] + (b*b*b - b) * y2a[khi];
	*y = aaa + bbb * h*h /6.0;
}

void spline_interp_u16(unsigned short x[], unsigned short y[], int n,
				unsigned short x2[], unsigned short y2[], int n2)
{
	int i;
	double *xa, *ya, *y_sec, y_temp;
	xa = malloc((n+1) * sizeof(double));
	ya = malloc((n+1) * sizeof(double));
	y_sec = malloc((n+1) * sizeof(double));

	xa[0] = ya[0] = y_sec[0] = 0;
	for (i = 1; i <= n; i++) {
		xa[i] = (double)x[i-1];
		ya[i] = (double)y[i-1];
		y_sec[i] = 0;
	}
	spline(xa, ya, n, 1e+31, 1e+31, y_sec);
	for (i = 0; i < n2; i++) {
		splint(xa, ya, y_sec, n, (double)(x2[i]), &y_temp);
		y2[i] = (unsigned short)(y_temp+0.5);
	}
#if 0
	printf("\n");

	for (i = 0; i < n2; i++) {
		printf("%d,", y2[i]);
	}

	printf("\n");
#endif
	free(xa);
	free(ya);
	free(y_sec);
}

