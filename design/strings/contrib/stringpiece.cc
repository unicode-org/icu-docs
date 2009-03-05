// Copyright (C) 2009, International Business Machines
// Corporation and others. All Rights Reserved.
//
// Copyright 2004 and onwards Google Inc.
//
// Author: wilsonh@google.com (Wilson Hsieh)
//

#include "strings/stringpiece.h"

#include <algorithm>
#include <climits>
#include <iostream>

// Application-specific header file includes omitted.

HASH_NAMESPACE_DECLARATION_START
size_t hash<StringPiece>::operator()(const StringPiece& s) const {
  return HashTo32(s.data(), s.size());
}
HASH_NAMESPACE_DECLARATION_END

std::ostream& operator<<(std::ostream& o, const StringPiece& piece) {
  o.write(piece.data(), piece.size());
  return o;
}

bool operator==(const StringPiece& x, const StringPiece& y) {
  int len = x.size();
  if (len != y.size()) {
    return false;
  }
  const char* p = x.data();
  const char* p2 = y.data();
  // Test last byte in case strings share large common prefix
  if ((len > 0) && (p[len-1] != p2[len-1])) return false;
  // At this point we can, but don't have to, ignore the last byte.  We use
  // this observation to fold the odd-length case into the even-length case.
  const char* p_limit = p + (len & ~1);
  while (p + sizeof(uint64) <= p_limit) {
    if ((UNALIGNED_LOAD64(p) != UNALIGNED_LOAD64(p2))) {
      return false;
    }
    p += sizeof(uint64);
    p2 += sizeof(uint64);
  }
  const int N = p_limit - p;
  DCHECK(N == 0 || N == 2 || N == 4 || N == 6);
  if (N >= 4 && UNALIGNED_LOAD32(p) != UNALIGNED_LOAD32(p2)) {
    return false;
  }
  return ((N & 0x2) == 0) ||
      UNALIGNED_LOAD16(p + (N & 0x4)) == UNALIGNED_LOAD16(p2 + (N & 0x4));
}

StringPiece::StringPiece(const StringPiece& x, int pos)
    : ptr_(x.ptr_ + pos), length_(x.length_ - pos) {
  DCHECK_LE(0, pos);
  DCHECK_LE(pos, x.length_);
}

StringPiece::StringPiece(const StringPiece& x, int pos, int len)
    : ptr_(x.ptr_ + pos), length_(min(len, x.length_ - pos)) {
  DCHECK_LE(0, pos);
  DCHECK_LE(pos, x.length_);
  DCHECK_GE(len, 0);
}

void StringPiece::CopyToString(string* target) const {
  target->resize(length_);
  memcpy(&*target->begin(), ptr_, length_);
}

void StringPiece::AppendToString(string* target) const {
  size_t old_size = target->size();
  target->resize(old_size + length_);
  memcpy(&*target->begin() + old_size, ptr_, length_);
}

int StringPiece::copy(char* buf, size_type n, size_type pos) const {
  int ret = min(length_ - pos, n);
  memcpy(buf, ptr_ + pos, ret);
  return ret;
}

int StringPiece::find(const StringPiece& s, size_type pos) const {
  if (length_ < 0 || pos > static_cast<size_type>(length_))
    return npos;

  const char* result = std::search(ptr_ + pos, ptr_ + length_,
                                   s.ptr_, s.ptr_ + s.length_);
  const size_type xpos = result - ptr_;
  return xpos + s.length_ <= length_ ? xpos : npos;
}

int StringPiece::find(char c, size_type pos) const {
  if (length_ <= 0 || pos >= static_cast<size_type>(length_)) {
    return npos;
  }
  const char* result = std::find(ptr_ + pos, ptr_ + length_, c);
  return result != ptr_ + length_ ? result - ptr_ : npos;
}

int StringPiece::rfind(const StringPiece& s, size_type pos) const {
  if (length_ < s.length_) return npos;
  const size_t ulen = length_;
  if (s.length_ == 0) return min(ulen, pos);

  const char* last = ptr_ + min(ulen - s.length_, pos) + s.length_;
  const char* result = std::find_end(ptr_, last, s.ptr_, s.ptr_ + s.length_);
  return result != last ? result - ptr_ : npos;
}

int StringPiece::rfind(char c, size_type pos) const {
  if (length_ <= 0) return npos;
  for (int i = min(pos, static_cast<size_type>(length_ - 1));
       i >= 0; --i) {
    if (ptr_[i] == c) {
      return i;
    }
  }
  return npos;
}

// For each character in characters_wanted, sets the index corresponding
// to the ASCII code of that character to 1 in table.  This is used by
// the find_.*_of methods below to tell whether or not a character is in
// the lookup table in constant time.
// The argument `table' must be an array that is large enough to hold all
// the possible values of an unsigned char.  Thus it should be be declared
// as follows:
//   bool table[UCHAR_MAX + 1]
static inline void BuildLookupTable(const StringPiece& characters_wanted,
                                    bool* table) {
  const int length = characters_wanted.length();
  const char* const data = characters_wanted.data();
  for (int i = 0; i < length; ++i) {
    table[static_cast<unsigned char>(data[i])] = true;
  }
}

int StringPiece::find_first_of(const StringPiece& s, size_type pos) const {
  if (length_ <= 0 || s.length_ <= 0) {
    return npos;
  }
  // Avoid the cost of BuildLookupTable() for a single-character search.
  if (s.length_ == 1) return find_first_of(s.ptr_[0], pos);

  bool lookup[UCHAR_MAX + 1] = { false };
  BuildLookupTable(s, lookup);
  for (int i = pos; i < length_; ++i) {
    if (lookup[static_cast<unsigned char>(ptr_[i])]) {
      return i;
    }
  }
  return npos;
}

int StringPiece::find_first_not_of(const StringPiece& s, size_type pos) const {
  if (length_ <= 0) return npos;
  if (s.length_ <= 0) return 0;
  // Avoid the cost of BuildLookupTable() for a single-character search.
  if (s.length_ == 1) return find_first_not_of(s.ptr_[0], pos);

  bool lookup[UCHAR_MAX + 1] = { false };
  BuildLookupTable(s, lookup);
  for (int i = pos; i < length_; ++i) {
    if (!lookup[static_cast<unsigned char>(ptr_[i])]) {
      return i;
    }
  }
  return npos;
}

int StringPiece::find_first_not_of(char c, size_type pos) const {
  if (length_ <= 0) return npos;

  for (; pos < static_cast<size_type>(length_); ++pos) {
    if (ptr_[pos] != c) {
      return pos;
    }
  }
  return npos;
}

int StringPiece::find_last_of(const StringPiece& s, size_type pos) const {
  if (length_ <= 0 || s.length_ <= 0) return npos;
  // Avoid the cost of BuildLookupTable() for a single-character search.
  if (s.length_ == 1) return find_last_of(s.ptr_[0], pos);

  bool lookup[UCHAR_MAX + 1] = { false };
  BuildLookupTable(s, lookup);
  for (int i = min(pos, static_cast<size_type>(length_ - 1));
       i >= 0; --i) {
    if (lookup[static_cast<unsigned char>(ptr_[i])]) {
      return i;
    }
  }
  return npos;
}

int StringPiece::find_last_not_of(const StringPiece& s, size_type pos) const {
  if (length_ <= 0) return npos;

  int i = min(pos, static_cast<size_type>(length_ - 1));
  if (s.length_ <= 0) return i;

  // Avoid the cost of BuildLookupTable() for a single-character search.
  if (s.length_ == 1) return find_last_not_of(s.ptr_[0], pos);

  bool lookup[UCHAR_MAX + 1] = { false };
  BuildLookupTable(s, lookup);
  for (; i >= 0; --i) {
    if (!lookup[static_cast<unsigned char>(ptr_[i])]) {
      return i;
    }
  }
  return npos;
}

int StringPiece::find_last_not_of(char c, size_type pos) const {
  if (length_ <= 0) return npos;

  for (int i = min(pos, static_cast<size_type>(length_ - 1));
       i >= 0; --i) {
    if (ptr_[i] != c) {
      return i;
    }
  }
  return npos;
}

StringPiece StringPiece::substr(size_type pos, size_type n) const {
  if (pos > length_) pos = length_;
  if (n > length_ - pos) n = length_ - pos;
  return StringPiece(ptr_ + pos, n);
}

const StringPiece::size_type StringPiece::npos = size_type(-1);
