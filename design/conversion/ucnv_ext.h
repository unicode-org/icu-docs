/*
******************************************************************************
*
*   Copyright (C) 2003, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
******************************************************************************
*   file name:  ucnv_ext.h
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2003jun13
*   created by: Markus W. Scherer
*
*   Conversion extensions
*/

/*
 * Modified from icuhtml/design/conversion/conversion_extensions.html
 *
 * Conversion extensions serve two purposes:
 * 1. They support m:n mappings.
 * 2. They support extension-only conversion files that are used together
 *    with the regular conversion data in base files.
 *
 * A base file may contain an extension table (explicitly requested or
 * implicitly generated for m:n mappings), but its extension table is not
 * used when an extension-only file is used.
 *
 * It is an error if a base file contains any regular (not extension) mapping
 * from the same sequence as a mapping in the extension file
 * because the base mapping would hide the extension mapping.
 *
 *
 * Data for conversion extensions:
 *
 * Several tables store pairs of Unicode and codepage strings.
 * Each string has a length of 1.._CNV_EXT_MAX_LENGTH.
 * Unicode strings do not begin with trail surrogates, and do not end with
 * lead surrogates.
 *
 * There is a fixed-width table for fast conversion and compact storage of the
 * most common mappings.
 *
 * Each segment of the fixed-width table is sorted by codepage bytes.
 * Only one side can be sorted. Conversion to Unicode is more common
 * because input volume is almost always larger than output volume.
 * Unicode charsets are always recommended, but there is only a choice
 * for output - the input conversion depends on the data.
 *
 * The second table stores pairs of variable-length strings.
 * Segments of the variable-width table are not sorted because the
 * table structure with its variable-length records does not allow
 * random access anyway.
 *
 * Search roundtrips then (reverse) fallbacks because roundtrip mappings are
 * more common.
 * A linear search for fromUnicode conversion simply spans both the roundtrip
 * and fallback segments, which are stored in this order.
 *
 * Mappings from multiple characters (code points or codepage state
 * table sequences) must be searched preferring the longest match.
 * For this to work and be efficient, the variable-width table must contain
 * all multiple-character mappings (even if they map from 2 bytes to the BMP)
 * as well as(!) all mappings that contain prefixes of the multiple characters.
 * If an extension table is built on top of a base table in another file
 * and a base table entry is a prefix of a multi-character mapping, then
 * this is an error.
 *
 *
 * Data structure:
 *
 * int32_t indexes[16]; -- or indexes[indexes[0]/2] to be forward compatible
 *
 * array of indexes to extension table segments
 * each indexes[i] is a 16-bit-addressed index from the start of indexes itself
 *
 * indexes[0], [1], [2] contain indexes to the fixedTable[] segments for
 * reverse fallback/roundtrip/fallback mappings
 *
 * [0] also indicates the length of the indexes[] array before the
 *     conversion data (==2*16 uint16_t==16 uint32_t)
 * [3], [4], [5] contain indexes to the variableTable[] segments
 * [3] also indicates the first index after the fixedTable[]
 * [6] indicates the first index after the variableTable[]
 * [7]..[14] reserved
 * [15] number of bytes for the entire extension structure
 * [>15] reserved; there are indexes[0]/2 indexes
 *
 *
 * uint16_t fixedTable[(i3-i0)*2];
 *
 * each two words contain a Unicode BMP code point and 1..2 charset bytes,
 * both encoded as platform-endian 16-bit words
 *
 *
 * uint16_t variableTable[i6];
 *
 * each pair of (Unicode units, charset bytes) is encoded as follows:
 *   uint16_t lengths;
 *   UChar unicodeUnits[lengths bits 3..0]
 *   uint8_t charsetBytes[lengths bits 7..4]
 *     padded to 16-bit boundary;
 *     total number of 16-bit units for the pair
 *     including lengths word: lengths bits 15..8
 */
enum {
    _CNV_EXT_INDEXES_LIMIT,

    _CNV_EXT_FIXED_RFB=_CNV_EXT_INDEXES_LIMIT,
    _CNV_EXT_FIXED_RT,
    _CNV_EXT_FIXED_FB,
    _CNV_EXT_FIXED_LIMIT,

    _CNV_EXT_VAR_RFB=_CNV_EXT_FIXED_LIMIT,
    _CNV_EXT_VAR_RT,
    _CNV_EXT_VAR_FB,
    _CNV_EXT_VAR_LIMIT,

    _CNV_EXT_SIZE=15
};

enum {
    /**
     * Max 15 UChars or bytes per conversion unit.
     * Corresponds to 4 bits per length field in the variable-width
     * extension data table.
     */
    _CNV_EXT_MAX_LENGTH=15
};

#define _CNV_EXT_VAR_UCHARS(len16) ((len16)&0xf)
#define _CNV_EXT_VAR_BYTES(len16) (((len16)>>4)&0xf)
#define _CNV_EXT_VAR_LENGTH(len16) ((len16)>>8)
