/*
********************************************************************************
*   Copyright (C) 2005, International Business Machines
*   Corporation and others.  All Rights Reserved.
********************************************************************************
*
* File WINDTFMT.H
*
********************************************************************************
*/

#ifndef __WINDTFMT
#define __WINDTFMT

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include <windows.h>

#include "unicode/format.h"
#include "unicode/datefmt.h"
#include "unicode/calendar.h"
#include "unicode/ustring.h"
#include "unicode/locid.h"

/**
 * \file 
 * \brief C++ API: Format dates using Windows API.
 */

U_NAMESPACE_BEGIN

class Win32DateFormat : public DateFormat
{
public:
    Win32DateFormat(DateFormat::EStyle timeStyle, DateFormat::EStyle dateStyle, const Locale &locale, UErrorCode &status);

    Win32DateFormat(const Win32DateFormat &other);

    virtual ~Win32DateFormat();

    virtual Format *clone(void) const;

    Win32DateFormat &operator=(const Win32DateFormat &other);

    UnicodeString &format(Calendar &cal, UnicodeString &appendTo, FieldPosition &pos) const;

    UnicodeString& format(UDate date, UnicodeString& appendTo) const;

    void parse(const UnicodeString& text, Calendar& cal, ParsePosition& pos) const;

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
    void formatDate(const SYSTEMTIME *st, UnicodeString &appendTo) const;
    void formatTime(const SYSTEMTIME *st, UnicodeString &appendTo) const;

    UChar *fBuffer;
    int32_t fBufLen;
    DateFormat::EStyle fTimeStyle;
    DateFormat::EStyle fDateStyle;
    int32_t fLCID;

};

inline UnicodeString &Win32DateFormat::format(UDate date, UnicodeString& appendTo) const {
    return DateFormat::format(date, appendTo);
}

U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_FORMATTING */

#endif // __WINDTFMT
