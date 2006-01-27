/*
**********************************************************************
*   Copyright (C) 2006, International Business Machines
*   Corporation and others.  All Rights Reserved.
**********************************************************************
*/

#include "unicode/utypes.h"
#include "unicode/timezone.h"
#include "unicode/unistr.h"
#include "unicode/strenum.h"

#include "wintz.h"

#include <tchar.h>
#include <windows.h>
#include <stdio.h>

int _tmain(int argc, _TCHAR* argv[])
{
    UErrorCode status = U_ZERO_ERROR;
    StringEnumeration *zones = TimeZone::createEnumeration();
    int32_t zoneCount = zones->count(status);
    int32_t missingCount = 0;

    for(const UnicodeString *zone = zones->snext(status); zone != NULL; zone = zones->snext(status)) {
        TIME_ZONE_INFORMATION tzi;

        if (! uprv_getWindowsTimeZoneInfo(&tzi, zone->getBuffer(), zone->length())) {
            UBool found = FALSE;
            int32_t ec = TimeZone::countEquivalentIDs(*zone);

            for (int z = 0; z < ec; z += 1) {
                UnicodeString equiv = TimeZone::getEquivalentID(*zone, z);

                if (found = uprv_getWindowsTimeZoneInfo(&tzi, equiv.getBuffer(), equiv.length())) {
                    break;
                }
            }

            if (! found) {
                UnicodeString zz(*zone);

                missingCount += 1;
                wprintf(L"%s\n", zz.getTerminatedBuffer());
            }
        }
    }

    printf("\nOut of %d total time zone IDs, %d do not have Windows equivalents.\n", zoneCount, missingCount);

    delete zones;
	return 0;
}

