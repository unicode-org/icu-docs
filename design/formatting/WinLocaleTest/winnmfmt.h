/*
********************************************************************************
*   Copyright (C) 2005-2006, International Business Machines
*   Corporation and others.  All Rights Reserved.
********************************************************************************
*
* File WINNMFMT.H
*
********************************************************************************
*/

#ifndef __WINNMFMT
#define __WINNMFMT

#include "unicode/utypes.h"

#ifdef U_WINDOWS

#if !UCONFIG_NO_FORMATTING

#include <windows.h>

#include "unicode/format.h"
#include "unicode/datefmt.h"
#include "unicode/calendar.h"
#include "unicode/ustring.h"
#include "unicode/locid.h"

/**
 * \file 
 * \brief C++ API: Format numbers using Windows API.
 */

U_NAMESPACE_BEGIN

class Win32NumberFormat : public NumberFormat
{
public:
    Win32NumberFormat(const Locale &locale, UBool currency, UErrorCode &status);

    Win32NumberFormat(const Win32NumberFormat &other);

    virtual ~Win32NumberFormat();

    virtual Format *clone(void) const;

    Win32NumberFormat &operator=(const Win32NumberFormat &other);

    /**
     * Format a double number. Concrete subclasses must implement
     * these pure virtual methods.
     *
     * @param number    The value to be formatted.
     * @param appendTo  Output parameter to receive result.
     *                  Result is appended to existing contents.
     * @param pos       On input: an alignment field, if desired.
     *                  On output: the offsets of the alignment field.
     * @return          Reference to 'appendTo' parameter.
     * @draft ICU 3.6
     */
    virtual UnicodeString& format(double number,
                                  UnicodeString& appendTo,
                                  FieldPosition& pos) const;
    /**
     * Format a long number. Concrete subclasses must implement
     * these pure virtual methods.
     *
     * @param number    The value to be formatted.
     * @param appendTo  Output parameter to receive result.
     *                  Result is appended to existing contents.
     * @param pos       On input: an alignment field, if desired.
     *                  On output: the offsets of the alignment field.
     * @return          Reference to 'appendTo' parameter.
     * @draft ICU 3.6
    */
    virtual UnicodeString& format(int32_t number,
                                  UnicodeString& appendTo,
                                  FieldPosition& pos) const;

    /**
     * Format an int64 number.
     *
     * @param number    The value to be formatted.
     * @param appendTo  Output parameter to receive result.
     *                  Result is appended to existing contents.
     * @param pos       On input: an alignment field, if desired.
     *                  On output: the offsets of the alignment field.
     * @return          Reference to 'appendTo' parameter.
     * @draft ICU 3.6
    */
    virtual UnicodeString& format(int64_t number,
                                  UnicodeString& appendTo,
                                  FieldPosition& pos) const;

    virtual UnicodeString &format(double number, UnicodeString &appendTo) const;
    virtual UnicodeString &format(int32_t number, UnicodeString &appendTo) const;
    virtual UnicodeString &format(int64_t number, UnicodeString &appendTo) const;

    virtual void parse(const UnicodeString& text, Formattable& result, ParsePosition& parsePosition) const;

    /**
     * Return the class ID for this class. This is useful only for comparing to
     * a return value from getDynamicClassID(). For example:
     * <pre>
     * .   Base* polymorphic_pointer = createPolymorphicObject();
     * .   if (polymorphic_pointer->getDynamicClassID() ==
     * .       erived::getStaticClassID()) ...
     * </pre>
     * @return          The class ID for all objects of this class.
     * @stable ICU 2.0
     */
    static UClassID U_EXPORT2 getStaticClassID(void);

    /**
     * Returns a unique class ID POLYMORPHICALLY. Pure virtual override. This
     * method is to implement a simple version of RTTI, since not all C++
     * compilers support genuine RTTI. Polymorphic operator==() and clone()
     * methods call this method.
     *
     * @return          The class ID for this object. All objects of a
     *                  given class have the same class ID.  Objects of
     *                  other classes have different class IDs.
     * @stable ICU 2.0
     */
    virtual UClassID getDynamicClassID(void) const;

private:
    union FormatInfo
    {
        NUMBERFMTW   number;
        CURRENCYFMTW currency;
    };

    void growBuffer(int newLength) const;
    int safe_swprintf(const wchar_t *format, ...) const;

    UnicodeString &format(const UChar *number, UnicodeString &appendTo) const;

    UChar *fNumber;
    int32_t fNumLen;
    UChar *fBuffer;
    int32_t fBufLen;
    UBool fCurrency;
    int32_t fLCID;
    FormatInfo fFormatInfo;
    NumberFormat *fNumberFormat;

};

inline UnicodeString &Win32NumberFormat::format(double number, UnicodeString &appendTo) const
{
    return NumberFormat::format(number, appendTo);
}

inline UnicodeString &Win32NumberFormat::format(int32_t number, UnicodeString &appendTo) const
{
    return NumberFormat::format(number, appendTo);
}

inline UnicodeString &Win32NumberFormat::format(int64_t number, UnicodeString &appendTo) const
{
    return NumberFormat::format(number, appendTo);
}

U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_FORMATTING */

#endif // #ifdef U_WINDOWS

#endif // __WINNMFMT
