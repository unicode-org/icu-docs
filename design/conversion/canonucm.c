/*
*******************************************************************************
*
*   Copyright (C) 2000-2003, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*   file name:  canonucm.c
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2000nov08
*   created by: Markus W. Scherer
*
*   This tool reads a .ucm file and canonicalizes it: In the CHARMAP section,
*   - sort by Unicode code points
*   - print all code points in uppercase hexadecimal
*   - print all Unicode code points with 4, 5, or 6 digits as needed
*   - remove the comments
*   - remove unnecessary spaces
*
*   Starting 2003oct09, canonucm handles m:n mappings as well, but requires
*   a more elaborate build using the ICU common (icuuc) and toolutil libraries.
*/

#include "unicode/utypes.h"
#include "ucnv_ext.h"
#include "ucm.h"
#include <stdio.h>
#include <string.h>

extern int
main(int argc, const char *argv[]) {
    char line[200];
    char *key, *value;

    UCMFile *ucm;

    ucm=ucm_open();

    /* parse the input file from stdin */
    /* read and copy header */
    do {
        if(gets(line)==NULL) {
            fprintf(stderr, "error: no mapping section");
            return 1;
        }
        puts(line);
    } while(ucm_parseHeaderLine(ucm, line, &key, &value) ||
            0!=strcmp(line, "CHARMAP"));

    ucm_processStates(&ucm->states);

    /*
     * If there is _no_ <icu:base> base table name, then parse the base table
     * and then an optional extension table.
     *
     * If there _is_ a base table name, then only parse
     * the then-mandatory extension table.
     */
    if(ucm->baseName[0]==0) {
        /* copy empty and comment lines before the first mapping */
        for(;;) {
            if(gets(line)==NULL) {
                fprintf(stderr, "error: no mappings");
                return 1;
            }
            if(line[0]!=0 && line[0]!='#') {
                break;
            }
            puts(line);
        }

        /* process the base charmap section, start with the line read above */
        for(;;) {
            /* ignore empty and comment lines */
            if(line[0]!=0 && line[0]!='#') {
                if(0!=strcmp(line, "END CHARMAP")) {
                    ucm_addMappingFromLine(ucm, line, TRUE);
                } else {
                    /* sort and write all mappings */
                    ucm_sortTable(ucm->base);
                    ucm_printTable(ucm->base, stdout);

                    /* output "END CHARMAP" */
                    puts(line);
                    break;
                }
            }
            /* read the next line */
            if(gets(line)==NULL) {
                fprintf(stderr, "incomplete charmap section\n");
                return U_INVALID_TABLE_FORMAT;
            }
        }
    }

    /* do the same with an extension table section, ignore lines before it */
    for(;;) {
        if(gets(line)==NULL) {
            if(ucm->baseName[0]==0) {
                break; /* the extension table is optional if we parsed a base table */
            } else {
                fprintf(stderr, "missing extension charmap section when <icu:base> specified\n");
                return U_INVALID_TABLE_FORMAT;
            }
        }
        if(line[0]!=0 && line[0]!='#') {
            if(strcmp(line, "CHARMAP")) {
                /* process the extension table's charmap section, start with the line read above */
                for(;;) {
                    if(gets(line)==NULL) {
                        fprintf(stderr, "incomplete extension charmap section\n");
                        return U_INVALID_TABLE_FORMAT;
                    }

                    /* ignore empty and comment lines */
                    if(line[0]!=0 && line[0]!='#') {
                        if(0!=strcmp(line, "END CHARMAP")) {
                            ucm_addMappingFromLine(ucm, line, FALSE);
                        } else {
                            break;
                        }
                    }
                }
                break;
            } else {
                fprintf(stderr, "unexpected text after the base mapping table\n");
                return U_INVALID_TABLE_FORMAT;
            }
        }
    }

    if(ucm->ext->mappingsLength>0) {
        puts("\nCHARMAP");

        /* sort and write all extension mappings */
        ucm_sortTable(ucm->ext);
        ucm_printTable(ucm->ext, stdout);

        /* output "END CHARMAP" */
        puts(line);
    }

    ucm_close(ucm);
    return 0;
}
