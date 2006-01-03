/*
**********************************************************************
*   Copyright (C) 1997-2006, International Business Machines
*   Corporation and others.  All Rights Reserved.
**********************************************************************
*/

#include <iostream>
#include <tchar.h>
#include <windows.h>

#include "unicode/utypes.h"
#include "unicode/ustring.h"
#include "unicode/unistr.h"
#include "unicode/uchar.h"
#include "unicode/ucnv.h"
#include "unicode/utmscale.h"
#include "unicode/locid.h"
#include "unicode/smpdtfmt.h"
#include "unicode/unorm.h"
#include "unicode/ures.h"
#include "unicode/decimfmt.h"
#include "unicode/dcfmtsym.h"

#include "windtfmt.h"
#include "winnmfmt.h"
#include "wincoll.h"

// NOTE: a PRIVATE interface!
#include "locmap.h"

#define ARRAY_SIZE(array) (sizeof array / sizeof array[0])

struct LCIDRecord
{
    int32_t lcid;
    const char *localeID;
};

struct PatternTranslationRecord
{
    UnicodeString windows;
    UnicodeString icu;
};

struct StringSortRecord
{
    UnicodeString str;
    uint8_t sortKey[250];
};

PatternTranslationRecord translations[] = {
    {UNICODE_STRING_SIMPLE("ddd"),  UNICODE_STRING_SIMPLE("EEE")},
    {UNICODE_STRING_SIMPLE("dddd"), UNICODE_STRING_SIMPLE("EEEE")},
    {UNICODE_STRING_SIMPLE("gg"),   UNICODE_STRING_SIMPLE("G")},
    {UNICODE_STRING_SIMPLE("t"),    UNICODE_STRING_SIMPLE("a")},
    {UNICODE_STRING_SIMPLE("tt"),   UNICODE_STRING_SIMPLE("aa")}
};
    

int32_t translationCount = ARRAY_SIZE(translations);

UnicodeString strings[] = {
    UNICODE_STRING_SIMPLE("billet"),
    UNICODE_STRING_SIMPLE("bills"),
    UNICODE_STRING_SIMPLE("bill's"),
    UNICODE_STRING_SIMPLE("cannot"),
    UNICODE_STRING_SIMPLE("cant"),
    UNICODE_STRING_SIMPLE("can't"),
    UNICODE_STRING_SIMPLE("con"),
    UNICODE_STRING_SIMPLE("coop"),
    UNICODE_STRING_SIMPLE("sued"),
    UNICODE_STRING_SIMPLE("sues"),
    UNICODE_STRING_SIMPLE("sue's"),
    UNICODE_STRING_SIMPLE("t-ant"),
    UNICODE_STRING_SIMPLE("tanya"),
    UNICODE_STRING_SIMPLE("t-aria"),
    UNICODE_STRING_SIMPLE("went"),
    UNICODE_STRING_SIMPLE("were"),
    UNICODE_STRING_SIMPLE("we're")
};

int32_t stringCount = ARRAY_SIZE(strings);

void printHeader(FILE *fp, char* title){
    fprintf(fp,"%s","<html><head><meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\">\n");
    fprintf(fp,"<title>%s</title>\n",title);
    fprintf(fp,"</title>\n");
    fprintf(fp,"</head>\n");
    fprintf(fp,"<body><p><b>%s",title);
    fprintf(fp,"</b></p>\n");
}

void printFooter(FILE *fp){
    fprintf(fp,"%s","</body></html>\n");
}

void printTableHeader(FILE *fp, char *lcid, char* locale, char *calendarType, char *winDateFormat, char *winTimeFormat, char *winFormatString,\
                      char *uDateFormat, char *uTimeFormat, char *uFormatString, char *wFormatString)
{
    fprintf( fp, "<table border=\"1\" cellspacing=\"0\">\n");
    fprintf( fp, "\t<tr>\n");
    fprintf( fp, "\t\t<th>%s</th>\n", lcid);
    fprintf( fp, "\t\t<th>%s</th>\n", locale);
    //fprintf( fp, "\t\t<th>%s</th>\n", calendarType);
    fprintf( fp, "\t\t<th>%s</th>\n", winDateFormat);
    fprintf( fp, "\t\t<th>%s</th>\n", winTimeFormat);
    fprintf( fp, "\t\t<th>%s</th>\n", winFormatString);
    fprintf( fp, "\t\t<th>%s</th>\n", uDateFormat);
    fprintf( fp, "\t\t<th>%s</th>\n", uTimeFormat);
    fprintf( fp, "\t\t<th>%s</th>\n", uFormatString);
    fprintf( fp, "\t\t<th>%s</th>\n", wFormatString);

    fprintf( fp, "\t</tr>\n");

}

void printTableRow(FILE* fp, const char *lcid, const char *locale, const char *calendarType, const char *winDateFormat, const char *winTimeFormat, const char *winFormatString,
                   const char *uDateFormat, const char *uTimeFormat, const char *uFormatString, const char *wFormatString, int bgcolor){
    fprintf( fp, "\t<tr>\n");
    fprintf( fp, "\t\t<td>%s</td>\n", lcid);
    fprintf( fp, "\t\t<td>%s</td>\n", locale);
    //fprintf( fp, "\t\t<td>%s</td>\n", calendarType);
    fprintf( fp, "\t\t<td>%s</td>\n", winDateFormat);
    fprintf( fp, "\t\t<td>%s</td>\n", winTimeFormat);
    fprintf( fp, "\t\t<td>%s</td>\n", winFormatString);
    fprintf( fp, "\t\t<td>%s</td>\n", uDateFormat);
    fprintf( fp, "\t\t<td>%s</td>\n", uTimeFormat);

    fprintf( fp, "\t\t<td bgcolor=\"#%6X\">%s</td>\n", bgcolor, uFormatString);

    fprintf( fp, "\t\t<td>%s</td>\n", wFormatString);

    fprintf( fp, "\t</tr>\n");

    
}

void printTableFooter(FILE* fp){
    fprintf( fp,"</table>\n");
}

int toUTF8Chars(const UChar *s, int32_t sourceLen, char* out, int32_t outLen, UErrorCode *status)
{
    /* converter */
    UConverter *converter;
    const UChar *mySource;
    const UChar *mySourceEnd;
    char *myTarget;
    int32_t arraySize;

    if(s == NULL)
    {
        return 0;
    }

    char* buf = (char*) malloc(sizeof(char) * sourceLen * 4);

    /* set up the conversion parameters */
    mySource     = s;
    mySourceEnd  = mySource + sourceLen;
    myTarget     = buf;
    arraySize    = sourceLen * 4;

    /* open a default converter */
    converter = ucnv_open("UTF-8", status);

    /* if we failed, clean up and exit */
    if(U_SUCCESS(*status)){

    /* reset the error code */
    *status = U_ZERO_ERROR;

    /* perform the conversion */
    ucnv_fromUnicode(converter, &myTarget,  myTarget + arraySize,
		        &mySource, mySourceEnd, NULL,
		        TRUE, status);
    }

    int i=0;

    for(; i < myTarget-buf && i < outLen; i++){
        out[i] = buf[i];
    }

    /* null-terminate the output */
    out[i] = 0;

    i = ((i-1)<buf-myTarget? -1 : 0);

    free(buf);

    /* close the converter */
    ucnv_close(converter);
    return i;
}

UnicodeString &translate(UnicodeString &windowsPattern)
{
    for (int i = 0; i < translationCount; i += 1) {
        if (translations[i].windows == windowsPattern) {
            return translations[i].icu;
        }
    }

    return windowsPattern;
}

void convertDateTimeToICU(WCHAR winFormat[], int32_t winLen, UnicodeString &icuFormat)
{
    UnicodeString patternChars = UNICODE_STRING_SIMPLE("dghHmMsty");

    for (int32_t c = 0; c < winLen; c += 1) {
        UChar ch = winFormat[c];

        if (ch == '\'') {
            icuFormat += ch;

            for (int i = c + 1; i < winLen; i += 1) {
                UChar qch = winFormat[i];

                if (qch == '\'') {
                    if (i+1 < winLen && winFormat[i+1] == '\'') {
                        icuFormat += "\'\'";
                        i += 1;
                    } else {
                        icuFormat += qch;
                        c = i;
                        break;
                    }
                } else {
                    icuFormat += qch;
                }
            }
        } else if (patternChars.indexOf(ch) >= 0) {
            UChar pc = ch;
            UnicodeString ps;

            while (winFormat[c] == pc) {
                ps += winFormat[c++];
            }

            icuFormat += translate(ps);
            c -= 1;
        } else {
            icuFormat += ch;
        }
    }
}

void convertDigits(UnicodeString &input, UnicodeString &output)
{
    UnicodeString digits = UNICODE_STRING_SIMPLE("0123456789");

    for (int i = 0; i < input.length(); i += 1) {
        UChar ch = input[i];

        if (u_isdigit(ch)) {
            output += digits[u_charDigitValue(ch)];
        } else {
            output += ch;
        }
    }
}

char *getCalendarType(int32_t type)
{
    switch (type)
    {
    case 1:
    case 2:
        return "@calendar=gregorian";

    case 3:
        return "@calendar=japanese";

    case 6:
        return "@calendar=islamic";

    case 7:
        return "@calendar=buddhist";

    case 8:
        return "@calendar=hebrew";

    default:
        return "";
    }
}

LCIDRecord *lcidRecords = NULL;
int32_t lcidMax   = 0;
int32_t lcidCount = 0;

//
// Straight insertion sort from Knuth vol. III, pg. 81
//
void sortLocales(LCIDRecord *records, int32_t count)
{
    for (int32_t j = 1; j < count; j += 1) {
        int32_t i;
        LCIDRecord v = records[j];

        for (i = j - 1; i >= 0; i -= 1) {
            if (strcmp(v.localeID, records[i].localeID) >= 0) {
                break;
            }

            records[i + 1] = records[i];
        }

        records[i + 1] = v;
    }
}

//
// Straight insertion sort from Knuth vol. III, pg. 81
//
void sortStrings(Win32Collator &coll, StringSortRecord *records, int32_t count)
{
    UErrorCode status = U_ZERO_ERROR;

    for (int32_t j = 1; j < count; j += 1) {
        int32_t i;
        StringSortRecord v = records[j];

        for (i = j - 1; i >= 0; i -= 1) {
            if (coll.compare(v.str, records[i].str, status) >= 0) {
                break;
            }

            records[i + 1] = records[i];
        }

        records[i + 1] = v;
    }
}

void checkCollation(const Locale &locale)
{
    UErrorCode status = U_ZERO_ERROR;
    Win32Collator coll(locale, status);

    for(int i = 0; i < stringCount; i += 1) {
        uint8_t sortkey_i[256];
        int32_t size_i = coll.getSortKey(strings[i], sortkey_i, 256);

        for(int j = 0; j < stringCount; j += 1) {
            uint8_t sortkey_j[256];
            int32_t expectedResult = (i < j? -1 : (i == j? 0 : 1));
            int32_t size_j = coll.getSortKey(strings[j], sortkey_j, 256);
            int32_t size_cmp = size_i <= size_j? size_i : size_j;

            if (memcmp(sortkey_i, sortkey_j, size_cmp) != expectedResult) {
                printf("Compare failed for locale %s: %S vs. %S.\n", locale.getBaseName(), strings[i].getBuffer(), strings[j].getBuffer());
            }
        }
    }
}

void showLocales()
{
    FILE *file;
    SYSTEMTIME winNow;
    UDate icuNow = 0;
    SYSTEMTIME st;
    FILETIME ft;

    GetSystemTime(&st);
    SystemTimeToFileTime(&st, &ft);
    SystemTimeToTzSpecificLocalTime(NULL, &st, &winNow);

    int64_t wftNow = ((int64_t) ft.dwHighDateTime << 32) + ft.dwLowDateTime;
    UErrorCode status = U_ZERO_ERROR;

    int64_t udtsNow = utmscale_fromInt64(wftNow, UDTS_WINDOWS_FILE_TIME, &status);

    icuNow = (UDate) utmscale_toInt64(udtsNow, UDTS_ICU4C_TIME, &status);

    file = fopen("DateTimeFormats.html", "wb");
    printHeader(file, "Windows Date/Time Formats");
    printTableHeader(file, "LCID", "Locale", "Calendar", "Win Date Format", "Win Time Format", "Win Sample",
        "ICU Date Format", "ICU TimeFormat", "ICU Sample", "windtfmt");

    sortLocales(lcidRecords, lcidCount);

    for(int i = 0; i < lcidCount; i += 1) {
        UErrorCode status = U_ZERO_ERROR;
        WCHAR calendarType[3], longDateFormat[81], longTimeFormat[81], buffer[256];

        GetLocaleInfoW(lcidRecords[i].lcid, LOCALE_SLONGDATE,   longDateFormat, 81);
        GetLocaleInfoW(lcidRecords[i].lcid, LOCALE_STIMEFORMAT, longTimeFormat, 81);
        GetLocaleInfoW(lcidRecords[i].lcid, LOCALE_ICALENDARTYPE, calendarType, 3);

        char localeID[64];
        int32_t calType = 0;

        swscanf(calendarType, L"%d", &calType);
        sprintf(localeID, "%s%s", lcidRecords[i].localeID, getCalendarType(calType));

        UnicodeString uLongDateFormat, uLongTimeFormat, uBuffer, wBuffer, nBuffer;
        Locale ulocale(localeID);
        char utf8LCID[32], utf8CalendarType[32], utf8WinLongDate[256], utf8WinLongTime[256], utf8WinFormatString[256],
            utf8ULongDate[256], utf8ULongTime[256], utf8UFormatString[256], utf8WFormatString[256];

        convertDateTimeToICU(longDateFormat, (int32_t) wcslen(longDateFormat), uLongDateFormat);
        convertDateTimeToICU(longTimeFormat, (int32_t) wcslen(longTimeFormat), uLongTimeFormat);

        sprintf(utf8LCID, "%08x", lcidRecords[i].lcid);
        toUTF8Chars(longDateFormat, (int32_t) wcslen(longDateFormat), utf8WinLongDate, 256, &status);
        toUTF8Chars(longTimeFormat, (int32_t) wcslen(longTimeFormat), utf8WinLongTime, 256, &status);
        toUTF8Chars(calendarType,   (int32_t) wcslen(calendarType),   utf8CalendarType,  8, &status);

        toUTF8Chars(uLongDateFormat.getBuffer(), uLongDateFormat.length(), utf8ULongDate, 256, &status);
        toUTF8Chars(uLongTimeFormat.getBuffer(), uLongTimeFormat.length(), utf8ULongTime, 256, &status);

        int len = GetDateFormatW(lcidRecords[i].lcid, 0, &winNow, longDateFormat, buffer, 256);

        buffer[len-1] = 0x20;
        GetTimeFormatW(lcidRecords[i].lcid, 0, &winNow, longTimeFormat, &buffer[len], 256 - len);

        UResourceBundle *bundle = ures_open(NULL, lcidRecords[i].localeID, &status);

        ures_close(bundle);

        if (U_SUCCESS(status) && status != U_USING_DEFAULT_WARNING) {
            UnicodeString asciiZero = UNICODE_STRING_SIMPLE("0");
            SimpleDateFormat sdf(uLongDateFormat + " " + uLongTimeFormat, ulocale, status);
            DecimalFormat df(*(DecimalFormat *) sdf.getNumberFormat());
            DecimalFormatSymbols asciiSymbols(*df.getDecimalFormatSymbols());

            asciiSymbols.setSymbol(DecimalFormatSymbols::kZeroDigitSymbol, asciiZero);
            
            df.setDecimalFormatSymbols(asciiSymbols);
            sdf.setNumberFormat(df);

            sdf.format(icuNow, uBuffer);
        }

        toUTF8Chars(buffer, (int32_t) wcslen(buffer), utf8WinFormatString, 256, &status);
        toUTF8Chars(uBuffer.getBuffer(), uBuffer.length(), utf8UFormatString, 256, &status);

        int bgcolor  = 0xFFFFFF;
        int32_t blen = (int32_t) wcslen(buffer);

        if (unorm_compare(buffer, blen, uBuffer.getBuffer(), uBuffer.length(), U_FOLD_CASE_DEFAULT, &status) != 0) {
            if (unorm_compare(buffer, blen, uBuffer.getBuffer(), uBuffer.length(), U_COMPARE_IGNORE_CASE, &status) != 0) {
                UnicodeString asciiDigits;

                convertDigits(uBuffer, asciiDigits);

                if (unorm_compare(buffer, blen, asciiDigits.getBuffer(), asciiDigits.length(), U_COMPARE_IGNORE_CASE, &status) == 0) {
                    bgcolor = 0xFF7F00; /* orange */
                } else {
                    bgcolor = 0xFF0000; /* red */
                }
            } else {
                bgcolor = 0xFFFF00; /* yellow */
            }
        }

        Win32DateFormat *wdf = new Win32DateFormat(DateFormat::kFull, DateFormat::kFull, ulocale, status);

        wdf->format(icuNow, wBuffer);
        toUTF8Chars(wBuffer.getBuffer(), wBuffer.length(), utf8WFormatString, 256, &status);

        double number = 123456789.12;
        Win32NumberFormat *wnf = new Win32NumberFormat(ulocale, FALSE, status);

        wnf->format((int64_t) number, nBuffer);

        printTableRow(file, utf8LCID, lcidRecords[i].localeID, utf8CalendarType, utf8WinLongDate, utf8WinLongTime, utf8WinFormatString,
            utf8ULongDate, utf8ULongTime, utf8UFormatString, utf8WFormatString, bgcolor);

        checkCollation(ulocale);
    }

    printTableFooter(file);
    printFooter(file);

    fclose(file);
}

BOOL CALLBACK EnumLocalesProc(LPWSTR lpLocaleString)
{
    UErrorCode status = U_ZERO_ERROR;

    if (lcidCount >= lcidMax) {
        LCIDRecord *newRecords = new LCIDRecord[lcidMax + 32];

        for (int i = 0; i < lcidMax; i += 1) {
            newRecords[i] = lcidRecords[i];
        }

        delete[] lcidRecords;
        lcidRecords = newRecords;
        lcidMax += 32;
    }

    swscanf(lpLocaleString, L"%8x", &lcidRecords[lcidCount].lcid);

    lcidRecords[lcidCount].localeID = uprv_convertToPosix(lcidRecords[lcidCount].lcid, &status);

    lcidCount += 1;

    return TRUE;
}

int _tmain(int argc, _TCHAR* argv[])
{
    lcidMax = 64;
    lcidCount = 0;
    lcidRecords = new LCIDRecord[lcidMax];

    EnumSystemLocalesW(EnumLocalesProc, LCID_INSTALLED);

    showLocales();

	return 0;
}

