/*
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 * Author: Andrey Ryabinin <a.ryabinin@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <debug.h>
#include <rtthread.h>

#define pr_fmt(fmt) "kasan test: %s " fmt, __func__

#ifndef ARRAY_SIZE
# define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif

#define KMALLOC_MAX_CACHE_SIZE (16 * 1024)

/*
 * Note: test functions are marked noinline so that their names appear in
 * reports.
 */

static noinline void kmalloc_oob_right(void)
{
    char *ptr;
    size_t size = 123;

    printf("out-of-bounds to right\n");
    ptr = malloc(size);
    if (!ptr)
    {
        printf("Allocation failed\n");
        return;
    }

    ptr[size] = 'x';
    free(ptr);
}

static noinline void kmalloc_oob_left(void)
{
    char *ptr;
    size_t size = 15;

    printf("out-of-bounds to left\n");
    ptr = malloc(size);
    if (!ptr)
    {
        printf("Allocation failed\n");
        return;
    }

    *ptr = *(ptr - 1);
    free(ptr);
}

static noinline void kmalloc_pagealloc_oob_right(void)
{
    char *ptr;
    size_t size = KMALLOC_MAX_CACHE_SIZE + 10;

    /* Allocate a chunk that does not fit into a SLUB cache to trigger
     * the page allocator fallback.
     */
    printf("kmalloc pagealloc allocation: out-of-bounds to right\n");
    ptr = malloc(size);
    if (!ptr)
    {
        printf("Allocation failed\n");
        return;
    }

    ptr[size] = 0;
    free(ptr);
}

static noinline void kmalloc_large_oob_right(void)
{
    char *ptr;
    size_t size = KMALLOC_MAX_CACHE_SIZE - 256;
    /* Allocate a chunk that is large enough, but still fits into a slab
     * and does not trigger the page allocator fallback in SLUB.
     */
    printf("kmalloc large allocation: out-of-bounds to right\n");
    ptr = malloc(size);
    if (!ptr)
    {
        printf("Allocation failed\n");
        return;
    }

    ptr[size] = 0;
    free(ptr);
}

static noinline void kmalloc_oob_krealloc_more(void)
{
    char *ptr1, *ptr2;
    size_t size1 = 17;
    size_t size2 = 19;

    printf("out-of-bounds after krealloc more\n");
    ptr1 = malloc(size1);
    ptr2 = realloc(ptr1, size2);
    if (!ptr1 || !ptr2)
    {
        printf("Allocation failed\n");
        free(ptr1);
        return;
    }

    ptr2[size2] = 'x';
    free(ptr2);
}

static noinline void kmalloc_oob_krealloc_less(void)
{
    char *ptr1, *ptr2;
    size_t size1 = 17;
    size_t size2 = 15;

    printf("out-of-bounds after krealloc less\n");
    ptr1 = malloc(size1);
    ptr2 = realloc(ptr1, size2);
    if (!ptr1 || !ptr2)
    {
        printf("Allocation failed\n");
        free(ptr1);
        return;
    }
    ptr2[size2] = 'x';
    free(ptr2);
}

static noinline void kmalloc_oob_16(void)
{
    struct
    {
        uint64_t words[2];
    } *ptr1, *ptr2;

    printf("kmalloc out-of-bounds for 16-bytes access\n");
    ptr1 = malloc(sizeof(*ptr1) - 3);
    ptr2 = malloc(sizeof(*ptr2));
    if (!ptr1 || !ptr2)
    {
        printf("Allocation failed\n");
        free(ptr1);
        free(ptr2);
        return;
    }
    *ptr1 = *ptr2;
    free(ptr1);
    free(ptr2);
}

static noinline void kmalloc_oob_memset_2(void)
{
    char *ptr;
    size_t size = 8;

    printf("out-of-bounds in memset2\n");
    ptr = malloc(size);
    if (!ptr)
    {
        printf("Allocation failed\n");
        return;
    }

    memset(ptr + 7, 0, 2);
    free(ptr);
}

static noinline void kmalloc_oob_memset_4(void)
{
    char *ptr;
    size_t size = 8;

    printf("out-of-bounds in memset4\n");
    ptr = malloc(size);
    if (!ptr)
    {
        printf("Allocation failed\n");
        return;
    }

    memset(ptr + 5, 0, 4);
    free(ptr);
}

static noinline void kmalloc_oob_memset_8(void)
{
    char *ptr;
    size_t size = 8;

    printf("out-of-bounds in memset8\n");
    ptr = malloc(size);
    if (!ptr)
    {
        printf("Allocation failed\n");
        return;
    }

    memset(ptr + 1, 0, 8);
    free(ptr);
}

static noinline void kmalloc_oob_memset_16(void)
{
    char *ptr;
    size_t size = 16;

    printf("out-of-bounds in memset16\n");
    ptr = malloc(size);
    if (!ptr)
    {
        printf("Allocation failed\n");
        return;
    }

    memset(ptr + 1, 0, 16);
    free(ptr);
}

static noinline void kmalloc_oob_in_memset(void)
{
    char *ptr;
    size_t size = 666;

    printf("out-of-bounds in memset\n");
    ptr = malloc(size);
    if (!ptr)
    {
        printf("Allocation failed\n");
        return;
    }

    memset(ptr, 0, size + 5);
    free(ptr);
}


static noinline void kmalloc_uaf(void)
{
    char *ptr;
    size_t size = 10;

    printf("use-after-free\n");
    ptr = malloc(size);
    if (!ptr)
    {
        printf("Allocation failed\n");
        return;
    }

    free(ptr);
    *(ptr + 8) = 'x';
}

static noinline void kmalloc_uaf_memset(void)
{
    char *ptr;
    size_t size = 33;

    printf("use-after-free in memset\n");
    ptr = malloc(size);
    if (!ptr)
    {
        printf("Allocation failed\n");
        return;
    }

    free(ptr);
    memset(ptr, 0, size);
}

static noinline void kmalloc_uaf2(void)
{
    char *ptr1, *ptr2;
    size_t size = 43;

    printf("use-after-free after another kmalloc\n");
    ptr1 = malloc(size);
    if (!ptr1)
    {
        printf("Allocation failed\n");
        return;
    }

    free(ptr1);
    ptr2 = malloc(size);
    if (!ptr2)
    {
        printf("Allocation failed\n");
        return;
    }

    ptr1[40] = 'x';
    if (ptr1 == ptr2)
    {
        printf("Could not detect use-after-free: ptr1 == ptr2\n");
    }
    free(ptr2);
}

static char global_array[10];

static noinline void kasan_global_oob(void)
{
    volatile int i = 3;
    char *p = &global_array[ARRAY_SIZE(global_array) + i];

    printf("out-of-bounds global variable\n");
    *(volatile char *)p;
}

static noinline void kasan_stack_oob(void)
{
    char stack_array[10];
    volatile int i = 0;
    char *p = &stack_array[ARRAY_SIZE(stack_array) + i];

    printf("out-of-bounds on stack\n");
    *(volatile char *)p;
}

static noinline void kasan_page_alloc_oob(void)
{
    char *page;
    char *ptr;

    page = rt_page_alloc(2);
    ptr = page;
    page += 2 * 4096 + 1;

    printf("out-of-bounds on rt_page_alloc\n");
    *(volatile char *)page;
    rt_page_free(ptr, 2);
}

static noinline void use_after_scope_test(void)
{
    volatile char *volatile p;

    printf("use-after-scope on int\n");
    {
        int local = 0;

        p = (char *)&local;
    }
    p[0] = 1;
    p[3] = 1;

    printf("use-after-scope on array\n");
    {
        char local[1024] = {0};

        p = local;
    }
    p[0] = 1;
    p[1023] = 1;
}

static noinline void kasan_alloca_oob_left(void)
{
    volatile int i = 10;
    char alloca_array[i];
    char *p = alloca_array - 1;

    printf("out-of-bounds to left on alloca\n");
    *(volatile char *)p;
}

static noinline void kasan_alloca_oob_right(void)
{
    volatile int i = 10;
    char alloca_array[i];
    char *p = alloca_array + i;

    printf("out-of-bounds to right on alloca\n");
    *(volatile char *)p;
}

static noinline void kasan_double_free_small(void)
{
    char *p = malloc(100);

    printf("double free small test case\n");
    free(p);
    free(p);
}

static noinline void kasan_double_free_large(void)
{
    char *p = malloc(16384 + 10);

    printf("double free large test case\n");
    free(p);
    free(p);
}

static noinline void kasan_double_free_pages(void)
{
    char *p = rt_page_alloc(10);

    printf("double free large test case\n");
    rt_page_free(p, 10);
    rt_page_free(p, 10);
}

static int kmalloc_tests_init(void)
{
    /*
     * Temporarily enable multi-shot mode. Otherwise, we'd only get a
     * report for the first case.
     */
    int kasan_save_enable_multi_shot(void);
    int kasan_restore_multi_shot(int shot);

    int multishot = kasan_save_enable_multi_shot();

    kmalloc_oob_right();
    kmalloc_oob_left();
    kmalloc_pagealloc_oob_right();
    kmalloc_large_oob_right();
    kmalloc_oob_krealloc_more();
    kmalloc_oob_krealloc_less();
    kmalloc_oob_16();
    kmalloc_oob_in_memset();
    kmalloc_oob_memset_2();
    kmalloc_oob_memset_4();
    kmalloc_oob_memset_8();
    kmalloc_oob_memset_16();
    kmalloc_uaf();
    kmalloc_uaf_memset();
    kmalloc_uaf2();
    kasan_stack_oob();
    kasan_global_oob();
    kasan_alloca_oob_left();
    kasan_alloca_oob_right();
    use_after_scope_test();
    kasan_double_free_small();
    kasan_double_free_large();
    kasan_double_free_pages();

    kasan_restore_multi_shot(multishot);

    return 0;
}

static int cmd_kmalloc_oob_right(int argc, char **argv)
{
    kmalloc_oob_right();
    return 0;
}
FINSH_FUNCTION_EXPORT_ALIAS(cmd_kmalloc_oob_right, __cmd_kmalloc_oob_right, kasan kmalloc oob right);

static int cmd_kmalloc_oob_left(int argc, char **argv)
{
    kmalloc_oob_left();
    return 0;
}
FINSH_FUNCTION_EXPORT_ALIAS(cmd_kmalloc_oob_left, __cmd_kmalloc_oob_left, kasan kmalloc oob left);

static int cmd_kmalloc_oob_krealloc_more(int argc, char **argv)
{
    kmalloc_oob_krealloc_more();
    return 0;
}
FINSH_FUNCTION_EXPORT_ALIAS(cmd_kmalloc_oob_krealloc_more, __cmd_kmalloc_oob_krealloc_more, kmalloc oob realloc more);

static int cmd_kmalloc_oob_krealloc_less(int argc, char **argv)
{
    kmalloc_oob_krealloc_less();
    return 0;
}
FINSH_FUNCTION_EXPORT_ALIAS(cmd_kmalloc_oob_krealloc_less, __cmd_kmalloc_oob_krealloc_less, kmalloc oob realloc less);

static int cmd_kmalloc_pagealloc_oob_right(int argc, char **argv)
{
    kmalloc_pagealloc_oob_right();
    return 0;
}
FINSH_FUNCTION_EXPORT_ALIAS(cmd_kmalloc_pagealloc_oob_right, __cmd_kmalloc_pagealloc_oob_right, kmalloc pagealloc oob right);

static int cmd_kmalloc_large_oob_right(int argc, char **argv)
{
    kmalloc_large_oob_right();
    return 0;
}
FINSH_FUNCTION_EXPORT_ALIAS(cmd_kmalloc_large_oob_right, __cmd_kmalloc_large_oob_right, kmalloc large oob right);

static int cmd_kmalloc_oob_16(int argc, char **argv)
{
    kmalloc_oob_16();
    return 0;
}
FINSH_FUNCTION_EXPORT_ALIAS(cmd_kmalloc_oob_16, __cmd_kmalloc_oob_16, kmalloc oob 16);

static int cmd_kmalloc_oob_in_memset_2(int argc, char **argv)
{
    kmalloc_oob_memset_2();
    return 0;
}
FINSH_FUNCTION_EXPORT_ALIAS(cmd_kmalloc_oob_in_memset_2, __cmd_kmalloc_oob_in_memset_2, kmalloc oob in memset 2);

static int cmd_kmalloc_oob_in_memset_4(int argc, char **argv)
{
    kmalloc_oob_memset_4();
    return 0;
}
FINSH_FUNCTION_EXPORT_ALIAS(cmd_kmalloc_oob_in_memset_4, __cmd_kmalloc_oob_in_memset_4, kmalloc oob in memset 4);

static int cmd_kmalloc_oob_in_memset_8(int argc, char **argv)
{
    kmalloc_oob_memset_8();
    return 0;
}
FINSH_FUNCTION_EXPORT_ALIAS(cmd_kmalloc_oob_in_memset_8, __cmd_kmalloc_oob_in_memset_8, kmalloc oob in memset 8);

static int cmd_kmalloc_oob_in_memset_16(int argc, char **argv)
{
    kmalloc_oob_memset_16();
    return 0;
}
FINSH_FUNCTION_EXPORT_ALIAS(cmd_kmalloc_oob_in_memset_16, __cmd_kmalloc_oob_in_memset_16, kmalloc oob in memset 16);

static int cmd_kmalloc_oob_in_memset(int argc, char **argv)
{
    kmalloc_oob_in_memset();
    return 0;
}
FINSH_FUNCTION_EXPORT_ALIAS(cmd_kmalloc_oob_in_memset, __cmd_kmalloc_oob_in_memset, kmalloc oob in memset);

static int cmd_kmalloc_uaf(int argc, char **argv)
{
    kmalloc_uaf();
    return 0;
}
FINSH_FUNCTION_EXPORT_ALIAS(cmd_kmalloc_uaf, __cmd_kmalloc_uaf, kmalloc uaf);

static int cmd_kmalloc_uaf_memset(int argc, char **argv)
{
    kmalloc_uaf_memset();
    return 0;
}
FINSH_FUNCTION_EXPORT_ALIAS(cmd_kmalloc_uaf_memset, __cmd_kmalloc_uaf_memset, kmalloc uaf memset);

static int cmd_kmalloc_uaf2(int argc, char **argv)
{
    kmalloc_uaf2();
    return 0;
}
FINSH_FUNCTION_EXPORT_ALIAS(cmd_kmalloc_uaf2, __cmd_kmalloc_uaf2, kmalloc uaf2);

static int cmd_kasan_global_oob(int argc, char **argv)
{
    kasan_global_oob();
    return 0;
}
FINSH_FUNCTION_EXPORT_ALIAS(cmd_kasan_global_oob, __cmd_kasan_global_oob, kasan global oob);

static int cmd_kasan_stack_oob(int argc, char **argv)
{
    kasan_stack_oob();
    return 0;
}
FINSH_FUNCTION_EXPORT_ALIAS(cmd_kasan_stack_oob, __cmd_kasan_stack_oob, kasan stack oob);

static int cmd_kasan_page_alloc_oob(int argc, char **argv)
{
    kasan_page_alloc_oob();
    return 0;
}
FINSH_FUNCTION_EXPORT_ALIAS(cmd_kasan_page_alloc_oob, __cmd_kasan_page_alloc_oob, kasan page alloc oob);

static int cmd_use_after_scope_test(int argc, char **argv)
{
    use_after_scope_test();
    return 0;
}
FINSH_FUNCTION_EXPORT_ALIAS(cmd_use_after_scope_test, __cmd_use_after_scope_test, use after scope test);

static int cmd_kasan_alloca_oob_left(int argc, char **argv)
{
    kasan_alloca_oob_left();
    return 0;
}
FINSH_FUNCTION_EXPORT_ALIAS(cmd_kasan_alloca_oob_left, __cmd_kasan_alloca_oob_left, kasan alloca oob left);

static int cmd_kasan_alloca_oob_right(int argc, char **argv)
{
    kasan_alloca_oob_right();
    return 0;
}
FINSH_FUNCTION_EXPORT_ALIAS(cmd_kasan_alloca_oob_right, __cmd_kasan_alloca_oob_right, kasan alloca oob right);

static int cmd_kasan_double_free_small(int argc, char **argv)
{
    kasan_double_free_small();
    return 0;
}
FINSH_FUNCTION_EXPORT_ALIAS(cmd_kasan_double_free_small, __cmd_kasan_double_free_small, kasan dobule free small);

static int cmd_kasan_double_free_large(int argc, char **argv)
{
    kasan_double_free_large();
    return 0;
}
FINSH_FUNCTION_EXPORT_ALIAS(cmd_kasan_double_free_large, __cmd_kasan_double_free_large, kasan dobule free large);

static int cmd_kasan_double_free_pages(int argc, char **argv)
{
    kasan_double_free_pages();
    return 0;
}
FINSH_FUNCTION_EXPORT_ALIAS(cmd_kasan_double_free_pages, __cmd_kasan_double_free_pages, kasan dobule free pages);

static int cmd_kmalloc_tests(int argc, char **argv)
{
    kmalloc_tests_init();
    return 0;
}
FINSH_FUNCTION_EXPORT_ALIAS(cmd_kmalloc_tests, __cmd_kmalloc_tests, kmalloc tests);
