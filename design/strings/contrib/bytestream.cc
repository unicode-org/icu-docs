// Copyright (C) 2009, International Business Machines
// Corporation and others. All Rights Reserved.
//
// Copyright 2007 Google Inc. All Rights Reserved.
// Author: sanjay@google.com (Sanjay Ghemawat)

#include "strings/bytestream.h"
// Application-specific header file includes omitted.

namespace strings {

void CopierMap::Register(ByteSink* sink, Copier function) {
  void* vt = vtable(sink);
  MutexLock l(&lock_);
  for (int i = 0; i < kMaxCopiers; i++) {
    if (entry_[i].sink_vtable == NULL || entry_[i].sink_vtable == vt) {
      entry_[i].sink_vtable = vt;
      entry_[i].function = function;
      return;
    }
  }
  LOG(WARNING) << "CopierMap::Register failed since we are out of slots";
}

void ByteSource::CopyTo(ByteSink* sink, size_t n) {
  while (n > 0) {
    StringPiece fragment = Peek();
    if (fragment.empty()) {
      LOG(DFATAL) << "ByteSource::CopyTo() overran input.";
      break;
    }
    size_t fragment_size = min(n, implicit_cast<size_t>(fragment.size()));
    sink->Append(fragment.data(), fragment_size);
    Skip(fragment_size);
    n -= fragment_size;
  }
}

char* ByteSink::GetAppendBuffer(size_t min_capacity,
                                size_t desired_capacity_hint,
                                char* scratch, size_t scratch_capacity,
                                size_t* result_capacity) {
  CHECK_GE(min_capacity, 1);
  CHECK_GE(scratch_capacity, min_capacity);
  *result_capacity = scratch_capacity;
  return scratch;
}

void ByteSink::Flush() {}

void UncheckedArrayByteSink::Append(const char* data, size_t n) {
  if (data != dest_) {
    // Catch cases where the pointer returned by GetAppendBuffer() was modified.
    DCHECK(!(dest_ <= data && data < (dest_ + n)))
        << "Append() data[] overlaps with dest_[]";
    memcpy(dest_, data, n);
  }
  dest_ += n;
}

char* UncheckedArrayByteSink::GetAppendBuffer(size_t min_capacity,
                                              size_t desired_capacity_hint,
                                              char* scratch,
                                              size_t scratch_capacity,
                                              size_t* result_capacity) {
  CHECK_GE(min_capacity, 1);
  CHECK_GE(scratch_capacity, min_capacity);
  *result_capacity = max(min_capacity, desired_capacity_hint);
  return dest_;
}

CheckedArrayByteSink::CheckedArrayByteSink(char* outbuf, size_t capacity)
    : outbuf_(outbuf), capacity_(capacity), size_(0), overflowed_(false) {
}

void CheckedArrayByteSink::Append(const char* bytes, size_t n) {
  size_t available = capacity_ - size_;
  if (n > available) {
    n = available;
    overflowed_ = true;
  }
  if (n > 0 && bytes != (outbuf_ + size_)) {
    // Catch cases where the pointer returned by GetAppendBuffer() was modified.
    DCHECK(!(outbuf_ <= bytes && bytes < (outbuf_ + capacity_)))
        << "Append() bytes[] overlaps with outbuf_[]";
    memcpy(outbuf_ + size_, bytes, n);
  }
  size_ += n;
}

char* CheckedArrayByteSink::GetAppendBuffer(size_t min_capacity,
                                            size_t desired_capacity_hint,
                                            char* scratch,
                                            size_t scratch_capacity,
                                            size_t* result_capacity) {
  CHECK_GE(min_capacity, 1);
  CHECK_GE(scratch_capacity, min_capacity);
  size_t available = capacity_ - size_;
  if (available >= min_capacity) {
    *result_capacity = available;
    return outbuf_ + size_;
  } else {
    *result_capacity = scratch_capacity;
    return scratch;
  }
}

GrowingArrayByteSink::GrowingArrayByteSink(size_t estimated_size)
    : capacity_(estimated_size),
      buf_(new char[estimated_size]),
      size_(0) {
}

GrowingArrayByteSink::~GrowingArrayByteSink() {
  delete[] buf_;  // Just in case the user didn't call GetBuffer.
}

void GrowingArrayByteSink::Append(const char* bytes, size_t n) {
  size_t available = capacity_ - size_;
  if (bytes != (buf_ + size_)) {
    // Catch cases where the pointer returned by GetAppendBuffer() was modified.
    // We need to test for this before calling Expand() which may reallocate.
    DCHECK(!(buf_ <= bytes && bytes < (buf_ + capacity_)))
        << "Append() bytes[] overlaps with buf_[]";
  }
  if (n > available) {
    Expand(n - available);
  }
  if (n > 0 && bytes != (buf_ + size_)) {
    memcpy(buf_ + size_, bytes, n);
  }
  size_ += n;
}

char* GrowingArrayByteSink::GetAppendBuffer(size_t min_capacity,
                                            size_t desired_capacity_hint,
                                            char* scratch,
                                            size_t scratch_capacity,
                                            size_t* result_capacity) {
  CHECK_GE(min_capacity, 1);
  CHECK_GE(scratch_capacity, min_capacity);
  size_t available = capacity_ - size_;
  if (available < min_capacity) {
    Expand(max(min_capacity, desired_capacity_hint) - available);
    available = capacity_ - size_;
  }
  *result_capacity = available;
  return buf_ + size_;
}

char* GrowingArrayByteSink::GetBuffer(size_t* nbytes) {
  ShrinkToFit();
  char* b = buf_;
  *nbytes = size_;
  buf_ = NULL;
  size_ = capacity_ = 0;
  return b;
}

void GrowingArrayByteSink::Expand(size_t amount) {  // Expand by at least 50%.
  size_t new_capacity = max(capacity_ + amount, (3 * capacity_) / 2);
  char* bigger = new char[new_capacity];
  memcpy(bigger, buf_, size_);
  delete[] buf_;
  buf_ = bigger;
  capacity_ = new_capacity;
}

void GrowingArrayByteSink::ShrinkToFit() {
  // Shrink only if the buffer is large and size_ is less than 3/4
  // of capacity_.
  if (capacity_ > 256 && size_ < (3 * capacity_) / 4) {
    char* just_enough = new char[size_];
    memcpy(just_enough, buf_, size_);
    delete[] buf_;
    buf_ = just_enough;
    capacity_ = size_;
  }
}

void StringByteSink::Append(const char* data, size_t n) {
  dest_->append(data, n);
}

size_t ArrayByteSource::Available() const {
  return input_.size();
}

StringPiece ArrayByteSource::Peek() {
  return input_;
}

void ArrayByteSource::Skip(size_t n) {
  DCHECK_LE(n, input_.size());
  input_.remove_prefix(n);
}

LimitByteSource::LimitByteSource(ByteSource *source, size_t limit)
  : source_(source),
    limit_(limit) {
}

size_t LimitByteSource::Available() const {
  size_t available = source_->Available();
  if (available > limit_) {
    available = limit_;
  }

  return available;
}

StringPiece LimitByteSource::Peek() {
  StringPiece piece(source_->Peek());
  if (piece.size() > limit_) {
    piece.set(piece.data(), limit_);
  }

  return piece;
}

void LimitByteSource::Skip(size_t n) {
  DCHECK_LE(n, limit_);
  source_->Skip(n);
  limit_ -= n;
}

void LimitByteSource::CopyTo(ByteSink *sink, size_t n) {
  DCHECK_LE(n, limit_);
  source_->CopyTo(sink, n);
  limit_ -= n;
}

}  // end namespace strings
