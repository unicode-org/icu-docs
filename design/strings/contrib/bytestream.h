// Copyright (C) 2009, International Business Machines
// Corporation and others. All Rights Reserved.
//
// Copyright 2007 Google Inc. All Rights Reserved.
// Author: sanjay@google.com (Sanjay Ghemawat)
//
// Abstract interfaces that consumes a sequence of bytes (ByteSink)
// or produce a sequence of bytes (ByteSource).
//
// Used so that we can write a single piece of code that can operate
// on a variety of output string types.
//
// Various implementations of these interfaces are provided:
//   ByteSink:
//      UncheckedArrayByteSink  Write to a flat array, without bounds checking
//      CheckedArrayByteSink    Write to a flat array, with bounds checking
//      GrowingArrayByteSink    Allocate and write to a growable buffer
//      StringByteSink          Write to an STL string
//
//   ByteSource:
//      ArrayByteSource         Reads from flat array or string/StringPiece

#ifndef STRINGS_BYTESTREAM_H__
#define STRINGS_BYTESTREAM_H__

#include <string>
#include "strings/stringpiece.h"
// Application-specific header file includes omitted.

namespace strings {

// A ByteSink can be filled with bytes
class ByteSink {
 public:
  ByteSink() { }
  virtual ~ByteSink() { }

  // Append "bytes[0,n-1]" to this.
  virtual void Append(const char* bytes, size_t n) = 0;

  // Returns a writable buffer for appending and writes the buffer's capacity to
  // *result_capacity. Guarantees *result_capacity>=min_capacity.
  // May return a pointer to the caller-owned scratch buffer which must have
  // scratch_capacity>=min_capacity.
  // The returned buffer is only valid until the next operation
  // on this ByteSink.
  //
  // After writing at most *result_capacity bytes, call Append() with the
  // pointer returned from this function and the number of bytes written.
  // Many Append() implementations will avoid copying bytes if this function
  // returned an internal buffer.
  //
  // Partial usage example:
  //  size_t capacity;
  //  char* buffer = sink->GetAppendBuffer(..., &capacity);
  //  ... Write n bytes into buffer, with n <= capacity.
  //  sink->Append(buffer, n);
  // In many implementations, that call to Append will avoid copying bytes.
  //
  // If the ByteSink allocates or reallocates an internal buffer, it should use
  // the desired_capacity_hint if appropriate.
  // If a caller cannot provide a reasonable guess at the desired capacity,
  // it should pass desired_capacity_hint=0.
  //
  // If a non-scratch buffer is returned, the caller may only pass
  // a prefix to it to Append().
  // That is, it is not correct to pass an interior pointer to Append().
  //
  // The default implementation always returns the scratch buffer.
  virtual char* GetAppendBuffer(size_t min_capacity,
                                size_t desired_capacity_hint,
                                char* scratch, size_t scratch_capacity,
                                size_t* result_capacity);

  // Flush internal buffers.
  // Some byte sinks use internal buffers or provide buffering
  // and require calling Flush() at the end of the stream.
  // The default implementation of Flush() does nothing.
  virtual void Flush();

 private:
  DISALLOW_EVIL_CONSTRUCTORS(ByteSink);
};

// A ByteSource yields a sequence of N bytes
class ByteSource {
 public:
  ByteSource() { }
  virtual ~ByteSource() { }

  // Return the number of bytes left to read from the source
  virtual size_t Available() const = 0;

  // Peek at the next flat region of the source.  Does not reposition
  // the source.  The returned region is empty iff Available()==0.
  //
  // The returned region is valid until the next call to Skip() or
  // until this object is destroyed, whichever occurs first.
  //
  // The returned region may be larger than Available() (for example
  // if this ByteSource is a view on a substring of a larger source).
  // The caller is responsible for ensuring that it only reads the
  // Available() bytes.
  virtual StringPiece Peek() = 0;

  // Skip the next n bytes.  Invalidates any StringPiece returned by
  // a previous call to Peek().
  // REQUIRES: Available() >= n
  virtual void Skip(size_t n) = 0;

  // Writes the next n bytes in "this" to "sink" and advances "this"
  // past the copied bytes.  The default implementation of this method
  // just copies the bytes normally, but subclasses might override
  // CopyTo to optimize certain cases.
  //
  // REQUIRES: Available() >= n
  virtual void CopyTo(ByteSink* sink, size_t n);

  // TODO: Add a "virtual ByteSource* Clone()" method if needed

 private:
  DISALLOW_EVIL_CONSTRUCTORS(ByteSource);
};

// -------------------------------------------------------------
// Some standard implementations

// Implementation of ByteSink that writes to a flat byte array.
// There is no bounds-checking done.  The caller must ensure that
// the destination array is big enough.
class UncheckedArrayByteSink : public ByteSink {
 public:
  explicit UncheckedArrayByteSink(char* dest) : dest_(dest) { }
  virtual void Append(const char* data, size_t n);
  virtual char* GetAppendBuffer(size_t min_capacity,
                                size_t desired_capacity_hint,
                                char* scratch, size_t scratch_capacity,
                                size_t* result_capacity);

  // Return the current output pointer so that a caller can see how
  // many bytes were produced.
  // Note: this is not a ByteSink method.
  char* CurrentDestination() const { return dest_; }

 private:
  char* dest_;
  DISALLOW_EVIL_CONSTRUCTORS(UncheckedArrayByteSink);
};

// Implementation of ByteSink that writes to a flat byte array,
// with bounds-checking:
// This sink will not write more than capacity bytes to outbuf.
// If more than capacity bytes are Append()ed, then excess bytes are ignored,
// and Overflowed() will return true.
// Overflow does not cause a runtime error (CHECK etc.).
class CheckedArrayByteSink : public ByteSink {
 public:
  CheckedArrayByteSink(char* outbuf, size_t capacity);
  virtual void Append(const char* bytes, size_t n);
  virtual char* GetAppendBuffer(size_t min_capacity,
                                size_t desired_capacity_hint,
                                char* scratch, size_t scratch_capacity,
                                size_t* result_capacity);
  // Returns the number of bytes actually written to the sink.
  int NumberOfBytesWritten() const { return size_; }
  // Returns true if any bytes were discarded, i.e., if there was an
  // attempt to write more than 'capacity' bytes.
  bool Overflowed() const { return overflowed_; }
 private:
  char* outbuf_;
  const size_t capacity_;
  size_t size_;
  bool overflowed_;
  DISALLOW_EVIL_CONSTRUCTORS(CheckedArrayByteSink);
};

// A byte sink that allocates an internal buffer (a char array) and
// expands it as needed, similar to a string. Call GetBuffer to get
// the allocated buffer, which the caller must free with
// delete[]. GetBuffer also sets the internal buffer to be empty,
// and subsequent output to this sink will create a new buffer. The
// destructor will free the internal buffer if it is not empty.
class GrowingArrayByteSink : public strings::ByteSink {
 public:
  explicit GrowingArrayByteSink(size_t estimated_size);
  virtual ~GrowingArrayByteSink();
  virtual void Append(const char* bytes, size_t n);
  virtual char* GetAppendBuffer(size_t min_capacity,
                                size_t desired_capacity_hint,
                                char* scratch, size_t scratch_capacity,
                                size_t* result_capacity);
  // Returns the allocated buffer, and sets nbytes to its size.
  char* GetBuffer(size_t* nbytes);

 private:
  void Expand(size_t amount);
  void ShrinkToFit();

  size_t capacity_;
  char* buf_;
  size_t size_;
  DISALLOW_EVIL_CONSTRUCTORS(GrowingArrayByteSink);
};

// Implementation of ByteSink that writes to a "string".
class StringByteSink : public ByteSink {
 public:
  explicit StringByteSink(string* dest) : dest_(dest) { }
  virtual void Append(const char* data, size_t n);
 private:
  string* dest_;
  DISALLOW_EVIL_CONSTRUCTORS(StringByteSink);
};

// Implementation of ByteSource that reads from a flat array of bytes
class ArrayByteSource : public ByteSource {
 public:
  explicit ArrayByteSource(const StringPiece& s) : input_(s) { }

  virtual size_t Available() const;
  virtual StringPiece Peek();
  virtual void Skip(size_t n);
 private:
  StringPiece   input_;

  DISALLOW_EVIL_CONSTRUCTORS(ArrayByteSource);
};

// Implementation of ByteSource that wraps another ByteSource, limiting the
// number of bytes returned.
//
// The caller maintains ownership of the underlying source, and may not use the
// underlying source while using the LimitByteSource object.  The underlying
// source's pointer is advanced by n bytes every time this LimitByteSource
// object is advanced by n.
class LimitByteSource : public ByteSource {
 public:
  // Wrap "source" to return no more than "limit" bytes.
  LimitByteSource(ByteSource *source, size_t limit);

  virtual size_t Available() const;
  virtual StringPiece Peek();
  virtual void Skip(size_t n);

  // We override CopyTo so that we can forward to the underlying source, in
  // case it has an efficient implementation of CopyTo.
  virtual void CopyTo(ByteSink *sink, size_t n);

 private:
  ByteSource *source_;
  size_t limit_;
};

// -------------------------------------------------------------
// Support for custom copiers for particular source/sink type
// combinations.  A CopierMap maps from sink type to a custom
// function to use to copy to that sink type.

class CopierMap {
 public:
  CopierMap(base::LinkerInitialized x);

  typedef void (*Copier)(ByteSource*, ByteSink*, size_t);

  // Arrange to use the specified "function" when copying to a sink
  // with the same type as "sink".
  //
  // All Register() calls must occur before any Get() calls.
  void Register(ByteSink* sink, Copier function);

  // Return the function to use for copying to the specified sink.
  // Returns NULL if no custom function was registered.
  Copier Get(ByteSink* sink) const;

 private:
  static const int kMaxCopiers = 4;
  struct Entry {
    void* sink_vtable;
    Copier function;
  };

  Mutex lock_;
  Entry entry_[kMaxCopiers];

  // HACK: we rely on the ability to extract the vtable pointer to do
  // dynamic type checking.  This differs from other forms of dynamic
  // type checking (such as dynamic_cast, visitor pattern, or a
  // virtual type-cast method) in that if Y is a subclass of X, then a
  // Y object's type is different from an X object's type.  We rely on
  // this property for safety since a copier for class X may not be a
  // valid copier for the subclass Y.
  static void* vtable(ByteSink* object) {
    return *(reinterpret_cast<void**>(object));
  }

  DISALLOW_EVIL_CONSTRUCTORS(CopierMap);
};

inline CopierMap::CopierMap(base::LinkerInitialized x)
    : lock_(base::LINKER_INITIALIZED) {
  // We do not set any fields and instead rely on static initialization
  // to zero out the fields.
}

inline CopierMap::Copier CopierMap::Get(ByteSink* sink) const {
  void* vt = vtable(sink);
  // A little hand-unrolling
  if (entry_[0].sink_vtable == vt) { return entry_[0].function; } 
  if (entry_[1].sink_vtable == vt) { return entry_[1].function; } 
  for (int i = 2; i < kMaxCopiers; i++) {
    if (entry_[i].sink_vtable == vt || entry_[i].sink_vtable == NULL) {
      return entry_[i].function;
    }
  }
  return NULL;
}

}  // end namespace strings

#endif  // STRINGS_BYTESOURCE_H__
