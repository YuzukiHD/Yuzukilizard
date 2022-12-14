/*
 * memtester version 4
 *
 * Very simple but very effective user-space memory tester.
 * Originally by Simon Kirby <sim@stormix.com> <sim@neato.org>
 * Version 2 by Charles Cazabon <charlesc-memtester@pyropus.ca>
 * Version 3 not publicly released.
 * Version 4 rewrite:
 * Copyright (C) 2004-2012 Charles Cazabon <charlesc-memtester@pyropus.ca>
 * Licensed under the terms of the GNU General Public License version 2 (only).
 * See the file COPYING for details.
 *
 */

#define __version__ "4.3.0"

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
//#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include "finsh_api.h"

#include "memtypes.h"
#include "memsizes.h"
#include "tests.h"

#define EXIT_FAIL_NONSTARTER    0x01
#define EXIT_FAIL_ADDRESSLINES  0x02
#define EXIT_FAIL_OTHERTEST     0x04

struct test tests[] = {
    { "Compare XOR", test_xor_comparison },
    { "Compare SUB", test_sub_comparison },
    { "Compare MUL", test_mul_comparison },
    { "Compare DIV",test_div_comparison },
    { "Compare OR", test_or_comparison },
    { "Compare AND", test_and_comparison },
    { "Sequential Increment", test_seqinc_comparison },
    { "Solid Bits", test_solidbits_comparison },
    { "Block Sequential", test_blockseq_comparison },
    { "Checkerboard", test_checkerboard_comparison },
    { "Bit Spread", test_bitspread_comparison },
    { "Bit Flip", test_bitflip_comparison },
    { "Walking Ones", test_walkbits1_comparison },
    { "Walking Zeroes", test_walkbits0_comparison },
#ifdef TEST_NARROW_WRITES
    { "8-bit Writes", test_8bit_wide_random },
    { "16-bit Writes", test_16bit_wide_random },
#endif
    { NULL, NULL }
};

long    sysconf (int __name)
{
    return __name;
}

/* Sanity checks and portability helper macros. */
#ifdef _SC_VERSION
void check_posix_system(void) {
    if (sysconf(_SC_VERSION) < 198808L) {
        fprintf(stderr, "A POSIX system is required.  Don't be surprised if "
            "this craps out.\n");
        fprintf(stderr, "_SC_VERSION is %lu\n", sysconf(_SC_VERSION));
    }
}
#else
#define check_posix_system()
#endif

#ifdef _SC_PAGE_SIZE
int memtester_pagesize(void) {
    int pagesize = sysconf(_SC_PAGE_SIZE) * 1024;
    if (pagesize == -1) {
        perror("get page size failed");
        return (EXIT_FAIL_NONSTARTER);
    }
    printf("pagesize is %ld\n", (long) pagesize);
    return pagesize;
}
#else
int memtester_pagesize(void) {
    printf("sysconf(_SC_PAGE_SIZE) not supported; using pagesize of 8192\n");
    return 8192;
}
#endif

/* Some systems don't define MAP_LOCKED.  Define it to 0 here
   so it's just a no-op when ORed with other constants. */
#ifndef MAP_LOCKED
  #define MAP_LOCKED 0
#endif

/* Function declarations */
void usage(char *me);

/* Global vars - so tests have access to this information */
int use_phys = 0;
off_t physaddrbase = 0;

/* Function definitions */
void usage(char *me) {
    fprintf(stderr, "\n"
            "Usage: %s -s <size>[B|K|M|G] [-m testmask] [-t times]\n"
            "\n"
            "[testmask]: specify tests which we want run.\n"
            "bit0  Compare XOR\n"
            "bit1  Compare SUB\n"
            "bit2  Compare MUL\n"
            "bit3  Compare DIV\n"
            "bit4  Compare OR\n"
            "bit5  Compare AND\n"
            "bit6  Sequential Increment\n"
            "bit7  Solid Bits\n"
            "bit8  Block Sequential\n"
            "bit9  Checkerboard\n"
            "bit10 Bit Spread\n"
            "bit11 Bit Flip\n"
            "bit12 Walking Ones\n"
            "bit13 Walking Zeroes\n"
            "bit14 8-bit Writes(ifdef TEST_NARROW_WRITE)\n"
            "bit15 16-bit Writes(ifdef TEST_NARROW_WRITE)\n"
            "\n"
            "eg:\n"
            "    memtester -s 10M -m 0xc -t 1\n"
            "    means run Compare DIV&MUL tests only to test 10M memory loop 1 time.\n",
            me);
}

int cmd_memtest(int argc, char **argv) {
    ul i;
    size_t pagesize, wantmb, wantbytes, wantbytes_orig, bufsize,
         halflen, count;
    char *addrsuffix;
    void volatile *buf, *aligned;
    ulv *bufa, *bufb;
    int exit_code = 0;
    int memfd, opt, memshift;
    size_t maxbytes = -1; /* addressable memory, in bytes */
    size_t maxmb = (maxbytes >> 20) + 1; /* addressable memory, in MB */
    char *env_testmask = 0;
    //default value
    size_t wantraw = 10;
    char *memsuffix = "M";
    ul testmask = 0xFFFF;
    ul loop, loops = 0;

    loops = 0;
    printf("memtester version " __version__ " (%d-bit)\n", UL_LEN);
    printf("Copyright (C) 2001-2012 Charles Cazabon.\n");
    printf("Licensed under the GNU General Public License version 2 (only).\n");
    printf("\n");
    check_posix_system();
    pagesize = memtester_pagesize();
    /* If MEMTESTER_TEST_MASK is set, we use its value as a mask of which
       tests we run.
     */
    if ((env_testmask = getenv("MEMTESTER_TEST_MASK"))) {
        errno = 0;
        testmask = strtoul(env_testmask, 0, 0);
        if (errno) {
            fprintf(stderr, "error parsing MEMTESTER_TEST_MASK %s: %s\n",
                    env_testmask, strerror(errno));
            usage(argv[0]); /* doesn't return */
	    return (EXIT_FAIL_NONSTARTER);
        }
        printf("using testmask 0x%lx\n", testmask);
    }

    optind = 0;
    while ((opt = getopt(argc, argv, "qm:s:t:")) != -1) {
        switch (opt) {
            case 'q':
                quiet = 1;
                break;
            case 'm':
                errno = 0;
		testmask = strtoul(optarg, 0, 0);
                if (errno) {
                    fprintf(stderr, "error parsing testmask %s: %s\n",
                        optarg, strerror(errno));
                    usage(argv[0]); /* doesn't return */
		    return (EXIT_FAIL_NONSTARTER);
                }
		printf("===testmask %ld\n", testmask);
                break;
	    case 's':
		errno = 0;
		wantraw = strtoul(optarg, &memsuffix, 0);
		if (errno != 0) {
			fprintf(stderr, "failed to parse memory argument");
			usage(argv[0]); /* doesn't return */
			return (EXIT_FAIL_NONSTARTER);
		}
		break;
	    case 't':
		errno = 0;
		loops = strtoul(optarg, 0, 10);
		if (errno != 0) {
			fprintf(stderr, "failed to parse number of loops");
			usage(argv[0]); /* doesn't return */
			return (EXIT_FAIL_NONSTARTER);
		}
		break;

            default: /* '?' */
                usage(argv[0]); /* doesn't return */
		return (EXIT_FAIL_NONSTARTER);
        }
    }
    if(0 == loops) loops = 10;
    printf("loops %lu times\n", loops);
    printf("memsuffix %c\n", *memsuffix);
    switch (*memsuffix) {
        case 'G':
        case 'g':
            memshift = 30; /* gigabytes */
            break;
        case 'M':
        case 'm':
            memshift = 20; /* megabytes */
            break;
        case 'K':
        case 'k':
            memshift = 10; /* kilobytes */
            break;
        case 'B':
        case 'b':
            memshift = 0; /* bytes*/
            break;
        case '\0':  /* no suffix */
            memshift = 20; /* megabytes */
            break;
        default:
            /* bad suffix */
            usage(argv[0]); /* doesn't return */
	    return (EXIT_FAIL_NONSTARTER);
    }
    wantbytes_orig = wantbytes = ((size_t) wantraw << memshift);
    wantmb = (wantbytes_orig >> 20);
    if (wantmb > maxmb) {
        fprintf(stderr, "This system can only address %llu MB.\n", (ull) maxmb);
        return (EXIT_FAIL_NONSTARTER);
    }
    if (wantbytes < pagesize) {
        fprintf(stderr, "bytes %ld < pagesize %ld -- memory argument too large?\n",
                wantbytes, pagesize);
        return (EXIT_FAIL_NONSTARTER);
    }

    printf("want %lluMB (%llu bytes)\n", (ull) wantmb, (ull) wantbytes);
    buf = NULL;

    while (!buf && wantbytes) {
	buf = (void volatile *) malloc(wantbytes);
	if (!buf) wantbytes -= pagesize;
    }
    aligned = buf;
    bufsize = wantbytes;
    printf("got  %lluMB (%llu bytes)", (ull) wantbytes >> 20,
		    (ull) wantbytes);
    fflush(stdout);

    halflen = bufsize / 2;
    count = halflen / sizeof(ul);
    bufa = (ulv *) aligned;
    bufb = (ulv *) ((size_t) aligned + halflen);

    for(loop=1; ((!loops) || loop <= loops); loop++) {
        printf("Loop %lu", loop);
        if (loops) {
            printf("/%lu", loops);
        }
        printf(":\n");
	printf("<============Prepare============>\n");
        printf("  %-20s: ", "Stuck Address");
        fflush(stdout);
        if (!test_stuck_address(aligned, bufsize / sizeof(ul))) {
             printf("ok\n");
        } else {
            exit_code |= EXIT_FAIL_ADDRESSLINES;
            goto out;
        }
        printf("  %-20s: ", "Random Value");
        fflush(stdout);
	if (!test_random_value(bufa, bufb, count)) {
	    printf("ok\n");
	    printf("<============Start test============>\n");
	} else {
	    exit_code |= EXIT_FAIL_OTHERTEST;
	    goto out;
	}
        for (i=0;;i++) {
            if (!tests[i].name) break;
            /* If using a custom testmask, only run this test if the
               bit corresponding to this test was set by the user.
             */
            if (testmask && (!((1 << i) & testmask))) {
                continue;
            }
            printf("  %-20s: ", tests[i].name);
            if (!tests[i].fp(bufa, bufb, count)) {
                printf("ok\n");
            } else {
                exit_code |= EXIT_FAIL_OTHERTEST;
                goto out;
            }
            fflush(stdout);
        }
        printf("\n");
        fflush(stdout);
    }
out:
    printf("Done.\n");
    fflush(stdout);
    free((void *)buf);
    return exit_code;
    //exit(exit_code);
}
FINSH_FUNCTION_EXPORT_CMD(cmd_memtest, __cmd_memtester, memory test);
