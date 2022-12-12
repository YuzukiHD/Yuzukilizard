/* zlib.h -- interface of the 'zlib' general purpose compression library
  version 1.2.2, October 3rd, 2004

  Copyright (C) 1995-2004 Jean-loup Gailly and Mark Adler

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.

  Jean-loup Gailly jloup@gzip.org
  Mark Adler madler@alumni.caltech.edu

*/
/****************************************************************************
 The deflate compression method (the only one supported in this version)
*****************************************************************************/
#ifndef _CDX_ZLIB_H_
#define _CDX_ZLIB_H_
#include <stdio.h>
#include <stdlib.h>
#include <CdxTypes.h>

#define ZALLOC(size)    malloc(size)
#define ZFREE(pbuf)        free(pbuf)
#define zmemcpy         memcpy

#define ZLIB_VERSION    "1.2.2"
#define Z_OK            0
#define Z_STREAM_END    1
#define Z_NEED_DICT        2
#define Z_FINISH        4
#define Z_BLOCK            5
#define Z_DEFLATED        8
#define Z_ERRNO            (-1)
#define Z_STREAM_ERROR    (-2)
#define Z_DATA_ERROR    (-3)
#define Z_MEM_ERROR        (-4)
#define Z_BUF_ERROR        (-5)
#define Z_VERSION_ERROR    (-6)
#define ENOUGH            1440
#define MAXD            154
#define MAXBITS            15
#define OFF                1
#define NMAX            5552
#define BASE            65521UL    // largest prime smaller than 65536
#define CHUNK            16384
#define MAX_WBITS        15
#define DEF_WBITS        MAX_WBITS
#define Z_NO_FLUSH        0

typedef enum {
    HEAD,        // i: waiting for magic header
    DICTID,        // i: waiting for dictionary check value
    DICT,        // waiting for inflateSetDictionary() call
    TYPE,        // i: waiting for type bits, including last-flag bit
    TYPEDO,        // i: same, but skip check to exit inflate on new block
    STORED,        // i: waiting for stored size (length and complement)
    COPY,        // i/o: waiting for input or output to copy stored block
    TABLE,        // i: waiting for dynamic block table lengths
    LENLENS,    // i: waiting for code length code lengths
    CODELENS,    // i: waiting for length/lit and distance code lengths
    LEN,        // i: waiting for length/lit code
    LENEXT,        // i: waiting for length extra bits
    DIST,        // i: waiting for distance code
    DISTEXT,    // i: waiting for distance extra bits
    MATCH,        // o: waiting for output space to copy string
    LIT,        // o: waiting for output space to write literal
    CHECK,        // i: waiting for 32-bit check value
    DONE,        // finished check, done -- remain here until reset
    BAD,        // got a data error -- remain here until reset
    MEM,        // got an inflate() memory error -- remain here until reset
    SYNC        // looking for synchronization bytes to restart inflate()
} inflate_mode;

typedef struct z_code{
    unsigned char    op;        // operation, extra bits, table bits
    unsigned char    bits;    // bits in this part of the code
    unsigned short    val;    // offset in table or code value
} code;

typedef enum {
    CODES,
    LENS,
    DISTS
} codetype;

typedef struct inflate_state {
    inflate_mode mode;        // current inflate mode
    int last;                // true if processing last block
    int wrap;                // bit 0 true for zlib, bit 1 true for gzip
    int havedict;            // true if dictionary provided
    int flags;            // gzip header method and flags (0 if zlib)
    unsigned int check;            // protected copy of check value
    unsigned int total;            // protected copy of output count
    // sliding window
    unsigned int wbits;            // log base 2 of requested window size
    unsigned int wsize;            // window size or zero if not using window
    unsigned int whave;            // valid bytes in the window
    unsigned int write;            // window write index
    unsigned char window[1<<DEF_WBITS];            // allocated sliding window, if needed
    // bit accumulator
    unsigned int hold;                // input bit accumulator
    unsigned int bits;                // number of bits in "in"
    // for string and stored block copying
    unsigned int length;            // literal or length of data to copy
    unsigned int offset;            // distance back to copy string from
    // for table and code decoding
    unsigned int extra;            // extra bits needed
    // fixed and dynamic code tables
    code *lencode;            // starting table for length/literal codes
    code *distcode;            // starting table for distance codes
    unsigned int lenbits;            // index bits for lencode
    unsigned int distbits;            // index bits for distcode
    // dynamic table building
    unsigned int ncode;            // number of code length code lengths
    unsigned int nlen;                // number of length code lengths
    unsigned int ndist;            // number of distance code lengths
    unsigned int have;                // number of code lengths in lens[]
    code  *next;            // next available space in codes[]
    unsigned short lens[320];        // temporary storage for code lengths
    unsigned short work[288];        // work area for code table building
    code codes[ENOUGH];        // space for code tables
    int fixedtables_virgin;
    code fixed[544];
    code *lenfix;
    code *distfix;
} inflate_state;

typedef struct z_stream {
    struct inflate_state *state; // not visible by applications

    unsigned char    *next_in;        // next input byte
    unsigned int    avail_in;        // number of bytes available at next_in
    unsigned int    total_in;        // total nb of input bytes read so far

    unsigned char    *next_out;        // next output byte should be put there
    unsigned int    avail_out;        // remaining free space at next_out
    unsigned int    total_out;        // total nb of bytes output so far

    int    data_type;        // best guess about the data type: ascii or binary
    unsigned int    adler;            // adler32 value of the uncompressed data
    unsigned int    reserved;        // reserved for future use
} z_stream;

int decode_zlib_data(unsigned char * source, unsigned char * dest,
                     unsigned int DataInSize, unsigned int DataOutSize);

#endif

