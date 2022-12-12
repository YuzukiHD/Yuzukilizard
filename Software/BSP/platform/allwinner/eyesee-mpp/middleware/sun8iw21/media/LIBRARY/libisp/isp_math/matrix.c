#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>
#include "matrix.h"

#define EPSILON 0.000001

bool isEqual(double a, double b)
{
	return fabs(a - b) < EPSILON;
}

int check_input(double* x, double*y, char* c) {
        int out=0;
        if (x==y) {
                printf("Error in %s: input equals output \n", c);
                out=1;
        }
        return out;
}

void mat2cvec(int m, int n, double** mat, double* cvec) {
        /*
                Turns a matrix (given by a double pointer) into its C vector format
                (single vector, rowwise). The matrix "mat" needs to be an n*m matrix
                The vector "vec" is a vector of lenght nm
	*/
        int i,j;
        for (i=0; i<m; i++) for (j=0; j<n; j++) cvec[i*n+j] = mat[i][j];
}

void mat2fvec(int m, int n, double** mat, double* fvec) {
        /*
                Turns a matrix (given by a double pointer) into its Fortran vector format
                (single vector, colunmwise). The matrix "mat" needs to be an m*n matrix
                The vector "vec" is a vector of lenght mn
	*/
        int i,j;
        for (i=0; i<m; i++) for (j=0; j<n; j++) fvec[i+m*j] = mat[i][j];
}

void cvec2mat(int m, int n, double* cvec, double** mat) {
        /*
                Turns a matrix in C vector format into a matrix given by a double pointer
                The matrices have dimension m*n
	*/
        int i,j;
        for (i=0; i<m; i++) for (j=0; j<n; j++)  mat[i][j] = cvec[i*n+j];
}

void fvec2mat(int m, int n, double* fvec, double** mat) {
        /*
                Turns a matrix in C vector format into a matrix given by a double pointer
                The matrices have dimension m*n
	*/
        int i,j;
        for (i=0; i<m; i++) for (j=0; j<n; j++)  mat[i][j] = fvec[i+m*j];
}

void cvec2fvec(int m, int n, double *cvec, double* fvec) {
        /*
                Turn matrix in C vector format into matrix in Fortran vector format
                Matrix has dimension m*n
	*/
        int i,j;
        check_input(cvec, fvec, "cvec2fvec");
        for (i=0; i<m; i++) for (j=0; j<n; j++) fvec[i+m*j] = cvec[n*i+j];
}

void fvec2cvec(int m, int n, double *fvec, double* cvec) {
        /*
                Turn matrix in Fortran vector format into matrix in C vector format
                Matrix has dimension m*n
	*/
        int i,j;
        check_input(cvec, fvec, "fvec2cvec");
        for (i=0; i<m; i++) for (j=0; j<n; j++) cvec[n*i+j] = fvec[i+m*j];
}

Matrix *create_matrix(int lines, int columns)
{
        int i;

	Matrix *a = malloc(sizeof(struct Matrix));
	if(a == NULL) {
		return NULL;
	}

	// set matrix atributes
	a->lines       = lines;
	a->columns     = columns;
	a->determinant = 0.0;
	a->trace       = 0.0;
	a->inverse     = NULL;

	// set matrix's value
	a->value = malloc(lines * sizeof(double *));
	if(a->value == NULL) {
		free(a);
		return NULL;
	}

	for(i = 0; i < lines; i++) {
		a->value[i] = calloc(columns, sizeof(double));
		if(a->value == NULL) {
			return NULL;
		}
	}

	return a;
}

void destroy_matrix(Matrix *a)
{
	int lines, i;

	if(a == NULL)
		return;

	lines = a->lines;

	// free the value of matrix
	for(i = 0; i < lines; i++) {
		free(a->value[i]);
	}
	free(a->value);

	if(a->inverse != NULL) {
		destroy_matrix(a->inverse);
	}
	free(a);
}

void copy_matrix(Matrix *a, Matrix **destination)
{
        int i, j;

	for(i = 0; i < a->lines; i++) {
		for(j = 0; j < a->columns; j++) {
			(*destination)->value[i][j] = a->value[i][j];
		}
	}
}

void read_matrix(Matrix *a)
{
        int i, j;
	int lines    = a->lines;
	int columns = a->columns;

	for(i = 0; i < lines; i++) {
		for(j = 0; j < columns; j++) {
			printf("introduceti elementul a[%d][%d]: ", i, j);
			scanf("%lf", &a->value[i][j]);
		}
	}
}

int print_matrix(Matrix *a, int lines, int columns)
{
        int i, j;

	if(a == NULL) {
		return MATRIX_NOT_EXISTS;
	}

	if (lines == 0 || columns == 0) {
		lines   = a->lines;
		columns = a->columns;
	}

	for(i = 0; i < lines; i++) {
		for(j = 0; j < columns; j++) {
			printf("%lf ", a->value[i][j]);
		}
		printf("\n");
	}

	return NO_ERROR;
}

bool compare_matrices(Matrix *a, Matrix *b)
{
        int i, j;

	// To be equal 2 matrices shouls have the same dimensions
	if(a->lines != b->lines)
		return 0;
	if(a->columns != b->columns)
		return 0;

	// They have the same dimensions, now we should check to see if
	// they have the same values
	for(i = 0; i < a->lines; i++) {
		for(j = 0; j < a->columns; j++) {
			if(!isEqual(a->value[i][j], b->value[i][j]))
				return false;
		}
	}
	return true;
}


Matrix *get_minor(Matrix *a, int line, int columns)
{
	int i = 0, j = 0, x = 0, y = 0;

	Matrix *minor = create_matrix(a->lines - 1, a->columns - 1);
	if(minor == NULL) {
		return NULL;
	}

	for(i = 0; i < a->lines; i++) {
		if(i == line)
			x--;
		for(j = 0; j < a->columns; j++) {
			if(i != line && j != columns) {
				minor->value[x][y] = a->value[i][j];
				y++;
			}
		}
		x++;
		y = 0;
	}

	return minor;
}

double get_determinant(Matrix *a)
{
	double det;
	Matrix *m;
	int i = 0;

	if (a->lines == 1 && a->columns == 1) {
		return a->value[0][0];
	}
	else {
	  det = 0;
	  for (i = 0; i < a->columns; i++) {
	      m = get_minor(a, 0, i);
	      det += (a->value[0][i]) * (pow(-1, 2 + i)) * (get_determinant(m));
	      destroy_matrix(m);
	  }
	  return det;
	}
}


int get_transpose(Matrix *a, Matrix **transpose)
{
	int i = 0, j = 0;

	*transpose = create_matrix(a->columns, a->lines);
	if(*transpose == NULL) {
		return CANT_CREATE_MATRIX;
	}

	for(i = 0; i < a->lines; i++) {
		for(j = 0; j < a->columns; j++) {
			(*transpose)->value[j][i] = a->value[i][j];
		}
	}

	return NO_ERROR;
}


void multiply_matrix_with_scalar(Matrix *a, double scalar)
{
	int i = 0, j = 0;

	for(i = 0; i < a->lines; i++) {
		for(j = 0; j < a->columns; j++) {
			a->value[i][j] *= scalar;
		}
	}
}

int compute_inverse(Matrix *a)
{
	int i = 0, j = 0;

	if(a->determinant == 0.0) {
		return NO_INVERSE;
	}

	Matrix *transpose;
	Matrix *adjugate = create_matrix(a->lines, a->columns);
	Matrix *current_minor;

	get_transpose(a, &transpose);
	for(i = 0; i < transpose->lines; i++) {
		for(j = 0; j < transpose->columns; j++) {
			current_minor         = get_minor(transpose, i, j);
			adjugate->value[i][j] = pow(-1.0, (double)i + (double)j) *
				get_determinant(current_minor);

			destroy_matrix(current_minor);
		}
	}
	destroy_matrix(transpose);

	a->inverse = adjugate;
	multiply_matrix_with_scalar(a->inverse, 1/a->determinant);

	return NO_ERROR;
}

int multiply_matrices(Matrix *a, Matrix *b,
					  Matrix **result)
{
	int i = 0, j = 0, k = 0, columns = 0, x = 0, y = 0;

	if(a->columns != b->lines) {
		return SIZE_NOT_MATCH;
	}

	*result = create_matrix(a->lines, b->columns);
	if(*result == NULL) {
		return CANT_CREATE_MATRIX;
	}

	for(i = 0; i < a->lines; i++) {
		y = 0;
		for(k = 0, columns = b->columns; columns > 0; columns--) {
			for(j = 0; j < a->columns; j++) {
				(*result)->value[x][y] += a->value[i][j] * b->value[j][k];
			}
			k++;
			y++;
		}
		x++;
	}

	return NO_ERROR;
}

Matrix *matrix_pow(Matrix *a, int power)
{
	int i = 0;

	Matrix *result = create_matrix(a->lines, a->columns);
	copy_matrix(a, &result);

	for(i = 0; i < power-1; i++) {
		multiply_matrices(result, a, &result);
	}

	return result;
}

int add_matrices(Matrix *a, Matrix *b, Matrix **result)
{
	int i = 0, j = 0;

	if(a->lines != b->lines && a->columns != b->columns) {
		return NOT_SAME_SIZE;
	}

	*result = create_matrix(a->lines, a->columns);

	for(i = 0; i < a->lines; i++) {
		for(j = 0; j < a->columns; j++) {
			(*result)->value[i][j] = a->value[i][j] + b->value[i][j];
		}
	}

	return NO_ERROR;
}

int compute_trace(Matrix *a)
{
	int i = 0;

	if(a->columns != a->lines) {
		return NOT_SQUARE_MATRIX; //error
	}

	for(i = 0; i < a->columns; i++) {
		a->trace += a->value[i][i];
	}

	return NO_ERROR;
}

/**
 * Bilinear resize grayscale image.
 * pixels is an array of size w * h.
 * Target dimension is w2 * h2.
 * w2 * h2 cannot be zero.
 *
 * @param pixels Image pixels.
 * @param w Image width.
 * @param h Image height.
 * @param w2 New width.
 * @param h2 New height.
 * @return New array with size w2 * h2.
 */
int matrix_bilinear_resize(Matrix *pixels, Matrix *output, int w, int h, int w2, int h2)
{
	int x, y, i, j;
	double A, B, C, D, gray ;
	double x_ratio = ((double)(w-1))/w2 ;
	double y_ratio = ((double)(h-1))/h2 ;
	double x_diff, y_diff;

	for (i = 0; i < h2; i++) {
		for (j = 0; j < w2; j++) {
			x = (int)(x_ratio * j);
			y = (int)(y_ratio * i);
			x_diff = (x_ratio * j) - x;
			y_diff = (y_ratio * i) - y;

			A = pixels->value[y][x];
			B = pixels->value[y][x+1];
			C = pixels->value[y+1][x];
			D = pixels->value[y+1][x+1];

			// Y = A(1-w)(1-h) + B(w)(1-h) + C(h)(1-w) + Dwh
			gray = (double)(
				A*(1-x_diff)*(1-y_diff) +  B*(x_diff)*(1-y_diff) +
				C*(y_diff)*(1-x_diff)   +  D*(x_diff*y_diff)
				);

			output->value[i][j] = gray;
		}
	}

	return 0;
}


