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
 * @return lookup value for the byte, if found; else 0
 */
static U_INLINE uint32_t
ucnv_extFindToU(const uint32_t *toUSection, int32_t length, uint8_t byte) {
    uint32_t word;
    int32_t i, start, limit;

    /*
     * Shift byte once instead of each section word and add 0xffffff.
     * We will compare the shifted/added byte (bbffffff) against
     * section words which have byte values in the same bit position.
     * If and only if byte bb < section byte ss then bbffffff<ssvvvvvv
     * for all v=0..f
     * so we need not mask off the lower 24 bits of each section word.
     */
    word=_CNV_EXT_TO_U_MAKE_WORD(byte, _CNV_EXT_TO_U_VALUE_MASK);

    /* binary search */
    start=0;
    limit=length;
    for(;;) {
        i=limit-start;
        if(i<=1) {
            break; /* done */
        }
        /* start<limit-1 */

        if(i<=4) {
            /* linear search for the last part */
            if(word>=toUSection[start]) {
                break;
            }
            if(++start<limit && word>=toUSection[start]) {
                break;
            }
            if(++start<limit && word>=toUSection[start]) {
                break;
            }
            /* always break at start==limit-1 */
            ++start;
            break;
        }

        i=(start+limit)/2;
        if(word<toUSection[i]) {
            limit=i;
        } else {
            start=i;
        }
    }

    /* did we really find it? */
    if(start<limit && byte==_CNV_EXT_TO_U_GET_BYTE(word=toUSection[start])) {
        return _CNV_EXT_TO_U_GET_VALUE(word); /* never 0 */
    } else {
        return 0; /* not found */
    }
}

/*
 * @return index of the UChar, if found; else <0
 */
static U_INLINE int32_t
ucnv_extFindFromU(const UChar *fromUSection, int32_t length, UChar u) {
    int32_t i, start, limit;

    /* binary search */
    start=0;
    limit=length;
    for(;;) {
        i=limit-start;
        if(i<=1) {
            break; /* done */
        }
        /* start<limit-1 */

        if(i<=4) {
            /* linear search for the last part */
            if(u>=fromUSection[start]) {
                break;
            }
            if(++start<limit && u>=fromUSection[start]) {
                break;
            }
            if(++start<limit && u>=fromUSection[start]) {
                break;
            }
            /* always break at start==limit-1 */
            ++start;
            break;
        }

        i=(start+limit)/2;
        if(u<fromUSection[i]) {
            limit=i;
        } else {
            start=i;
        }
    }

    /* did we really find it? */
    if(start<limit && u==fromUSection[start]) {
        return start;
    } else {
        return -1; /* not found */
    }
}

static U_INLINE UBool
ucnv_extFromUUseFallback(UBool useFallback,
                         const UChar *pre, int32_t preLength) {
    UChar32 c;
    int32_t i;

    /* get the first code point in pre[] */
    if(preLength==0) {
        c=0;
    } else {
        i=0;
        U16_NEXT(pre, i, preLength, c);
    }
    return FROM_U_USE_FALLBACK(useFallback, c);
}

/*
 * @param cx pointer to extension data; if NULL, returns 0
 * @param pre UChars that must match; !initialMatch: partial match with them
 * @param preLength length of pre, >=1
 * @param src UChars that can be used to complete a match
 * @param srcLength length of src, >=0
 * @param pResult [out] address of pointer to result bytes
 *                      set only in case of a match
 * @param pResultLength [out] address of result length variable;
 *                            gets a negative value if the length variable
 *                            itself contains the length and bytes, encoded in
 *                            the format of fromUTableValues[] and then inverted
 * @param useFallback "use fallback" flag, usually from cnv->useFallback
 * @param flush TRUE if the end of the input stream is reached
 * @return >0: matched, return value=total match length (number of input units matched)
 *          0: no match
 *         <0: partial match, return value=negative total match length
 *             (partial matches are never returned for flush==TRUE)
 *             (partial matches are never returned as being longer than _CNV_EXT_MAX_LENGTH)
 */
static int8_t
ucnv_extMatchFromU(const int32_t *cx,
                   const UChar *pre, int32_t preLength,
                   const UChar *src, int32_t srcLength,
                   const char **pResult, int32_t *pResultLength,
                   UBool useFallback, UBool flush) {
    const UChar *fromUTableUChars, *fromUSectionUChars;
    const uint32_t *fromUTableValues, *fromUSectionValues;

    uint32_t value, matchValue;
    int32_t i, j, index, length, matchLength;
    UChar c;

    if(cx==NULL) {
        return 0; /* no extension data, no match */
    }

    /* initialize */
    fromUTableUChars=(const UChar *)cx+cx[_CNV_EXT_FROM_U_UCHARS_INDEX];
    fromUTableValues=(const uint32_t *)cx+cx[_CNV_EXT_FROM_U_VALUES_INDEX];

    matchValue=0;
    i=j=index=matchLength=0;

    /* we must not remember fallback matches when not using fallbacks */

    /* match input units until there is a full match or the input is consumed */
    for(;;) {
        /* go to the next section */
        fromUSectionUChars=fromUTableUChars+index;
        fromUSectionValues=fromUTableValues+index;

        /* read first pair of the section */
        length=*fromUSectionUChars++;
        value=*fromUSectionValues++;
        if( value!=0 &&
            (_CNV_EXT_FROM_U_IS_ROUNDTRIP(value) ||
             ucnv_extFromUUseFallback(useFallback, pre, preLength)
        ) {
            /* remember longest match so far */
            matchValue=value;
            matchLength=i+j;
        }

        /* match pre[] then src[] */
        if(i<preLength) {
            c=pre[i++];
        } else if(j<srcLength) {
            c=src[j++];
        } else {
            /* all input consumed, partial match */
            if(flush || (length=(i+j))>_CNV_EXT_MAX_LENGTH) {
                /*
                 * end of the entire input stream, stop with the longest match so far
                 * or: partial match must not be longer than _CNV_EXT_MAX_LENGTH
                 * because it must fit into state buffers
                 */
                break;
            } else {
                /* continue with more input next time */
                return -length;
            }
        }

        /* search for the current UChar */
        index=ucnv_extFindFromU(fromUSectionUChars, length, c);
        if(index<0) {
            /* no match here, stop with the longest match so far */
            break;
        } else {
            value=fromUSectionValues[index];
            if(_CNV_EXT_FROM_U_IS_PARTIAL(value)) {
                /* partial match, continue */
                index=(int32_t)_CNV_EXT_FROM_U_GET_PARTIAL_INDEX(value);
            } else if(_CNV_EXT_FROM_U_IS_ROUNDTRIP(value) ||
                      ucnv_extFromUUseFallback(useFallback, pre, preLength)
            ) {
                /* full match, stop with result */
                matchValue=value;
                matchLength=i+j;
                break;
            }
        }
    }

    if(matchLength==0) {
        /* no match at all */
        return 0;
    }

    /* return result */
    matchValue=_CNV_EXT_FROM_U_MASK_ROUNDTRIP(matchValue);
    length=(int32_t)_CNV_EXT_FROM_U_GET_LENGTH(matchValue);
    if(length<=_CNV_EXT_FROM_U_MAX_DIRECT_LENGTH) {
        *pResultLength=-(int32_t)matchValue;
    } else {
        *pResultLength=length;
        *pResult=(const char *)cx+cx[_CNV_EXT_FROM_U_BYTES_INDEX]+_CNV_EXT_FROM_U_GET_DATA(matchValue);
    }

    return matchLength;
}

static void
ucnv_extWriteFromU(UConverter *cnv,
                   const char *result, int32_t resultLength,
                   char **target, const char *targetLimit,
                   int32_t **offsets, int32_t srcIndex,
                   UBool flush,
                   UErrorCode *pErrorCode) {
    char buffer[4];

    int32_t *o;
    char *t;

    o=offsets!=NULL ? *offsets : NULL;
    t=*target;

    /* output the result */
    if(resultLength<0) {
        /*
         * Generate a byte array and then write it with the loop below.
         * This is not the fastest possible way, but it should be ok for
         * extension mappings, and it is much simpler.
         * Offset and overflow handling are only done once this way.
         */
        uint32_t value;

        resultLength=-resultLength;
        value=(uint32_t)_CNV_EXT_FROM_U_GET_DATA(resultLength);
        resultLength=_CNV_EXT_FROM_U_GET_LENGTH(resultLength);
        /* resultLength<=_CNV_EXT_FROM_U_MAX_DIRECT_LENGTH==3 */

        result=buffer;
        switch(resultLength) {
        case 3:
            *result++=(char)(value>>16);
        case 2:
            *result++=(char)(value>>8);
        case 1:
            *result++=(char)value;
        default:
            break; /* will never occur */
        }
        result=buffer;
    }

    /* with correct data we have resultLength>0 */
    if(resultLength<=0) {
        return;
    }

    /* write result to target */
    do {
        *t++=*result++;
        if(o!=NULL) {
            *o=srcIndex;
        }
    } while(--resultLength>0 && t!=targetLimit);

    if(o!=NULL) {
        *offsets=o;
    }
    *target=t;

    if(resultLength>0) {
        if(cnv!=NULL) {
            /* write overflow result to overflow buffer */
            uint8_t *overflow=(uint8_t *)cnv->charErrorBuffer;

            cnv->charErrorBufferLength=(int8_t)resultLength;
            while(resultLength>0 && t!=targetLimit) {
                *overflow++=*result++;
                --resultLength;
            }
        }

        *pErrorCode=U_BUFFER_OVERFLOW_ERROR;
    }
}

/*
 * target<targetLimit; set error code for unmappable & overflow
 *
 * Either used in full conversion function with
 *   cnv!=NULL
 *   pSimpleValue==pSimpleLength==NULL
 *   flush either way
 * or used in simple, single-character conversion with
 *   cnv==NULL
 *   pSimpleValue!=NULL and pSimpleLength!=NULL
 *   flush==TRUE
 */
U_CFUNC void U_CALLCONV
ucnv_extInitialMatchFromU(UConverter *cnv,
                          const int32_t *cx,
                          UChar32 cp,
                          const UChar **src, const UChar *srcLimit,
                          char **target, const char *targetLimit,
                          int32_t **offsets, int32_t srcIndex,
                          uint32_t *pSimpleValue, int32_t *pSimpleLength,
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
    match=ucnv_extMatchFromU(cx,
                             pre, preLength,
                             *src, (int32_t)(srcLimit-*src),
                             &result, &resultLength,
                             useFallback, flush);
    if(match>0) {
        /* advance src pointer for the consumed input */
        *src+=match-preLength;

        /* write result */
        if(pSimpleValue==NULL) {
            /* write result to target */
            ucnv_extWriteFromU(cnv,
                               result, resultLength,
                               target, targetLimit,
                               offsets, srcIndex,
                               pErrorCode);
        } else {
            /* write result for simple, single-character conversion */
            if(resultLength<0) {
                resultLength=-resultLength;
                *pSimpleValue=(uint32_t)_CNV_EXT_FROM_U_GET_DATA(resultLength);
                *pSimpleLength=_CNV_EXT_FROM_U_GET_LENGTH(resultLength);
            } else if(resultLength==4) {
                /* de-serialize a 4-byte result */
                *pSimpleValue=
                    (((uint32_t)result[0])<<24)|
                    (((uint32_t)result[1])<<16)|
                    (((uint32_t)result[2])<<8)|
                    ((uint32_t)result[3]);
                *pSimpleLength=4;
            } else /* resultLength>4 */ {
                /* too long for simple conversion, return no match */
                match=0;
                /* set the error code for unassigned */
                *pErrorCode=U_INVALID_CHAR_FOUND;
                /* assume that cnv==NULL so no output into cnv->invalidUCharBuffer[] */
            }
        }
    } else if(match<0) {
        /* save state for partial match (never for simple conversion because there flush==TRUE) */
        const UChar *s;
        int8_t j;

        /* copy pre[] and the newly consumed input to preFromU[] */
        /* pre[] first */
        for(j=0; j<preLength; ++j) {
            cnv->preFromU[j]=pre[j];
        }

        /* now append the newly consumed input */
        s=*src;
        match=-match;
        for(; j<match; ++j) {
            cnv->preFromU[j]=*s++;
        }
        *src=s; /* same as *src=srcLimit; because we reached the end of input */
        cnv->preFromULength=match;
    } else /* match==0 or simple conversion */ {
        /* no match, prepare for normal unassigned callback for cp */
        if(cnv!=NULL) {
            /* cnv==NULL for simple, single-character conversion */
            for(match==0; match<preLength; ++match) {
                cnv->invalidUCharBuffer[match]=pre[match];
            }
            cnv->invalidUCharLength=match;
        }

        /* set the error code for unassigned */
        *pErrorCode=U_INVALID_CHAR_FOUND;
    }
}

/* never called for simple, single-character conversion */
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

        match=ucnv_extMatchFromU(cx,
                                 cnv->preFromU, cnv->preFromULength,
                                 *src, (int32_t)(srcLimit-*src),
                                 &result, &resultLength,
                                 useFallback, flush);
        if(match>0) {
            /* advance src pointer for the consumed input */
            *src+=match-cnv->preFromULength;

            /* write result */
            ucnv_extWriteFromU(cnv,
                               result, resultLength,
                               target, targetLimit,
                               offsets, srcIndex,
                               pErrorCode);

            /* reset the preFromU buffer in the converter */
            cnv->preFromULength=0;
        } else if(match<0) {
            /* save state for partial match */
            const UChar *s;
            int8_t j;

            /* just _append_ the newly consumed input to preFromU[] */
            s=*src;
            match=-match;
            for(j=cnv->preFromULength; j<match; ++j) {
                cnv->preFromU[j]=*s++;
            }
            *src=s; /* same as *src=srcLimit; because we reached the end of input */
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
 *   + test fromU() functions first before writing toU()
 *   + use cnv->toUBytes[] for initial input
 * - EBCDIC_STATEFUL: support extensions, but the charset string must be
 *   either one single-byte character or a sequence of double-byte ones,
 *   to avoid state transitions inside the mapping and to avoid having to
 *   store character boundaries.
 *   The extension functions will need an additional EBCDIC state in/out
 *   parameter and will have to be able to insert an SI or SO before writing
 *   the mapping result.
 */
