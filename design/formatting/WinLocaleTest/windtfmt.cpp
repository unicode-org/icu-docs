/*
********************************************************************************
*   Copyright (C) 2005, International Business Machines
*   Corporation and others.  All Rights Reserved.
********************************************************************************
*
* File WINDTFMT.CPP
*
********************************************************************************
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include <iostream>
#include <tchar.h>
#include <windows.h>

#include "windtfmt.h"

#include "unicode/format.h"
#include "unicode/datefmt.h"
#include "unicode/calendar.h"
#include "unicode/locid.h"
#include "unicode/ustring.h"
#include "unicode/timezone.h"
#include "unicode/utmscale.h"

U_NAMESPACE_BEGIN

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(Win32DateFormat)

// TODO: Range-check timeStyle, dateStyle
Win32DateFormat::Win32DateFormat(DateFormat::EStyle timeStyle, DateFormat::EStyle dateStyle, const Locale &locale, UErrorCode &status)
  : fTimeStyle(timeStyle), fDateStyle(dateStyle)
{
    if (!U_FAILURE(status)) {
        fLCID = locale.getLCID();
        fCalendar = Calendar::createInstance(locale, status);
        fBuffer = new UChar[256];
        fBufLen = 256;
    }
}

Win32DateFormat::Win32DateFormat(const Win32DateFormat &other)
  : DateFormat(other)
{
    *this = other;
}

Win32DateFormat::~Win32DateFormat()
{
    delete[] fBuffer;
    delete fCalendar;
}

Win32DateFormat &Win32DateFormat::operator=(const Win32DateFormat &other)
{
    DateFormat::operator=(other);

    delete[] fBuffer;
    delete fCalendar;

    this->fTimeStyle = other.fTimeStyle;
    this->fDateStyle = other.fDateStyle;
    this->fLCID      = other.fLCID;
    this->fCalendar  = other.fCalendar->clone();

    this->fBuffer = new UChar[256];
    this->fBufLen = 256;

    return *this;
}

Format *Win32DateFormat::clone(void) const
{
    return new Win32DateFormat(*this);
}

// TODO: Is just ignoring pos the right thing?
UnicodeString &Win32DateFormat::format(Calendar &cal, UnicodeString &appendTo, FieldPosition &pos) const
{
    FILETIME ft;
    SYSTEMTIME st;
    SYSTEMTIME stl;
    UErrorCode status = U_ZERO_ERROR;
    int64_t uct = utmscale_fromInt64((int64_t) cal.getTime(status), UDTS_ICU4C_TIME, &status);
    int64_t uft = utmscale_toInt64(uct, UDTS_WINDOWS_FILE_TIME, &status);

    ft.dwLowDateTime = (DWORD) (uft & 0xFFFFFFFF);
    ft.dwHighDateTime = (DWORD) ((uft >> 32) & 0xFFFFFFFF);

    // TODO: is default TZ the right thing to use?
    FileTimeToSystemTime(&ft, &st);
    SystemTimeToTzSpecificLocalTime(NULL, &st, &stl);

    if (fDateStyle != DateFormat::kNone) {
        formatDate(&stl, appendTo);

        // TODO: is <date><sp><time> always the right thing,
        // or should we use the message format pattern to
        // compose the final output?
        if (fTimeStyle != DateFormat::kNone) {
            appendTo += 0x20;
        }
    }

    if (fDateStyle != DateFormat::kNone) {
        formatTime(&stl, appendTo);
    }

    return appendTo;
}

void Win32DateFormat::parse(const UnicodeString& text, Calendar& cal, ParsePosition& pos) const
{
    pos.setErrorIndex(pos.getIndex());
}

static DWORD dfFlags[] = {DATE_LONGDATE, DATE_LONGDATE, DATE_SHORTDATE, DATE_SHORTDATE};

void Win32DateFormat::formatDate(const SYSTEMTIME *st, UnicodeString &appendTo) const
{
    GetDateFormat(fLCID, dfFlags[fDateStyle], st, NULL, fBuffer, fBufLen);
    appendTo.append(fBuffer, (int32_t) _tcslen(fBuffer));
}

static DWORD tfFlags[] = {0, 0, 0, TIME_NOSECONDS};

void Win32DateFormat::formatTime(const SYSTEMTIME *st, UnicodeString &appendTo) const
{
    GetTimeFormat(fLCID, tfFlags[fTimeStyle], st, NULL, fBuffer, fBufLen);
    appendTo.append(fBuffer, (int32_t) _tcslen(fBuffer));
}

U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_FORMATTING */
