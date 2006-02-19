/*
*******************************************************************************
*
*   Copyright (C) 2005-2006, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*   file name:  genudata.cpp
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2005apr17
*   created by: Markus Scherer
*/

#include <stdio.h>
#include "unicode/utypes.h"
#include "unicode/uchar.h"
#include "unicode/unistr.h"
#include "cmemory.h"
#include "hash.h"
#include "xmlparser.h"
#include "genudata.h"

/* store unique names */
class UniqueNames {
public:
    UniqueNames(UErrorCode &errorCode) : fNames(errorCode) {}

    ~UniqueNames() {}

    UniqueNames &add(const UnicodeString &s) {
        const UHashElement *he=fNames.find(s);
        if(he==NULL) {
            // add this new name
            UErrorCode errorCode=U_ZERO_ERROR; // ignore errors here
            fNames.puti(s, 0, errorCode);
        }
        return *this;
    }

    // iterate over names
    // initialize i with 0 before the first call
    const UnicodeString *next(int32_t &i) {
        const UHashElement *he=fNames.nextElement(i);
        if(he==NULL) {
            return NULL;
        } else {
            return (const UnicodeString *)he->key.pointer;
        }
    }

private:
    // prevent default construction etc.
    UniqueNames();
    UniqueNames(const UniqueNames &other);
    UniqueNames &operator=(const UniqueNames &other);

    Hashtable fNames;
};

class GenUData {
public:
    GenUData(UErrorCode &errorCode) :
        fParser(NULL),
        fRoot(NULL),
        s_repertoire("repertoire", -1, US_INV),
        s_blocks("blocks", -1, US_INV),
        s_group("group", -1, US_INV),
        s_code_point("code-point", -1, US_INV),
        s_reserved("reserved", -1, US_INV),
        s_noncharacter("noncharacter", -1, US_INV),
        s_surrogate("surrogate", -1, US_INV),
        s_char("char", -1, US_INV),
        s_cp("cp", -1, US_INV),
        s_first_cp("first-cp", -1, US_INV),
        s_last_cp("last-cp", -1, US_INV),
        s_type("type", -1, US_INV),
        unknownCPAttributes(errorCode) {}

    ~GenUData() {
        delete fParser;
    }

    void parseFile(const char *filename, UErrorCode &errorCode) {
        if(U_FAILURE(errorCode)) {
            return;
        }
        fParser=UXMLParser::createParser(errorCode);
        if(U_FAILURE(errorCode)) {
            fprintf(stderr, "genudata: unable to create an XML parser - %s\n",
                    u_errorName(errorCode));
            return;
        }
        fRoot=fParser->parseFile(filename, errorCode);
        if(U_SUCCESS(errorCode)) {
            parseRepertoire(errorCode);
            parseBlocks(errorCode);

            if(U_SUCCESS(errorCode)) {
                const UnicodeString *s;
                int32_t i;

                printf("unknown code point attributes:\n");
                for(i=0; (s=unknownCPAttributes.next(i))!=NULL;) {
                    printf("    %s\n", toChars(*s));
                }
            }
        }
    }

private:
    // prevent default construction etc.
    GenUData();
    GenUData(const UniqueNames &other);
    GenUData &operator=(const UniqueNames &other);

    UXMLParser *fParser;
    const UXMLElement *fRoot;

    UnicodeString
        s_repertoire,
        s_blocks,
        s_group,
        s_code_point,
        s_reserved,
        s_noncharacter,
        s_surrogate,
        s_char,
        s_cp,
        s_first_cp,
        s_last_cp,
        s_type;

    UniqueNames unknownCPAttributes;

    char charBuffer[4][200];

    void parseRepertoire(UErrorCode &errorCode) {
        const UXMLElement *rep, *cp;
        int32_t i;

        if(U_FAILURE(errorCode)) {
            return;
        }

        rep=fRoot->getChildElement(s_repertoire);
        if(rep==NULL) {
            fprintf(stderr, "genudata: missing <repertoire>\n");
            errorCode=U_INVALID_FORMAT_ERROR;
            return;
        }

        printf("<repertoire> has %ld child nodes\n", (long)rep->countChildren());
        for(i=0; U_SUCCESS(errorCode) && (cp=rep->nextChildElement(i))!=NULL;) {
            parseCP(cp, NULL, errorCode);
        }
    }

    void parseCP(const UXMLElement *cp, GenUDataProps *groupProps, UErrorCode &errorCode) {
        if(U_FAILURE(errorCode)) {
            return;
        }

        const UnicodeString *name;
        int32_t i;

        GenUDataProps props;
        int32_t type;
        UBool isGroup;

        name=&cp->getTagName();
        isGroup=FALSE;
        if(*name==s_char) {
            type=TYPE_CHAR;
        } else if(*name==s_group) {
            // allow a group only as an immediate child of <repertoire>
            if(groupProps==NULL) {
                isGroup=TRUE;
                type=TYPE_NONE; // there must be a type attribute
            } else {
                fprintf(stderr, "genudata: nested <group> not allowed\n");
                errorCode=U_INVALID_FORMAT_ERROR;
                return;
            }
        } else if(*name==s_code_point) {
            type=TYPE_NONE; // there must be a type attribute
        } else if(*name==s_reserved) {
            type=TYPE_RESERVED;
        } else if(*name==s_noncharacter) {
            type=TYPE_NONCHARACTER;
        } else if(*name==s_surrogate) {
            type=TYPE_SURROGATE;
        } else {
            fprintf(stderr, "genudata: unknown repertoire element <%s>\n", toChars(*name));
            errorCode=U_INVALID_FORMAT_ERROR;
            return;
        }

        if(groupProps!=NULL) {
            // copy group properties to code point properties
            props=*groupProps;
        } else {
            // reset properties for the top-level item
            uprv_memset(&props, 0, sizeof(props));
        }
        props.start=props.end=-1;
        props.type=type;
        parseCPAttributes(cp, isGroup, &props, errorCode);
        if(U_FAILURE(errorCode)) {
            return;
        }
        if(props.start>=0 || props.end>=0 || props.type!=TYPE_NONE) {
            // set properties for binary data files
            // TODO
        } else if(!isGroup) {
            fprintf(stderr, "genudata: <%s> without first/last code point or type\n", toChars(*name));
            errorCode=U_INVALID_FORMAT_ERROR;
            return;
        }

        if(isGroup) {
            // process the group children (code point elements, not groups)
            const UXMLElement *cpChild;
            for(i=0; U_SUCCESS(errorCode) && (cpChild=cp->nextChildElement(i))!=NULL;) {
                parseCP(cpChild, &props, errorCode);
            }
        }
    }

    void parseCPAttributes(const UXMLElement *cp, UBool isGroup, GenUDataProps *props, UErrorCode &errorCode) {
        UnicodeString name, value;
        int32_t i, count;

        count=cp->countAttributes();
        for(i=0; U_SUCCESS(errorCode) && i<count; ++i) {
            cp->getAttribute(i, name, value);
            if(name==s_type) {
                int32_t type;
                if(value==s_reserved) {
                    type=TYPE_RESERVED;
                } else if(value==s_noncharacter) {
                    type=TYPE_NONCHARACTER;
                } else if(value==s_surrogate) {
                    type=TYPE_SURROGATE;
                } else if(value==s_char) {
                    type=TYPE_CHAR;
                } else {
                    fprintf(stderr, "genudata: unknown type \"%s\"\n", toChars(value));
                    errorCode=U_INVALID_FORMAT_ERROR;
                    return;
                }
                if(props->type==TYPE_NONE) {
                    props->type=type; // set type for <code-point> or <group> element
                } else if(props->type!=type) {
                    fprintf(stderr, "genudata: type \"%s\" contradicts element name <%s>\n",
                        toChars(value), toChars(cp->getTagName(), 1));
                    errorCode=U_INVALID_FORMAT_ERROR;
                    return;
                }
            } else if(!isGroup && name==s_cp) {
                props->start=props->end=int32FromHex(value, errorCode);
            } else if(isGroup && name==s_first_cp) {
                props->start=int32FromHex(value, errorCode);
            } else if(isGroup && name==s_last_cp) {
                props->end=int32FromHex(value, errorCode);
            } else {
                // look up attribute name as Unicode property alias
                UProperty prop=u_getPropertyEnum(toChars(name));
                if(prop!=UCHAR_INVALID_CODE) {
                    if(prop<UCHAR_BINARY_LIMIT) {
                        props->binProps[prop]=toUBool(value, errorCode);
                    } else if(prop<UCHAR_INT_LIMIT) {
                        int32_t v=u_getPropertyValueEnum(prop, toChars(value, 1));
                        if(v==UCHAR_INVALID_CODE) {
                            if(prop==UCHAR_CANONICAL_COMBINING_CLASS) {
                                // try numeric rather than symbolic values
                                v=int32FromDec(value, errorCode);
                            }
                            if(v==UCHAR_INVALID_CODE) {
                                // unable to parse the property value
                                fprintf(stderr, "genudata: unable to parse property value %s=\"%s\"\n",
                                    toChars(name), toChars(value, 1));
                                // TODO errorCode=U_INVALID_FORMAT_ERROR;
                            }
                        }
                        props->intProps[prop-UCHAR_INT_START]=v;
                    } else if(prop==UCHAR_GENERAL_CATEGORY_MASK) {
                        int32_t v=u_getPropertyValueEnum(prop, toChars(value, 1));

                        // turn mask values into UCharCategory values
                        int32_t bit;

                        for(bit=0; bit<32; ++bit) {
                            if(v&((int32_t)1<<bit)) {
                                v=bit;
                                break;
                            }
                        }
                        props->intProps[UCHAR_GENERAL_CATEGORY-UCHAR_INT_START]=v;
                    } else {
                        // TODO other Unicode property
                        props->binProps[0]=2; // TODO breakpoint opportunity, remove
                    }
                } else {
                    // unknown attribute
                    unknownCPAttributes.add(name);
                }
            }
        }
    }

    void parseBlocks(UErrorCode &errorCode) {
        const UXMLElement *blocks;

        blocks=fRoot->getChildElement(s_blocks);
        if(blocks==NULL) {
            fprintf(stderr, "genudata: missing <blocks>\n");
            // not an error - errorCode=U_INVALID_FORMAT_ERROR;
            return;
        }

        printf("<blocks> has %ld child nodes\n", (long)blocks->countChildren());
    }

    const char *toChars(const UnicodeString &s, int32_t bufferIndex=0) {
        char *buffer=charBuffer[bufferIndex];
        int32_t length=s.extract(0, 0x7fffffff, buffer, (int32_t)sizeof(charBuffer[0]));
        return length<sizeof(charBuffer[0]) ? buffer : "(overflow)";
    }

    int32_t int32FromDec(const UnicodeString &s, UErrorCode &errorCode) {
        if(U_FAILURE(errorCode)) {
            return -1;
        }

        const UChar *buffer;
        int32_t length, result;
        UChar c;

        result=0;
        buffer=s.getBuffer();
        length=s.length();
        if(length>0) {
            for(;;) {
                c=*buffer++;
                if(0x30<=c && c<=0x39) {
                    result=(result*10)+(c-0x30); // 0-9
                } else {
                    break; // error
                }
                if(--length==0) {
                    return result; // done and successful
                }
            }
        }

        // error
        fprintf(stderr, "genudata: not a decimal integer in \"%s\"\n", toChars(s));
        errorCode=U_INVALID_FORMAT_ERROR;
        return -1;
    }

    int32_t int32FromHex(const UnicodeString &s, UErrorCode &errorCode) {
        if(U_FAILURE(errorCode)) {
            return -1;
        }

        const UChar *buffer;
        int32_t length, nibble, result;
        UChar c;

        result=0;
        buffer=s.getBuffer();
        length=s.length();
        if(length>0) {
            for(;;) {
                c=*buffer++;
                if(0x30<=c && c<=0x39) {
                    nibble=c-0x30; // 0-9
                } else if(0x41<=c && c<=0x46) {
                    nibble=c-(0x41-10); // A-F
                } else if(0x61<=c && c<=0x66) {
                    nibble=c-(0x61-10); // a-f
                } else {
                    break; // error
                }
                result=(result<<4)|nibble;
                if(--length==0) {
                    return result; // done and successful
                }
            }
        }

        // error
        fprintf(stderr, "genudata: not a hex integer in \"%s\"\n", toChars(s));
        errorCode=U_INVALID_FORMAT_ERROR;
        return -1;
    }

    UBool toUBool(const UnicodeString &s, UErrorCode &errorCode) {
        if(U_FAILURE(errorCode)) {
            return FALSE;
        }

        const UChar *buffer;
        int32_t length;
        UChar c;

        buffer=s.getBuffer();
        length=s.length();
        if(length>0) {
            c=*buffer;
            if(c==0x59 || c==0x79 || c==0x54 || c==0x74 || c==0x31) {
                return TRUE; // YyTt1
            } else if(c==0x4e || c==0x6e || c==0x46 || c==0x66 || c==0x30) {
                return FALSE; // NnFf0
            }
        }

        // error
        fprintf(stderr, "genudata: not a boolean value in \"%s\"\n", toChars(s));
        errorCode=U_INVALID_FORMAT_ERROR;
        return FALSE;
    }
};

extern int
main(int argc, const char *argv[]) {
    UErrorCode errorCode;

    if(argc<=1) {
        fprintf(stderr,
            "usage: %s pathname-to-ucd.xml\n",
            argv[0]);
        return U_ILLEGAL_ARGUMENT_ERROR;
    }

    errorCode=U_ZERO_ERROR;
    GenUData genUData(errorCode);
    genUData.parseFile(argv[1], errorCode);
    return errorCode;
}
