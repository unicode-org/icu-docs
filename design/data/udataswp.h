/*
*******************************************************************************
*
*   Copyright (C) 2003, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*   file name:  udataswp.h
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2003jun05
*   created by: Markus W. Scherer
*
*   Definitions for ICU data transformations for different platforms,
*   changing between big- and little-endian data and/or between
*   charset families (ASCII<->EBCDIC).
*
*   This is currently a design document.
*   Latest Version: http://oss.software.ibm.com/cvs/icu/~checkout~/icuhtml/design/data/udataswp.h
*   CVS: http://oss.software.ibm.com/cvs/icu/icuhtml/design/data/udataswp.h
*/

#include "unicode/utypes.h"

/* forward declaration */
struct UDataSwapper;
typedef struct UDataSwapper UDataSwapper;

/**
 * Function type for data transformation.
 * Transforms data in-place, or just returns the length of the data if
 * the preflight flag is TRUE.
 *
 * @param ds Pointer to UDataSwapper containing global data about the
 *           transformation and function pointers for handling primitive
 *           types.
 * @param data Pointer to the data to be transformed or examined.
 * @param length Length of the data, counting bytes. May be -1 if not known.
 *               The length cannot be determined from the data itself for all
 *               types of data (e.g., not for simple arrays of integers).
 * @param preflight If FALSE, transform the data in-place;
 *                  if TRUE, treat the data pointer as a const pointer and
 *                  only determine the length of the data.
 * @param pErrorCode ICU UErrorCode parameter, must not be NULL and must
 *                   fulfill U_SUCCESS on input.
 * @return The actual length of the data.
 *
 * @see UDataSwapper
 * @draft ICU 2.8
 */
typedef int32_t U_CALLCONV
UDataSwapFn(const UDataSwapper *ds,
            char *data, int32_t length,
            UBool preflight,
            UErrorCode pErrorCode);

/**
 * Read one uint16_t from the input data.
 * @draft ICU 2.8
 */
typedef uint16_t U_CALLCONV
UDataReadUInt16(const UDataSwapper *ds, uint16_t *data);

/**
 * Read one uint32_t from the input data.
 * @draft ICU 2.8
 */
typedef uint32_t U_CALLCONV
UDataReadUInt32(const UDataSwapper *ds, uint32_t *data);

/**
 * Read one invariant character from the input data.
 * @return The invariant character, converted to the local charset family,
 *         or '\xff' if it is not an invariant character.
 * @draft ICU 2.8
 */
typedef UChar U_CALLCONV
UDataReadInvChar(const UDataSwapper *ds, const char *pc);

/**
 * Compare invariant-character strings, one in the input data and the
 * other one caller-provided in the local charset family.
 * You can use -1 for the length parameters of NUL-terminated strings as usual.
 * @draft ICU 2.8
 */
typedef int32_t U_CALLCONV
UDataCompareInvChars(const UDataSwapper *ds,
                     const char *inString, int32_t inLength,
                     const UChar *localString, int32_t localLength);

struct UDataSwapper {
    /** Input endianness. @draft ICU 2.8 */
    UBool inIsBigEndian;
    /** Input charset family. @see U_CHARSET_FAMILY @draft ICU 2.8 */
    int8_t inCharset;
    /** Output endianness. @draft ICU 2.8 */
    UBool outIsBigEndian;
    /** Output charset family. @see U_CHARSET_FAMILY @draft ICU 2.8 */
    int8_t outCharset;

    // basic functions for reading data values

    /** Read one uint16_t from the input data. @draft ICU 2.8 */
    UDataReadUInt16 *readUInt16;
    /** Read one uint32_t from the input data. @draft ICU 2.8 */
    UDataReadUInt32 *readUInt32;
    /** Read one invariant character from the input data. @draft ICU 2.8 */
    UDataReadInvChar *readInvChar;
    /** Compare invariant-character strings. @draft ICU 2.8 */
    UDataCompareInvChars *compareInvChars;

    // basic functions for data transformations

    /** Transform an array of 16-bit integers. @draft ICU 2.8 */
    UDataSwapFn *swapArray16;
    /** Transform an array of 32-bit integers. @draft ICU 2.8 */
    UDataSwapFn *swapArray32;
    /** Transform an invariant-character string. @draft ICu 2.8 */
    UDataSwapFn *swapInvChars;
};

U_CAPI UDataSwapper * U_EXPORT2
udata_openSwapper(UBool inIsBigEndian, int8_t inCharset,
                  UBool outIsBigEndian, int8_t outCharset,
                  UErrorCode *pErrorCode);

/**
 * Open a UDataSwapper for the given input data and the specified output
 * characteristics.
 * Values of -1 for any of the characteristics mean the local platform's
 * characteristics.
 *
 * @see udata_swap
 * @draft ICU 2.8
 */
U_CAPI UDataSwapper * U_EXPORT2
udata_openSwapperForInputData(const char *data, int32_t length,
                              UBool outIsBigEndian, int8_t outCharset,
                              UErrorCode *pErrorCode);

U_CAPI void U_EXPORT2
udata_closeSwapper(UDataSwapper *ds);

/**
 * Identifies and then transforms the ICU data piece in-place, or determines
 * its length. See UDataSwapFn.
 * This function handles .dat data packages as well as single data pieces
 * and internally dispatches to per-type swap functions.
 *
 * @see UDataSwapFn
 * @see udata_openSwapper
 * @see udata_openSwapperForInputData
 * @draft ICU 2.8
 */
U_CAPI void U_EXPORT2
udata_swap(const UDataSwapper *ds,
           char *data, int32_t length,
           UErrorCode *pErrorCode);

/* Implementation functions ------------------------------------------------- */

/** Swap a UTrie. @draft ICU 2.8 */
U_CAPI int32_t U_EXPORT2
udata_swapUTrie(const UDataSwapper *ds,
                char *data, int32_t length,
                UBool preflight,
                UErrorCode pErrorCode);

/** Enumerate a UTrie. May be necessary (not sure) to swap data indexed by UTrie result values. @draft ICU 2.8 */
void
udata_enumUTrie();

/**
 * Read the beginning of an ICU data piece, recognize magic bytes,
 * fill in the UDataInfo.
 * Set a U_UNSUPPORTED_ERROR if it does not look like an ICU data piece.
 *
 * @draft ICU 2.8
 */
void
udata_readUDataInfo(const UDataSwapper *ds,
                    const char *inData, int32_t length,
                    UDataInfo *pInfo,
                    UErrorCode *pErrorCode);
