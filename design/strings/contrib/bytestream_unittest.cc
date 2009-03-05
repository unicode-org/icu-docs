// Copyright (C) 2009, International Business Machines
// Corporation and others. All Rights Reserved.
//
// Copyright 2007 Google Inc. All Rights Reserved.
// Author: kenton@google.com (Kenton Varda)

#include "strings/bytestream.h"
// Application-specific header file includes omitted.

namespace strings {
namespace {

// We use this class instead of ArrayByteSource to simulate a ByteSource that
// contains multiple fragments.  ArrayByteSource returns the entire array in
// one fragment.
class MockByteSource : public ByteSource {
 public:
  MockByteSource(const StringPiece& data, int block_size)
    : data_(data), block_size_(block_size) {}

  size_t Available() const { return data_.size(); }
  StringPiece Peek() {
    return data_.substr(0, min(block_size_, data_.size()));
  }
  void Skip(size_t n) { data_.remove_prefix(n); }

 private:
  StringPiece data_;
  int block_size_;
};

TEST(ByteSourceTest, CopyTo) {
  StringPiece data("Hello world!");
  MockByteSource source(data, 3);
  string str;
  StringByteSink sink(&str);

  source.CopyTo(&sink, data.size());
  EXPECT_EQ(data, str);
}

TEST(ByteSourceTest, CopySubstringTo) {
  StringPiece data("Hello world!");
  MockByteSource source(data, 3);
  source.Skip(1);
  string str;
  StringByteSink sink(&str);

  source.CopyTo(&sink, data.size() - 2);
  EXPECT_EQ(data.substr(1, data.size() - 2), str);
  EXPECT_EQ("!", source.Peek());
}

TEST(ByteSourceTest, LimitByteSource) {
  StringPiece data("Hello world!");
  MockByteSource source(data, 3);
  LimitByteSource limit_source(&source, 6);
  EXPECT_EQ(6, limit_source.Available());
  limit_source.Skip(1);
  EXPECT_EQ(5, limit_source.Available());

  {
    string str;
    StringByteSink sink(&str);
    limit_source.CopyTo(&sink, limit_source.Available());
    EXPECT_EQ("ello ", str);
    EXPECT_EQ(0, limit_source.Available());
    EXPECT_EQ(6, source.Available());
  }

  {
    string str;
    StringByteSink sink(&str);
    source.CopyTo(&sink, source.Available());
    EXPECT_EQ("world!", str);
    EXPECT_EQ(0, source.Available());
  }
}

TEST(ByteSinkTest, GetAppendBuffer) {
  char scratch[8];
  string str;
  StringByteSink sink(&str);  // Does not override GetAppendBuffer().
  sink.Append("a", 1);
  size_t capacity = 0;
  char* p = sink.GetAppendBuffer(3, 99, scratch, sizeof(scratch), &capacity);
  EXPECT_EQ(sizeof(scratch), capacity);  // Default always returns scratch.
  EXPECT_EQ(scratch, p);
  strcpy(p, "bc");
  sink.Append(p, 2);  // Do not include the NUL.
  EXPECT_EQ(3, str.length());
  EXPECT_STREQ("abc", str.c_str());
}

TEST(UncheckedArrayByteSinkTest, GetAppendBuffer) {
  char fixed_array[4];
  char scratch[8];
  UncheckedArrayByteSink sink(fixed_array);
  sink.Append("a", 1);
  size_t capacity = 0;
  char* p = sink.GetAppendBuffer(3, 99, scratch, sizeof(scratch), &capacity);
  EXPECT_EQ(99, capacity);  // This sink gives us whatever we ask for.
  EXPECT_EQ(fixed_array + 1, p);
  strcpy(p, "bc");
  sink.Append(p, 3);  // Include the NUL.
  EXPECT_EQ(fixed_array + 4, sink.CurrentDestination());
  EXPECT_STREQ("abc", fixed_array);
}

TEST(CheckedArrayByteSinkTest, GetAppendBuffer) {
  char fixed_array[4];
  char scratch[8];
  CheckedArrayByteSink sink(fixed_array, sizeof(fixed_array));
  sink.Append("a", 1);
  size_t capacity = 0;
  char* p = sink.GetAppendBuffer(3, 99, scratch, sizeof(scratch), &capacity);
  EXPECT_EQ(3, capacity);  // This sink gives us its remaining buffer.
  EXPECT_EQ(fixed_array + 1, p);
  strcpy(p, "bc");
  sink.Append(p, 3);  // Include the NUL.
  EXPECT_EQ(4, sink.NumberOfBytesWritten());
  EXPECT_STREQ("abc", fixed_array);
  EXPECT_FALSE(sink.Overflowed());
  p = sink.GetAppendBuffer(3, 99, scratch, sizeof(scratch), &capacity);
  EXPECT_EQ(sizeof(scratch), capacity);  // The array is full.
  EXPECT_EQ(scratch, p);
  strcpy(p, "de");
  sink.Append(p, 2);  // Anything >0.
  EXPECT_EQ(4, sink.NumberOfBytesWritten());  // No change except Overflowed().
  EXPECT_STREQ("abc", fixed_array);
  EXPECT_TRUE(sink.Overflowed());
}

TEST(GrowingArrayByteSinkTest, GetAppendBuffer) {
  char scratch[40];
  GrowingArrayByteSink sink(4);
  sink.Append("a", 1);
  size_t capacity = 0;
  char* p = sink.GetAppendBuffer(3, 99, scratch, sizeof(scratch), &capacity);
  EXPECT_GE(capacity, 3);  // This sink allocated 4 or more bytes.
  EXPECT_NE(scratch, p);
  memcpy(p, "bcd", 3);  // Do not write the NUL.
  sink.Append(p, 3);
  p = sink.GetAppendBuffer(20, 30, scratch, sizeof(scratch), &capacity);
  EXPECT_GE(capacity, 30);  // Should have triggered expansion (reallocation).
  EXPECT_NE(scratch, p);
  strcpy(p, "efghijklmnopqrstuvw");
  sink.Append(p, 20);  // Include the NUL.
  size_t length;
  p = sink.GetBuffer(&length);
  EXPECT_EQ(24, length);
  EXPECT_STREQ("abcdefghijklmnopqrstuvw", p);
  delete [] p;
  p = sink.GetBuffer(&length);
  EXPECT_EQ(0, length);
  EXPECT_TRUE(p == NULL);
  delete [] p;
}

// Verify that ByteSink is subclassable and Flush() overridable.
class FlushingByteSink : public StringByteSink {
 public:
  FlushingByteSink(string* dest) : StringByteSink(dest) {}
  virtual void Flush() { Append("z", 1); }
 private:
  DISALLOW_EVIL_CONSTRUCTORS(FlushingByteSink);
};

// Write and Flush via the ByteSink superclass interface.
void WriteAndFlush(ByteSink* s) {
  s->Append("abc", 3);
  s->Flush();
}

TEST(ByteSinkTest, Flush) {
  string str;
  FlushingByteSink f_sink(&str);
  WriteAndFlush(&f_sink);
  EXPECT_STREQ("abcz", str.c_str());
}

}  // namespace
}  // namespace strings
