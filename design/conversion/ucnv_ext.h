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

#ifndef __UCNV_EXT_H__
#define __UCNV_EXT_H__

/*
 * See icuhtml/design/conversion/conversion_extensions.html
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
 * One set of data structures per conversion direction (to/from Unicode).
 * The data structures are sorted by input units to allow for binary search.
 * Input sequences of more than one unit are handled like contraction tables
 * in collation:
 * The lookup value of a unit points to another table that is to be searched
 * for the next unit, recursively.
 *
 * Long output strings are stored in separate arrays, with length and index
 * in the lookup tables.
 * Output results also include a flag distinguishing roundtrip from
 * (reverse) fallback mappings.
 *
 * Input Unicode strings must not begin or end with unpaired surrogates
 * to avoid problems with matches on parts of surrogate pairs.
 *
 * Mappings from multiple characters (code points or codepage state
 * table sequences) must be searched preferring the longest match.
 * For this to work and be efficient, the variable-width table must contain
 * all mappings that contain prefixes of the multiple characters.
 * If an extension table is built on top of a base table in another file
 * and a base table entry is a prefix of a multi-character mapping, then
 * this is an error.
 *
 *
 * Data structure:
 *
 * int32_t indexes[>=16];
 *
 *   Array of indexes and lengths etc. The length of the array is at least 16.
 *   The actual length is stored in indexes[0] to be forward compatible.
 *
 *   Each index to another array is the number of array base units
 *   from indexes[].
 *   Each length of an array is the number of array base units in that array.
 *
 *   Usage of indexes[i]:
 *   [0]  length of indexes[]
 *
 *   [1]  index of toUTable[] (array of uint32_t)
 *   [2]  length of toUTable[]
 *   [3]  index of toUUChars[] (array of UChar)
 *   [4]  length of toUUChars[]
 *
 *   [5]  index of fromUTableUChars[] (array of UChar)
 *   [6]  index of fromUTableValues[] (array of uint32_t)
 *   [7]  length of fromUTableUChars[] and fromUTableValues[]
 *   [8]  index of fromUBytes[] (array of char)
 *   [9]  length of fromUBytes[]
 *
 *   [10]..[14] reserved
 *   [15] number of bytes for the entire extension structure
 *   [>15] reserved; there are indexes[0] indexes
 *
 *
 * uint32_t toUTable[];
 *
 *   Array of byte/value pairs for lookups for toUnicode conversion.
 *   The array is partitioned into sections like collation contraction tables.
 *   Each section contains one word with the number of following words and
 *   a default value for when the lookup in this section yields no match.
 *
 *   Each uint32_t contains an input byte value in bits 31..24 and the
 *   corresponding lookup value in bits 23..0.
 *   Interpret the value as follows:
 *     if(value<0x1f0000) {
 *       partial match - use value as index to the next toUTable section
 *       and match the next unit; (0 indexes toUTable[0])
 *     } else {
 *       if(bit 23 set) {
 *         roundtrip;
 *       } else {
 *         fallback;
 *       }
 *       unset value bit 23;
 *       if(value<=0x2fffff) {
 *         (value-0x1f0000) is a code point; (BMP: value<=0x1fffff)
 *       } else {
 *         bits 17..0 (value&0x3ffff) is an index to
 *           the result UChars in toUUChars[]; (0 indexes toUUChars[0])
 *         length of the result=((value>>18)-12); (length=0..19)
 *       }
 *     }
 *
 *   The first word in a section contains the number of following words in the
 *   input byte position (bits 31..24, number=1..0xff).
 *   The value of the initial word is used when the current byte is not found
 *   in this section.
 *   If the value is not 0, then it represents a result as above.
 *   If the value is 0, then the search has to return a shorter match with an
 *   earlier default value as the result, or result in "unmappable" even for the
 *   initial bytes.
 *
 *
 * UChar toUUChars[];
 *
 *   Contains toUnicode mapping results, stored as sequences of UChars.
 *   Indexes and lengths stored in the toUTable[].
 *
 *
 * UChar fromUTableUChars[];
 * uint32_t fromUTableValues[];
 *
 *   The fromUTable is split into two arrays, but works otherwise much like
 *   the toUTable. The array is partitioned into sections like collation
 *   contraction tables and toUTable.
 *   A row in the table consists of same-index entries in fromUTableUChars[]
 *   and fromUTableValues[].
 *
 *   Interpret a value as follows:
 *     if(value<=0xffffff) { (bits 31..24 are 0)
 *       partial match - use value as index to the next fromUTable section
 *       and match the next unit; (0 indexes fromUTable[0])
 *     } else {
 *       if(bit 31 set) {
 *         roundtrip;
 *       } else {
 *         fallback;
 *       }
 *       length=(value>>24)&0x7f; (bits 30..24)
 *       if(length==1..3) {
 *         bits 23..0 contain 1..3 bytes, padded with 00s on the left;
 *       } else {
 *         bits 23..0 (value&0xffffff) is an index to
 *           the result bytes in fromUBytes[]; (0 indexes fromUBytes[0])
 *       }
 *     }
 *       
 *   The first pair in a section contains the number of following pairs in the
 *   UChar position (16 bits, number=1..0xffff).
 *   The value of the initial pair is used when the current UChar is not found
 *   in this section.
 *   If the value is not 0, then it represents a result as above.
 *   If the value is 0, then the search has to return a shorter match with an
 *   earlier default value as the result, or result in "unmappable" even for the
 *   initial UChars.
 *
 *
 * char fromUBytes[];
 *
 *   Contains fromUnicode mapping results, stored as sequences of chars.
 *   Indexes and lengths stored in the fromUTableValues[].
 */
enum {
    _CNV_EXT_INDEXES_LENGTH,

    _CNV_EXT_TO_U_INDEX,
    _CNV_EXT_TO_U_LENGTH,
    _CNV_EXT_TO_U_UCHARS_INDEX,
    _CNV_EXT_TO_U_UCHARS_LENGTH,

    _CNV_EXT_FROM_U_UCHARS_INDEX,
    _CNV_EXT_FROM_U_VALUES_INDEX,
    _CNV_EXT_FROM_U_LENGTH,
    _CNV_EXT_FROM_U_BYTES_INDEX,
    _CNV_EXT_FROM_U_BYTES_LENGTH,

    _CNV_EXT_SIZE=15,
    _CNV_EXT_INDEXES_MIN_LENGTH=16
};

enum {
    /* need a limit for state buffers */
    _CNV_EXT_MAX_LENGTH=16;
};

/* toUnicode helpers -------------------------------------------------------- */

#define _CNV_EXT_TO_U_BYTE_SHIFT 24
#define _CNV_EXT_TO_U_VALUE_MASK 0xffffff
#define _CNV_EXT_TO_U_MIN_CODE_POINT 0x1f0000
#define _CNV_EXT_TO_U_MAX_CODE_POINT 0x2fffff
#define _CNV_EXT_TO_U_ROUNDTRIP_FLAG ((uint32_t)1<<23)
#define _CNV_EXT_TO_U_INDEX_MASK 0x3ffff
#define _CNV_EXT_TO_U_LENGTH_SHIFT 18
#define _CNV_EXT_TO_U_LENGTH_OFFSET 12

/* maximum number of indexed UChars */
#define _CNV_EXT_TO_U_MAX_LENGTH 19

#define _CNV_EXT_TO_U_MAKE_WORD(byte, value) ((uint32_t)(byte)<<_CNV_EXT_TO_U_BYTE_SHIFT)|(value))

#define _CNV_EXT_TO_U_GET_BYTE(word) ((word)>>_CNV_EXT_TO_U_BYTE_SHIFT)
#define _CNV_EXT_TO_U_GET_VALUE(word) ((word)&_CNV_EXT_TO_U_VALUE_MASK)

#define _CNV_EXT_TO_U_IS_PARTIAL(value) ((value)<_CNV_EXT_TO_U_MIN_CODE_POINT)
#define _CNV_EXT_TO_U_GET_PARTIAL_INDEX(value) (value)

#define _CNV_EXT_TO_U_IS_ROUNDTRIP(value) (((value)&_CNV_EXT_TO_U_ROUNDTRIP_FLAG)!=0)
#define _CNV_EXT_TO_U_MASK_ROUNDTRIP(value) ((value)&~_CNV_EXT_TO_U_ROUNDTRIP_FLAG)

/* use after masking off the roundtrip flag */
#define _CNV_EXT_TO_U_IS_CODE_POINT(value) ((value)<=_CNV_EXT_TO_U_MAX_CODE_POINT)
#define _CNV_EXT_TO_U_GET_CODE_POINT(value) ((value)-_CNV_EXT_TO_U_MIN_CODE_POINT)

#define _CNV_EXT_TO_U_GET_INDEX(value) ((value)&_CNV_EXT_TO_U_INDEX_MASK)
#define _CNV_EXT_TO_U_GET_LENGTH(value) (((value)>>_CNV_EXT_TO_U_LENGTH_SHIFT)-_CNV_EXT_TO_U_LENGTH_OFFSET)

/* fromUnicode helpers ------------------------------------------------------ */

#define _CNV_EXT_FROM_U_LENGTH_SHIFT 24
#define _CNV_EXT_FROM_U_ROUNDTRIP_FLAG ((uint32_t)1<<31)
#define _CNV_EXT_FROM_U_DATA_MASK 0xffffff

/* at most 3 bytes in the lower part of the value */
#define _CNV_EXT_FROM_U_MAX_DIRECT_LENGTH 3

/* maximum number of indexed bytes */
#define _CNV_EXT_FROM_U_MAX_LENGTH 0x7f

#define _CNV_EXT_FROM_U_IS_PARTIAL(value) (((value)>>_CNV_EXT_FROM_U_LENGTH_SHIFT)==0)
#define _CNV_EXT_FROM_U_GET_PARTIAL_INDEX(value) (value)

#define _CNV_EXT_FROM_U_IS_ROUNDTRIP(value) (((value)&_CNV_EXT_FROM_U_ROUNDTRIP_FLAG)!=0)
#define _CNV_EXT_FROM_U_MASK_ROUNDTRIP(value) ((value)&~_CNV_EXT_FROM_U_ROUNDTRIP_FLAG)

/* use after masking off the roundtrip flag */
#define _CNV_EXT_FROM_U_GET_LENGTH(value) ((value)>>_CNV_EXT_FROM_U_LENGTH_SHIFT)

/* get bytes or bytes index */
#define _CNV_EXT_FROM_U_GET_DATA(value) ((value)&_CNV_EXT_FROM_U_DATA_MASK)

#endif
