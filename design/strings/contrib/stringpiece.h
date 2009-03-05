// Copyright (C) 2009, International Business Machines
// Corporation and others. All Rights Reserved.
//
// Copyright 2001 and onwards Google Inc.
// Author: Sanjay Ghemawat
//
// A string-like object that points to a sized piece of memory.
//
// Functions or methods may use const StringPiece& parameters to accept either
// a "const char*" or a "string" value that will be implicitly converted to
// a StringPiece.  The implicit conversion means that it is often appropriate
// to include this .h file in other files rather than forward-declaring
// StringPiece as would be appropriate for most other Google classes.
//
// Systematic usage of StringPiece is encouraged as it will reduce unnecessary
// conversions from "const char*" to "string" and back again.
//
//
// Arghh!  I wish C++ literals were "string".

#ifndef STRINGS_STRINGPIECE_H__
#define STRINGS_STRINGPIECE_H__

#include <string.h>
#include <iosfwd>
#include <string>

// Application-specific header file includes omitted.

class StringPiece {
 private:
  const char*   ptr_;
  int           length_;

 public:
  // We provide non-explicit singleton constructors so users can pass
  // in a "const char*" or a "string" wherever a "StringPiece" is
  // expected.
  StringPiece() : ptr_(NULL), length_(0) { }
  StringPiece(const char* str)
    : ptr_(str), length_((str == NULL) ? 0 : static_cast<int>(strlen(str))) { }
  StringPiece(const string& str)
    : ptr_(str.data()), length_(static_cast<int>(str.size())) { }
  StringPiece(const char* offset, int len) : ptr_(offset), length_(len) { }
  // Substring of another StringPiece.
  // pos must be non-negative and <= x.length().
  StringPiece(const StringPiece& x, int pos);
  // Substring of another StringPiece.
  // pos must be non-negative and <= x.length().
  // len must be non-negative and will be pinned to at most x.length() - pos.
  StringPiece(const StringPiece& x, int pos, int len);

  // data() may return a pointer to a buffer with embedded NULs, and the
  // returned buffer may or may not be null terminated.  Therefore it is
  // typically a mistake to pass data() to a routine that expects a NUL
  // terminated string.
  const char* data() const { return ptr_; }
  int size() const { return length_; }
  int length() const { return length_; }
  bool empty() const { return length_ == 0; }

  void clear() { ptr_ = NULL; length_ = 0; }
  void set(const char* data, int len) { ptr_ = data; length_ = len; }
  void set(const char* str) {
    ptr_ = str;
    if (str != NULL)
      length_ = static_cast<int>(strlen(str));
    else
      length_ = 0;
  }
  void set(const void* data, int len) {
    ptr_ = reinterpret_cast<const char*>(data);
    length_ = len;
  }

  char operator[](int i) const { return ptr_[i]; }

  void remove_prefix(int n) {
    ptr_ += n;
    length_ -= n;
  }

  void remove_suffix(int n) {
    length_ -= n;
  }

  int compare(const StringPiece& x) const {
    int r = wordmemcmp(ptr_, x.ptr_, min(length_, x.length_));
    if (r == 0) {
      if (length_ < x.length_) r = -1;
      else if (length_ > x.length_) r = +1;
    }
    return r;
  }

  string as_string() const {
    return string(data(), size());
  }
  // We also define ToString() here, since many other string-like
  // interfaces name the routine that converts to a C++ string
  // "ToString", and it's confusing to have the method that does that
  // for a StringPiece be called "as_string()".  We also leave the
  // "as_string()" method defined here for existing code.
  string ToString() const {
    return string(data(), size());
  }

  void CopyToString(string* target) const;
  void AppendToString(string* target) const;

  // Does "this" start with "x"
  bool starts_with(const StringPiece& x) const {
    return ((length_ >= x.length_) &&
            (wordmemcmp(ptr_, x.ptr_, x.length_) == 0));
  }

  // Does "this" end with "x"
  bool ends_with(const StringPiece& x) const {
    return ((length_ >= x.length_) &&
            (wordmemcmp(ptr_ + (length_-x.length_), x.ptr_, x.length_) == 0));
  }

  // standard STL container boilerplate
  typedef char value_type;
  typedef const char* pointer;
  typedef const char& reference;
  typedef const char& const_reference;
  typedef size_t size_type;
  typedef ptrdiff_t difference_type;
  static const size_type npos;
  typedef const char* const_iterator;
  typedef const char* iterator;
  typedef std::reverse_iterator<const_iterator> const_reverse_iterator;
  typedef std::reverse_iterator<iterator> reverse_iterator;
  iterator begin() const { return ptr_; }
  iterator end() const { return ptr_ + length_; }
  const_reverse_iterator rbegin() const {
    return const_reverse_iterator(ptr_ + length_);
  }
  const_reverse_iterator rend() const {
    return const_reverse_iterator(ptr_);
  }
  // STLS says return size_type, but Google says return int
  int max_size() const { return length_; }
  int capacity() const { return length_; }

  int copy(char* buf, size_type n, size_type pos = 0) const;

  int find(const StringPiece& s, size_type pos = 0) const;
  int find(char c, size_type pos = 0) const;
  int rfind(const StringPiece& s, size_type pos = npos) const;
  int rfind(char c, size_type pos = npos) const;

  int find_first_of(const StringPiece& s, size_type pos = 0) const;
  int find_first_of(char c, size_type pos = 0) const { return find(c, pos); }
  int find_first_not_of(const StringPiece& s, size_type pos = 0) const;
  int find_first_not_of(char c, size_type pos = 0) const;
  int find_last_of(const StringPiece& s, size_type pos = npos) const;
  int find_last_of(char c, size_type pos = npos) const { return rfind(c, pos); }
  int find_last_not_of(const StringPiece& s, size_type pos = npos) const;
  int find_last_not_of(char c, size_type pos = npos) const;

  StringPiece substr(size_type pos, size_type n = npos) const;

  // An optimized version of memcmp(), used by several member
  // functions. Public so the non-friend operator< can use it.
  static int wordmemcmp(const char* p, const char* p2, size_t N) {
    // Compare 8 bytes at a time as long as we can, and then use
    // a small memcmp to do a byte at a time for the rest
    const char* p_limit = p + N;
    while (p + sizeof(uint64) <= p_limit &&
           (UNALIGNED_LOAD64(p) == UNALIGNED_LOAD64(p2))) {
      p += sizeof(uint64);
      p2 += sizeof(uint64);
    }
    N = p_limit - p;
    return memcmp(p, p2, N);
  }
};

bool operator==(const StringPiece& x, const StringPiece& y);

inline bool operator!=(const StringPiece& x, const StringPiece& y) {
  return !(x == y);
}

inline bool operator<(const StringPiece& x, const StringPiece& y) {
  const int r = StringPiece::wordmemcmp(x.data(), y.data(),
                                        min(x.size(), y.size()));
  return ((r < 0) || ((r == 0) && (x.size() < y.size())));
}

inline bool operator>(const StringPiece& x, const StringPiece& y) {
  return y < x;
}

inline bool operator<=(const StringPiece& x, const StringPiece& y) {
  return !(x > y);
}

inline bool operator>=(const StringPiece& x, const StringPiece& y) {
  return !(x < y);
}

// SWIG thinks this is a duplicate definition. Omit it.
#ifndef SWIG
DECLARE_POD(StringPiece);       // So vector<StringPiece> becomes really fast
#endif

// ------------------------------------------------------------------
// Functions used to create STL containers that use StringPiece
//  Remember that a StringPiece's lifetime had better be less than
//  that of the underlying string or char*.  If it is not, then you
//  cannot safely store a StringPiece into an STL container
// ------------------------------------------------------------------

// SWIG doesn't know how to parse this stuff properly. Omit it.
#ifndef SWIG
HASH_NAMESPACE_DECLARATION_START
template<> struct hash<StringPiece> {
  size_t operator()(const StringPiece& s) const;
};
HASH_NAMESPACE_DECLARATION_END
#endif

// allow StringPiece to be logged
extern ostream& operator<<(ostream& o, const StringPiece& piece);

#endif  // STRINGS_STRINGPIECE_H__
