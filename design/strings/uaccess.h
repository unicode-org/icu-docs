/*
*******************************************************************************
*
*   Copyright (C) 2004, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*   file name:  uaccess.h
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2004oct06
*   created by: Markus W. Scherer
*/

#ifndef __UACCESS_H__
#define __UACCESS_H__

/**
 * \file
 * \brief C API: Unicode Text Access API
 *
 * Issues:
 * - Error handling - add UErrorCode parameters? Add UBool return values to void functions?
 * - This version does not expose NUL-termination to the caller.
 * - This version assumes option 2 (index mapping done by provider functions).
 * - This version uses one API for read-only as well as read-write access,
 *   with a way to find out whether the text object is writable or not.
 * - This version does not support absolute UTF-16 indexes when native indexes are used.
 * - Should the copy() function have a UBool for whether to copy or move the text?
 *
 * @see UTextAccess
 */

#include "unicode/utypes.h"

#ifndef U_HIDE_DRAFT_API

U_CDECL_BEGIN

struct UTextAccess;
typedef struct UTextAccess UTextAccess; /**< C typedef for struct UTextAccess. @draft ICU 3.4 */

struct UTextAccessChunk;
typedef struct UTextAccessChunk UTextAccessChunk; /**< C typedef for struct UTextAccessChunk. @draft ICU 3.4 */

struct UTextAccessChunk {
    int32_t sizeOfStruct;
    int32_t start;
    int32_t limit;
    int32_t chunkLength; /* number of UChars */
    const UChar *chunk;

    /* need UBool nonUTF16Indexes; ? */
    /* if so, then need padding for safe extensibility? */
};

/**
 * UTextAccess caller properties (bit field indexes).
 *
 * @see UTextAccess
 * @draft ICU 3.4
 */
enum {
    /**
     * The caller uses more random access than iteration.
     * @draft ICU 3.4
     */
    UTEXT_CALLER_RANDOM_ACCESS,
    /**
     * The caller requires UTF-16 index semantics.
     * @draft ICU 3.4
     */
    UTEXT_CALLER_REQUIRES_UTF16,
    /**
     * The caller provides a suggested chunk size in bits 32..16.
     * @draft ICU 3.4
     */
    UTEXT_CALLER_CHUNK_SIZE_SHIFT=16
};

/**
 * UTextAccess provider properties (bit field indexes).
 *
 * @see UTextAccess
 * @draft ICU 3.4
 */
enum {
    /**
     * The provider works with non-UTF-16 ("native") text indexes.
     * For example, byte indexes into UTF-8 text or UTF-32 indexes into UTF-32 text.
     * @draft ICU 3.4
     */
    UTEXT_PROVIDER_NON_UTF16_INDEXES,
    /**
     * The provider can return the text length inexpensively.
     * @draft ICU 3.4
     */
    UTEXT_PROVIDER_LENGTH_IS_INEXPENSIVE,
    /**
     * Text chunks remain valid and usable until the text object is modified or
     * deleted, not just until the next time the access() function is called
     * (which is the default).
     * @draft ICU 3.4
     */
    UTEXT_PROVIDER_STABLE_CHUNKS,
    /**
     * The provider supports modifying the text via the replace() and copy()
     * functions.
     * @see Replaceable
     * @draft ICU 3.4
     */
    UTEXT_PROVIDER_WRITABLE,
    /**
     * There is meta data associated with the text.
     * @see Replaceable::hasMetaData()
     * @draft ICU 3.4
     */
    UTEXT_PROVIDER_HAS_META_DATA
};

/**
 * Function type declaration for UTextAccess.clone().
 *
 * TBD
 *
 * May return NULL if the object cannot be cloned.
 *
 * @see UTextAccess
 * @draft ICU 3.4
 */
typedef UTextAccess * U_CALLCONV
UTextAccessClone(UTextAccess *ta);

/**
 * Function type declaration for UTextAccess.exchangeProperties().
 *
 * TBD
 *
 * @param callerProperties Bit field with caller properties.
 *        If negative, none are communicated, only the provider properties
 *        are requested.
 * @return Provider properties bit field.
 *
 * @see UTextAccess
 * @draft ICU 3.4
 */
typedef int32_t U_CALLCONV
UTextAccessExchangeProperties(UTextAccess *ta, int32_t callerProperties);

/**
 * Function type declaration for UTextAccess.length().
 *
 * TBD
 *
 * @see UTextAccess
 * @draft ICU 3.4
 */
typedef int32_t U_CALLCONV
UTextAccessLength(UTextAccess *ta);

/**
 * Function type declaration for UTextAccess.access().
 *
 * @param index Requested (native) index. The returned chunk must contain text
 *        so that start<=index<=limit.
 * @return Chunk-relative UTF-16 offset corresponding to the requested index.
 *         Negative value if a chunk cannot be accessed
 *         (the requested index is out of bounds).
 *
 * @see UTextAccess
 * @draft ICU 3.4
 */
typedef int32_t U_CALLCONV
UTextAccessAccess(UTextAccess *ta, int32_t index, UTextAccessChunk *chunk);

/**
 * Function type declaration for UTextAccess.extract().
 *
 * TBD
 *
 * @return Number of UChars extracted.
 *         May exceed destCapacity (overflow, must not actually write
 *         more than destCapacity UChars).
 *         Negative if start and limit do not specify accessible text.
 *
 * @see UTextAccess
 * @draft ICU 3.4
 */
typedef int32_t U_CALLCONV
UTextAccessExtract(UTextAccess *ta,
                   int32_t start, int32_t limit,
                   UChar *dest, int32_t destCapacity);

/**
 * Function type declaration for UTextAccess.replace().
 *
 * TBD
 *
 * @return Delta between the limit of the replacement text and the limit argument,
 *         that is, the signed number of (native) storage units by which
 *         the old and the new pieces of text differ.
 *
 * @see UTextAccess
 * @draft ICU 3.4
 */
typedef void U_CALLCONV
UTextAccessReplace(UTextAccess *ta,
                   int32_t start, int32_t limit,
                   const UChar *src, int32_t length);

/**
 * Function type declaration for UTextAccess.copy().
 *
 * Copies a substring of this object, retaining metadata.
 * This method is used to duplicate or reorder substrings.
 * The destination index must not overlap the source range.
 *
 * TBD
 *
 * @see UTextAccess
 * @draft ICU 3.4
 */
typedef void U_CALLCONV
UTextAccessCopy(UTextAccess *ta,
                int32_t start, int32_t limit,
                int32_t destIndex);

/**
 * Function type declaration for UTextAccess.mapOffsetToNative().
 *
 * TBD
 *
 * @param offset UTF-16 offset relative to the current text chunk,
 *               0<=offset<=chunk->.
 * @return Absolute (native) index corresponding to the UTF-16 offset
 *         relative to the current text chunk.
 *
 * @see UTextAccess
 * @draft ICU 3.4
 */
typedef int32_t U_CALLCONV
UTextAccessMapOffsetToNative(UTextAccess *ta, UTextAccessChunk *chunk, int32_t offset);

/**
 * Function type declaration for UTextAccess.mapIndexToUTF16().
 *
 * TBD
 *
 * @param index Absolute (native) text index, chunk->start<=index<=chunk->limit.
 * @return Chunk-relative UTF-16 offset corresponding to the absolute (native)
 *         index.
 *
 * @see UTextAccess
 * @draft ICU 3.4
 */
typedef void U_CALLCONV
UTextAccessMapIndexToUTF16(UTextAccess *ta, UTextAccessChunk *chunk, int32_t index);

struct UTextAccess {
    /**
     * (protected) Pointer to string or wrapped object or similar.
     * Not used by caller.
     * @draft ICU 3.4
     */
    const void *context;

    /**
     * (protected) Pointer fields for use by text provider.
     * Not used by caller.
     * @draft ICU 3.4
     */
    const void *p, *q, *r;

    /**
     * (public) sizeOfStruct=sizeof(UTextAccess)
     * Allows possible backward compatible extension.
     *
     * @draft ICU 3.4
     */
    int32_t sizeOfStruct;

    /**
     * (protected) Integer fields for use by text provider.
     * Not used by caller.
     * @draft ICU 3.4
     */
    int32_t a, b, c;

    /**
     * (public) TBD
     *
     * @see UTextAccessClone
     * @draft ICU 3.4
     */
    UTextAccessClone *clone;

    /**
     * (public) TBD
     *
     * @see UTextAccessExchangeProperties
     * @draft ICU 3.4
     */
    UTextAccessExchangeProperties *exchangeProperties;

    /**
     * (public) Returns the length of the text.
     * May be expensive to compute!
     *
     * @see UTextAccessLength
     * @draft ICU 3.4
     */
    UTextAccessLength *length;

    /**
     * (public) Access to a chunk of text.
     * Does not copy text but instead gives access to a portion of it.
     *
     * The intention is that for discontiguous storage the chunk would be an actual
     * storage block used for storing the text.
     * For contiguously stored text with known length, the whole text would be returned.
     * For NUL-terminated text, the implementation may scan forward in exponentially
     * larger chunks instead of finding the NUL right away.
     *
     * In: Text index; the returned chunk of text must contain the index.
     * Out:
     * - Pointer to chunk start
     * - Start and limit indexes corresponding to the chunk;
     *   it must be start<=input index<limit
     * - Indication of success: If the input index is negative or >=length then
     *   failure needs to be indicated, probably by returning a NULL pointer
     *
     * @see UTextAccessAccess
     * @draft ICU 3.4
     */
    UTextAccessAccess *access;

    /**
     * (public) Copy a chunk of text into a buffer.
     * Does it need a return value indicating success/failure?
     * The signature shown here is the same as in UReplaceable.
     * Not strictly minimally necessary; Replaceable has it.
     *
     * @see UTextAccessExtract
     * @draft ICU 3.4
     */
    UTextAccessExtract *extract;

    /**
     * (public) TBD
     *
     * @see UTextAccessReplace
     * @draft ICU 3.4
     */
    UTextAccessReplace *replace;

    /**
     * (public) TBD
     *
     * @see UTextAccessCopy
     * @draft ICU 3.4
     */
    UTextAccessCopy *copy;

    /**
     * (public) TBD
     *
     * @see UTextAccessMapOffsetToNative
     * @draft ICU 3.4
     */
    UTextAccessMapOffsetToNative *mapOffsetToNative;

    /**
     * (public) TBD
     *
     * @see UTextAccessMapIndexToUTF16
     * @draft ICU 3.4
     */
    UTextAccessMapIndexToUTF16 *mapIndexToUTF16;
};

U_CDECL_END

#ifdef XP_CPLUSPLUS

U_NAMESPACE_BEGIN

class CharAccess {
public:
    // all-inline, and stack-allocatable
    // constructors, get/set UTextAccess, etc.
    // needs to have state besides the current chunk: at least the current index
    // for performance, may use a current-position pointer and chunk start/limit
    // pointers and translate back into indexes only when necessary

    inline CharAccess(UTextAccess *ta);
    inline CharAccess(UTextAccess *ta, int32_t callerProperties);

    /**
     * Returns the code point at the requested index,
     * or U_SENTINEL (-1) if it is out of bounds.
     */
    inline UChar32 char32At(int32_t index);

    // U_SENTINEL (-1) if out of bounds
    inline UChar32 next32();
    inline UChar32 previous32();

    inline UBool setIndex(int32_t index);   // use origin enum as in UCharIterator?
    inline int32_t getIndex();              // use origin enum as in UCharIterator?
    inline UBool moveIndex(int32_t delta);  // signed delta code points

    // convenience wrappers for length(), access(), extract()?
    // getChunkStart(), getChunkLimit() for the current chunk?

private:
    UTextAccess *ta;
    UTextAccessChunk chunk;
    int32_t chunkOffset;
    int32_t providerProperties; // -1 if not known yet
};

U_NAMESPACE_END

#endif /* C++ */

#endif /* U_HIDE_DRAFT_API */

#endif
