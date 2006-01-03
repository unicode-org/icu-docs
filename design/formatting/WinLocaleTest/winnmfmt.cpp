/*
********************************************************************************
*   Copyright (C) 2005-2006, International Business Machines
*   Corporation and others.  All Rights Reserved.
********************************************************************************
*
* File WINNMFMT.CPP
*
********************************************************************************
*/

#include "unicode/utypes.h"

#ifdef U_WINDOWS

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

#define INITIAL_BUFFER_SIZE 64

UINT getGrouping(const char *grouping)
{
    UINT g = 0;

    for (const char *s = grouping; *s != '\0'; s += 1) {
        if (*s > '0' && *s < '9') {
            g = g * 10 + (*s - '0');
        }
    }

    return g;
}

void getNumberFormat(NUMBERFMTW *fmt, int32_t lcid)
{
    int STR_LEN = 100;
    char buf[10];

    GetLocaleInfoW(lcid, LOCALE_RETURN_NUMBER|LOCALE_IDIGITS, (LPWSTR) &fmt->NumDigits, STR_LEN);
    GetLocaleInfoW(lcid, LOCALE_RETURN_NUMBER|LOCALE_ILZERO,  (LPWSTR) &fmt->LeadingZero, STR_LEN);

    GetLocaleInfoA(lcid, LOCALE_SGROUPING, buf, 10);
    fmt->Grouping = getGrouping(buf);

    fmt->lpDecimalSep = new UChar[6];
    GetLocaleInfoW(lcid, LOCALE_SDECIMAL,  fmt->lpDecimalSep,  6);

    fmt->lpThousandSep = new UChar[6];
    GetLocaleInfoW(lcid, LOCALE_STHOUSAND, fmt->lpThousandSep, 6);

    GetLocaleInfoW(lcid, LOCALE_RETURN_NUMBER|LOCALE_INEGNUMBER, (LPWSTR) &fmt->NegativeOrder, STR_LEN);
}

void freeNumberFormat(NUMBERFMTW *fmt)
{
    delete[] fmt->lpThousandSep;
    delete[] fmt->lpDecimalSep;
}

void getCurrencyFormat(CURRENCYFMTW *fmt, int32_t lcid)
{
    int STR_LEN = 100;
    char buf[10];

    GetLocaleInfoW(lcid, LOCALE_RETURN_NUMBER|LOCALE_IDIGITS, (LPWSTR) &fmt->NumDigits, STR_LEN);
    GetLocaleInfoW(lcid, LOCALE_RETURN_NUMBER|LOCALE_ILZERO,  (LPWSTR) &fmt->LeadingZero, STR_LEN);

    GetLocaleInfoA(lcid, LOCALE_SMONGROUPING, buf, 10);
    fmt->Grouping = getGrouping(buf);

    fmt->lpDecimalSep = new UChar[6];
    GetLocaleInfoW(lcid, LOCALE_SMONDECIMALSEP,  fmt->lpDecimalSep,  6);

    fmt->lpThousandSep = new UChar[6];
    GetLocaleInfoW(lcid, LOCALE_SMONTHOUSANDSEP, fmt->lpThousandSep, 6);

    GetLocaleInfoW(lcid, LOCALE_RETURN_NUMBER|LOCALE_INEGCURR,  (LPWSTR) &fmt->NegativeOrder, STR_LEN);
    GetLocaleInfoW(lcid, LOCALE_RETURN_NUMBER|LOCALE_ICURRENCY, (LPWSTR) &fmt->PositiveOrder, STR_LEN);

    fmt->lpCurrencySymbol = new UChar[8];
    GetLocaleInfoW(lcid, LOCALE_SCURRENCY, (LPWSTR) fmt->lpCurrencySymbol, 8);
}

void freeCurrencyFormat(CURRENCYFMTW *fmt)
{
    delete[] fmt->lpCurrencySymbol;
    delete[] fmt->lpThousandSep;
    delete[] fmt->lpDecimalSep;
}

// TODO: keep locale too?
Win32NumberFormat::Win32NumberFormat(const Locale &locale, UBool currency, UErrorCode &status)
  : NumberFormat(), fCurrency(currency)
{
    if (!U_FAILURE(status)) {
        fLCID = locale.getLCID();

        fNumber = new UChar[INITIAL_BUFFER_SIZE];
        fNumLen = INITIAL_BUFFER_SIZE;

        fBuffer = new UChar[INITIAL_BUFFER_SIZE];
        fBufLen = INITIAL_BUFFER_SIZE;

        // TODO: is minimum fraction digits always the right thing to use?
        // TODO: should (re)setting NumDigits be done by getXXXFormat()?
        if (fCurrency) {
            getCurrencyFormat(&fFormatInfo.currency, fLCID);
            fNumberFormat = NumberFormat::createCurrencyInstance(locale, status);
            fFormatInfo.currency.NumDigits = (UINT) fNumberFormat->getMinimumFractionDigits();
        } else {
            getNumberFormat(&fFormatInfo.number, fLCID);
            fNumberFormat = NumberFormat::createInstance(locale, status);
            fFormatInfo.number.NumDigits = (UINT) fNumberFormat->getMinimumFractionDigits();
        }
    }
}

Win32NumberFormat::Win32NumberFormat(const Win32NumberFormat &other)
  : NumberFormat(other)
{
    *this = other;
}

Win32NumberFormat::~Win32NumberFormat()
{
    delete fNumberFormat;

    if (fCurrency) {
        freeCurrencyFormat(&fFormatInfo.currency);
    } else {
        freeNumberFormat(&fFormatInfo.number);
    }

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

    this->fNumber = new UChar[INITIAL_BUFFER_SIZE];
    this->fNumLen = INITIAL_BUFFER_SIZE;

    this->fBuffer = new UChar[INITIAL_BUFFER_SIZE];
    this->fBufLen = INITIAL_BUFFER_SIZE;

    return *this;
}

Format *Win32NumberFormat::clone(void) const
{
    return new Win32NumberFormat(*this);
}

UnicodeString& Win32NumberFormat::format(double number, UnicodeString& appendTo, FieldPosition& pos) const
{
    safe_swprintf(L"%f", number);
    return format(fNumber, appendTo);
}

UnicodeString& Win32NumberFormat::format(int32_t number, UnicodeString& appendTo, FieldPosition& pos) const
{
    safe_swprintf(L"%I32d", number);
    return format(fNumber, appendTo);
}

UnicodeString& Win32NumberFormat::format(int64_t number, UnicodeString& appendTo, FieldPosition& pos) const
{
    safe_swprintf(L"%I64d", number);
    return format(fNumber, appendTo);
}

// TODO: cache Locale and NumberFormat? Could keep locale passed to constructor...
void Win32NumberFormat::parse(const UnicodeString& text, Formattable& result, ParsePosition& parsePosition) const
{
    fNumberFormat->parse(text, result, parsePosition);
}

void Win32NumberFormat::growBuffer(int newLength) const
{
    Win32NumberFormat *realThis = (Win32NumberFormat *) this;

    delete[] realThis->fBuffer;
    realThis->fBuffer = new UChar[newLength];
    realThis->fBufLen = newLength;
}

int Win32NumberFormat::safe_swprintf(const wchar_t *format, ...) const
{
    va_list args;
    int result;

    va_start(args, format);
    result = vswprintf(fNumber, fNumLen, format, args);
    va_end(args);

    if (result < 0) {
        Win32NumberFormat *realThis = (Win32NumberFormat *) this;
        int newLength;

        va_start(args, format);
        newLength = _vscwprintf(format, args);
        va_end(args);

        delete[] realThis->fNumber;
        realThis->fNumber = new UChar[newLength];
        realThis->fNumLen = newLength;

        va_start(args, format);
        result = vswprintf(fNumber, fNumLen, format, args);
        va_end(args);
    }

    return result;
}

// TODO: Don't need number parameter since callers use fNumber...
UnicodeString &Win32NumberFormat::format(const UChar *number, UnicodeString &appendTo) const
{
    int result;

    if (fCurrency) {
        result = GetCurrencyFormatW(fLCID, 0, number, &fFormatInfo.currency, fBuffer, fBufLen);

        if (result == 0) {
            if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
                int newLength = GetCurrencyFormatW(fLCID, 0, number, &fFormatInfo.currency, NULL, 0);

                growBuffer(newLength);
                GetCurrencyFormatW(fLCID, 0, number,  &fFormatInfo.currency, fBuffer, fBufLen);
            }
        }
    } else {
        result = GetNumberFormatW(fLCID, 0, number, &fFormatInfo.number, fBuffer, fBufLen);

        if (result == 0) {
            if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
                int newLength = GetNumberFormatW(fLCID, 0, number, &fFormatInfo.number, NULL, 0);

                growBuffer(newLength);
                GetNumberFormatW(fLCID, 0, number, &fFormatInfo.number, fBuffer, fBufLen);
            }
        }
    }

    appendTo.append(fBuffer, (int32_t) wcslen(fBuffer));
    return appendTo;
}

U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_FORMATTING */

#endif // #ifdef U_WINDOWS
