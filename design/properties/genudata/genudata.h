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

#ifndef __GENUDATA_H__
#define __GENUDATA_H__

#include "unicode/utypes.h"
#include "unicode/uchar.h"

enum {
    TYPE_NONE,
    TYPE_RESERVED,
    TYPE_NONCHARACTER,
    TYPE_SURROGATE,
    TYPE_CHAR,
    TYPE_COUNT
};

struct GenUDataProps {
    UChar32 start, end;
    int32_t type;

    UBool binProps[UCHAR_BINARY_LIMIT-UCHAR_BINARY_START];
    int32_t intProps[UCHAR_INT_LIMIT-UCHAR_INT_START];
};
typedef struct GenUDataProps GenUDataProps;

#endif
