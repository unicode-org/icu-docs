/*
*******************************************************************************
*
*   Copyright (C) 2003, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*   file name:  ucmstate.c
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2003oct09
*   created by: Markus W. Scherer
*
*   This file handles ICU .ucm file state information as part of the ucm module.
*   Most of this code used to be in makeconv.c.
*/

#include "unicode/utypes.h"
#include "uparse.h"
#include "ucnvmbcs.h"
#include "ucnv_ext.h"
#include "ucm.h"
#include <stdio.h>
#include <string.h>

/* MBCS state handling ------------------------------------------------------ */

/*
 * state table row grammar (ebnf-style):
 * (whitespace is allowed between all tokens)
 *
 * row=[[firstentry ','] entry (',' entry)*]
 * firstentry="initial" | "surrogates"
 *            (initial state (default for state 0), output is all surrogate pairs)
 * entry=range [':' nextstate] ['.' action]
 * range=number ['-' number]
 * nextstate=number
 *           (0..7f)
 * action='u' | 's' | 'p' | 'i'
 *        (unassigned, state change only, surrogate pair, illegal)
 * number=(1- or 2-digit hexadecimal number)
 */
static const char *
parseState(const char *s, int32_t state[256], uint32_t *pFlags) {
    const char *t;
    uint32_t start, end, i;
    int32_t entry;

    /* initialize the state: all illegal with U+ffff */
    for(i=0; i<256; ++i) {
        state[i]=MBCS_ENTRY_FINAL(0, MBCS_STATE_ILLEGAL, 0xffff);
    }

    /* skip leading white space */
    s=u_skipWhitespace(s);

    /* is there an "initial" or "surrogates" directive? */
    if(strncmp("initial", s, 7)==0) {
        *pFlags=MBCS_STATE_FLAG_DIRECT;
        s=u_skipWhitespace(s+7);
        if(*s++!=',') {
            return s-1;
        }
    } else if(*pFlags==0 && strncmp("surrogates", s, 10)==0) {
        *pFlags=MBCS_STATE_FLAG_SURROGATES;
        s=u_skipWhitespace(s+10);
        if(*s++!=',') {
            return s-1;
        }
    } else if(*s==0) {
        /* empty state row: all-illegal */
        return NULL;
    }

    for(;;) {
        /* read an entry, the start of the range first */
        s=u_skipWhitespace(s);
        start=strtoul(s, (char **)&t, 16);
        if(s==t || 0xff<start) {
            return s;
        }
        s=u_skipWhitespace(t);

        /* read the end of the range if there is one */
        if(*s=='-') {
            s=u_skipWhitespace(s+1);
            end=strtoul(s, (char **)&t, 16);
            if(s==t || end<start || 0xff<end) {
                return s;
            }
            s=u_skipWhitespace(t);
        } else {
            end=start;
        }

        /* determine the state entrys for this range */
        if(*s!=':' && *s!='.') {
            /* the default is: final state with valid entries */
            entry=MBCS_ENTRY_FINAL(0, MBCS_STATE_VALID_16, 0);
        } else {
            entry=MBCS_ENTRY_TRANSITION(0, 0);
            if(*s==':') {
                /* get the next state, default to 0 */
                s=u_skipWhitespace(s+1);
                i=strtoul(s, (char **)&t, 16);
                if(s!=t) {
                    if(0x7f<i) {
                        return s;
                    }
                    s=u_skipWhitespace(t);
                    entry=MBCS_ENTRY_SET_STATE(entry, i);
                }
            }

            /* get the state action, default to valid */
            if(*s=='.') {
                /* this is a final state */
                entry=MBCS_ENTRY_SET_FINAL(entry);

                s=u_skipWhitespace(s+1);
                if(*s=='u') {
                    /* unassigned set U+fffe */
                    entry=MBCS_ENTRY_FINAL_SET_ACTION_VALUE(entry, MBCS_STATE_UNASSIGNED, 0xfffe);
                    s=u_skipWhitespace(s+1);
                } else if(*s=='p') {
                    if(*pFlags!=MBCS_STATE_FLAG_DIRECT) {
                        entry=MBCS_ENTRY_FINAL_SET_ACTION(entry, MBCS_STATE_VALID_16_PAIR);
                    } else {
                        entry=MBCS_ENTRY_FINAL_SET_ACTION(entry, MBCS_STATE_VALID_16);
                    }
                    s=u_skipWhitespace(s+1);
                } else if(*s=='s') {
                    entry=MBCS_ENTRY_FINAL_SET_ACTION(entry, MBCS_STATE_CHANGE_ONLY);
                    s=u_skipWhitespace(s+1);
                } else if(*s=='i') {
                    /* illegal set U+ffff */
                    entry=MBCS_ENTRY_FINAL_SET_ACTION_VALUE(entry, MBCS_STATE_ILLEGAL, 0xffff);
                    s=u_skipWhitespace(s+1);
                } else {
                    /* default to valid */
                    entry=MBCS_ENTRY_FINAL_SET_ACTION(entry, MBCS_STATE_VALID_16);
                }
            } else {
                /* this is an intermediate state, nothing to do */
            }
        }

        /* adjust "final valid" states according to the state flags */
        if(MBCS_ENTRY_FINAL_ACTION(entry)==MBCS_STATE_VALID_16) {
            switch(*pFlags) {
            case 0:
                /* no adjustment */
                break;
            case MBCS_STATE_FLAG_DIRECT:
                /* set the valid-direct code point to "unassigned"==0xfffe */
                entry=MBCS_ENTRY_FINAL_SET_ACTION_VALUE(entry, MBCS_STATE_VALID_DIRECT_16, 0xfffe);
                break;
            case MBCS_STATE_FLAG_SURROGATES:
                entry=MBCS_ENTRY_FINAL_SET_ACTION_VALUE(entry, MBCS_STATE_VALID_16_PAIR, 0);
                break;
            default:
                break;
            }
        }

        /* set this entry for the range */
        for(i=start; i<=end; ++i) {
            state[i]=entry;
        }

        if(*s==',') {
            ++s;
        } else {
            return *s==0 ? NULL : s;
        }
    }
}

U_CAPI void U_EXPORT2
ucm_addState(UCMStates *states, const char *s) {
    const char *error;

    if(states->countStates==MBCS_MAX_STATE_COUNT) {
        fprintf(stderr, "ucm error: too many states (maximum %u)\n", MBCS_MAX_STATE_COUNT);
        exit(U_INVALID_TABLE_FORMAT);
    }

    error=parseState(s, states->stateTable[states->countStates],
                       &states->stateFlags[states->countStates]);
    if(error!=NULL) {
        fprintf(stderr, "ucm error: parse error in state definition at '%s'\n", error);
        exit(U_INVALID_TABLE_FORMAT);
    }

    ++states->countStates;
}

static void
sumUpStates(UCMStates *states) {
    int32_t entry, sum, state, cell, count;
    UBool allStatesReady;

    /*
     * Sum up the offsets for all states.
     * In each final state (where there are only final entries),
     * the offsets add up directly.
     * In all other state table rows, for each transition entry to another state,
     * the offsets sum of that state needs to be added.
     * This is achieved in at most countStates iterations.
     */
    allStatesReady=FALSE;
    for(count=states->countStates; !allStatesReady && count>=0; --count) {
        allStatesReady=TRUE;
        for(state=states->countStates-1; state>=0; --state) {
            if(!(states->stateFlags[state]&MBCS_STATE_FLAG_READY)) {
                allStatesReady=FALSE;
                sum=0;

                /* at first, add up only the final delta offsets to keep them <512 */
                for(cell=0; cell<256; ++cell) {
                    entry=states->stateTable[state][cell];
                    if(MBCS_ENTRY_IS_FINAL(entry)) {
                        switch(MBCS_ENTRY_FINAL_ACTION(entry)) {
                        case MBCS_STATE_VALID_16:
                            states->stateTable[state][cell]=MBCS_ENTRY_FINAL_SET_VALUE(entry, sum);
                            sum+=1;
                            break;
                        case MBCS_STATE_VALID_16_PAIR:
                            states->stateTable[state][cell]=MBCS_ENTRY_FINAL_SET_VALUE(entry, sum);
                            sum+=2;
                            break;
                        default:
                            /* no addition */
                            break;
                        }
                    }
                }

                /* now, add up the delta offsets for the transitional entries */
                for(cell=0; cell<256; ++cell) {
                    entry=states->stateTable[state][cell];
                    if(MBCS_ENTRY_IS_TRANSITION(entry)) {
                        if(states->stateFlags[MBCS_ENTRY_TRANSITION_STATE(entry)]&MBCS_STATE_FLAG_READY) {
                            states->stateTable[state][cell]=MBCS_ENTRY_TRANSITION_SET_OFFSET(entry, sum);
                            sum+=states->stateOffsetSum[MBCS_ENTRY_TRANSITION_STATE(entry)];
                        } else {
                            /* that next state does not have a sum yet, we cannot finish the one for this state */
                            sum=-1;
                            break;
                        }
                    }
                }

                if(sum!=-1) {
                    states->stateOffsetSum[state]=sum;
                    states->stateFlags[state]|=MBCS_STATE_FLAG_READY;
                }
            }
        }
    }

    if(!allStatesReady) {
        fprintf(stderr, "ucm error: the state table contains loops\n");
        exit(U_INVALID_TABLE_FORMAT);
    }

    /*
     * For all "direct" (i.e., initial) states>0,
     * the offsets need to be increased by the sum of
     * the previous initial states.
     */
    sum=states->stateOffsetSum[0];
    for(state=1; state<states->countStates; ++state) {
        if((states->stateFlags[state]&0xf)==MBCS_STATE_FLAG_DIRECT) {
            int32_t sum2=sum;
            sum+=states->stateOffsetSum[state];
            for(cell=0; cell<256; ++cell) {
                entry=states->stateTable[state][cell];
                if(MBCS_ENTRY_IS_TRANSITION(entry)) {
                    states->stateTable[state][cell]=MBCS_ENTRY_TRANSITION_ADD_OFFSET(entry, sum2);
                }
            }
        }
    }
    /* ### TODO move to genmbcs.c -- if(VERBOSE) {
        printf("the total number of offsets is 0x%lx=%ld\n", sum, sum);
    } */

    /* round up to the next even number to have the following data 32-bit-aligned */
    states->countToUCodeUnits=(sum+1)&~1;
}

U_CAPI void U_EXPORT2
ucm_processStates(UCMStates *states) {
    int32_t entry, state, cell, count;

    if(states->conversionType==UCNV_UNSUPPORTED_CONVERTER) {
        fprintf(stderr, "ucm error: missing conversion type\n");
        exit(U_INVALID_TABLE_FORMAT);
    }

    /* ### TODO keep in makeconv.c/readHeaderFromFile() -- if(staticData->subChar1!=0 &&
              !staticData->conversionType==UCNV_MBCS &&
              !staticData->conversionType==UCNV_EBCDIC_STATEFUL
    ) {
        fprintf(stderr, "ucm error: <subchar1> defined for a type other than MBCS or EBCDIC_STATEFUL\n");
        *pErrorCode=U_INVALID_TABLE_FORMAT;
    }*/

    if(states->countStates==0) {
        switch(states->conversionType) {
        case UCNV_SBCS:
            /* SBCS: use MBCS data structure with a default state table */
            if(states->maxCharLength!=1) {
                fprintf(stderr, "error: SBCS codepage with max B/char!=1\n");
                exit(U_INVALID_TABLE_FORMAT);
            }
            states->conversionType=UCNV_MBCS;
            ucm_addState(states, "0-ff");
            break;
        case UCNV_MBCS:
            fprintf(stderr, "ucm error: missing state table information (<icu:state>) for MBCS\n");
            exit(U_INVALID_TABLE_FORMAT);
            break;
        case UCNV_EBCDIC_STATEFUL:
            /* EBCDIC_STATEFUL: use MBCS data structure with a default state table */
            if(states->minCharLength!=1 || states->maxCharLength!=2) {
                fprintf(stderr, "error: DBCS codepage with min B/char!=1 or max B/char!=2\n");
                exit(U_INVALID_TABLE_FORMAT);
            }
            states->conversionType=UCNV_MBCS;
            ucm_addState(states, "0-ff, e:1.s, f:0.s");
            ucm_addState(states, "initial, 0-3f:4, e:1.s, f:0.s, 40:3, 41-fe:2, ff:4");
            ucm_addState(states, "0-40:1.i, 41-fe:1., ff:1.i");
            ucm_addState(states, "0-ff:1.i, 40:1.");
            ucm_addState(states, "0-ff:1.i");
            break;
        case UCNV_DBCS:
            /* DBCS: use MBCS data structure with a default state table */
            if(states->minCharLength!=2 || states->maxCharLength!=2) {
                fprintf(stderr, "error: DBCS codepage with min or max B/char!=2\n");
                exit(U_INVALID_TABLE_FORMAT);
            }
            states->conversionType = UCNV_MBCS;
            ucm_addState(states, "0-3f:3, 40:2, 41-fe:1, ff:3");
            ucm_addState(states, "41-fe");
            ucm_addState(states, "40");
            ucm_addState(states, "");
            break;
        default:
            fprintf(stderr, "ucm error: unknown charset structure\n");
            exit(U_INVALID_TABLE_FORMAT);
            break;
        }
    }

    /*
     * check that the min/max character lengths are reasonable;
     * to do this right, all paths through the state table would have to be
     * recursively walked while keeping track of the sequence lengths,
     * but these simple checks cover most state tables in practice
     */
    if(states->maxCharLength<states->minCharLength) {
        fprintf(stderr, "ucm error: max B/char < min B/char\n");
        exit(U_INVALID_TABLE_FORMAT);
    }

    /* count non-direct states and compare with max B/char */
    count=0;
    for(state=0; state<states->countStates; ++state) {
        if((states->stateFlags[1]&0xf)!=MBCS_STATE_FLAG_DIRECT) {
            ++count;
        }
    }
    if(states->maxCharLength>count+1) {
        fprintf(stderr, "ucm error: max B/char too large\n");
        exit(U_INVALID_TABLE_FORMAT);
    }

    if(states->minCharLength==1) {
        int32_t action;

        /*
         * if there are single-byte characters,
         * then the initial state must have direct result states
         */
        for(cell=0; cell<256; ++cell) {
            entry=states->stateTable[0][cell];
            if( MBCS_ENTRY_IS_FINAL(entry) &&
                ((action=MBCS_ENTRY_FINAL_ACTION(entry))==MBCS_STATE_VALID_DIRECT_16 ||
                 action==MBCS_STATE_UNASSIGNED)
            ) {
                break;
            }
        }

        if(cell==256) {
            fprintf(stderr, "ucm error: min B/char too small\n");
            exit(U_INVALID_TABLE_FORMAT);
        }
    }

    /*
     * make sure that all "next state" values are within limits
     * and that all next states after final ones have the "direct"
     * flag of initial states
     */
    for(state=states->countStates-1; state>=0; --state) {
        for(cell=0; cell<256; ++cell) {
            entry=states->stateTable[state][cell];
            if((uint8_t)MBCS_ENTRY_STATE(entry)>=states->countStates) {
                fprintf(stderr, "ucm error: state table entry [%x][%x] has a next state of %x that is too high\n",
                    state, cell, MBCS_ENTRY_STATE(entry));
                exit(U_INVALID_TABLE_FORMAT);
            }
            if(MBCS_ENTRY_IS_FINAL(entry) && (states->stateFlags[MBCS_ENTRY_STATE(entry)]&0xf)!=MBCS_STATE_FLAG_DIRECT) {
                fprintf(stderr, "ucm error: state table entry [%x][%x] is final but has a non-initial next state of %x\n",
                    state, cell, MBCS_ENTRY_STATE(entry));
                exit(U_INVALID_TABLE_FORMAT);
            } else if(MBCS_ENTRY_IS_TRANSITION(entry) && (states->stateFlags[MBCS_ENTRY_STATE(entry)]&0xf)==MBCS_STATE_FLAG_DIRECT) {
                fprintf(stderr, "ucm error: state table entry [%x][%x] is not final but has an initial next state of %x\n",
                    state, cell, MBCS_ENTRY_STATE(entry));
                exit(U_INVALID_TABLE_FORMAT);
            }
        }
    }

    /* is this an SI/SO (like EBCDIC-stateful) state table? */
    if(states->countStates>=2 && (states->stateFlags[1]&0xf)==MBCS_STATE_FLAG_DIRECT) {
        if(states->maxCharLength!=2) {
            fprintf(stderr, "ucm error: SI/SO codepages must have max 2 bytes/char (not %x)\n", states->maxCharLength);
            exit(U_INVALID_TABLE_FORMAT);
        }
        if(states->countStates<3) {
            fprintf(stderr, "ucm error: SI/SO codepages must have at least 3 states (not %x)\n", states->countStates);
            exit(U_INVALID_TABLE_FORMAT);
        }
        /* are the SI/SO all in the right places? */
        if( states->stateTable[0][0xe]==MBCS_ENTRY_FINAL(1, MBCS_STATE_CHANGE_ONLY, 0) &&
            states->stateTable[0][0xf]==MBCS_ENTRY_FINAL(0, MBCS_STATE_CHANGE_ONLY, 0) &&
            states->stateTable[1][0xe]==MBCS_ENTRY_FINAL(1, MBCS_STATE_CHANGE_ONLY, 0) &&
            states->stateTable[1][0xf]==MBCS_ENTRY_FINAL(0, MBCS_STATE_CHANGE_ONLY, 0)
        ) {
            states->conversionType=MBCS_OUTPUT_2_SISO;
        } else {
            fprintf(stderr, "ucm error: SI/SO codepages must have in states 0 and 1 transitions e:1.s, f:0.s\n");
            exit(U_INVALID_TABLE_FORMAT);
        }
        state=2;
    } else {
        state=1;
    }

    /* check that no unexpected state is a "direct" one */
    while(state<states->countStates) {
        if((states->stateFlags[state]&0xf)==MBCS_STATE_FLAG_DIRECT) {
            fprintf(stderr, "ucm error: state %d is 'initial' - not supported except for SI/SO codepages\n", state);
            exit(U_INVALID_TABLE_FORMAT);
        }
        ++state;
    }

    sumUpStates(states);

    /* ### TODO in genmbcs.c/MBCSProcessStates() keep the following allocation code */
}

U_CAPI int32_t U_EXPORT2
ucm_countChars(UCMStates *states,
               const uint8_t *bytes, int32_t length) {
    uint32_t offset;
    int32_t i, entry, count;
    uint8_t state;

    offset=0;
    i=count=0;
    state=0;

    if(states->countStates==0) {
        fprintf(stderr, "ucm error: there is no state information!\n");
        return -1;
    }

    /* for SI/SO (like EBCDIC-stateful), double-byte sequences start in state 1 */
    if(length==2 && states->conversionType==MBCS_OUTPUT_2_SISO) {
        state=1;
    }

    /*
     * Walk down the state table like in conversion,
     * much like getNextUChar().
     * We assume that c<=0x10ffff.
     */
    for(i=0; i<length; ++i) {
        entry=states->stateTable[state][bytes[i]];
        if(MBCS_ENTRY_IS_TRANSITION(entry)) {
            state=(uint8_t)MBCS_ENTRY_TRANSITION_STATE(entry);
            offset+=MBCS_ENTRY_TRANSITION_OFFSET(entry);
        } else {
            switch(MBCS_ENTRY_FINAL_ACTION(entry)) {
            case MBCS_STATE_ILLEGAL:
                fprintf(stderr, "ucm error: byte sequence ends in illegal state\n");
                return -1;
            case MBCS_STATE_CHANGE_ONLY:
                fprintf(stderr, "ucm error: byte sequence ends in state-change-only\n");
                return -1;
            case MBCS_STATE_UNASSIGNED:
                fprintf(stderr, "ucm error: byte sequence ends in unassigned state\n");
                return -1;
            case MBCS_STATE_FALLBACK_DIRECT_16:
            case MBCS_STATE_VALID_DIRECT_16:
            case MBCS_STATE_FALLBACK_DIRECT_20:
            case MBCS_STATE_VALID_DIRECT_20:
            case MBCS_STATE_VALID_16:
            case MBCS_STATE_VALID_16_PAIR:
                /* count a complete character and prepare for a new one */
                ++count;
                state=(uint8_t)MBCS_ENTRY_FINAL_STATE(entry);
                offset=0;
                break;
            default:
                /* reserved, must never occur */
                fprintf(stderr, "ucm error: byte sequence reached reserved action code, entry: 0x%02lx\n", entry);
                return -1;
            }
        }
    }

    if(offset!=0) {
        fprintf(stderr, "ucm error: byte sequence too short, ends in non-final state %hu: 0x%02lx\n", state);
        return -1;
    }

    /*
     * for SI/SO (like EBCDIC-stateful), multiple-character results
     * must consist of only double-byte sequences
     */
    if(states->conversionType==MBCS_OUTPUT_2_SISO && length!=2*count) {
        fprintf(stderr, "ucm error: SI/SO (like EBCDIC-stateful) result with %d characters does not contain all DBCS\n", count);
        return -1;
    }

    return count;
}

U_CAPI UBool U_EXPORT2
ucm_parseHeaderLine(UCMFile *ucm,
                    char *line, char **pKey, char **pValue) {
    UCMStates *states;
    char *s, *end;
    char c;

    states=&ucm->states;

    /* remove comments and trailing CR and LF and remove whitespace from the end */
    for(end=line; (c=*end)!=0; ++end) {
        if(c=='#' || c=='\r' || c=='\n') {
            break;
        }
    }
    while(end>line && (*(end-1)==' ' || *(end-1)=='\t')) {
        --end;
    }
    *end=0;

    /* skip leading white space and ignore empty lines */
    s=(char *)u_skipWhitespace(line);
    if(*s==0) {
        return TRUE;
    }

    /* stop at the beginning of the mapping section */
    if(memcmp(s, "CHARMAP", 7)==0) {
        return FALSE;
    }

    /* get the key name, bracketed in <> */
    if(*s!='<') {
        fprintf(stderr, "ucm error: no header field <key> in line \"%s\"\n", line);
        exit(U_INVALID_TABLE_FORMAT);
    }
    *pKey=++s;
    while(*s!='>') {
        if(*s==0) {
            fprintf(stderr, "ucm error: incomplete header field <key> in line \"%s\"\n", line);
            exit(U_INVALID_TABLE_FORMAT);
        }
        ++s;
    }
    *s=0;

    /* get the value string, possibly quoted */
    s=(char *)u_skipWhitespace(s+1);
    if(*s!='"') {
        *pValue=s;
    } else {
        /* remove the quotes */
        *pValue=s+1;
        if(end>*pValue && *(end-1)=='"') {
            *--end=0;
        }
    }

    /* collect the information from the header field, ignore unknown keys */
    if(strcmp(*pKey, "uconv_class")==0) {
        if(strcmp(*pValue, "DBCS")==0) {
            states->conversionType=UCNV_DBCS;
        } else if(strcmp(*pValue, "SBCS")==0) {
            states->conversionType = UCNV_SBCS;
        } else if(strcmp(*pValue, "MBCS")==0) {
            states->conversionType = UCNV_MBCS;
        } else if(strcmp(*pValue, "EBCDIC_STATEFUL")==0) {
            states->conversionType = UCNV_EBCDIC_STATEFUL;
        } else {
            fprintf(stderr, "ucm error: unknown <uconv_class> %s\n", *pValue);
            exit(U_INVALID_TABLE_FORMAT);
        }
        return TRUE;
    } else if(strcmp(*pKey, "mb_cur_max")==0) {
        c=**pValue;
        if('1'<=c && c<='4' && (*pValue)[1]==0) {
            states->maxCharLength=(int8_t)(c-'0');
        } else {
            fprintf(stderr, "ucm error: illegal <mb_cur_max> %s\n", *pValue);
            exit(U_INVALID_TABLE_FORMAT);
        }
        return TRUE;
    } else if(strcmp(*pKey, "mb_cur_min")==0) {
        c=**pValue;
        if('1'<=c && c<='4' && (*pValue)[1]==0) {
            states->minCharLength=(int8_t)(c-'0');
        } else {
            fprintf(stderr, "ucm error: illegal <mb_cur_min> %s\n", *pValue);
            exit(U_INVALID_TABLE_FORMAT);
        }
        return TRUE;
    } else if(strcmp(*pKey, "icu:state")==0) {
        /* if an SBCS/DBCS/EBCDIC_STATEFUL converter has icu:state, then turn it into MBCS */
        switch(states->conversionType) {
        case UCNV_SBCS:
        case UCNV_DBCS:
        case UCNV_EBCDIC_STATEFUL:
            states->conversionType=UCNV_MBCS;
            break;
        case UCNV_MBCS:
            break;
        default:
            fprintf(stderr, "ucm error: <icu:state> entry for non-MBCS table or before the <uconv_class> line\n");
            exit(U_INVALID_TABLE_FORMAT);
        }

        if(states->maxCharLength==0) {
            fprintf(stderr, "ucm error: <icu:state> before the <mb_cur_max> line\n");
            exit(U_INVALID_TABLE_FORMAT);
        }
        ucm_addState(states, *pValue);
        return TRUE;
    } else if(strcmp(*pKey, "icu:base")==0) {
        if(**pValue==0) {
            fprintf(stderr, "ucm error: <icu:base> without a base table name\n");
            exit(U_INVALID_TABLE_FORMAT);
        }
        strcpy(ucm->baseName, *pValue);
        return TRUE;
    }

    return FALSE;
}
