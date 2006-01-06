/*
********************************************************************************
*   Copyright (C) 2005-2006, International Business Machines
*   Corporation and others.  All Rights Reserved.
********************************************************************************
*
* File WIN32TZ.H
*
********************************************************************************
*/

#ifndef __WIN32TZ
#define __WIN32TZ

#include "unicode/utypes.h"

#ifdef U_WINDOWS

#include <windows.h>

/**
 * \file 
 * \brief C++ API: Utilities for dealing w/ Windows time zones.
 */

U_NAMESPACE_BEGIN

class TimeZone;

class Win32TimeZone
{
public:
    static void getWindowsTimeZoneInfo(TIME_ZONE_INFORMATION *zoneInfo, const TimeZone *tz);

private:
    static int32_t fWinType;
};

U_NAMESPACE_END

#endif // #ifdef U_WINDOWS

#endif // __WIN32TZ
