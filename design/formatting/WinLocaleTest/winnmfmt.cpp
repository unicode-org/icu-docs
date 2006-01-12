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

#define STACK_BUFFER_SIZE 32

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
  : NumberFormat(), fCurrency(currency), fFractionDigitsSet(FALSE)
{
    if (!U_FAILURE(status)) {
        fLCID = locale.getLCID();

        if (fCurrency) {
            getCurrencyFormat(&fFormatInfo.currency, fLCID);
        } else {
            getNumberFormat(&fFormatInfo.number, fLCID);
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
    if (fCurrency) {
        freeCurrencyFormat(&fFormatInfo.currency);
    } else {
        freeNumberFormat(&fFormatInfo.number);
    }
}

Win32NumberFormat &Win32NumberFormat::operator=(const Win32NumberFormat &other)
{
    NumberFormat::operator=(other);

    this->fCurrency          = other.fCurrency;
    this->fLCID              = other.fLCID;
    this->fFormatInfo        = other.fFormatInfo;
    this->fFractionDigitsSet = other.fFractionDigitsSet;

    return *this;
}

Format *Win32NumberFormat::clone(void) const
{
    return new Win32NumberFormat(*this);
}

UnicodeString& Win32NumberFormat::format(double number, UnicodeString& appendTo, FieldPosition& pos) const
{
    return format(getMaximumFractionDigits(), appendTo, L"%f", number);
}

UnicodeString& Win32NumberFormat::format(int32_t number, UnicodeString& appendTo, FieldPosition& pos) const
{
    return format(getMinimumFractionDigits(), appendTo, L"%I32d", number);
}

UnicodeString& Win32NumberFormat::format(int64_t number, UnicodeString& appendTo, FieldPosition& pos) const
{
    return format(getMinimumFractionDigits(), appendTo, L"%I64d", number);
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
void Win32NumberFormat::setMaximumFractionDigits(int32_t newValue)
{
    fFractionDigitsSet = TRUE;
    NumberFormat::setMaximumFractionDigits(newValue);
}

void Win32NumberFormat::setMinimumFractionDigits(int32_t newValue)
{
    fFractionDigitsSet = TRUE;
    NumberFormat::setMinimumFractionDigits(newValue);
}

UnicodeString &Win32NumberFormat::format(int32_t numDigits, UnicodeString &appendTo, wchar_t *fmt, ...) const
{
    wchar_t nStackBuffer[STACK_BUFFER_SIZE];
    wchar_t *nBuffer = nStackBuffer;
    va_list args;
    int result;

    va_start(args, fmt);
    result = vswprintf(nBuffer, STACK_BUFFER_SIZE, fmt, args);
    va_end(args);

    if (result < 0) {
        int newLength;

        va_start(args, fmt);
        newLength = _vscwprintf(fmt, args);
        va_end(args);

        nBuffer = new UChar[newLength];

        va_start(args, fmt);
        result = vswprintf(nBuffer, newLength, fmt, args);
        va_end(args);
    }

    UChar stackBuffer[STACK_BUFFER_SIZE];
    UChar *buffer = stackBuffer;
    FormatInfo formatInfo;

    formatInfo = fFormatInfo;

    if (fCurrency) {
        if (fFractionDigitsSet) {
            formatInfo.currency.NumDigits = (UINT) numDigits;
        }

        if (!isGroupingUsed()) {
            formatInfo.currency.Grouping = 0;
        }

        result = GetCurrencyFormatW(fLCID, 0, nBuffer, &formatInfo.currency, buffer, STACK_BUFFER_SIZE);

        if (result == 0) {
            if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
                int newLength = GetCurrencyFormatW(fLCID, 0, nBuffer, &formatInfo.currency, NULL, 0);

                buffer = new UChar[newLength];
                GetCurrencyFormatW(fLCID, 0, nBuffer,  &formatInfo.currency, buffer, newLength);
            }
        }
    } else {
        if (fFractionDigitsSet) {
            formatInfo.number.NumDigits = (UINT) numDigits;
        }

        if (!isGroupingUsed()) {
            formatInfo.number.Grouping = 0;
        }

        result = GetNumberFormatW(fLCID, 0, nBuffer, &formatInfo.number, buffer, STACK_BUFFER_SIZE);

        if (result == 0) {
            if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
                int newLength = GetNumberFormatW(fLCID, 0, nBuffer, &formatInfo.number, NULL, 0);

                buffer = new UChar[newLength];
                GetNumberFormatW(fLCID, 0, nBuffer, &formatInfo.number, buffer, newLength);
            }
        }
    }

    appendTo.append(buffer, (int32_t) wcslen(buffer));

    if (buffer != stackBuffer) {
        delete[] buffer;
    }

    if (nBuffer != nStackBuffer) {
        delete[] nBuffer;
    }

    return appendTo;
}

U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_FORMATTING */

#endif // #ifdef U_WINDOWS
