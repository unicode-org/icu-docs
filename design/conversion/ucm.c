/*
*******************************************************************************
*
*   Copyright (C) 2003, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*   file name:  ucm.c
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2003jun20
*   created by: Markus W. Scherer
*
*   This file reads a .ucm file, stores its mappings and sorts them.
*
*   Bytes are sorted lexically. This is necessary because byte sequences
*   can have widely varying lengths.
*
*   This is different from the old canonucm which stored bytes in 32-bit words,
*   right-justified them and filled unused upper bits with 0s.
*   It makes a difference for canonucm when the same code point maps from
*   different byte sequences.
*
*   Keeping bytes right-justified for widely varying lengths would be hard
*   for the parser and for sorting and searching.
*
*   Before this file was created for conversion extensions,
*   ICU and its tools could not handle byte sequences with more than 4 bytes.
*/

#include "unicode/utypes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* -------------------------------------------------------------------------- */

/*
 * Per-mapping data structure
 *
 * u if uLen==1: Unicode code point
 *   else index to uLen code points
 * b if bLen<=4: up to 4 bytes
 *   else index to bLen bytes
 * uLen number of code points
 * bLen number of words containing left-justified bytes
 * bIsMultipleChars indicates that the bytes contain more than one sequence
 *                  according to the state table
 * f flag for roundtrip (0), fallback (1), sub mapping (2), reverse fallback (3)
 *   same values as in the source file after |
 */
typedef struct Mapping {
    UChar32 u;
    union {
        uint32_t bIndex;
        uint8_t bytes[4];
    } b;
    int8_t uLen, bLen, bIsMultipleChars, f;
} Mapping;

enum {
    UCM_FLAGS_INITIAL,  /* no mappings parsed yet */
    UCM_FLAGS_IMPLICIT, /* .ucm file has mappings without | fallback indicators, later wins */
    UCM_FLAGS_EXPLICIT  /* .ucm file has mappings with | fallback indicators */
};

/*

allow file without fallback indicators for backward compatibility
only for makeconv
must not sort such mappings
disallow when using extension tables because that requires sorting

rptp2ucm has its own mapping parser and sets all-|1 and |3 mappings; normalization function generates |0 and |2

*/

typedef struct Table {
    Mapping *mappings;
    int32_t mappingsCapacity, mappingsLength;

    UChar32 *codePoints;
    int32_t codePointsCapacity, codePointsLength;

    uint8_t *bytes;
    int32_t bytesCapacity, bytesLength;

    /* index map for mapping by bytes first */
    int32_t *reverseMap;

    int8_t flagsType; /* UCM_FLAGS_INITIAL etc. */
} Table;

/* data for sorting, static because qsort does not pass through a context pointer */
static Table *qsortTable=NULL;

/* simple accesses ---------------------------------------------------------- */

static int32_t
getCodePoints(const Table *t, const Mapping *m, const UChar32 **p) {
    int32_t length;

    length=m->uLen;
    if(length==1) {
        *p=&m->u;
    } else {
        *p=t->codePoints+m->u;
    }
    return length;
}

static int32_t
getBytes(const Table *t, const Mapping *m, const uint8_t **p) {
    int32_t length;

    length=m->bLen;
    if(length<=4) {
        *p=m->b.bytes;
    } else {
        *p=t->bytes+m->b.bIndex;
    }
    return length;
}

/* mapping comparisons ------------------------------------------------------ */

static int32_t
compareUnicode(const Mapping *l, const Mapping *r) {
    const UChar32 *lu, *ru;
    int32_t result, i, length;

    if(l->uLen==1 && r->uLen==1) {
        /* compare two single code points */
        return l->u-r->u;
    }

    /* get pointers to the code point sequences */
    if(l->uLen==1) {
        lu=&l->u;
    } else {
        lu=qsortTable->codePoints+l->u;
    }
    if(r->uLen==1) {
        ru=&r->u;
    } else {
        ru=qsortTable->codePoints+r->u;
    }

    /* get the minimum length */
    if(l->uLen<=r->uLen) {
        length=l->uLen;
    } else {
        length=r->uLen;
    }

    /* compare the code points */
    for(i=0; i<length; ++i) {
        result=lu[i]-ru[i];
        if(result!=0) {
            return result;
        }
    }

    /* compare the lengths */
    return l->uLen-r->uLen;
}

static int32_t
compareBytes(const Mapping *l, const Mapping *r) {
    const uint8_t *lb, *rb;
    int32_t result, i, length;

    /* get pointers to the byte sequences */
    if(l->bLen<=4) {
        lb=l->b.bytes;
    } else {
        lb=qsortTable->bytes+l->b.bIndex;
    }
    if(r->bLen<=4) {
        rb=r->b.bytes;
    } else {
        rb=qsortTable->bytes+r->b.bIndex;
    }

    /* get the minimum length */
    if(l->bLen<=r->bLen) {
        length=l->bLen;
    } else {
        length=r->bLen;
    }

    /* compare the bytes */
    for(i=0; i<length; ++i) {
        result=lb[i]-rb[i];
        if(result!=0) {
            return result;
        }
    }

    /* compare the lengths */
    return l->bLen-r->bLen;
}

/* lexically compare Mappings for sorting */
static int
compareMappings(const void *left, const void *right, UBool uFirst) {
    const Mapping *l=(const Mapping *)left, *r=(const Mapping *)right;
    int32_t result;

    /* choose which side to compare first */
    if(uFirst) {
        /* Unicode then bytes */
        result=compareUnicode(l, r);
        if(result==0) {
            result=compareBytes(l, r);
        }
    } else {
        /* bytes then Unicode */
        result=compareBytes(l, r);
        if(result==0) {
            result=compareUnicode(l, r);
        }
    }

    /*
     * when returning the final result, shift it to the right by 16 bits
     * with sign-extension to take care of int possibly being 16 bits wide
     */
    if(result!=0) {
        return (int)(result>>16)|1;
    }

    /* compare the flags */
    return (int)(l->f-r->f);
}

/* sorting by Unicode first sorts mappings directly */
static int
compareMappingsUnicodeFirst(const void *left, const void *right) {
    return compareMappings(left, right, TRUE);
}

/* sorting by bytes first sorts the reverseMap; use indirection to mappings */
static int
compareMappingsBytesFirst(const void *left, const void *right) {
    int32_t l=*(const int32_t *)left, r=*(const int32_t *)right;
    return compareMappings(qsortTable->mappings+left,
                           qsortTable->mappings+right, FALSE);
}

static void
sortTable(Table *t) {
    int32_t i;

    qsortTable=t;

    /* 1. sort by Unicode first */
    qsort(t->mappings, t->mappingsLength, sizeof(Mapping), compareMappingsUnicodeFirst);

    /* build the reverseMap */
    if(t->reverseMap==NULL) {
        t->reverseMap=(int32_t *)malloc(t->mappingsLength*sizeof(int32_t));
        if(t->reverseMap==NULL) {
            fprintf(stderr, "ucm error: unable to allocate reverseMap\n");
            exit(1);
        }
    }
    for(i=0; i<t->reverseMap; ++i) {
        t->reverseMap[i]=i;
    }

    /* 2. sort reverseMap by mappings bytes first */
    qsort(t->reverseMap, t->mappingsLength, sizeof(int32_t), compareMappingsBytesFirst);
}

/*

TODO normalization function for a table
sort table
if there are mappings with the same code points and bytes but |1 and |3, merge them into one |0 (or make |2 where necessary)
if mappings were merged, sort again
-> for rptp2ucm

*/

/* ucm parser --------------------------------------------------------------- */

/*

separate header parser
keep copy of all lines before the first CHARMAP for canonucm
parse state table etc. in order to detect multiple-character byte sequences
call CHARMAP parser once or twice
keep comments between two CHARMAPs


CHARMAP parser
handle sequences of up to length _CNV_EXT_MAX_LENGTH on each side
forbid surrogate code points, at least as first and last code points in a sequence
option for automatically adding multi-char mappings into a secondary table

 */

/* parse a mapping line; must not be empty */
static void
parseMappingLine(Mapping *m,
                 UChar32 codePoints[_CNV_EXT_MAX_LENGTH],
                 uint8_t bytes[_CNV_EXT_MAX_LENGTH],
                 const char *line) {
    const char *s;
    char *end;
    int32_t uLen, bLen;
    int8_t f;

    s=line;
    uLen=bLen=0;

    /* parse code points */
    while(*s=='<') {
        if(uLen==_CNV_EXT_MAX_LENGTH) {
            fprintf(stderr, "ucm error: too many code points on \"%s\"\n", line);
            exit(1);
        }
        if( s[1]!='U' ||
            (codePoints[uLen]=(UChar32)strtoul(s+2, &end, 16), end)==s+2 ||
            *end!='>'
        ) {
            fprintf(stderr, "ucm error: Unicode code point must be formatted as <UXXXX> (1..6 hex digits) - \"%s\"\n", line);
            exit(1);
        }
        if((uint32_t)codePoints[uLen]>0x10ffff || U_IS_SURROGATE(codePoints[uLen])) {
            fprintf(stderr, "ucm error: Unicode code point must be 0..d7ff or e000..10ffff - \"%s\"\n", line);
            exit(1);
        }
        ++uLen;
        s=end+1;
    }

    if(uLen==0) {
        fprintf(stderr, "ucm error: no Unicode code points on \"%s\"\n", line);
        exit(1);
    }

    /* skip white space */
    while(*s==' ' || *s=='\t') {
        ++s;
    }

    /* parse bytes */
    while(*s=='\\') {
        if(bLen==_CNV_EXT_MAX_LENGTH) {
            fprintf(stderr, "ucm error: too many bytes on \"%s\"\n", line);
            exit(1);
        }
        if( s[1]!='x' ||
            (bytes[bLen]=(uint8_t)strtoul(s+2, &end, 16), end)!=s+4
        ) {
            fprintf(stderr, "ucm error: byte must be formatted as \\xXX (2 hex digits) - \"%s\"\n", line);
            exit(1);
        }
        ++bLen;
        s=end;
    }

    if(bLen==0) {
        fprintf(stderr, "ucm error: no bytes on \"%s\"\n", line);
        exit(1);
    }

    /* skip everything until the fallback indicator, even the start of a comment */
    for(;;) {
        if(*s==0) {
            f=-1; /* no fallback indicator */
            break;
        } else if(*s=='|') {
            f=(int8_t)(s[1]-'0');
            if((uint8_t)f>3) {
                fprintf(stderr, "ucm error: fallback indicator must be |0..|3 - \"%s\"\n", line);
                exit(1);
            }
            break;
        }
        ++s;
    }

    m->uLen=uLen;
    m->bLen=bLen;
    m->f=f;
}

/* lookups ------------------------------------------------------------------ */

/*

binary search for first mapping with some code point or byte sequence
check if a code point is the first of any mapping (RT or FB)
check if a byte sequence is a prefix of any mapping (RT or RFB)
check if there is a mapping with the same source units; return whether the target is same or different

*/

/* ucm writer --------------------------------------------------------------- */

/* allows canonucm to just supply main() and arguments */

/*

    /* sort and write all mappings * /
    if(mappingsTop>0) {
        qsort(mappings, mappingsTop, sizeof(Mapping), compareMappings);
        for(i=0; i<mappingsTop; ++i) {
            b=mappings[i].b;
            if(b<=0xff) {
                printf("<U%04lX> \\x%02lX |%lu\n", mappings[i].u, b, mappings[i].f);
            } else if(b<=0xffff) {
                printf("<U%04lX> \\x%02lX\\x%02lX |%lu\n", mappings[i].u, b>>8, b&0xff, mappings[i].f);
            } else if(b<=0xffffff) {
                printf("<U%04lX> \\x%02lX\\x%02lX\\x%02lX |%lu\n", mappings[i].u, b>>16, (b>>8)&0xff, b&0xff, mappings[i].f);
            } else {
                printf("<U%04lX> \\x%02lX\\x%02lX\\x%02lX\\x%02lX |%lu\n", mappings[i].u, b>>24, (b>>16)&0xff, (b>>8)&0xff, b&0xff, mappings[i].f);
            }
        }
    }
    /* output "END CHARMAP" * /
    puts(line);

*/

/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
