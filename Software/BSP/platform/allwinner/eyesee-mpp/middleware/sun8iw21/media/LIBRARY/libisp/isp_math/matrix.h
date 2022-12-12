#ifndef MATRICI_H
#define MATRICI_H

#include <stdbool.h>

enum ERROR_CODE {
	NOT_SQUARE_MATRIX  = -1,
	NO_INVERSE         = -2,
	NO_ERROR           =  1,
	MATRIX_NOT_EXISTS  = -3,
	NOT_SAME_SIZE      = -4,
	SIZE_NOT_MATCH     = -5,
	CANT_CREATE_MATRIX = -6,
};

/**
 * @Matrix
 *
 * @brief The basic data structure used by this library to represent a matrix.
 *
 * @var Matrix::line The number of lines of a matrix.
 * @var Matrix::columns The number of columns of a matrix.
 * @var Matrix::determinant The determinant of a matrix.
 * @var Matrix::trace The trace of a matrix.
 * @var Matrix::value Pointer to a dynamically alocated 2 dimensional array.
 * @var Matrix::inverse Pointer to a Matrix which represents the inverse
 *      of the current matrix.
 */
typedef struct Matrix {
	int lines;
	int columns;

	double determinant;
	double trace;

	double **value;
	struct Matrix *inverse;
} Matrix;

void mat2cvec(int m, int n, double** mat, double* cvec);

void mat2fvec(int m, int n, double** mat, double* fvec);

void cvec2mat(int m, int n, double* cvec, double** mat);

void fvec2mat(int m, int n, double* fvec, double** mat);

void cvec2fvec(int m, int n, double *cvec, double* fvec);

void fvec2cvec(int m, int n, double *fvec, double* cvec);

/**
 * @brief Returns a pointer to a Matrix with `rows` rows and `columns`
 *        columns. Note that the new matrix is empty, but it's elemnts are, by
 *        default 0.
 *
 * @param rows The number of rows of the matrix.
 * @param columns The number of columns of the matrix.
 *
 * @return Matrix A pointer to an empty Matrix.
 */
Matrix *create_matrix(int rows, int columns);

/**
 * @brief Utility function used to read a matrix from the standard input.
 *
 * @param a The matrix in which the readed elements will be stored.
 *
 * @retrun void It doesn't return anything.
 */
void read_matrix(Matrix *a);

/**
 * @brief utility function used to print a matrix to the standar output.
 *
 * @param a The matrix that should be printed.
 *
 * @return int If the matrix doesn't exists it will return MATRIX_NOT_EXISTS
 *         and NO_ERROR if the matrix was successfully printed.
 */
int print_matrix(Matrix *a, int lines, int columns);

/**
 * @brief Returns the minorant of a matrix by a specific row and column.
 *
 * @param a The matrix that we want to compute the minorant of.
 * @param line The line That should be elimined when selecting the minorant.
 * @param colum The column that should be elimined when computing the minorant.
 *
 * @return Matrix * It returns NULL or a pointer o to a Matrix
 *         that represents the minorant of the matrix `a`.
 */
Matrix *get_minor(Matrix *a, int line, int column);

/**
 * @brief Multiplies a matrix `a` with an scalar. Note that the new value of
 *        the matrix will be the result of this operation.
 *
 * @param a The matrix that should be multiplied with an scalar.
 * @param scalar The scalar that should be multiplied withe the matrix `a`.
 *
 * @return void It doesn't return anything.
 */
void multiply_matrix_with_scalar(Matrix *a, double scalar);

/**
 * @brief Stores the transpose of the matrix `a` in the matrix `transpose`.
 *
 * @param a The matrix that the transpose of we will compute.
 * @param transpose This matrix will hold the value of the matrix `a`. Note
 *        that you should pass a reference to the matrix transpose. That's
 *        the way a call to this function would look like:
 *        `get_transpose(some_matrix, &transpose_of_a)`. Also, keep in mind
 *        that the matrix `transpose` doesn't need to be initialized, the
 *        function will do that for you;
 *
 * @return int CANT_CREATE_MATRIX if the `transpose` can't be created and
 *         NO_ERROR if all wen ok.
 */
int get_transpose(Matrix *a, Matrix **transpose);

/**
 * @bried Dealocates the memory allocated for a matrix, after calling this
 *        function on a matrix all memory used for that matrix will be
 *        dealocated, this includes, also, the memory alocate for it's inverse.
 *
 * @param a The matrix that should be destroyed.
 *
 * @return void It doesn't return anything.
 */
void destroy_matrix(Matrix *a);

/**
 * @brief Computes the inverse of the matrix `a`. Note that the inverse of
 *        matrix `a` will be stored in the `inverse` member of `a`. So, after
 *        calling this function you will access the inverse of `a` like this:
 *        `a->inverse`.
 *
 * @param a The matrix that we want to compute inverse of.
 *
 * @return int If the matrix doesn't have an inverse the NO_INVERSE error will
 *         be returned, otherwise NO_ERROR.
 */
int compute_inverse(Matrix *a);

/**
 * @brief Returns the determinant of a matrix.
 *
 * @param a The matrix whose determinant will be computed.
 *
 * @return double The determinant of matrix `a`.
 */
double get_determinant(Matrix *a);

/**
 * @brief Multiplies the matrices `a` and `b` and stores their results in the
 *        matrix `result`. Note that the function will alocate memory for the
 *        matrix `result`.
 *
 * @param a The matrix who will be multiplied with matrix `b`.
 * @param b The matrix wo will be multiplied with matrix `a`.
 *
 * @return int SIZE_NOT_MATCH if matrix `a` doesn't have the same number of
 *         lines as the matrix `b`, CANT_CREATE_MATRIX if the function can't
 *         allocate memory for the matrix `result` and NO_ERROR if the matrices
 *         successully multiplied.
 */
int multiply_matrices(Matrix *a, Matrix *b,
					  Matrix **result);



/**
 * @brief Use this function to add 2 matrices. Note that the result will
 *        be stored in a third matrix.
 *
 * @param a An instance of Matrix.
 * @param b An instance of Matrix.
 * @param c An instance of Matrix.
 *
 * @return int Returns and error status.
 *
 */
int add_matrices(Matrix *a, Matrix *b, Matrix **result);

/**
 * @brief Copies the contents of the matrix `a` to the matrix `destinaation`.
 *
 * @param a The matrix whose contents we will copy.
 * @param destination The matrix where the contents of `a` will be stored. Note
 *        that the functio doesn't allocate memory for `destination` you
 *        should create this matrix by yourself.
 *
 * @return void It doesn't return anything.
 */
void copy_matrix(Matrix *a, Matrix **destination);

/**
 * @brief raise the matrix `a` to the power `power`.
 *
 * @param a The matrix that will be raised to the power `power`.
 * @param power The power that the matrix `a` will be raised to.
 *
 * @return Matrix * Returns a matrix that will contain the result of
 *         raising `a` to `power`.
 */
Matrix *matrix_pow(Matrix *a, int power);

/**
 * @brief Use this function to compare to matrices. The criteria
 *        used to compare the matrices are the dimensions and the
 *        actual values of the 2 matrices.
 *
 * @param a An instance of struct matrix.
 * @param b An instance of sruct martix.
 *
 * @return bool 1 if the matrices are equal and 0 otherwise.
 *
 */
bool compare_matrices(Matrix *a, Matrix *b);

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
int matrix_bilinear_resize(Matrix *pixels, Matrix *output, int w, int h, int w2, int h2);


#endif
