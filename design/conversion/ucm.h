/*
*******************************************************************************
*
*   Copyright (C) 2003, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*   file name:  ucm.h
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2003jun20
*   created by: Markus W. Scherer
*
*   Definitions for the .ucm file parser and handler module ucm.c.
*/

#ifndef __UCM_H__
#define __UCM_H__

#include "unicode/utypes.h"
#include "ucnvmbcs.h"
#include "ucnv_ext.h"
#include <stdio.h>

U_CDECL_BEGIN

/* ### TODO move to ucnvmbcs.h */
enum {
    MBCS_MAX_STATE_COUNT=128
};

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
typedef struct UCMapping {
    UChar32 u;
    union {
        uint32_t index;
        uint8_t bytes[4];
    } b;
    int8_t uLen, bLen, f;
} UCMapping;

enum {
    UCM_FLAGS_INITIAL,  /* no mappings parsed yet */
    UCM_FLAGS_IMPLICIT, /* .ucm file has mappings without | fallback indicators, later wins */
    UCM_FLAGS_EXPLICIT  /* .ucm file has mappings with | fallback indicators */
};

typedef struct UCMTable {
    UCMapping *mappings;
    int32_t mappingsCapacity, mappingsLength;

    UChar32 *codePoints;
    int32_t codePointsCapacity, codePointsLength;

    uint8_t *bytes;
    int32_t bytesCapacity, bytesLength;

    /* index map for mapping by bytes first */
    int32_t *reverseMap;

    int8_t flagsType; /* UCM_FLAGS_INITIAL etc. */
} UCMTable;

enum {
    MBCS_STATE_FLAG_DIRECT=1,
    MBCS_STATE_FLAG_SURROGATES,

    MBCS_STATE_FLAG_READY=16
};

typedef struct UCMStates {
    int32_t stateTable[MBCS_MAX_STATE_COUNT][256];
    uint32_t stateFlags[MBCS_MAX_STATE_COUNT],
             stateOffsetSum[MBCS_MAX_STATE_COUNT];

    int32_t countStates, minCharLength, maxCharLength, countToUCodeUnits;
    int8_t conversionType;
} UCMStates;

typedef struct UCMFile {
    UCMTable *base, *ext;
    UCMStates states;

    char baseName[UCNV_MAX_CONVERTER_NAME_LENGTH];
} UCMFile;

/* simple accesses ---------------------------------------------------------- */

#define UCM_GET_CODE_POINTS(t, m) \
    (((m)->uLen==1) ? &(m)->u : (t)->codePoints+(m)->u)

#define UCM_GET_BYTES(t, m) \
    (((m)->bLen<=4) ? (m)->b.bytes : (t)->bytes+(m)->b.index)

/* APIs --------------------------------------------------------------------- */

U_CAPI UCMFile * U_EXPORT2
ucm_open(void);

U_CAPI void U_EXPORT2
ucm_close(UCMFile *ucm);

U_CAPI UBool U_EXPORT2
ucm_parseHeaderLine(UCMFile *ucm,
                    char *line, char **pKey, char **pValue);

U_CAPI void U_EXPORT2
ucm_addMappingFromLine(UCMFile *ucm, const char *line, UBool forBase);


U_CAPI UCMTable * U_EXPORT2
ucm_openTable(void);

U_CAPI void U_EXPORT2
ucm_closeTable(UCMTable *table);

U_CAPI void U_EXPORT2
ucm_sortTable(UCMTable *t);

U_CAPI void U_EXPORT2
ucm_printTable(UCMTable *table, FILE *f);


U_CAPI void U_EXPORT2
ucm_addState(UCMStates *states, const char *s);

U_CAPI void U_EXPORT2
ucm_processStates(UCMStates *states);

U_CAPI int32_t U_EXPORT2
ucm_countChars(UCMStates *states,
               const uint8_t *bytes, int32_t length);


U_CAPI int8_t U_EXPORT2
ucm_parseBytes(uint8_t bytes[UCNV_EXT_MAX_LENGTH], const char *line, const char **ps);

U_CAPI void U_EXPORT2
ucm_parseMappingLine(UCMapping *m,
                     UChar32 codePoints[UCNV_EXT_MAX_LENGTH],
                     uint8_t bytes[UCNV_EXT_MAX_LENGTH],
                     const char *line);

U_CAPI void U_EXPORT2
ucm_addMapping(UCMTable *table,
               UCMapping *m,
               UChar32 codePoints[UCNV_EXT_MAX_LENGTH],
               uint8_t bytes[UCNV_EXT_MAX_LENGTH]);

U_CDECL_END

#endif
