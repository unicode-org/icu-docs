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
    /** Transform an invariant-character string. @draft ICU 2.8 */
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
 * Sets a U_UNSUPPORTED_ERROR if the data format is not recognized.
 *
 * @see UDataSwapFn
 * @see udata_openSwapper
 * @see udata_openSwapperForInputData
 * @draft ICU 2.8
 */
U_CAPI int32_t U_EXPORT2
udata_swap(const UDataSwapper *ds,
           char *data, int32_t length,
           UBool preflight,
           UErrorCode *pErrorCode);

/* Implementation functions ------------------------------------------------- */

/*
 * The swap function for each data format is closely related to the code using
 * the data normally and requires access to internal structures.
 * Swap functions should also be relatively small.
 * The best is probably to declare them in this common header and to implement
 * them in the same files where the data is normally used.
 *
 * This is fairly clean in that each implementation file only knows about its
 * local format and includes this header,
 * instead of one swapper implementation knowing everything and including all
 * other internal headers.
 *
 * However, this means to add the data swapping into the common library.
 * We need to discuss the cost (library size increase) and what this means for
 * data formats that are used in the i18n library.
 * Note that it will eventually be desirable to have the swapping implementation
 * in the runtime library - for runtime swapping, which costs setup time
 * but simplifies installation.
 * Preflighting will be particularly important for that, for allocation of
 * a modifiable copy of the data.
 */

#include "unicode/udata.h" /* UDataInfo */
#include "ucmndata.h" /* DataHeader */

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
 * swap the structure.
 * Set a U_UNSUPPORTED_ERROR if it does not look like an ICU data piece.
 *
 * @return The size of the data header, in bytes.
 *
 * @draft ICU 2.8
 */
U_CAPI int32_t U_EXPORT2
udata_swapDataHeader(const UDataSwapper *ds,
                     char *data, int32_t length,
                     UBool preflight,
                     UErrorCode *pErrorCode) {
    DataHeader *Header;
    uint16_t headerSize, infoSize;

    /* argument checking */
    if(pErrorCode==NULL || U_FAILURE(*pErrorCode)) {
        return 0;
    }
    if(data==NULL || length<=0 || pInfo==NULL) {
        *pErrorCode=U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }

    /* check minimum length and magic bytes */
    pHeader=(DataHeader *)data;
    if( length<(sizeof(DataHeader)) ||
        pHeader->dataHeader.magic1!=0xda ||
        pHeader->dataHeader.magic2!=0x27 ||
        (headerSize=ds->readUInt16(ds, &pHeader->dataHeader.headerSize))<sizeof(DataHeader) ||
        (infoSize=ds->readUInt16(ds, &pHeader->dataHeader.info.size))<sizeof(UDataInfo)
    ) {
        *pErrorCode=U_UNSUPPORTED_ERROR;
        return 0;
    }

    /* Most of the fields are just bytes and need no swapping. */
    if(!preflight) {
        /* swap headerSize */
        ds->swapArray16(ds, &pHeader->dataHeader.headerSize, 2, FALSE, pErrorCode);

        /* swap UDataInfo size and reservedWord */
        ds->swapArray16(ds, &pHeader->dataHeader.info.size, 4, FALSE, pErrorCode);

        /* swap copyright statement after the UDataInfo */
        infoSize+=sizeof(pHeader->dataHeader);
        data+=infoSize;
        headerSize-=infoSize;
        /* get the length of the string */
        for(length=0; length<headerSize && data[length]!=0; ++length) {}
        /* swap the string contents */
        ds->swapInvChars(ds, data, length, FALSE, pErrorCode);
    }

    return headerSize;
}

static const struct {
    uint8_t dataFormat[4];
    UDataSwapFn *swapFn;
} swapFns[]={
    { { 0x52, 0x65, 0x73, 0x42 }, udata_swapResourceBundle }    /* dataFormat="ResB" */
};

U_CAPI int32_t U_EXPORT2
udata_swap(const UDataSwapper *ds,
           char *data, int32_t length,
           UBool preflight,
           UErrorCode *pErrorCode) {
    DataHeader *Header;
    int32_t headerSize, i;

    if(pErrorCode==NULL || U_FAILURE(*pErrorCode)) {
        return 0;
    }

    /*
     * Preflight the header first; checks for illegal arguments, too.
     * Do not swap the header right away because the format-specific swapper
     * will swap it, get the headerSize again, and also use the header
     * information. Otherwise we would have to pass some of the information
     * and not be able to use the UDataSwapFn signature.
     */
    headerSize=udata_swapDataHeader(ds, data, length, TRUE, pErrorCode);

    /*
     * If we wanted udata_swap() to also handle non-loadable data like a UTrie,
     * then we could check here for further known magic values and structures.
     */
    if(U_FAILURE(*pErrorCode)) {
        return 0; /* the data format was not recognized */
    }

    /* dispatch to the swap function for the dataFormat */
    pHeader=(DataHeader *)data;
    for(i=0; i<LENGTHOF(swapFns); ++i) {
        if(0==uprv_memcmp(swapFns[i].dataFormat, pHeader->info.dataFormat, 4)) {
            return swapFns[i].swapFn(ds, data, length, preflight, pErrorCode);
        }
    }

    /* the dataFormat was not recognized */
    *pErrorCode=U_UNSUPPORTED_ERROR;
    return 0;
}

U_CAPI int32_t U_EXPORT2
udata_swapResourceBundle(const UDataSwapper *ds,
                         char *data, int32_t length,
                         UBool preflight,
                         UErrorCode *pErrorCode) {
    /*
    need to always enumerate the entire tree
    track lowest address of any item, use as limit for char keys[]
    track highest address of any item to return
    should have thought of storing those in the data...
    detect & swap collation binaries
    ignore other binaries?
    */
}
