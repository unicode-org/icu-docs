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
*   It implements handling of Unicode conversion mappings from .ucm files
*   for makeconv, canonucm, rptp2ucm, etc.
*
*   Unicode code point sequences with a length of more than 1,
*   as well as byte sequences with more than 4 bytes or more than one complete
*   character sequence are handled to support m:n mappings.
*/

#include "unicode/utypes.h"
#include "uarrsort.h"
#include "uparse.h"
#include "ucnvmbcs.h"
#include "ucnv_ext.h"
#include "ucm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* -------------------------------------------------------------------------- */

/*

allow file without fallback indicators for backward compatibility
only for makeconv
must not sort such mappings
disallow when using extension tables because that requires sorting

rptp2ucm has its own mapping parser and sets all-|1 and |3 mappings; normalization function generates |0 and |2

*/

/* mapping comparisons ------------------------------------------------------ */

static int32_t
compareUnicode(UCMTable *table, const UCMapping *l, const UCMapping *r) {
    const UChar32 *lu, *ru;
    int32_t result, i, length;

    if(l->uLen==1 && r->uLen==1) {
        /* compare two single code points */
        return l->u-r->u;
    }

    /* get pointers to the code point sequences */
    lu=UCM_GET_CODE_POINTS(table, l);
    ru=UCM_GET_CODE_POINTS(table, r);

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
compareBytes(UCMTable *table, const UCMapping *l, const UCMapping *r, UBool lexical) {
    const uint8_t *lb, *rb;
    int32_t result, i, length;

    /*
     * A lexical comparison is used for sorting in the builder, to allow
     * an efficient search for a byte sequence that could be a prefix
     * of a previously entered byte sequence.
     *
     * Comparing by lengths first is for compatibility with old .ucm tools
     * like canonucm and rptp2ucm.
     */
    if(lexical) {
        /* get the minimum length and continue */
        if(l->bLen<=r->bLen) {
            length=l->bLen;
        } else {
            length=r->bLen;
        }
    } else {
        /* compare lengths first */
        result=l->bLen-r->bLen;
        if(result!=0) {
            return result;
        } else {
            length=l->bLen;
        }
    }

    /* get pointers to the byte sequences */
    lb=UCM_GET_BYTES(table, l);
    rb=UCM_GET_BYTES(table, r);

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

/* compare UCMappings for sorting */
static int32_t
compareMappings(UCMTable *table, const void *left, const void *right, UBool uFirst) {
    const UCMapping *l=(const UCMapping *)left, *r=(const UCMapping *)right;
    int32_t result;

    /* choose which side to compare first */
    if(uFirst) {
        /* Unicode then bytes */
        result=compareUnicode(table, l, r);
        if(result==0) {
            result=compareBytes(table, l, r, FALSE); /* not lexically, like canonucm */
        }
    } else {
        /* bytes then Unicode */
        result=compareBytes(table, l, r, TRUE); /* lexically, for builder */
        if(result==0) {
            result=compareUnicode(table, l, r);
        }
    }

    if(result!=0) {
        return result;
    }

    /* compare the flags */
    return l->f-r->f;
}

/* sorting by Unicode first sorts mappings directly */
static int32_t
compareMappingsUnicodeFirst(const void *context, const void *left, const void *right) {
    return compareMappings((UCMTable *)context, left, right, TRUE);
}

/* sorting by bytes first sorts the reverseMap; use indirection to mappings */
static int32_t
compareMappingsBytesFirst(const void *context, const void *left, const void *right) {
    UCMTable *table=(UCMTable *)context;
    int32_t l=*(const int32_t *)left, r=*(const int32_t *)right;
    return compareMappings(table, table->mappings+l, table->mappings+r, FALSE);
}

U_CAPI void U_EXPORT2
ucm_sortTable(UCMTable *t) {
    UErrorCode errorCode;
    int32_t i;

    errorCode=U_ZERO_ERROR;

    /* 1. sort by Unicode first */
    uprv_sortArray(t->mappings, t->mappingsLength, sizeof(UCMapping),
                   compareMappingsUnicodeFirst, t,
                   FALSE, &errorCode);

    /* build the reverseMap */
    if(t->reverseMap==NULL) {
        t->reverseMap=(int32_t *)malloc(t->mappingsLength*sizeof(int32_t));
        if(t->reverseMap==NULL) {
            fprintf(stderr, "ucm error: unable to allocate reverseMap\n");
            exit(U_MEMORY_ALLOCATION_ERROR);
        }
    }
    for(i=0; i<t->mappingsLength; ++i) {
        t->reverseMap[i]=i;
    }

    /* 2. sort reverseMap by mappings bytes first */
    uprv_sortArray(t->reverseMap, t->mappingsLength, sizeof(int32_t),
                   compareMappingsBytesFirst, t,
                   FALSE, &errorCode);

    if(U_FAILURE(errorCode)) {
        fprintf(stderr, "ucm error: sortTable()/uprv_sortArray() fails - %s\n",
                u_errorName(errorCode));
        exit(errorCode);
    }
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
handle sequences of up to length UCNV_EXT_MAX_LENGTH on each side
forbid surrogate code points, at least as first and last code points in a sequence
option for automatically adding multi-char mappings into a secondary table

 */

U_CAPI int8_t U_EXPORT2
ucm_parseBytes(uint8_t bytes[UCNV_EXT_MAX_LENGTH], const char *line, const char **ps) {
    const char *s=*ps;
    char *end;
    int8_t bLen;

    bLen=0;
    for(;;) {
        /* skip an optional plus sign */
        if(bLen>0 && *s=='+') {
            ++s;
        }
        if(*s!='\\') {
            break;
        }

        if(bLen==UCNV_EXT_MAX_LENGTH) {
            fprintf(stderr, "ucm error: too many bytes on \"%s\"\n", line);
            exit(U_INVALID_TABLE_FORMAT);
        }
        if( s[1]!='x' ||
            (bytes[bLen]=(uint8_t)strtoul(s+2, &end, 16), end)!=s+4
        ) {
            fprintf(stderr, "ucm error: byte must be formatted as \\xXX (2 hex digits) - \"%s\"\n", line);
            exit(U_INVALID_TABLE_FORMAT);
        }
        ++bLen;
        s=end;
    }

    *ps=s;
    return bLen;
}

/* parse a mapping line; must not be empty */
U_CAPI void U_EXPORT2
ucm_parseMappingLine(UCMapping *m,
                     UChar32 codePoints[UCNV_EXT_MAX_LENGTH],
                     uint8_t bytes[UCNV_EXT_MAX_LENGTH],
                     const char *line) {
    const char *s;
    char *end;
    int8_t uLen, bLen, f;

    s=line;
    uLen=bLen=0;

    /* parse code points */
    for(;;) {
        /* skip an optional plus sign */
        if(uLen>0 && *s=='+') {
            ++s;
        }
        if(*s!='<') {
            break;
        }

        if(uLen==UCNV_EXT_MAX_LENGTH) {
            fprintf(stderr, "ucm error: too many code points on \"%s\"\n", line);
            exit(U_MEMORY_ALLOCATION_ERROR);
        }
        if( s[1]!='U' ||
            (codePoints[uLen]=(UChar32)strtoul(s+2, &end, 16), end)==s+2 ||
            *end!='>'
        ) {
            fprintf(stderr, "ucm error: Unicode code point must be formatted as <UXXXX> (1..6 hex digits) - \"%s\"\n", line);
            exit(U_INVALID_TABLE_FORMAT);
        }
        if((uint32_t)codePoints[uLen]>0x10ffff || U_IS_SURROGATE(codePoints[uLen])) {
            fprintf(stderr, "ucm error: Unicode code point must be 0..d7ff or e000..10ffff - \"%s\"\n", line);
            exit(U_INVALID_TABLE_FORMAT);
        }
        ++uLen;
        s=end+1;
    }

    if(uLen==0) {
        fprintf(stderr, "ucm error: no Unicode code points on \"%s\"\n", line);
        exit(U_INVALID_TABLE_FORMAT);
    } else if(uLen==1) {
        m->u=codePoints[0];
    }

    s=u_skipWhitespace(s);

    /* parse bytes */
    bLen=ucm_parseBytes(bytes, line, &s);

    if(bLen==0) {
        fprintf(stderr, "ucm error: no bytes on \"%s\"\n", line);
        exit(U_INVALID_TABLE_FORMAT);
    } else if(bLen<=4) {
        memcpy(m->b.bytes, bytes, bLen);
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
                exit(U_INVALID_TABLE_FORMAT);
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

/* general APIs ------------------------------------------------------------- */

U_CAPI UCMTable * U_EXPORT2
ucm_openTable() {
    UCMTable *table=(UCMTable *)malloc(sizeof(UCMTable));
    if(table==NULL) {
        fprintf(stderr, "ucm error: unable to allocate a UCMTable\n");
        exit(U_MEMORY_ALLOCATION_ERROR);
    }

    memset(table, 0, sizeof(UCMTable));
    return table;
}

U_CAPI void U_EXPORT2
ucm_closeTable(UCMTable *table) {
    if(table!=NULL) {
        free(table->mappings);
        free(table->codePoints);
        free(table->bytes);
        free(table->reverseMap);
        free(table);
    }
}

U_CAPI void U_EXPORT2
ucm_addMapping(UCMTable *table,
               UCMapping *m,
               UChar32 codePoints[UCNV_EXT_MAX_LENGTH],
               uint8_t bytes[UCNV_EXT_MAX_LENGTH]) {
    UCMapping *tm;
    int32_t index;

    if(table->mappingsLength>=table->mappingsCapacity) {
        /* make the mappings array larger */
        if(table->mappingsCapacity==0) {
            table->mappingsCapacity=300;
        } else {
            table->mappingsCapacity*=2;
        }
        table->mappings=(UCMapping *)realloc(table->mappings,
                                             table->mappingsCapacity*sizeof(UCMapping));
        if(table->mappings==NULL) {
            fprintf(stderr, "ucm error: unable to allocate %d UCMappings\n",
                            table->mappingsCapacity);
            exit(U_MEMORY_ALLOCATION_ERROR);
        }

        if(table->reverseMap!=NULL) {
            /* the reverseMap must be reallocated in a new sort */
            free(table->reverseMap);
            table->reverseMap=NULL;
        }
    }

    if(m->uLen>1 && table->codePointsCapacity==0) {
        table->codePointsCapacity=10000;
        table->codePoints=(UChar32 *)malloc(table->codePointsCapacity*4);
        if(table->codePoints==NULL) {
            fprintf(stderr, "ucm error: unable to allocate %d UChar32s\n",
                            table->codePointsCapacity);
            exit(U_MEMORY_ALLOCATION_ERROR);
        }
    }

    if(m->bLen>4 && table->bytesCapacity==0) {
        table->bytesCapacity=10000;
        table->bytes=(uint8_t *)malloc(table->bytesCapacity);
        if(table->bytes==NULL) {
            fprintf(stderr, "ucm error: unable to allocate %d bytes\n",
                            table->bytesCapacity);
            exit(U_MEMORY_ALLOCATION_ERROR);
        }
    }

    if(m->uLen>1) {
        index=table->codePointsLength;
        table->codePointsLength+=m->uLen;
        if(table->codePointsLength>table->codePointsCapacity) {
            fprintf(stderr, "ucm error: too many code points in multiple-code point mappings\n");
            exit(U_MEMORY_ALLOCATION_ERROR);
        }

        memcpy(table->codePoints+index, codePoints, m->uLen*4);
        m->u=index;
    }

    if(m->bLen>4) {
        index=table->bytesLength;
        table->bytesLength+=m->bLen;
        if(table->bytesLength>table->bytesCapacity) {
            fprintf(stderr, "ucm error: too many bytes in mappings with >4 charset bytes\n");
            exit(U_MEMORY_ALLOCATION_ERROR);
        }

        memcpy(table->bytes+index, bytes, m->bLen);
        m->b.index=index;
    }

    tm=table->mappings+table->mappingsLength++;
    memcpy(tm, m, sizeof(UCMapping));
}

U_CAPI void U_EXPORT2
ucm_printTable(UCMTable *table, FILE *f) {
    UCMapping *m;
    UChar32 *codePoints;
    uint8_t *bytes;

    int32_t i, j;

    m=table->mappings;
    for(i=0; i<table->mappingsLength; ++m, ++i) {
        codePoints=UCM_GET_CODE_POINTS(table, m);
        for(j=0; j<m->uLen; ++j) {
            fprintf(f, "<U%04lX>", codePoints[j]);
        }

        fputc(' ', f);

        bytes=UCM_GET_BYTES(table, m);
        for(j=0; j<m->bLen; ++j) {
            fprintf(f, "\\x%02X", bytes[j]);
        }

        if(m->f>=0) {
            fprintf(f, " |%lu\n", m->f);
        } else {
            fputs("\n", f);
        }
    }
}

U_CAPI UCMFile * U_EXPORT2
ucm_open() {
    UCMFile *ucm=(UCMFile *)malloc(sizeof(UCMFile));
    if(ucm==NULL) {
        fprintf(stderr, "ucm error: unable to allocate a UCMFile\n");
        exit(U_MEMORY_ALLOCATION_ERROR);
    }

    memset(ucm, 0, sizeof(UCMFile));

    ucm->base=ucm_openTable();
    ucm->ext=ucm_openTable();

    ucm->states.stateFlags[0]=MBCS_STATE_FLAG_DIRECT;
    ucm->states.conversionType=UCNV_UNSUPPORTED_CONVERTER;
    ucm->states.minCharLength=ucm->states.maxCharLength=1;

    return ucm;
}

U_CAPI void U_EXPORT2
ucm_close(UCMFile *ucm) {
    if(ucm!=NULL) {
        free(ucm->base);
        free(ucm->ext);
        free(ucm);
    }
}

U_CAPI void U_EXPORT2
ucm_addMappingFromLine(UCMFile *ucm, const char *line, UBool forBase) {
    UCMapping m={ 0 };
    UChar32 codePoints[UCNV_EXT_MAX_LENGTH];
    uint8_t bytes[UCNV_EXT_MAX_LENGTH];

    ucm_parseMappingLine(&m, codePoints, bytes, line);

    /*
     * Add the mapping to the base table if this is requested
     * and it is a 1:1 mapping.
     * Otherwise, add it to the extension table.
     */
    if(forBase && m.uLen==1) {
        int32_t count=ucm_countChars(&ucm->states, bytes, m.bLen);
        if(count==1) {
            ucm_addMapping(ucm->base, &m, codePoints, bytes);
            return;
        } else if(count<1) {
            /* illegal byte sequence */
            for(count=0; count<m.bLen; ++count) {
                fprintf(stderr, " %02X", bytes[count]);
            }
            exit(U_INVALID_TABLE_FORMAT);
        }
    }

    ucm_addMapping(ucm->ext, &m, codePoints, bytes);
}
