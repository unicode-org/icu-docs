/*
********************************************************************************
*   Copyright (C) 2005-2006, International Business Machines
*   Corporation and others.  All Rights Reserved.
********************************************************************************
*
* File WIN32TZ.CPP
*
********************************************************************************
*/

#include "unicode/utypes.h"

#ifdef U_WINDOWS

#include <iostream>
#include <tchar.h>
#include <windows.h>

#include "win32tz.h"

#include "unicode/unistr.h"
#include "unicode/ustring.h"
#include "unicode/timezone.h"

#include "uresimp.h"

U_NAMESPACE_BEGIN

#define ARRAY_SIZE(array) (sizeof array / sizeof array[0])

// The layout of the Tzi value in the registry
struct TZI
{
    int32_t bias;
    int32_t standardBias;
    int32_t daylightBias;
    SYSTEMTIME standardDate;
    SYSTEMTIME daylightDate;
};

struct WindowsICUMap
{
    const UnicodeString *icuid;
    const char *winid;
};

/**
 * Various registry keys and key fragments.
 */
static const char CURRENT_ZONE_REGKEY[] = "SYSTEM\\CurrentControlSet\\Control\\TimeZoneInformation\\";
static const char STANDARD_NAME_REGKEY[] = "StandardName";
static const char STANDARD_TIME_REGKEY[] = " Standard Time";
static const char TZI_REGKEY[] = "TZI";
static const char STD_REGKEY[] = "Std";

/**
 * HKLM subkeys used to probe for the flavor of Windows.  Note that we
 * specifically check for the "GMT" zone subkey; this is present on
 * NT, but on XP has become "GMT Standard Time".  We need to
 * discriminate between these cases.
 */
static const char* const WIN_TYPE_PROBE_REGKEY[] = {
    /* WIN_9X_ME_TYPE */
    "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Time Zones",

    /* WIN_NT_TYPE */
    "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Time Zones\\GMT"

    /* otherwise: WIN_2K_XP_TYPE */
};

/**
 * The time zone root subkeys (under HKLM) for different flavors of
 * Windows.
 */
static const char* const TZ_REGKEY[] = {
    /* WIN_9X_ME_TYPE */
    "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Time Zones\\",

    /* WIN_NT_TYPE | WIN_2K_XP_TYPE */
    "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Time Zones\\"
};

/**
 * Flavor of Windows, from our perspective.  Not a real OS version,
 * but rather the flavor of the layout of the time zone information in
 * the registry.
 */
enum {
    WIN_9X_ME_TYPE = 0,
    WIN_NT_TYPE = 1,
    WIN_2K_XP_TYPE = 2
};

// TODO: Sort on ICU ID?
// TODO: No static initialization!
// TODO: This data should come from ICU/CLDR...
static const WindowsICUMap ZONE_MAP[] = {
    {new UNICODE_STRING_SIMPLE("Etc/GMT+12"),           "Dateline"}, /* S (GMT-12:00) International Date Line West */

    {new UNICODE_STRING_SIMPLE("Pacific/Apia"),         "Samoa"}, /* S (GMT-11:00) Midway Island, Samoa */

    {new UNICODE_STRING_SIMPLE("Pacific/Honolulu"),     "Hawaiian"}, /* S (GMT-10:00) Hawaii */

    {new UNICODE_STRING_SIMPLE("America/Anchorage"),    "Alaskan"}, /* D (GMT-09:00) Alaska */

    {new UNICODE_STRING_SIMPLE("America/Los_Angeles"),  "Pacific"}, /* D (GMT-08:00) Pacific Time (US & Canada); Tijuana */

    {new UNICODE_STRING_SIMPLE("America/Phoenix"),      "US Mountain"}, /* S (GMT-07:00) Arizona */
    {new UNICODE_STRING_SIMPLE("America/Denver"),       "Mountain"}, /* D (GMT-07:00) Mountain Time (US & Canada) */
    {new UNICODE_STRING_SIMPLE("America/Chihuahua"),    "Mexico Standard Time 2"}, /* D (GMT-07:00) Chihuahua, La Paz, Mazatlan */

    {new UNICODE_STRING_SIMPLE("America/Managua"),      "Central America"}, /* S (GMT-06:00) Central America */
    {new UNICODE_STRING_SIMPLE("America/Regina"),       "Canada Central"}, /* S (GMT-06:00) Saskatchewan */
    {new UNICODE_STRING_SIMPLE("America/Mexico_City"),  "Mexico"}, /* D (GMT-06:00) Guadalajara, Mexico City, Monterrey */
    {new UNICODE_STRING_SIMPLE("America/Chicago"),      "Central"}, /* D (GMT-06:00) Central Time (US & Canada) */

    {new UNICODE_STRING_SIMPLE("America/Indianapolis"), "US Eastern"}, /* S (GMT-05:00) Indiana (East) */
    {new UNICODE_STRING_SIMPLE("America/Bogota"),       "SA Pacific"}, /* S (GMT-05:00) Bogota, Lima, Quito */
    {new UNICODE_STRING_SIMPLE("America/New_York"),     "Eastern"}, /* D (GMT-05:00) Eastern Time (US & Canada) */

    {new UNICODE_STRING_SIMPLE("America/Caracas"),      "SA Western"}, /* S (GMT-04:00) Caracas, La Paz */
    {new UNICODE_STRING_SIMPLE("America/Santiago"),     "Pacific SA"}, /* D (GMT-04:00) Santiago */
    {new UNICODE_STRING_SIMPLE("America/Halifax"),      "Atlantic"}, /* D (GMT-04:00) Atlantic Time (Canada) */

    {new UNICODE_STRING_SIMPLE("America/St_Johns"),     "Newfoundland"}, /* D (GMT-03:30) Newfoundland */

    {new UNICODE_STRING_SIMPLE("America/Buenos_Aires"), "SA Eastern"}, /* S (GMT-03:00) Buenos Aires, Georgetown */
    {new UNICODE_STRING_SIMPLE("America/Godthab"),      "Greenland"}, /* D (GMT-03:00) Greenland */
    {new UNICODE_STRING_SIMPLE("America/Sao_Paulo"),    "E. South America"}, /* D (GMT-03:00) Brasilia */

    {new UNICODE_STRING_SIMPLE("America/Noronha"),      "Mid-Atlantic"}, /* D (GMT-02:00) Mid-Atlantic */

    {new UNICODE_STRING_SIMPLE("Atlantic/Cape_Verde"),  "Cape Verde"}, /* S (GMT-01:00) Cape Verde Is. */
    {new UNICODE_STRING_SIMPLE("Atlantic/Azores"),      "Azores"}, /* D (GMT-01:00) Azores */

    {new UNICODE_STRING_SIMPLE("Africa/Casablanca"),    "Greenwich"}, /* S (GMT) Casablanca, Monrovia */
    {new UNICODE_STRING_SIMPLE("Europe/London"),        "GMT"}, /* D (GMT) Greenwich Mean Time : Dublin, Edinburgh, Lisbon, London */

    {new UNICODE_STRING_SIMPLE("Africa/Lagos"),         "W. Central Africa"}, /* S (GMT+01:00) West Central Africa */
    {new UNICODE_STRING_SIMPLE("Europe/Berlin"),        "W. Europe"}, /* D (GMT+01:00) Amsterdam, Berlin, Bern, Rome, Stockholm, Vienna */
    {new UNICODE_STRING_SIMPLE("Europe/Paris"),         "Romance"}, /* D (GMT+01:00) Brussels, Copenhagen, Madrid, Paris */
    {new UNICODE_STRING_SIMPLE("Europe/Sarajevo"),      "Central European"}, /* D (GMT+01:00) Sarajevo, Skopje, Warsaw, Zagreb */
    {new UNICODE_STRING_SIMPLE("Europe/Belgrade"),      "Central Europe"}, /* D (GMT+01:00) Belgrade, Bratislava, Budapest, Ljubljana, Prague */

    {new UNICODE_STRING_SIMPLE("Africa/Johannesburg"),  "South Africa"}, /* S (GMT+02:00) Harare, Pretoria */
    {new UNICODE_STRING_SIMPLE("Asia/Jerusalem"),       "Israel"}, /* S (GMT+02:00) Jerusalem */
    {new UNICODE_STRING_SIMPLE("Europe/Istanbul"),      "GTB"}, /* D (GMT+02:00) Athens, Istanbul, Minsk */
    {new UNICODE_STRING_SIMPLE("Europe/Helsinki"),      "FLE"}, /* D (GMT+02:00) Helsinki, Kyiv, Riga, Sofia, Tallinn, Vilnius */
    {new UNICODE_STRING_SIMPLE("Africa/Cairo"),         "Egypt"}, /* D (GMT+02:00) Cairo */
    {new UNICODE_STRING_SIMPLE("Europe/Bucharest"),     "E. Europe"}, /* D (GMT+02:00) Bucharest */

    {new UNICODE_STRING_SIMPLE("Africa/Nairobi"),       "E. Africa"}, /* S (GMT+03:00) Nairobi */
    {new UNICODE_STRING_SIMPLE("Asia/Riyadh"),          "Arab"}, /* S (GMT+03:00) Kuwait, Riyadh */
    {new UNICODE_STRING_SIMPLE("Europe/Moscow"),        "Russian"}, /* D (GMT+03:00) Moscow, St. Petersburg, Volgograd */
    {new UNICODE_STRING_SIMPLE("Asia/Baghdad"),         "Arabic"}, /* D (GMT+03:00) Baghdad */

    {new UNICODE_STRING_SIMPLE("Asia/Tehran"),          "Iran"}, /* D (GMT+03:30) Tehran */

    {new UNICODE_STRING_SIMPLE("Asia/Muscat"),          "Arabian"}, /* S (GMT+04:00) Abu Dhabi, Muscat */
    {new UNICODE_STRING_SIMPLE("Asia/Tbilisi"),         "Caucasus"}, /* D (GMT+04:00) Baku, Tbilisi, Yerevan */

    {new UNICODE_STRING_SIMPLE("Asia/Kabul"),           "Afghanistan"}, /* S (GMT+04:30) Kabul */

    {new UNICODE_STRING_SIMPLE("Asia/Karachi"),         "West Asia"}, /* S (GMT+05:00) Islamabad, Karachi, Tashkent */
    {new UNICODE_STRING_SIMPLE("Asia/Yekaterinburg"),   "Ekaterinburg"}, /* D (GMT+05:00) Ekaterinburg */

    {new UNICODE_STRING_SIMPLE("Asia/Calcutta"),        "India"}, /* S (GMT+05:30) Chennai, Kolkata, Mumbai, New Delhi */

    {new UNICODE_STRING_SIMPLE("Asia/Katmandu"),        "Nepal"}, /* S (GMT+05:45) Kathmandu */

    {new UNICODE_STRING_SIMPLE("Asia/Colombo"),         "Sri Lanka"}, /* S (GMT+06:00) Sri Jayawardenepura */
    {new UNICODE_STRING_SIMPLE("Asia/Dhaka"),           "Central Asia"}, /* S (GMT+06:00) Astana, Dhaka */
    {new UNICODE_STRING_SIMPLE("Asia/Novosibirsk"),     "N. Central Asia"}, /* D (GMT+06:00) Almaty, Novosibirsk */

    {new UNICODE_STRING_SIMPLE("Asia/Rangoon"),         "Myanmar"}, /* S (GMT+06:30) Rangoon */

    {new UNICODE_STRING_SIMPLE("Asia/Bangkok"),         "SE Asia"}, /* S (GMT+07:00) Bangkok, Hanoi, Jakarta */
    {new UNICODE_STRING_SIMPLE("Asia/Krasnoyarsk"),     "North Asia"}, /* D (GMT+07:00) Krasnoyarsk */

    {new UNICODE_STRING_SIMPLE("Australia/Perth"),      "W. Australia"}, /* S (GMT+08:00) Perth */
    {new UNICODE_STRING_SIMPLE("Asia/Taipei"),          "Taipei"}, /* S (GMT+08:00) Taipei */
    {new UNICODE_STRING_SIMPLE("Asia/Singapore"),       "Singapore"}, /* S (GMT+08:00) Kuala Lumpur, Singapore */
    {new UNICODE_STRING_SIMPLE("Asia/Hong_Kong"),       "China"}, /* S (GMT+08:00) Beijing, Chongqing, Hong Kong, Urumqi */
    {new UNICODE_STRING_SIMPLE("Asia/Irkutsk"),         "North Asia East"}, /* D (GMT+08:00) Irkutsk, Ulaan Bataar */

    {new UNICODE_STRING_SIMPLE("Asia/Tokyo"),           "Tokyo"}, /* S (GMT+09:00) Osaka, Sapporo, Tokyo */
    {new UNICODE_STRING_SIMPLE("Asia/Seoul"),           "Korea"}, /* S (GMT+09:00) Seoul */
    {new UNICODE_STRING_SIMPLE("Asia/Yakutsk"),         "Yakutsk"}, /* D (GMT+09:00) Yakutsk */

    {new UNICODE_STRING_SIMPLE("Australia/Darwin"),     "AUS Central"}, /* S (GMT+09:30) Darwin */
    {new UNICODE_STRING_SIMPLE("Australia/Adelaide"),   "Cen. Australia"}, /* D (GMT+09:30) Adelaide */

    {new UNICODE_STRING_SIMPLE("Pacific/Guam"),         "West Pacific"}, /* S (GMT+10:00) Guam, Port Moresby */
    {new UNICODE_STRING_SIMPLE("Australia/Brisbane"),   "E. Australia"}, /* S (GMT+10:00) Brisbane */
    {new UNICODE_STRING_SIMPLE("Asia/Vladivostok"),     "Vladivostok"}, /* D (GMT+10:00) Vladivostok */
    {new UNICODE_STRING_SIMPLE("Australia/Hobart"),     "Tasmania"}, /* D (GMT+10:00) Hobart */
    {new UNICODE_STRING_SIMPLE("Australia/Sydney"),     "AUS Eastern"}, /* D (GMT+10:00) Canberra, Melbourne, Sydney */

    {new UNICODE_STRING_SIMPLE("Asia/Magadan"),         "Central Pacific"}, /* S (GMT+11:00) Magadan, Solomon Is., New Caledonia */

    {new UNICODE_STRING_SIMPLE("Pacific/Fiji"),         "Fiji"}, /* S (GMT+12:00) Fiji, Kamchatka, Marshall Is. */
    {new UNICODE_STRING_SIMPLE("Pacific/Auckland"),     "New Zealand"}, /* D (GMT+12:00) Auckland, Wellington */

    {new UNICODE_STRING_SIMPLE("Pacific/Tongatapu"),    "Tonga"}, /* S (GMT+13:00) Nuku'alofa */
    NULL,                                               NULL
};

int32_t Win32TimeZone::fWinType = -1;

int32_t detectWindowsType()
{
    int32_t winType;
    LONG result;
    HKEY hkey;

    /* Detect the version of windows by trying to open a sequence of
        probe keys.  We don't use the OS version API because what we
        really want to know is how the registry is laid out.
        Specifically, is it 9x/Me or not, and is it "GMT" or "GMT
        Standard Time". */
    for (winType = 0; winType < 2; winType += 1) {
        result = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                                WIN_TYPE_PROBE_REGKEY[winType],
                                0,
                                KEY_QUERY_VALUE,
                                &hkey);
        RegCloseKey(hkey);

        if (result == ERROR_SUCCESS) {
            break;
        }
    }

    return winType;
}

// TODO: Binary search sorted ZONE_MAP...
const char *findWindowsZoneID(const UnicodeString &icuid)
{
    for (int i = 0; ZONE_MAP[i].icuid != NULL; i += 1) {
        if (icuid.compare(*ZONE_MAP[i].icuid) == 0) {
            return ZONE_MAP[i].winid;
        }
    }

    return NULL;
}

// TODO: Alternate Windows ID's?
void Win32TimeZone::getWindowsTimeZoneInfo(TIME_ZONE_INFORMATION *zoneInfo, const TimeZone *tz)
{
    UnicodeString icuid;
    const char *winid;
    
    // TODO: This isn't thread safe, but it's probably good enough.
    if (fWinType < 0) {
        fWinType = detectWindowsType();
    }

    tz->getID(icuid);
    winid = findWindowsZoneID(icuid);

    if (winid != NULL) {
        char subKeyName[96]; // TODO: why 96??
        DWORD cbData = sizeof(TZI);
        TZI tzi;
        LONG result;
        HKEY hkey;


        strcpy(subKeyName, TZ_REGKEY[1]);
        strcat(subKeyName, winid);

        if (fWinType != WIN_9X_ME_TYPE &&
            (winid[strlen(winid) - 1] != '2') &&
            !(fWinType == WIN_NT_TYPE && strcmp(winid, "GMT") == 0)) {
            strcat(subKeyName, STANDARD_TIME_REGKEY);
        }

        result = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                                subKeyName,
                                0,
                                KEY_QUERY_VALUE,
                                &hkey);

        result = RegQueryValueEx(hkey,
                                    TZI_REGKEY,
                                    NULL,
                                    NULL,
                                    (LPBYTE)&tzi,
                                    &cbData);

        RegCloseKey(hkey);

        if (result == ERROR_SUCCESS) {
            zoneInfo->Bias         = tzi.bias;
            zoneInfo->DaylightBias = tzi.daylightBias;
            zoneInfo->StandardBias = tzi.standardBias;
            zoneInfo->DaylightDate = tzi.daylightDate;
            zoneInfo->StandardDate = tzi.standardDate;

            return;
        }
    }

    // Can't find a match - use Windows default zone.
    GetTimeZoneInformation(zoneInfo);
}

U_NAMESPACE_END

#endif // #ifdef U_WINDOWS
