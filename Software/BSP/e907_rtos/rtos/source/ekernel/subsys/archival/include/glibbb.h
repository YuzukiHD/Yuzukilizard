#ifndef GLIBBB_H
#define GLIBBB_H
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
typedef struct llist_t
{
    struct llist_t *link;
    char *data;
} llist_t;

extern int fd_1, fd_2;
#define MAX(a,b) (((a)>(b))?(a):(b))

#define barrier() __asm__ __volatile__("":::"memory")
#define SET_PTR_TO_GLOBALS(x) do { \
        (*(struct globals**)&ptr_to_globals) = (void*)(x); \
        barrier(); \
    } while (0)
#define FREE_PTR_TO_GLOBALS() do { \
        if (ENABLE_FEATURE_CLEAN_UP) { \
            free(ptr_to_globals); \
        } \
    } while (0)

#define SEAMLESS_COMPRESSION (0 \
                              || ENABLE_FEATURE_SEAMLESS_XZ \
                              || ENABLE_FEATURE_SEAMLESS_LZMA \
                              || ENABLE_FEATURE_SEAMLESS_BZ2 \
                              || ENABLE_FEATURE_SEAMLESS_GZ \
                              || ENABLE_FEATURE_SEAMLESS_Z)

#define ENABLE_LONG_OPTS 1
#define ENABLE_FEATURE_GETOPT_LONG 0
# define IF_FEATURE_GZIP_DECOMPRESS(...) __VA_ARGS__

#define LONE_DASH(s)     ((s)[0] == '-' && !(s)[1])
#define NOT_LONE_DASH(s) ((s)[0] != '-' || (s)[1])
#define LONE_CHAR(s,c)     ((s)[0] == (c) && !(s)[1])
#define NOT_LONE_CHAR(s,c) ((s)[0] != (c) || (s)[1])
#define DOT_OR_DOTDOT(s) ((s)[0] == '.' && (!(s)[1] || ((s)[1] == '.' && !(s)[2])))

void xpipe(int filedes[2]);
/* In this form code with pipes is much more readable */
struct fd_pair
{
    int rd;
    int wr;
};
#define piped_pair(pair)  pipe(&((pair).rd))
#define xpiped_pair(pair) xpipe(&((pair).rd))

#define ARRAY_SIZE(x) ((unsigned)(sizeof(x) / sizeof((x)[0])))
#define BUILD_BUG_ON(condition) ((void)sizeof(char[1 - 2*!!(condition)]))
typedef unsigned long uoff_t;
void *xmalloc_read(int fd, size_t *maxsz_p);

void llist_add_to(llist_t **old_head, void *data);
void llist_add_to_end(llist_t **list_head, void *data);
void *llist_pop(llist_t **elm);
void llist_unlink(llist_t **head, llist_t *elm);
void llist_free(llist_t *elm, void (*freeit)(void *data));
llist_t *llist_rev(llist_t *list);
llist_t *llist_find_str(llist_t *first, const char *str);

void *xmalloc(size_t size);
void *xzalloc(size_t size);
ssize_t full_write(int fd, const void *buf, size_t len);
void xclose(int fd);
void xunlink(const char *pathname);
char *xstrdup(const char *s);
uint32_t getopt32(char **argv, const char *applet_opts, ...);

extern uint32_t *global_crc32_table;
extern uint32_t option_mask32;
uint32_t *crc32_filltable(uint32_t *tbl256, int endian);
uint32_t crc32_block_endian1(uint32_t val, const void *buf, unsigned len, uint32_t *crc_table);
uint32_t crc32_block_endian0(uint32_t val, const void *buf, unsigned len, uint32_t *crc_table);

void xmove_fd(int from, int to);
int open3_or_warn(const char *pathname, int flags, int mode);
char *xasprintf(const char *format, ...);


#define bb_path_motd_file "/etc/motd"

#define bb_dev_null "/dev/null"

extern int open_zipped(const char *fname, int fail_if_not_compressed);
extern void *xmalloc_open_zipped_read_close(const char *fname, size_t *maxsz_p);
int setup_unzip_on_fd(int fd, int fail_if_not_compressed);


extern void xfunc_die(void);
typedef long int off_t;
off_t bb_copyfd_eof(int fd1, int fd2);


uint32_t *crc32_filltable(uint32_t *tbl256, int endian);

int gunzip_main(int argc, char **argv);


void xwrite(int fd, const void *buf, size_t count);
ssize_t safe_write(int fd, const void *buf, size_t count);





#endif  /*GLIBBB_H*/
