#ifndef AW_ERRORS_H
#define AW_ERRORS_H

/*
* From A to Z. (If new function is added, obey the same order)
*
* Should call them in the following ways:
*   arg_error("my_fun: [w] is not properly set. ");
*   calloc_error("my_fun: fail to allocate memory for [buffer].");
*
* If an error is triggered, it will print something like this:
*   > arg_error: my_fun: [w] is not properly set.
*   > calloc_error: my_fun: fail to allocate memory for [buffer].
*/

/*
 * Common error.
*/

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/*
 * Argument error: alert if the input arguments are not properly set.
 * Note that this is only for non-pointer argument.
 * Alerting for input pointers, please use nullptr_error.
*/
void arg_error(const char *fun, const char *msg);

/*
 * calloc error: alert if failed to allocate memory via calloc.
*/
void calloc_error(const char *fun, const char *msg);

/*
* Common error: alert if a general/common error happens.
*/
void common_error(const char *fun, const char *msg);

/*
 * Common warning: give a warning if something happens, which is not an error.
*/
void common_warning(const char *fun, const char *msg);

/*
 * fopen error: alert if failed to open a file.
*/
void fopen_error(const char *fun, const char *msg);

/*
 * fread error: alert if failed to read content from a file pointer.
*/
void fread_error(const char *fun, const char *msg);

/*
 * fwrite error: alert if failed to write content to a file pointer.
*/
void fwrite_error(const char *fun, const char *msg);

/*
 * malloc error: alert if failed to allocate memory via malloc.
*/
void malloc_error(const char *fun, const char *msg);

/*
 * nullptr error: alert if some pointers are void.
*/
void nullptr_error(const char *fun, const char *msg);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // !AW_ERRORS_H

