/* zlib.c -- interface of the 'zlib' general purpose compression library
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

//#define LOG_NDEBUG 0
#define LOG_TAG "TEMP_TAG"

#include "zlib.h"
#include <string.h>

#define PUP(a)            *++(a)

/* Get a byte of input into the bit accumulator, or return from inflate()
   if there is no input available. */
#define PULLBYTE() \
    do { \
        if (have == 0) goto inf_leave; \
        have--; \
        hold += (unsigned int)(*next++) << bits; \
        bits += 8; \
    } while (0)

/* Assure that there are at least n bits in the bit accumulator.  If there is
   not enough available input to do that, then return from inflate(). */
#define NEEDBITS(n) \
    while (bits < (unsigned int)(n)) { \
        if (have == 0) goto inf_leave; \
        have--; \
        hold += (unsigned int)(*next++) << bits; \
        bits += 8; \
    }
// Return the low n bits of the bit accumulator (n < 16)
#define BITS(n) ((unsigned)hold & ((1U << (n)) - 1))

// Remove n bits from the bit accumulator
#define DROPBITS(n) \
    do { \
        hold >>= (n); \
        bits -= (unsigned)(n); \
    } while (0)
// Reverse the bytes in a 32-bit value
#define REVERSE(q) \
    ((((q) >> 24) & 0xff) + (((q) >> 8) & 0xff00) + \
     (((q) & 0xff00) << 8) + (((q) & 0xff) << 24))

static unsigned int adler32(unsigned int adler, unsigned char * buf, unsigned int len)
{
    unsigned int s1 = adler & 0xffff;
    unsigned int s2 = (adler >> 16) & 0xffff;
    int k;

    if (buf == NULL) return 1;

    while (len > 0) {
        k = len < NMAX ? (int)len : NMAX;
        len -= k;
        while (k--) {
            s1 += *buf++;
            s2 += s1;
        } ;
        s1 %= BASE;
        s2 %= BASE;
    }
    return (s2 << 16) | s1;
}

// Length codes 257..285 base
unsigned short z_lbase[31] = {
    3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
    35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258, 0, 0};
// Length codes 257..285 extra
unsigned short z_lext[31] = {
    16, 16, 16, 16, 16, 16, 16, 16, 17, 17, 17, 17, 18, 18, 18, 18,
    19, 19, 19, 19, 20, 20, 20, 20, 21, 21, 21, 21, 16, 199, 198};
// Distance codes 0..29 base
unsigned short z_dbase[32] = {
    1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
    257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145,
    8193, 12289, 16385, 24577, 0, 0};
// Distance codes 0..29 extra
unsigned short z_dext[32] = {
    16, 16, 16, 16, 17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 22,
    23, 23, 24, 24, 25, 25, 26, 26, 27, 27, 28, 28, 29, 29, 64, 64};

static int inflate_table(codetype type, unsigned short * lens, unsigned int codes,code ** table,
                  unsigned int * bits, unsigned short * work)
{
    unsigned int len;                // a code's length in bits
    unsigned int sym;                // index of code symbols
    unsigned int min, max;            // minimum and maximum code lengths
    unsigned int root;                // number of index bits for root table
    unsigned int curr;                // number of index bits for current table
    unsigned int drop;                // code bits to drop for sub-table
    int left;                // number of prefix codes available
    unsigned int used;                // code entries in table used
    unsigned int huff;                // Huffman code
    unsigned int incr;                // for incrementing code, index
    unsigned int fill;                // index for replicating entries
    unsigned int low;                // low bits for current root entry
    unsigned int mask;                // mask for low root bits
    code thish;                // table entry for duplication
    code *next;                // next available space in table
    unsigned short *base;            // base value table to use
    unsigned short *extra;            // extra bits table to use
    int end;                // use base and extra for symbol > end
    unsigned short count[MAXBITS+1];    // number of codes of each length
    unsigned short offs[MAXBITS+1];    // offsets in table for each length

    // accumulate lengths for codes (assumes lens[] all in 0..MAXBITS)
    for (len = 0; len <= MAXBITS; len++)
        count[len] = 0;
    for (sym = 0; sym < codes; sym++)
        count[lens[sym]]++;

    // bound code lengths, force root to be within code lengths
    root = *bits;
    for (max = MAXBITS; max >= 1; max--)
        if (count[max] != 0) break;
    if (root > max) root = max;
    if (max == 0) {                // no symbols to code at all
        thish.op = (unsigned char)64;    // invalid code marker
        thish.bits = (unsigned char)1;
        thish.val = (unsigned short)0;
        *(*table)++ = thish;    // make a table to force an error
        *(*table)++ = thish;
        *bits = 1;
        return 0;                // no symbols, but wait for decoding to report error
    }
    for (min = 1; min <= MAXBITS; min++)
        if (count[min] != 0) break;
    if (root < min) root = min;

    // check for an over-subscribed or incomplete set of lengths
    left = 1;
    for (len = 1; len <= MAXBITS; len++) {
        left <<= 1;
        left -= count[len];
        if (left < 0) return -1;    // over-subscribed
    }
    if (left > 0 && (type == CODES || (codes - count[0] != 1)))
        return -1;                    // incomplete set

    // generate offsets into symbol table for each length for sorting
    offs[1] = 0;
    for (len = 1; len < MAXBITS; len++)
    {
        offs[len + 1] = offs[len] + count[len];
    }

    // sort symbols by length, by symbol order within each length
    for (sym = 0; sym < codes; sym++)
        if (lens[sym] != 0)
            work[offs[lens[sym]]++] = (unsigned short)sym;

    // set up for code type
    switch (type) {
    case CODES:
        base = extra = work;    // dummy value--not used
        end = 19;
        break;
    case LENS:
        base = z_lbase;
        base -= 257;
        extra = z_lext;
        extra -= 257;
        end = 256;
        break;
    default:                    // DISTS
        base = z_dbase;
        extra = z_dext;
        end = -1;
    }

    // initialize state for loop
    huff = 0;                // starting code
    sym = 0;                // starting code symbol
    len = min;                // starting code length
    next = *table;            // current table to fill in
    curr = root;            // current table index bits
    drop = 0;                // current bits to drop from code for index
    low = (unsigned)(-1);    // trigger new sub-table when len > root
    used = 1U << root;        // use root table entries
    mask = used - 1;        // mask for comparing low

    // check available table space
    if (type == LENS && used >= ENOUGH - MAXD)
        return 1;

    // process all codes and make table entries
    for (;;) {
        // create table entry
        thish.bits = (unsigned char)(len - drop);
        if ((int)(work[sym]) < end) {
            thish.op = (unsigned char)0;
            thish.val = work[sym];
        }
        else if ((int)(work[sym]) > end) {
            thish.op = (unsigned char)(extra[work[sym]]);
            thish.val = base[work[sym]];
        }
        else {
            thish.op = (unsigned char)(32 + 64);    // end of block
            thish.val = 0;
        }

        // replicate for those indices with low len bits equal to huff
        incr = 1U << (len - drop);
        fill = 1U << curr;
        do {
            fill -= incr;
            next[(huff >> drop) + fill] = thish;
        } while (fill != 0);

        // backwards increment the len-bit code huff
        incr = 1U << (len - 1);
        while (huff & incr)
            incr >>= 1;
        if (incr != 0) {
            huff &= incr - 1;
            huff += incr;
        }
        else
            huff = 0;

        // go to next symbol, update count, len
        sym++;
        if (--(count[len]) == 0) {
            if (len == max) break;
            len = lens[work[sym]];
        }

        // create new sub-table if needed
        if (len > root && (huff & mask) != low) {
            // if first time, transition to sub-tables
            if (drop == 0)
                drop = root;

            // increment past last table
            next += 1U << curr;

            // determine length of next table
            curr = len - drop;
            left = (int)(1 << curr);
            while (curr + drop < max) {
                left -= count[curr + drop];
                if (left <= 0) break;
                curr++;
                left <<= 1;
            }

            // check for enough space
            used += 1U << curr;
            if (type == LENS && used >= ENOUGH - MAXD)
                return 1;

            // point entry in root table to sub-table
            low = huff & mask;
            (*table)[low].op = (unsigned char)curr;
            (*table)[low].bits = (unsigned char)root;
            (*table)[low].val = (unsigned short)(next - *table);
        }
    }

    thish.op = (unsigned char)64;    // invalid code marker
    thish.bits = (unsigned char)(len - drop);
    thish.val = (unsigned short)0;
    while (huff != 0) {
        // when done with sub-table, drop back to root table
        if (drop != 0 && (huff & mask) != low) {
            drop = 0;
            len = root;
            next = *table;
            thish.bits = (unsigned char)len;
        }

        // put invalid code marker in table
        next[huff >> drop] = thish;

        // backwards increment the len-bit code huff
        incr = 1U << (len - 1);
        while (huff & incr)
            incr >>= 1;
        if (incr != 0) {
            huff &= incr - 1;
            huff += incr;
        }
        else
            huff = 0;
    }

    // set return parameters
    *table += used;
    *bits = root;
    return 0;
}

static void fixedtables(struct inflate_state * state)
{
    // build fixed huffman tables if first call (may not be thread safe)
    if (state->fixedtables_virgin) {
        unsigned int sym, bits;
        code *next;

        // literal/length table
        sym = 0;
        while (sym < 144) state->lens[sym++] = 8;
        while (sym < 256) state->lens[sym++] = 9;
        while (sym < 280) state->lens[sym++] = 7;
        while (sym < 288) state->lens[sym++] = 8;
        next = state->fixed;
        state->lenfix = next;
        bits = 9;
        inflate_table(LENS, state->lens, 288, &(next), &(bits), state->work);

        // distance table
        sym = 0;
        while (sym < 32) state->lens[sym++] = 5;
        state->distfix = next;
        bits = 5;
        inflate_table(DISTS, state->lens, 32, &(next), &(bits), state->work);

        // do this just once
        state->fixedtables_virgin = 0;
    }

    state->lencode = state->lenfix;
    state->lenbits = 9;
    state->distcode = state->distfix;
    state->distbits = 5;
}

static int updatewindow(z_stream * strm, unsigned int out)
{
    struct inflate_state  *state;
    unsigned int copy, dist;

    state = (struct inflate_state  *)strm->state;

    // if window not in use yet, initialize
    if (state->wsize == 0) {
        state->wsize = 1U << state->wbits;
        state->write = 0;
        state->whave = 0;
    }

    // copy state->wsize or less output bytes into the circular window
    copy = out - strm->avail_out;
    if (copy >= state->wsize) {
        zmemcpy(state->window, strm->next_out - state->wsize, state->wsize);
        state->write = 0;
        state->whave = state->wsize;
    }
    else {
        dist = state->wsize - state->write;
        if (dist > copy) dist = copy;
        zmemcpy(state->window + state->write, strm->next_out - copy, dist);
        copy -= dist;
        if (copy) {
            zmemcpy(state->window, strm->next_out - copy, copy);
            state->write = copy;
            state->whave = state->wsize;
        }
        else {
            state->write += dist;
            if (state->write == state->wsize) state->write = 0;
            if (state->whave < state->wsize) state->whave += dist;
        }
    }
    return 0;
}

static void inflate_fast(z_stream * strm, unsigned int start)
{
    struct inflate_state  *state;
    unsigned char  *in;            // local strm->next_in
    unsigned char  *last;        // while in < last, enough input available
    unsigned char  *out;            // local strm->next_out
    unsigned char  *beg;            // inflate()'s initial strm->next_out
    unsigned char  *end;            // while out < end, enough space available
    unsigned int wsize;        // window size or zero if not using window
    unsigned int whave;        // valid bytes in the window
    unsigned int write;        // window write index
    unsigned char  *window;        // allocated sliding window, if wsize != 0
    unsigned int hold;            // local strm->hold
    unsigned int bits;            // local strm->bits
    code *lcode;        // local strm->lencode
    code *dcode;        // local strm->distcode
    unsigned int lmask;        // mask for first level of length codes
    unsigned int dmask;        // mask for first level of distance codes
    code thish;            // retrieved table entry
    unsigned int op;            // code bits, operation, extra bits,
                                // or window position, window bytes to copy
    unsigned int len;            // match length, unused bytes
    unsigned int dist;            // match distance
    unsigned char  *from;        // where to copy match from

    // copy state to local variables
    state = (struct inflate_state *)strm->state;
    in = strm->next_in - OFF;
    last = in + (strm->avail_in - 5);
    out = strm->next_out - OFF;
    beg = out - (start - strm->avail_out);
    end = out + (strm->avail_out - 257);
    wsize = state->wsize;
    whave = state->whave;
    write = state->write;
    window = state->window;
    hold = state->hold;
    bits = state->bits;
    lcode = state->lencode;
    dcode = state->distcode;
    lmask = (1U << state->lenbits) - 1;
    dmask = (1U << state->distbits) - 1;

    /* decode literals and length/distances until end-of-block or not enough
        input data or output space */
    do {
        if (bits < 15) {
            hold += (unsigned int)(PUP(in)) << bits;
            bits += 8;
            hold += (unsigned int)(PUP(in)) << bits;
            bits += 8;
        }
        thish = lcode[hold & lmask];
    dolen:
        op = (unsigned int)(thish.bits);
        hold >>= op;
        bits -= op;
        op = (unsigned int)(thish.op);
        if (op == 0) {                            // literal
            PUP(out) = (unsigned char)(thish.val);
        }
        else if (op & 16) {                        // length base
            len = (unsigned int)(thish.val);
            op &= 15;                            // number of extra bits
            if (op) {
                if (bits < op) {
                    hold += (unsigned int)(PUP(in)) << bits;
                    bits += 8;
                }
                len += (unsigned int)hold & ((1U << op) - 1);
                hold >>= op;
                bits -= op;
            }
            if (bits < 15) {
                hold += (unsigned int)(PUP(in)) << bits;
                bits += 8;
                hold += (unsigned int)(PUP(in)) << bits;
                bits += 8;
            }
            thish = dcode[hold & dmask];
        dodist:
            op = (unsigned int)(thish.bits);
            hold >>= op;
            bits -= op;
            op = (unsigned int)(thish.op);
            if (op & 16) {                        // distance base
                dist = (unsigned int)(thish.val);
                op &= 15;                        // number of extra bits
                if (bits < op) {
                    hold += (unsigned int)(PUP(in)) << bits;
                    bits += 8;
                    if (bits < op) {
                        hold += (unsigned int)(PUP(in)) << bits;
                        bits += 8;
                    }
                }
                dist += (unsigned int)hold & ((1U << op) - 1);
                hold >>= op;
                bits -= op;
                op = (unsigned int)(out - beg);        // max distance in output
                if (dist > op) {                // see if copy from window
                    op = dist - op;                // distance back in window
                    if (op > whave) {
                        //"invalid distance too far back";
                        state->mode = BAD;
                        break;
                    }
                    from = window - OFF;
                    if (write == 0) {            // very common case
                        from += wsize - op;
                        if (op < len) {            // some from window
                            len -= op;
                            do {
                                PUP(out) = PUP(from);
                            } while (--op);
                            from = out - dist;    // rest from output
                        }
                    }
                    else if (write < op) {        // wrap around window
                        from += wsize + write - op;
                        op -= write;
                        if (op < len) {            // some from end of window
                            len -= op;
                            do {
                                PUP(out) = PUP(from);
                            } while (--op);
                            from = window - OFF;
                            if (write < len) {    // some from start of window
                                op = write;
                                len -= op;
                                do {
                                    PUP(out) = PUP(from);
                                } while (--op);
                                from = out - dist;    // rest from output
                            }
                        }
                    }
                    else {                        // contiguous in window
                        from += write - op;
                        if (op < len) {            // some from window
                            len -= op;
                            do {
                                PUP(out) = PUP(from);
                            } while (--op);
                            from = out - dist;    // rest from output
                        }
                    }
                    while (len > 2) {
                        PUP(out) = PUP(from);
                        PUP(out) = PUP(from);
                        PUP(out) = PUP(from);
                        len -= 3;
                    }
                    if (len) {
                    PUP(out) = PUP(from);
                    if (len > 1)
                        PUP(out) = PUP(from);
                    }
                }
                else {
                    from = out - dist;            // copy direct from output
                    do {                        // minimum length is three
                        PUP(out) = PUP(from);
                        PUP(out) = PUP(from);
                        PUP(out) = PUP(from);
                        len -= 3;
                    } while (len > 2);
                    if (len) {
                        PUP(out) = PUP(from);
                        if (len > 1)
                            PUP(out) = PUP(from);
                    }
                }
            }
            else if ((op & 64) == 0) {            // 2nd level distance code
                thish = dcode[thish.val + (hold & ((1U << op) - 1))];
                goto dodist;
            }
            else {
                //"invalid distance code";
                state->mode = BAD;
                break;
            }
        }
        else if ((op & 64) == 0) {                // 2nd level length code
            thish = lcode[thish.val + (hold & ((1U << op) - 1))];
            goto dolen;
        }
        else if (op & 32) {                        // end-of-block
            state->mode = TYPE;
            break;
        }
        else {
            //"invalid literal/length code";
            state->mode = BAD;
            break;
        }
    } while (in < last && out < end);

    // return unused bytes (on entry, bits < 8, so in won't go too far back)
    len = bits >> 3;
    in -= len;
    bits -= len << 3;
    hold &= (1U << bits) - 1;

    // update state and return
    strm->next_in = in + OFF;
    strm->next_out = out + OFF;
    strm->avail_in = (unsigned int)(in < last ? 5 + (last - in) : 5 - (in - last));
    strm->avail_out = (unsigned int)(out < end ? 257 + (end - out) : 257 - (out - end));
    state->hold = hold;
    state->bits = bits;
    return;
}

// permutation of code lengths
cdx_int16 z_order[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};

static int inflate(z_stream * strm, int flush)
{
    struct inflate_state  *state;
    unsigned char  *next;        // next input
    unsigned char  *put;            // next output
    unsigned int have, left;    // available input and output
    unsigned int hold;            // bit buffer
    unsigned int bits;            // bits in bit buffer
    unsigned int in, out;        // save starting available input and output
    unsigned int copy;            // number of stored or match bytes to copy
    unsigned char *from;            // where to copy match bytes from
    code thish;            // current decoding table entry
    code last;            // parent table entry
    unsigned int len;            // length to copy for repeats, bits to drop
    int ret;            // return code

    if (strm == NULL || strm->state == NULL || strm->next_out == NULL ||
        (strm->next_in == NULL && strm->avail_in != 0))
        return Z_STREAM_ERROR;

    state = (struct inflate_state  *)strm->state;
    if (state->mode == TYPE)
        state->mode = TYPEDO;      // skip check
    // Load registers with state for speed
    put = strm->next_out;
    left = strm->avail_out;
    next = strm->next_in;
    have = strm->avail_in;
    hold = state->hold;
    bits = state->bits;
    in = have;
    out = left;
    ret = Z_OK;
    for (;;) {
        switch (state->mode) {
        case HEAD:
            if (state->wrap == 0) {
                state->mode = TYPEDO;
                break;
            }
            NEEDBITS(16);

            if (((BITS(8) << 8) + (hold >> 8)) % 31) {
                //"incorrect header check";
                state->mode = BAD;
                break;
            }
            if (BITS(4) != Z_DEFLATED) {
                //"unknown compression method";
                state->mode = BAD;
                break;
            }
            DROPBITS(4);
            if (BITS(4) + 8 > state->wbits) {
                //"invalid window size";
                state->mode = BAD;
                break;
            }
            strm->adler = state->check = 1;
            state->mode = hold & 0x200 ? DICTID : TYPE;
            // Clear the input bit accumulator
            hold = 0;
            bits = 0;
            break;
        case DICTID:
            NEEDBITS(32);
            strm->adler = state->check = REVERSE(hold);
            // Clear the input bit accumulator
            hold = 0;
            bits = 0;
            state->mode = DICT;
        case DICT:
            if (state->havedict == 0) {
                // Restore state from registers
                strm->next_out = put;
                strm->avail_out = left;
                strm->next_in = next;
                strm->avail_in = have;
                state->hold = hold;
                state->bits = bits;
                return Z_NEED_DICT;
            }
            strm->adler = state->check = 1;
            state->mode = TYPE;
        case TYPE:
            if (flush == Z_BLOCK)
                goto inf_leave;
        case TYPEDO:
            if (state->last) {
                // Remove zero to seven bits as needed to go to a byte boundary
                hold >>= bits & 7;
                bits -= bits & 7;
                state->mode = CHECK;
                break;
            }
            NEEDBITS(3);
            state->last = BITS(1);
            DROPBITS(1);
            switch (BITS(2)) {
            case 0:    // stored block
                state->mode = STORED;
                break;
            case 1:    // fixed block
                fixedtables(state);
                state->mode = LEN;
                break;
            case 2:    // dynamic block
                state->mode = TABLE;
                break;
            case 3:
                //"invalid block type";
                state->mode = BAD;
            }
            DROPBITS(2);
            break;
        case STORED:
            // Remove zero to seven bits as needed to go to a byte boundary
            hold >>= bits & 7;
            bits -= bits & 7;
            NEEDBITS(32);
            if ((hold & 0xffff) != ((hold >> 16) ^ 0xffff)) {
                //"invalid stored block lengths";
                state->mode = BAD;
                break;
            }
            state->length = (unsigned)hold & 0xffff;
            // Clear the input bit accumulator
            hold = 0;
            bits = 0;
            state->mode = COPY;
        case COPY:
            copy = state->length;
            if (copy) {
                if (copy > have) copy = have;
                if (copy > left) copy = left;
                if (copy == 0) goto inf_leave;
                zmemcpy(put, next, copy);
                have -= copy;
                next += copy;
                left -= copy;
                put += copy;
                state->length -= copy;
                break;
            }
            state->mode = TYPE;
            break;
        case TABLE:
            NEEDBITS(14);
            //literal/length���������õ��ķ�����������õ�BITS(5)+1��
            state->nlen = BITS(5) + 257;
            DROPBITS(5);
            //distance�����õ��ķ�����������õ�BIT(5)+1��
            state->ndist = BITS(5) + 1;
            DROPBITS(5);
            /*�����볤�ȡ��������õ��ı��볤�������ǰBITS(4)+4����볤�����鹲��19�
            ���磬���������ǰ5��Ϊ��16��17��18��0��8����5��ֱ��ʾ5�ֱ��볤�ȡ�
            ���볤�������еġ����볤�ȡ����Ǳ�ֱ��д��ѹ���ļ��еģ���ÿ��������õı��볤�Ȳ���
            ֱ��д���ļ�������������һ��huffman��������19����Ž��б��롣��19����ŵ�huffman���
            ���볤����һ��3λ���޷��ֵȷ����ѹ���ļ��й��õ��˱��볤�������е�ǰBITS(4)+4�
            ���ԣ���������[BITS(4)+4]*3λ�����˶ԡ����볤�ȡ�����huffman���������λ����֮��
            �Ǳ�huffman�����˵�literal/length��ų��Ⱥ�distance��ų��ȡ����仰˵��Ҫ�����
            literal/lenth��distance��ţ���Ҫ������Ӧ��huffman���������Ҫ֪��������ŵı��볤�ȡ�
            ���볤�ȱ��洢��ѹ���ļ��У�������ֱ������ֵ�ķ�ʽ�洢�ģ����Ǳ�huffman�����˵ģ�
            ������ġ����볤�ȡ�Ҳ��һ�����볤����ȷ����֪��������볤�ȣ����ɹ�����Ӧ��huffman����
            �Ӷ��á����볤�ȡ���huffman���롣�����볤�ȡ��ı��볤��ֱ����3λ�޷����洢��ѹ���ļ���*/
            state->ncode = BITS(4) + 4;
            DROPBITS(4);
            state->have = 0;
            state->mode = LENLENS;
        case LENLENS:
            while (state->have < state->ncode) {
                NEEDBITS(3);
                state->lens[z_order[state->have++]] = (unsigned short)BITS(3);
                DROPBITS(3);
            }
            while (state->have < 19)
                state->lens[z_order[state->have++]] = 0;
            state->next = state->codes;
            state->lencode = (code *)(state->next);
            state->lenbits = 7;
            ret = inflate_table(CODES, state->lens, 19, &(state->next),
                                &(state->lenbits), state->work);
            if (ret) {
                //"invalid code lengths set";
                state->mode = BAD;
                break;
            }
            state->have = 0;
            state->mode = CODELENS;
        case CODELENS:
            while (state->have < state->nlen + state->ndist) {
                for (;;) {
                    thish = state->lencode[BITS(state->lenbits)];
                    if ((unsigned int)(thish.bits) <= bits) break;
                    PULLBYTE();
                }
                if (thish.val < 16) {
                    NEEDBITS(thish.bits);
                    DROPBITS(thish.bits);
                    state->lens[state->have++] = thish.val;
                }
                else {
                    if (thish.val == 16) {
                        NEEDBITS(thish.bits + 2);
                        DROPBITS(thish.bits);
                        if (state->have == 0) {
                            //"invalid bit length repeat";
                            state->mode = BAD;
                            break;
                        }
                        len = state->lens[state->have - 1];
                        copy = 3 + BITS(2);
                        DROPBITS(2);
                    }
                    else if (thish.val == 17) {
                        NEEDBITS(thish.bits + 3);
                        DROPBITS(thish.bits);
                        len = 0;
                        copy = 3 + BITS(3);
                        DROPBITS(3);
                    }
                    else {
                        NEEDBITS(thish.bits + 7);
                        DROPBITS(thish.bits);
                        len = 0;
                        copy = 11 + BITS(7);
                        DROPBITS(7);
                    }
                    if (state->have + copy > state->nlen + state->ndist) {
                        //"invalid bit length repeat";
                        state->mode = BAD;
                        break;
                    }
                    while (copy--)
                        state->lens[state->have++] = (unsigned short)len;
                }
            }

            // handle error breaks in while
            if (state->mode == BAD) break;

            // build code tables
            state->next = state->codes;
            state->lencode = (code *)(state->next);
            state->lenbits = 9;
            ret = inflate_table(LENS, state->lens, state->nlen, &(state->next),
                                &(state->lenbits), state->work);
            if (ret) {
                //"invalid literal/lengths set";
                state->mode = BAD;
                break;
            }
            state->distcode = (code *)(state->next);
            state->distbits = 6;
            ret = inflate_table(DISTS, state->lens + state->nlen, state->ndist,
                                &(state->next), &(state->distbits), state->work);
            if (ret) {
                //"invalid distances set";
                state->mode = BAD;
                break;
            }
            state->mode = LEN;
        case LEN:
            if (have >= 6 && left >= 258) {
                // Restore state from registers
                strm->next_out = put;
                strm->avail_out = left;
                strm->next_in = next;
                strm->avail_in = have;
                state->hold = hold;
                state->bits = bits;

                if(state->lencode == NULL || state->distcode == NULL) {
                    state->mode = BAD;
                    break;
                }

                inflate_fast(strm, out);
                // Load registers with state for speed
                put = strm->next_out;
                left = strm->avail_out;
                next = strm->next_in;
                have = strm->avail_in;
                hold = state->hold;
                bits = state->bits;
                break;
            }
            for (;;) {
                thish = state->lencode[BITS(state->lenbits)];
                if ((unsigned int)(thish.bits) <= bits) break;
                PULLBYTE();
            }
            if (thish.op && (thish.op & 0xf0) == 0) {
                last = thish;
                for (;;) {
                    thish = state->lencode[last.val +
                            (BITS(last.bits + last.op) >> last.bits)];
                    if ((unsigned int)(last.bits + thish.bits) <= bits) break;
                    PULLBYTE();
                }
                DROPBITS(last.bits);
            }
            DROPBITS(thish.bits);
            state->length = (unsigned int)thish.val;
            if ((int)(thish.op) == 0) {
                state->mode = LIT;
                break;
            }
            if (thish.op & 32) {
                state->mode = TYPE;
                break;
            }
            if (thish.op & 64) {
                //"invalid literal/length code";
                state->mode = BAD;
                break;
            }
            state->extra = (unsigned int)(thish.op) & 15;
            state->mode = LENEXT;
        case LENEXT:
            if (state->extra) {
                NEEDBITS(state->extra);
                state->length += BITS(state->extra);
                DROPBITS(state->extra);
            }
            state->mode = DIST;
        case DIST:
            for (;;) {
                thish = state->distcode[BITS(state->distbits)];
                if ((unsigned int)(thish.bits) <= bits) break;
                PULLBYTE();
            }
            if ((thish.op & 0xf0) == 0) {
                last = thish;
                for (;;) {
                    thish = state->distcode[last.val +
                            (BITS(last.bits + last.op) >> last.bits)];
                    if ((unsigned int)(last.bits + thish.bits) <= bits) break;
                    PULLBYTE();
                }
                DROPBITS(last.bits);
            }
            DROPBITS(thish.bits);
            if (thish.op & 64) {
                //"invalid distance code";
                state->mode = BAD;
                break;
            }
            state->offset = (unsigned int)thish.val;
            state->extra = (unsigned int)(thish.op) & 15;
            state->mode = DISTEXT;
        case DISTEXT:
            if (state->extra) {
                NEEDBITS(state->extra);
                state->offset += BITS(state->extra);
                DROPBITS(state->extra);
            }
            if (state->offset > state->whave + out - left) {
                //"invalid distance too far back";
                state->mode = BAD;
                break;
            }
            state->mode = MATCH;
        case MATCH:
            if (left == 0) goto inf_leave;
            copy = out - left;
            if (state->offset > copy) {            // copy from window
                copy = state->offset - copy;
                if (copy > state->write) {
                    copy -= state->write;
                    from = state->window + (state->wsize - copy);
                }
                else
                    from = state->window + (state->write - copy);
                if (copy > state->length) copy = state->length;
            }
            else {                                // copy from output
                from = put - state->offset;
                copy = state->length;
            }
            if (copy > left) copy = left;
            left -= copy;
            state->length -= copy;
            do {
                *put++ = *from++;
            } while (--copy);
            if (state->length == 0) state->mode = LEN;
            break;
        case LIT:
            if (left == 0) goto inf_leave;
            *put++ = (unsigned char)(state->length);
            left--;
            state->mode = LEN;
            break;
        case CHECK:
            if (state->wrap) {
                NEEDBITS(32);
                out -= left;
                strm->total_out += out;
                state->total += out;
                if (out)
                    strm->adler = state->check = adler32(state->check, put - out, out);
                out = left;
                if ((REVERSE(hold)) != state->check) {
                    //"incorrect data check";
                    state->mode = BAD;
                    break;
                }
                // Clear the input bit accumulator
                hold = 0;
                bits = 0;
            }
            state->mode = DONE;
        case DONE:
            ret = Z_STREAM_END;
            goto inf_leave;
        case BAD:
            ret = Z_DATA_ERROR;
            goto inf_leave;
        case MEM:
            return Z_MEM_ERROR;
        case SYNC:
        default:
            return Z_STREAM_ERROR;
        }
    }

    /* Return from inflate(), updating the total counts and the check value.
       If there was no progress during the inflate() call, return a buffer
       error.  Call updatewindow() to create and/or update the window state.
       Note: a memory error from inflate() is non-recoverable.*/
inf_leave:
    // Restore state from registers
    strm->next_out = put;
    strm->avail_out = left;
    strm->next_in = next;
    strm->avail_in = have;
    state->hold = hold;
    state->bits = bits;
    if (state->wsize || (state->mode < CHECK && out != strm->avail_out)) {
        if (updatewindow(strm, out)) {
            state->mode = MEM;
            return Z_MEM_ERROR;
        }
    }
    in -= strm->avail_in;
    out -= strm->avail_out;
    strm->total_in += in;
    strm->total_out += out;
    state->total += out;
    if (state->wrap && out)
        strm->adler = state->check = adler32(state->check, strm->next_out - out, out);
    strm->data_type = state->bits + (state->last ? 64 : 0) +
                        (state->mode == TYPE ? 128 : 0);
    if (((in == 0 && out == 0) || flush == Z_FINISH) && ret == Z_OK)
        ret = Z_BUF_ERROR;
    return ret;
}

static int inflateInit(z_stream * strm, int windowBits)
{
    struct inflate_state *state;

    if (strm == NULL)
        return Z_STREAM_ERROR;

    state = strm->state;

    if (windowBits < 0) {
        state->wrap = 0;
        windowBits = -windowBits;
    }
    else {
        state->wrap = (windowBits >> 4) + 1;
    }
    if (windowBits < 8 || windowBits > 15) {
        return Z_STREAM_ERROR;
    }
    strm->total_in = strm->total_out = state->total = 0;
    strm->adler = 1;        // to support ill-conceived Java test suite
    state->wbits = (unsigned int)windowBits;
    state->mode = HEAD;
    state->last = 0;
    state->havedict = 0;
    state->wsize = 0;
    state->whave = 0;
    state->hold = 0;
    state->bits = 0;
    state->lencode = state->distcode = state->next = state->codes;
    state->fixedtables_virgin = 1;
    return Z_OK;
}

static int inflateEnd(z_stream * strm)
{
    if (strm == NULL || strm->state == NULL)
        return Z_STREAM_ERROR;
    return Z_OK;
}

z_stream strm;
inflate_state state;

int decode_zlib_data(unsigned char * source, unsigned char * dest,
                     unsigned int DataInSize, unsigned int DataOutSize)
{
    int ret;
    unsigned int have;

    if(source == NULL || dest==NULL)
        return Z_STREAM_ERROR;

    strm.state = &state;

    strm.avail_in = 0;        //��ʼ�ɻ�ȡ���ֽ���Ϊ0
    strm.next_in = NULL;    //��ʼ���뻺����Ϊ��
    ret = inflateInit(&strm, DEF_WBITS);
    if (ret != Z_OK)
        return ret;

    // decompress until deflate stream ends or end of file
    strm.avail_in=DataInSize;
    strm.next_in = source;    //����һ��Ҫ������ֽڿ�ʼ�Ļ�����,�����뻺����

    // run inflate() on input until output buffer not full
    strm.avail_out = DataOutSize;                //����������ʣ��ռ�
    strm.next_out = dest;                //����������ʼ��Ϊout
    ret = inflate(&strm, Z_NO_FLUSH);    //zlib����
    have = DataOutSize - strm.avail_out;        //�Ѿ����������ݵ��ֽ���

    inflateEnd(&strm);

    return have;
}

