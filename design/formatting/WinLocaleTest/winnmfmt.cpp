/*
********************************************************************************
*   Copyright (C) 2005, International Business Machines
*   Corporation and others.  All Rights Reserved.
********************************************************************************
*
* File WINNMFMT.CPP
*
********************************************************************************
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include <iostream>
#include <tchar.h>
#include <windows.h>

#include "winnmfmt.h"

#include "unicode/format.h"
#include "unicode/numfmt.h"
#include "unicode/locid.h"
#include "unicode/ustring.h"

// NOTE: a PRIVATE interface!
#include "locmap.h"

U_NAMESPACE_BEGIN

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(Win32NumberFormat)

// TODO: keep locale too?
Win32NumberFormat::Win32NumberFormat(const Locale &locale, UBool currency, UErrorCode &status)
  : NumberFormat(), fCurrency(currency)
{
    if (!U_FAILURE(status)) {
        fLCID = locale.getLCID();
        fNumber = new UChar[256];
        fBuffer = new UChar[256];
        fBufLen = 256;
    }
}

Win32NumberFormat::Win32NumberFormat(const Win32NumberFormat &other)
  : NumberFormat(other)
{
    *this = other;
}

Win32NumberFormat::~Win32NumberFormat()
{
    delete[] fBuffer;
    delete[] fNumber;
}

Win32NumberFormat &Win32NumberFormat::operator=(const Win32NumberFormat &other)
{
    NumberFormat::operator=(other);

    delete[] fBuffer;
    delete[] fNumber;

    this->fCurrency = other.fCurrency;
    this->fLCID      = other.fLCID;

    this->fNumber = new UChar[256];
    this->fBuffer = new UChar[256];
    this->fBufLen = 256;

    return *this;
}

Format *Win32NumberFormat::clone(void) const
{
    return new Win32NumberFormat(*this);
}

UnicodeString& Win32NumberFormat::format(double number, UnicodeString& appendTo, FieldPosition& pos) const
{
    _stprintf(fNumber, _T("%f"), number);
    return format(fNumber, appendTo);
}

UnicodeString& Win32NumberFormat::format(int32_t number, UnicodeString& appendTo, FieldPosition& pos) const
{
    _stprintf(fNumber, _T("%I32d"), number);
    return format(fNumber, appendTo);
}

UnicodeString& Win32NumberFormat::format(int64_t number, UnicodeString& appendTo, FieldPosition& pos) const
{
    _stprintf(fNumber, _T("%I64d"), number);
    return format(fNumber, appendTo);
}

// TODO: cache Locale and NumberFormat? Could keep locale passed to constructor...
void Win32NumberFormat::parse(const UnicodeString& text, Formattable& result, ParsePosition& parsePosition) const
{
    UErrorCode status = U_ZERO_ERROR;
    Locale loc(uprv_convertToPosix(fLCID, &status));
    NumberFormat *nf = fCurrency? NumberFormat::createCurrencyInstance(loc, status) : NumberFormat::createInstance(loc, status);

    nf->parse(text, result, parsePosition);
    delete nf;
}

// TODO: Don't need number parameter since callers use fNumber...
UnicodeString &Win32NumberFormat::format(const UChar *number, UnicodeString &appendTo) const
{
    if (fCurrency) {
        GetCurrencyFormat(fLCID, 0, number, NULL, fBuffer, fBufLen);
    } else {
        GetNumberFormat(fLCID, 0, number, NULL, fBuffer, fBufLen);
    }

    appendTo.append(fBuffer, (int32_t) _tcslen(fBuffer));
    return appendTo;
}

U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_FORMATTING */
