/*
******************************************************************************
*
*   Copyright (C) 2003, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
******************************************************************************
*   file name:  ucnv_ext.c
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2003jun13
*   created by: Markus W. Scherer
*
*   Conversion extensions
*/

#include "ucnv_ext.h"

/*
 * TODO _MBCSHeader
 *
 * - Increment minor version number.
 *   Old implementations will be able to handle the base table of a new-format
 *   file.
 * - Use flags:31..8 as 32-bit-addressed offset to auxiliary data.
 * - Aux data: Bunch of indexes (int32_t indexes[>=16]) followed by aligned data
 *   + number of indexes
 *   + index to name of base table (inv. chars)
 *   + index to extension table (32-bit-addressed index)
 *   + size of auxiliary data (bytes)
 *   + size of entire .cnv data (bytes)
 * - Extension-only table: set new outputType so that old code safely rejects
 *   the file.
 */

/* TODO UConverter */
{
    ...;

    /* fields for conversion extension */
    /*
     * TODO: probably need pointer to baseTableSharedData
     * and also copy the base table's pointers for the base table arrays etc.
     * into this sharedData
     */
    const int32_t *cx; /* extension data */

    /* store previous UChars/chars to continue partial matches */
    UChar preFromU[_CNV_EXT_MAX_LENGTH];
    char preToU[_CNV_EXT_MAX_LENGTH;
    int8_t preFromULength, preToULength; /* negative: replay */
};

/*
 * @param cx pointer to extension data; if NULL, returns 0
 * @param pre UChars that must match; !initialMatch: partial match with them
 * @param preLength length of pre, >=1
 * @param src UChars that can be used to complete a match
 * @param srcLength length of src, >=0
 * @param pResult [out] address of pointer to result bytes
 *                      set only in case of a match
 * @param pResultLength [out] address of result length variable;
 *                            gets -2 if *pResult points to a 16-bit value
 *                            where 1..2 bytes are stored in the high/low bytes
 *                            (1 byte: high byte=0; 2 bytes: high then low)
 * @param initialMatch TRUE if new match starting with pre
 *                     FALSE if trying to complete a previously partial match
 * @param useFallback TRUE if fallback mappings are used
 * @param flush TRUE if the end of the input stream is reached
 * @return >0: matched, return value=total match length
 *          0: no match
 *         <0: partial match, return value=negative total match length
 */
static int8_t
ucnv_extMatchFromU(const int32_t *cx,
                   const UChar *pre, int32_t preLength,
                   const UChar *src, int32_t srcLength,
                   const char **pResult, int32_t *pResultLength,
                   UBool initialMatch, UBool useFallback, UBool flush) {
    const uint16_t *p, *limit;
    uint16_t c;

    /* TODO: if match, test useFallback vs. PUA */

    if(cx==NULL) {
        return 0; /* no extension data, no match */
    }

    /* try the fixed-width table first */
    if(initialMatch && preLength==1) {
        p=(const uint16_t *)cx+cx[_CNV_EXT_FIXED_RT];
        limit=(const uint16_t *)cx+cx[useFallback ? _CNV_EXT_FIXED_LIMIT : _CNV_EXT_FIXED_FB];
        c=(uint16_t)*pre;

        for(; p<limit; p+=2) {
            if(c==*p) {
                /* match */
                *pResult=(const char *)(p+1);
                *pResultLength=-2;
                return 1;
            }
        }
    }

    /* try the variable-width table, search for the longest possible match */
    {
        const UChar *us;
        uint16_t i, j, len16, uLen;
        int8_t match, partialMatch;

        p=(const uint16_t *)cx+cx[_CNV_EXT_VAR_RT];
        limit=(const uint16_t *)cx+cx[useFallback ? _CNV_EXT_VAR_LIMIT : _CNV_EXT_VAR_FB];

        match=partialMatch=0;
        for(; p<limit; p+=_CNV_EXT_VAR_LENGTH(len16)) {
            /* match pre[]+src[] against the Unicode side of one variable-width table entry */
            len16=*p;
            uLen=_CNV_EXT_VAR_UCHARS(len16);
            if(uLen<preLength || (!initialMatch && uLen==preLength)) {
                /* the current pair cannot match because it does not have enough UChars */
                continue;
            }

            /* here: preLength<=uLen */
            us=(const UChar *)(p+1);
            for(i=0; i<preLength; ++i) {
                if(pre[i]!=us[i]) {
                    break; /* continue below: I want Java-style continue outerLoop; */
                }
            }
            if(i<preLength) {
                /* no match */
                continue;
            }

            /* pre matches us[] so far, match src against rest; keep j=i-preLength */
            for(j=0;; ++i, ++j) {
                if(i==uLen) {
                    /* match */
                    if(i>match) {
                        /* longer match than before, use it */
                        match=i;
                        *pResult=(const char *)(us+uLen);
                        *pResultLength=_CNV_EXT_VAR_BYTES(len16);
                    }
                    break; /* continue with the next table entry */
                }
                if(j==srcLength) {
                    if(flush) {
                        /* no match */
                        break;
                    } else {
                        /* partial match */
                        if(i>partialMatch) {
                            /* longer partial match than before */
                            partialMatch=i;
                        }
                        break; /* continue with the next table entry */
                    }
                }
                if(src[j]!=us[i]) {
                    /* no match */
                    break;
                }
            }
        }
    }

    /*
     * done searching the entire table
     *
     * if we are at the end of the input, then always use the longest match
     * if there is more to come, then return the longest match, even if it is partial
     */
    if(flush || match>partialMatch) {
        return match;
    } else {
        return -partialMatch;
    }
}

static void
ucnv_extWriteFromU(const char *result, int32_t resultLength,
                   char **target, const char *targetLimit,
                   int32_t **offsets, int32_t srcIndex,
                   UBool useFallback, UBool flush,
                   UErrorCode *pErrorCode) {
    int32_t *o;
    char *t;

    o=offsets!=NULL ? *offsets : NULL;
    t=*target;

    /* output the result */
    if(resultLength==-2) {
        uint16_t res16=*(const uint16_t *)result;
        /* res16!=0 forced by makeconv */
        if(res16>0xff) {
            *t++=(char)(res16>>8);
            if(o!=NULL) {
                *o=srcIndex;
            }
        }
        if(t!=targetLimit) {
            *t++=(char)(res16>>8);
            if(o!=NULL) {
                *o=srcIndex;
            }
        } else {
            /* TODO: buffer overflow */
        }
    } else /* resultLength>=1 */ {
        do {
            *t++=*result++;
            if(o!=NULL) {
                *o=srcIndex;
            }
        } while(--resultLength>0 && t!=targetLimit);

        /* TODO: buffer overflow */
        while(resultLength>0 && t!=targetLimit) {
            *overflow++=*result++;
            --resultLength;
        }
    }

    if(o!=NULL) {
        *offsets=o;
    }
    *target=t;
}

/* target<targetLimit; set error code for unmappable & overflow */
U_CFUNC void U_CALLCONV
ucnv_extInitialMatchFromU(UConverter *cnv,
                          const int32_t *cx,
                          UChar32 cp,
                          const UChar **src, const UChar *srcLimit,
                          char **target, const char *targetLimit,
                          int32_t **offsets, int32_t srcIndex,
                          UBool useFallback, UBool flush,
                          UErrorCode *pErrorCode) {
    UChar pre[U16_MAX_LENGTH];

    const char *result;

    int32_t preLength, resultLength;
    int8_t match;

    /* write the code point into the pre[] buffer */
    preLength=0;
    U16_APPEND_UNSAFE(pre, preLength, cp);

    /* try to match */
    match=_cxMatchFromU(cx,
                        pre, preLength,
                        *src, (int32_t)(srcLimit-*src),
                        &result, &resultLength,
                        TRUE, useFallback, flush);
    if(match>0) {
        /* advance src pointer for the consumed input */
        *src+=match-preLength;

        /* write result */
        _cxWriteFromU(result, resultLength,
                      target, targetLimit,
                      offsets, srcIndex,
                      pErrorCode);
    } else if(match<0) {
        /* save state for partial match */
        const UChar *s;
        int8_t i, j;

        /* copy pre[] and the newly consumed input to preFromU[] */
        /* pre[] first */
        for(j=0; j<preLength; ++j) {
            cnv->preFromU[j]=pre[j];
        }

        /* now append the newly consumed input */
        s=*src;
        match=-match;
        for(i=0; j<match; ++i, ++j) {
            cnv->preFromU[j]=s[i];
        }
        *src=sourceLimit; /* same as *src=s+i; */
        cnv->preFromULength=match;
    } else /* match==0 */ {
        /* no match, prepare for normal unassigned callback for cp */
        for(match==0; match<preLength; ++match) {
            cnv->invalidUCharBuffer[match]=pre[match];
        }
        cnv->invalidUCharLength=match;

        /* set the error code for unassigned */
        *pErrorCode=U_INVALID_CHAR_FOUND;
    }
}

U_CFUNC void U_CALLCONV
ucnv_extContinueMatchFromU(UConverter *cnv,
                           const int32_t *cx,
                           const UChar **src, const UChar *srcLimit,
                           char **target, const char *targetLimit,
                           int32_t **offsets, int32_t srcIndex,
                           UBool useFallback, UBool flush,
                           UErrorCode *pErrorCode) {
    if(cnv->preFromULength>0) {
        /* continue partial match with new input */
        const char *result;
        int32_t resultLength;
        int8_t match;

        match=_cxMatchFromU(cx,
                            cnv->preFromU, cnv->preFromULength,
                            *src, (int32_t)(srcLimit-*src),
                            &result, &resultLength,
                            FALSE, useFallback, flush);
        if(match>0) {
            /* advance src pointer for the consumed input */
            *src+=match-cnv->preFromULength;

            /* write result */
            _cxWriteFromU(result, resultLength,
                          target, targetLimit,
                          offsets, srcIndex,
                          pErrorCode);

            /* reset the preFromU buffer in the converter */
            cnv->preFromULength=0;
        } else if(match<0) {
            /* save state for partial match */
            const UChar *s;
            int8_t i, j;

            /* just _append_ the newly consumed input to preFromU[] */
            s=*src;
            match=-match;
            for(i=0, j=cnv->preFromULength; j<match; ++i, ++j) {
                cnv->preFromU[j]=s[i];
            }
            *src=sourceLimit; /* same as *src=s+i; */
            cnv->preFromULength=match;
        } else /* match==0 */ {
            /*
             * no match
             *
             * We need to split the previous input into two parts:
             *
             * 1. The first code point is unmappable - that's how we got into
             *    trying the extension data in the first place.
             *    We need to move it from the preFromU buffer
             *    to the error buffer, set an error code,
             *    and prepare the rest of the previous input for 2.
             *
             * 2. The rest of the previous input must be converted once we
             *    come back from the callback for the first code point.
             *    At that time, we have to try again from scratch to convert
             *    these input characters.
             *    The replay will be handled by the second half of this
             *    function.
             */
            int8_t i, j;

            /* move the first code point to the error buffer */
            i=0;
            U16_FWD_1(cnv->preFromU, i, cnv->preFromULength);
            for(j=0; j<i; j++) {
                cnv->invalidUCharBuffer[j]=cnv->preFromU[j];
            }
            cnv->invalidUCharLength=i;

            /* set the error code for unassigned */
            *pErrorCode=U_INVALID_CHAR_FOUND;

            /* move the rest of the previous input up to the beginning */
            for(j=0; i<cnv->preFromULength; ++i, ++j) {
                cnv->preFromU[j]=cnv->preFromU[i];
            }

            /* mark it for replay */
            cnv->preFromULength=-j;
        }
    } else if(cnv->preFromULength<0) {
        /* replay previous input after partial match could not be completed */
        UChar pre[_CNV_EXT_MAX_LENGTH];
        int32_t preLength;

        const UChar *preSrc;
        const char *oldTarget;
        int32_t *o;

        /* move the previous input into a local buffer to prevent it from being overridden */
        preLength=-cnv->preFromULength;
        uprv_memcpy(pre, cnv->preFromU, preLength*U_SIZEOF_UCHAR);

        /* reset the preFromU buffer in the converter */
        cnv->preFromULength=0;

        /* convert the previous input */
        preSrc=pre;
        oldTarget=target;
        ucnv_fromUnicode(cnv,
                         target, targetLimit,
                         &preSrc, pre+preLength,
                         NULL, flush,
                         pErrorCode);
        if(offsets!=NULL && (o=*offsets)!=NULL) {
            /* set offsets */
            while(oldTarget!=target) {
                *o++=-1; /* no source index for previous input */
            }
            *offsets=o;
        }

        if(preSrc!=(pre+preLength)) {
            /*
             * What to do with leftover previous input?
             * There might have been a conversion error with the first part;
             * there might be a buffer overflow.
             *
             * We need to put the rest back into preFromU for another replay.
             * It is safe to do so:
             *
             * There cannot be at the same time a new partial match in preFromU
             * (preFromULength>0) _and_ leftover characters in the local pre[]
             * buffer because a partial match occurs at the end
             * of the current input,
             * and leftover pre[] contents means that the end of the input
             * was not reached.
             *
             * There cannot be a new replay on the first part of the current
             * one either (preFromULength<0),
             * because a replay only occurs after a partial match
             * was continued and failed, and there cannot be a partial match
             * when pre[] was not consumed, as explained above.
             *
             * So either we get here and the preFromU buffer is empty
             * (preFromULength==0) or
             * the preFromU buffer is not empty and the local pre[] is consumed
             * (preSrc==(pre+preLength) and we don't get here.
             */
            int8_t i, j;

            for(i=(int8_t)(preSrc-pre), j=0; i<preLength; ++i, ++j) {
                cnv->preFromU[j]=pre[i];
            }
            cnv->preFromULength=-j; /* continue to replay */
        }
    }
    /* do nothing if cnv->preFromULength==0 */
}


/*
 * TODO
 *
 * - use ucnv_updateCallbackOffsets()?
 * - callbacks are allowed to use following input;
 *   if there is a replay buffer, then the callback must be given that
 *   instead of the current source pointer
 * - functions for toU()
 *   + test fromU() first
 *   + use cnv->toUBytes[] for initial input
 *   + use binary search on fixed-width table segments, RTs then RFBs
 *   + use linear search on variable-width table segments, RTs then RFBs
 * - Simple (single-code point) MBCS mapping functions need to handle extensions
 *   so that we can shrink tables for complex charsets.
 *   Sufficient to handle only 1:1 mappings.
 *   Pass cnv=NULL, use only initialMatch=TRUE, and never return a partial
 *   match.
 * - EBCDIC_STATEFUL: support extensions, but the charset string must be
 *   either one single-byte character or a sequence of double-byte ones,
 *   to avoid state transitions inside the mapping and to avoid having to
 *   store character boundaries.
 *   The extension functions will need an additional EBCDIC state in/out
 *   parameter and will have to be able to insert an SI or SO before writing
 *   the mapping result.
 */
