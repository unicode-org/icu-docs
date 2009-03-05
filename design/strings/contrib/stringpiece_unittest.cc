// Copyright (C) 2009, International Business Machines
// Corporation and others. All Rights Reserved.
//
// Copyright 2003 and onwards Google Inc.
// Author: Sanjay Ghemawat

#include <hash_map>
#include <map>
#include "strings/stringpiece.h"
// Application-specific header file includes omitted.

static void CheckComparisonOperators();
static void CheckSTLHasher();
static void CheckSTLComparator();
static void CheckSTLHash();
static void CheckSTL();
static void CheckCustom();
static void CheckNULL();
static void CheckComparisons2();

int main(int argc, char** argv) {
  InitGoogle(argv[0], &argc, &argv, true);

  CheckComparisonOperators();
  CheckSTLComparator();
  CheckSTLHasher();
  CheckSTLHash();
  CheckSTL();
  CheckCustom();
  CheckNULL();
  CheckComparisons2();

  return RUN_ALL_TESTS();
}

static void CheckSTLHash() {
  hash<StringPiece> hasher;

  StringPiece s1("foo");
  StringPiece s2("bar");
  StringPiece s3("baz");
  StringPiece s4("zot");

  CHECK(hasher(s1) != hasher(s2));
  CHECK(hasher(s2) != hasher(s3));
  CHECK(hasher(s3) != hasher(s4));

  CHECK(hasher(s1) == hasher(s1));
  CHECK(hasher(s2) == hasher(s2));
  CHECK(hasher(s3) == hasher(s3));
}

static void CheckSTLComparator() {
  string s1("foo");
  string s2("bar");
  string s3("baz");

  StringPiece p1(s1);
  StringPiece p2(s2);
  StringPiece p3(s3);

  typedef map<StringPiece, int> TestMap;
  TestMap map;

  map.insert(make_pair(p1, 0));
  map.insert(make_pair(p2, 1));
  map.insert(make_pair(p3, 2));
  CHECK(map.size() == 3);

  TestMap::const_iterator iter = map.begin();
  CHECK(iter->second == 1);
  ++iter;
  CHECK(iter->second == 2);
  ++iter;
  CHECK(iter->second == 0);
  ++iter;
  CHECK(iter == map.end());

  TestMap::iterator new_iter = map.find("zot");
  CHECK(new_iter == map.end());

  new_iter = map.find("bar");
  CHECK(new_iter != map.end());

  map.erase(new_iter);
  CHECK(map.size() == 2);

  iter = map.begin();
  CHECK(iter->second == 2);
  ++iter;
  CHECK(iter->second == 0);
  ++iter;
  CHECK(iter == map.end());
}

static void CheckSTLHasher() {
  string s1("foo");
  string s2("bar");
  string s3("baz");

  StringPiece p1(s1);
  StringPiece p2(s2);
  StringPiece p3(s3);

  typedef hash_map<StringPiece, int> TestMap;
  TestMap map;

  map.insert(make_pair(p1, 0));
  map.insert(make_pair(p2, 1));
  map.insert(make_pair(p3, 2));
  CHECK(map.size() == 3);

  bool found[3] = { false, false, false };
  for (TestMap::const_iterator iter = map.begin();
       iter != map.end(); ++iter) {
    int x = iter->second;
    CHECK(x >= 0 && x < 3);
    CHECK(! found[x]);
    found[x] = true;
  }
  CHECK(found[0] == true);
  CHECK(found[1] == true);
  CHECK(found[2] == true);

  TestMap::iterator new_iter = map.find("zot");
  CHECK(new_iter == map.end());

  new_iter = map.find("bar");
  CHECK(new_iter != map.end());

  map.erase(new_iter);
  CHECK(map.size() == 2);

  found[0] = false;
  found[1] = false;
  found[2] = false;
  for (TestMap::const_iterator iter = map.begin();
       iter != map.end(); ++iter) {
    int x = iter->second;
    CHECK(x >= 0 && x < 3);
    CHECK(! found[x]);
    found[x] = true;
  }
  CHECK(found[0] == true);
  CHECK(found[1] == false);
  CHECK(found[2] == true);
}

static void CheckComparisonOperators() {
#define CMP_Y(op, x, y)                                         \
  CHECK( (StringPiece((x)) op StringPiece((y))))                \
    << #x "=" << x << " " #y "=" << y;                          \
  CHECK( (StringPiece((x)).compare(StringPiece((y))) op 0))     \
           << #x "=" << x << " " #y "=" << y

#define CMP_N(op, x, y)                                         \
  CHECK(!(StringPiece((x)) op StringPiece((y))));               \
  CHECK(!(StringPiece((x)).compare(StringPiece((y))) op 0))

  CMP_Y(==, "",   "");
  CMP_Y(==, "a",  "a");
  CMP_Y(==, "aa", "aa");
  CMP_N(==, "a",  "");
  CMP_N(==, "",   "a");
  CMP_N(==, "a",  "b");
  CMP_N(==, "a",  "aa");
  CMP_N(==, "aa", "a");

  CMP_N(!=, "",   "");
  CMP_N(!=, "a",  "a");
  CMP_N(!=, "aa", "aa");
  CMP_Y(!=, "a",  "");
  CMP_Y(!=, "",   "a");
  CMP_Y(!=, "a",  "b");
  CMP_Y(!=, "a",  "aa");
  CMP_Y(!=, "aa", "a");

  CMP_Y(<, "a",  "b");
  CMP_Y(<, "a",  "aa");
  CMP_Y(<, "aa", "b");
  CMP_Y(<, "aa", "bb");
  CMP_N(<, "a",  "a");
  CMP_N(<, "b",  "a");
  CMP_N(<, "aa", "a");
  CMP_N(<, "b",  "aa");
  CMP_N(<, "bb", "aa");

  CMP_Y(<=, "a",  "a");
  CMP_Y(<=, "a",  "b");
  CMP_Y(<=, "a",  "aa");
  CMP_Y(<=, "aa", "b");
  CMP_Y(<=, "aa", "bb");
  CMP_N(<=, "b",  "a");
  CMP_N(<=, "aa", "a");
  CMP_N(<=, "b",  "aa");
  CMP_N(<=, "bb", "aa");

  CMP_N(>=, "a",  "b");
  CMP_N(>=, "a",  "aa");
  CMP_N(>=, "aa", "b");
  CMP_N(>=, "aa", "bb");
  CMP_Y(>=, "a",  "a");
  CMP_Y(>=, "b",  "a");
  CMP_Y(>=, "aa", "a");
  CMP_Y(>=, "b",  "aa");
  CMP_Y(>=, "bb", "aa");

  CMP_N(>, "a",  "a");
  CMP_N(>, "a",  "b");
  CMP_N(>, "a",  "aa");
  CMP_N(>, "aa", "b");
  CMP_N(>, "aa", "bb");
  CMP_Y(>, "b",  "a");
  CMP_Y(>, "aa", "a");
  CMP_Y(>, "b",  "aa");
  CMP_Y(>, "bb", "aa");

  string x;
  for (int i = 0; i < 256; i++) {
    x += 'a';
    string y = x;
    CMP_Y(==, x, y);
    for (int j = 0; j < i; j++) {
      string z = x;
      z[j] = 'b';       // Differs in position 'j'
      CMP_N(==, x, z);
      CMP_Y(<, x, z);
      CMP_Y(>, z, x);
      if (j + 1 < i) {
        z[j + 1] = 'A';  // Differs in position 'j+1' as well
        CMP_N(==, x, z);
        CMP_Y(<, x, z);
        CMP_Y(>, z, x);
        z[j + 1] = 'z';  // Differs in position 'j+1' as well
        CMP_N(==, x, z);
        CMP_Y(<, x, z);
        CMP_Y(>, z, x);
      }
    }
  }

#undef CMP_Y
#undef CMP_N
}

static void CheckSTL() {
  StringPiece a("abcdefghijklmnopqrstuvwxyz");
  StringPiece b("abc");
  StringPiece c("xyz");
  StringPiece d("foobar");
  StringPiece e;
  string temp("123");
  temp += '\0';
  temp += "456";
  StringPiece f(temp);

  CHECK_EQ(a[6], 'g');
  CHECK_EQ(b[0], 'a');
  CHECK_EQ(c[2], 'z');
  CHECK_EQ(f[3], '\0');
  CHECK_EQ(f[5], '5');

  CHECK_EQ(*d.data(), 'f');
  CHECK_EQ(d.data()[5], 'r');
  CHECK(e.data() == NULL);

  CHECK_EQ(*a.begin(), 'a');
  CHECK_EQ(*(b.begin() + 2), 'c');
  CHECK_EQ(*(c.end() - 1), 'z');

  CHECK_EQ(*a.rbegin(), 'z');
  CHECK_EQ(*(b.rbegin() + 2), 'a');
  CHECK_EQ(*(c.rend() - 1), 'x');
  CHECK(a.rbegin() + 26 == a.rend());

  CHECK_EQ(a.size(), 26);
  CHECK_EQ(b.size(), 3);
  CHECK_EQ(c.size(), 3);
  CHECK_EQ(d.size(), 6);
  CHECK_EQ(e.size(), 0);
  CHECK_EQ(f.size(), 7);

  CHECK(!d.empty());
  CHECK(d.begin() != d.end());
  CHECK(d.begin() + 6 == d.end());

  CHECK(e.empty());
  CHECK(e.begin() == e.end());

  d.clear();
  CHECK_EQ(d.size(), 0);
  CHECK(d.empty());
  CHECK(d.data() == NULL);
  CHECK(d.begin() == d.end());

  CHECK_GE(a.max_size(), a.capacity());
  CHECK_GE(a.capacity(), a.size());

  char buf[4] = { '%', '%', '%', '%' };
  CHECK_EQ(a.copy(buf, 4), 4);
  CHECK_EQ(buf[0], a[0]);
  CHECK_EQ(buf[1], a[1]);
  CHECK_EQ(buf[2], a[2]);
  CHECK_EQ(buf[3], a[3]);
  CHECK_EQ(a.copy(buf, 3, 7), 3);
  CHECK_EQ(buf[0], a[7]);
  CHECK_EQ(buf[1], a[8]);
  CHECK_EQ(buf[2], a[9]);
  CHECK_EQ(buf[3], a[3]);
  CHECK_EQ(c.copy(buf, 99), 3);
  CHECK_EQ(buf[0], c[0]);
  CHECK_EQ(buf[1], c[1]);
  CHECK_EQ(buf[2], c[2]);
  CHECK_EQ(buf[3], a[3]);

  CHECK_EQ(StringPiece::npos, string::npos);

  CHECK_EQ(a.find(b), 0);
  CHECK_EQ(a.find(b, 1), StringPiece::npos);
  CHECK_EQ(a.find(c), 23);
  CHECK_EQ(a.find(c, 9), 23);
  CHECK_EQ(a.find(c, StringPiece::npos), StringPiece::npos);
  CHECK_EQ(b.find(c), StringPiece::npos);
  CHECK_EQ(b.find(c, StringPiece::npos), StringPiece::npos);
  CHECK_EQ(a.find(d), 0);
  CHECK_EQ(a.find(e), 0);
  CHECK_EQ(a.find(d, 12), 12);
  CHECK_EQ(a.find(e, 17), 17);
  StringPiece g("xx not found bb");
  CHECK_EQ(a.find(g), StringPiece::npos);
  // empty string nonsense
  CHECK_EQ(d.find(b), StringPiece::npos);
  CHECK_EQ(e.find(b), StringPiece::npos);
  CHECK_EQ(d.find(b, 4), StringPiece::npos);
  CHECK_EQ(e.find(b, 7), StringPiece::npos);

  size_t empty_search_pos = string().find(string());
  CHECK_EQ(d.find(d), empty_search_pos);
  CHECK_EQ(d.find(e), empty_search_pos);
  CHECK_EQ(e.find(d), empty_search_pos);
  CHECK_EQ(e.find(e), empty_search_pos);
  CHECK_EQ(d.find(d, 4), string().find(string(), 4));
  CHECK_EQ(d.find(e, 4), string().find(string(), 4));
  CHECK_EQ(e.find(d, 4), string().find(string(), 4));
  CHECK_EQ(e.find(e, 4), string().find(string(), 4));

  CHECK_EQ(a.find('a'), 0);
  CHECK_EQ(a.find('c'), 2);
  CHECK_EQ(a.find('z'), 25);
  CHECK_EQ(a.find('$'), StringPiece::npos);
  CHECK_EQ(a.find('\0'), StringPiece::npos);
  CHECK_EQ(f.find('\0'), 3);
  CHECK_EQ(f.find('3'), 2);
  CHECK_EQ(f.find('5'), 5);
  CHECK_EQ(g.find('o'), 4);
  CHECK_EQ(g.find('o', 4), 4);
  CHECK_EQ(g.find('o', 5), 8);
  CHECK_EQ(a.find('b', 5), StringPiece::npos);
  // empty string nonsense
  CHECK_EQ(d.find('\0'), StringPiece::npos);
  CHECK_EQ(e.find('\0'), StringPiece::npos);
  CHECK_EQ(d.find('\0', 4), StringPiece::npos);
  CHECK_EQ(e.find('\0', 7), StringPiece::npos);
  CHECK_EQ(d.find('x'), StringPiece::npos);
  CHECK_EQ(e.find('x'), StringPiece::npos);
  CHECK_EQ(d.find('x', 4), StringPiece::npos);
  CHECK_EQ(e.find('x', 7), StringPiece::npos);

  CHECK_EQ(a.rfind(b), 0);
  CHECK_EQ(a.rfind(b, 1), 0);
  CHECK_EQ(a.rfind(c), 23);
  CHECK_EQ(a.rfind(c, 22), StringPiece::npos);
  CHECK_EQ(a.rfind(c, 1), StringPiece::npos);
  CHECK_EQ(a.rfind(c, 0), StringPiece::npos);
  CHECK_EQ(b.rfind(c), StringPiece::npos);
  CHECK_EQ(b.rfind(c, 0), StringPiece::npos);
  CHECK_EQ(a.rfind(d), a.as_string().rfind(string()));
  CHECK_EQ(a.rfind(e), a.as_string().rfind(string()));
  CHECK_EQ(a.rfind(d, 12), 12);
  CHECK_EQ(a.rfind(e, 17), 17);
  CHECK_EQ(a.rfind(g), StringPiece::npos);
  CHECK_EQ(d.rfind(b), StringPiece::npos);
  CHECK_EQ(e.rfind(b), StringPiece::npos);
  CHECK_EQ(d.rfind(b, 4), StringPiece::npos);
  CHECK_EQ(e.rfind(b, 7), StringPiece::npos);
  // empty string nonsense
  CHECK_EQ(d.rfind(d, 4), string().rfind(string()));
  CHECK_EQ(e.rfind(d, 7), string().rfind(string()));
  CHECK_EQ(d.rfind(e, 4), string().rfind(string()));
  CHECK_EQ(e.rfind(e, 7), string().rfind(string()));
  CHECK_EQ(d.rfind(d), string().rfind(string()));
  CHECK_EQ(e.rfind(d), string().rfind(string()));
  CHECK_EQ(d.rfind(e), string().rfind(string()));
  CHECK_EQ(e.rfind(e), string().rfind(string()));

  CHECK_EQ(g.rfind('o'), 8);
  CHECK_EQ(g.rfind('q'), StringPiece::npos);
  CHECK_EQ(g.rfind('o', 8), 8);
  CHECK_EQ(g.rfind('o', 7), 4);
  CHECK_EQ(g.rfind('o', 3), StringPiece::npos);
  CHECK_EQ(f.rfind('\0'), 3);
  CHECK_EQ(f.rfind('\0', 12), 3);
  CHECK_EQ(f.rfind('3'), 2);
  CHECK_EQ(f.rfind('5'), 5);
  // empty string nonsense
  CHECK_EQ(d.rfind('o'), StringPiece::npos);
  CHECK_EQ(e.rfind('o'), StringPiece::npos);
  CHECK_EQ(d.rfind('o', 4), StringPiece::npos);
  CHECK_EQ(e.rfind('o', 7), StringPiece::npos);

  CHECK_EQ(a.find_first_of(b), 0);
  CHECK_EQ(a.find_first_of(b, 0), 0);
  CHECK_EQ(a.find_first_of(b, 1), 1);
  CHECK_EQ(a.find_first_of(b, 2), 2);
  CHECK_EQ(a.find_first_of(b, 3), StringPiece::npos);
  CHECK_EQ(a.find_first_of(c), 23);
  CHECK_EQ(a.find_first_of(c, 23), 23);
  CHECK_EQ(a.find_first_of(c, 24), 24);
  CHECK_EQ(a.find_first_of(c, 25), 25);
  CHECK_EQ(a.find_first_of(c, 26), StringPiece::npos);
  CHECK_EQ(g.find_first_of(b), 13);
  CHECK_EQ(g.find_first_of(c), 0);
  CHECK_EQ(a.find_first_of(f), StringPiece::npos);
  CHECK_EQ(f.find_first_of(a), StringPiece::npos);
  // empty string nonsense
  CHECK_EQ(a.find_first_of(d), StringPiece::npos);
  CHECK_EQ(a.find_first_of(e), StringPiece::npos);
  CHECK_EQ(d.find_first_of(b), StringPiece::npos);
  CHECK_EQ(e.find_first_of(b), StringPiece::npos);
  CHECK_EQ(d.find_first_of(d), StringPiece::npos);
  CHECK_EQ(e.find_first_of(d), StringPiece::npos);
  CHECK_EQ(d.find_first_of(e), StringPiece::npos);
  CHECK_EQ(e.find_first_of(e), StringPiece::npos);

  CHECK_EQ(a.find_first_not_of(b), 3);
  CHECK_EQ(a.find_first_not_of(c), 0);
  CHECK_EQ(b.find_first_not_of(a), StringPiece::npos);
  CHECK_EQ(c.find_first_not_of(a), StringPiece::npos);
  CHECK_EQ(f.find_first_not_of(a), 0);
  CHECK_EQ(a.find_first_not_of(f), 0);
  CHECK_EQ(a.find_first_not_of(d), 0);
  CHECK_EQ(a.find_first_not_of(e), 0);
  // empty string nonsense
  CHECK_EQ(d.find_first_not_of(a), StringPiece::npos);
  CHECK_EQ(e.find_first_not_of(a), StringPiece::npos);
  CHECK_EQ(d.find_first_not_of(d), StringPiece::npos);
  CHECK_EQ(e.find_first_not_of(d), StringPiece::npos);
  CHECK_EQ(d.find_first_not_of(e), StringPiece::npos);
  CHECK_EQ(e.find_first_not_of(e), StringPiece::npos);

  StringPiece h("====");
  CHECK_EQ(h.find_first_not_of('='), StringPiece::npos);
  CHECK_EQ(h.find_first_not_of('=', 3), StringPiece::npos);
  CHECK_EQ(h.find_first_not_of('\0'), 0);
  CHECK_EQ(g.find_first_not_of('x'), 2);
  CHECK_EQ(f.find_first_not_of('\0'), 0);
  CHECK_EQ(f.find_first_not_of('\0', 3), 4);
  CHECK_EQ(f.find_first_not_of('\0', 2), 2);
  // empty string nonsense
  CHECK_EQ(d.find_first_not_of('x'), StringPiece::npos);
  CHECK_EQ(e.find_first_not_of('x'), StringPiece::npos);
  CHECK_EQ(d.find_first_not_of('\0'), StringPiece::npos);
  CHECK_EQ(e.find_first_not_of('\0'), StringPiece::npos);

  //  StringPiece g("xx not found bb");
  StringPiece i("56");
  CHECK_EQ(h.find_last_of(a), StringPiece::npos);
  CHECK_EQ(g.find_last_of(a), g.size()-1);
  CHECK_EQ(a.find_last_of(b), 2);
  CHECK_EQ(a.find_last_of(c), a.size()-1);
  CHECK_EQ(f.find_last_of(i), 6);
  CHECK_EQ(a.find_last_of('a'), 0);
  CHECK_EQ(a.find_last_of('b'), 1);
  CHECK_EQ(a.find_last_of('z'), 25);
  CHECK_EQ(a.find_last_of('a', 5), 0);
  CHECK_EQ(a.find_last_of('b', 5), 1);
  CHECK_EQ(a.find_last_of('b', 0), StringPiece::npos);
  CHECK_EQ(a.find_last_of('z', 25), 25);
  CHECK_EQ(a.find_last_of('z', 24), StringPiece::npos);
  CHECK_EQ(f.find_last_of(i, 5), 5);
  CHECK_EQ(f.find_last_of(i, 6), 6);
  CHECK_EQ(f.find_last_of(a, 4), StringPiece::npos);
  // empty string nonsense
  CHECK_EQ(f.find_last_of(d), StringPiece::npos);
  CHECK_EQ(f.find_last_of(e), StringPiece::npos);
  CHECK_EQ(f.find_last_of(d, 4), StringPiece::npos);
  CHECK_EQ(f.find_last_of(e, 4), StringPiece::npos);
  CHECK_EQ(d.find_last_of(d), StringPiece::npos);
  CHECK_EQ(d.find_last_of(e), StringPiece::npos);
  CHECK_EQ(e.find_last_of(d), StringPiece::npos);
  CHECK_EQ(e.find_last_of(e), StringPiece::npos);
  CHECK_EQ(d.find_last_of(f), StringPiece::npos);
  CHECK_EQ(e.find_last_of(f), StringPiece::npos);
  CHECK_EQ(d.find_last_of(d, 4), StringPiece::npos);
  CHECK_EQ(d.find_last_of(e, 4), StringPiece::npos);
  CHECK_EQ(e.find_last_of(d, 4), StringPiece::npos);
  CHECK_EQ(e.find_last_of(e, 4), StringPiece::npos);
  CHECK_EQ(d.find_last_of(f, 4), StringPiece::npos);
  CHECK_EQ(e.find_last_of(f, 4), StringPiece::npos);

  CHECK_EQ(a.find_last_not_of(b), a.size()-1);
  CHECK_EQ(a.find_last_not_of(c), 22);
  CHECK_EQ(b.find_last_not_of(a), StringPiece::npos);
  CHECK_EQ(b.find_last_not_of(b), StringPiece::npos);
  CHECK_EQ(f.find_last_not_of(i), 4);
  CHECK_EQ(a.find_last_not_of(c, 24), 22);
  CHECK_EQ(a.find_last_not_of(b, 3), 3);
  CHECK_EQ(a.find_last_not_of(b, 2), StringPiece::npos);
  // empty string nonsense
  CHECK_EQ(f.find_last_not_of(d), f.size()-1);
  CHECK_EQ(f.find_last_not_of(e), f.size()-1);
  CHECK_EQ(f.find_last_not_of(d, 4), 4);
  CHECK_EQ(f.find_last_not_of(e, 4), 4);
  CHECK_EQ(d.find_last_not_of(d), StringPiece::npos);
  CHECK_EQ(d.find_last_not_of(e), StringPiece::npos);
  CHECK_EQ(e.find_last_not_of(d), StringPiece::npos);
  CHECK_EQ(e.find_last_not_of(e), StringPiece::npos);
  CHECK_EQ(d.find_last_not_of(f), StringPiece::npos);
  CHECK_EQ(e.find_last_not_of(f), StringPiece::npos);
  CHECK_EQ(d.find_last_not_of(d, 4), StringPiece::npos);
  CHECK_EQ(d.find_last_not_of(e, 4), StringPiece::npos);
  CHECK_EQ(e.find_last_not_of(d, 4), StringPiece::npos);
  CHECK_EQ(e.find_last_not_of(e, 4), StringPiece::npos);
  CHECK_EQ(d.find_last_not_of(f, 4), StringPiece::npos);
  CHECK_EQ(e.find_last_not_of(f, 4), StringPiece::npos);

  CHECK_EQ(h.find_last_not_of('x'), h.size() - 1);
  CHECK_EQ(h.find_last_not_of('='), StringPiece::npos);
  CHECK_EQ(b.find_last_not_of('c'), 1);
  CHECK_EQ(h.find_last_not_of('x', 2), 2);
  CHECK_EQ(h.find_last_not_of('=', 2), StringPiece::npos);
  CHECK_EQ(b.find_last_not_of('b', 1), 0);
  // empty string nonsense
  CHECK_EQ(d.find_last_not_of('x'), StringPiece::npos);
  CHECK_EQ(e.find_last_not_of('x'), StringPiece::npos);
  CHECK_EQ(d.find_last_not_of('\0'), StringPiece::npos);
  CHECK_EQ(e.find_last_not_of('\0'), StringPiece::npos);

  CHECK_EQ(a.substr(0, 3), b);
  CHECK_EQ(a.substr(23), c);
  CHECK_EQ(a.substr(23, 3), c);
  CHECK_EQ(a.substr(23, 99), c);
  CHECK_EQ(a.substr(0), a);
  CHECK_EQ(a.substr(3, 2), "de");
  // empty string nonsense
  CHECK_EQ(a.substr(99, 2), e);
  CHECK_EQ(d.substr(99), e);
  CHECK_EQ(d.substr(0, 99), e);
  CHECK_EQ(d.substr(99, 99), e);

  // Substring constructors.
  CHECK_EQ(StringPiece(a, 0, 3), b);
  CHECK_EQ(StringPiece(a, 23), c);
  CHECK_EQ(StringPiece(a, 23, 3), c);
  CHECK_EQ(StringPiece(a, 23, 99), c);
  CHECK_EQ(StringPiece(a, 0), a);
  CHECK_EQ(StringPiece(a, 3, 2), "de");
  // empty string nonsense
  CHECK_EQ(StringPiece(d, 0, 99), e);
  // Verify that they work taking an actual string, not just a StringPiece.
  string a2 = a.as_string();
  CHECK_EQ(StringPiece(a2, 0, 3), b);
  CHECK_EQ(StringPiece(a2, 23), c);
  CHECK_EQ(StringPiece(a2, 23, 3), c);
  CHECK_EQ(StringPiece(a2, 23, 99), c);
  CHECK_EQ(StringPiece(a2, 0), a);
  CHECK_EQ(StringPiece(a2, 3, 2), "de");
}

static void CheckCustom() {
  StringPiece a("foobar");
  string s1("123");
  s1 += '\0';
  s1 += "456";
  StringPiece b(s1);
  StringPiece e;
  string s2;

  // CopyToString
  a.CopyToString(&s2);
  CHECK_EQ(s2.size(), 6);
  CHECK_EQ(s2, "foobar");
  b.CopyToString(&s2);
  CHECK_EQ(s2.size(), 7);
  CHECK_EQ(s1, s2);
  e.CopyToString(&s2);
  CHECK(s2.empty());

  // AppendToString
  s2.erase();
  a.AppendToString(&s2);
  CHECK_EQ(s2.size(), 6);
  CHECK_EQ(s2, "foobar");
  a.AppendToString(&s2);
  CHECK_EQ(s2.size(), 12);
  CHECK_EQ(s2, "foobarfoobar");

  // starts_with
  CHECK(a.starts_with(a));
  CHECK(a.starts_with("foo"));
  CHECK(a.starts_with(e));
  CHECK(b.starts_with(s1));
  CHECK(b.starts_with(b));
  CHECK(b.starts_with(e));
  CHECK(e.starts_with(""));
  CHECK(!a.starts_with(b));
  CHECK(!b.starts_with(a));
  CHECK(!e.starts_with(a));

  // ends with
  CHECK(a.ends_with(a));
  CHECK(a.ends_with("bar"));
  CHECK(a.ends_with(e));
  CHECK(b.ends_with(s1));
  CHECK(b.ends_with(b));
  CHECK(b.ends_with(e));
  CHECK(e.ends_with(""));
  CHECK(!a.ends_with(b));
  CHECK(!b.ends_with(a));
  CHECK(!e.ends_with(a));

  // remove_prefix
  StringPiece c(a);
  c.remove_prefix(3);
  CHECK_EQ(c, "bar");
  c = a;
  c.remove_prefix(0);
  CHECK_EQ(c, a);
  c.remove_prefix(c.size());
  CHECK_EQ(c, e);

  // remove_suffix
  c = a;
  c.remove_suffix(3);
  CHECK_EQ(c, "foo");
  c = a;
  c.remove_suffix(0);
  CHECK_EQ(c, a);
  c.remove_suffix(c.size());
  CHECK_EQ(c, e);

  // set
  c.set("foobar", 6);
  CHECK_EQ(c, a);
  c.set("foobar", 0);
  CHECK_EQ(c, e);
  c.set("foobar", 7);
  CHECK_NE(c, a);

  c.set("foobar");
  CHECK_EQ(c, a);

  c.set(static_cast<const void*>("foobar"), 6);
  CHECK_EQ(c, a);
  c.set(static_cast<const void*>("foobar"), 0);
  CHECK_EQ(c, e);
  c.set(static_cast<const void*>("foobar"), 7);
  CHECK_NE(c, a);

  // as_string
  string s3(a.as_string().c_str(), 7);
  CHECK_EQ(c, s3);
  string s4(e.as_string());
  CHECK(s4.empty());

  // ToString
  {
    string s3(a.ToString().c_str(), 7);
    CHECK_EQ(c, s3);
    string s4(e.ToString());
    CHECK(s4.empty());
  }
}

static void CheckNULL() {
  // we used to crash here, but now we don't.
  StringPiece s(NULL);
  CHECK_EQ(s.data(), (const char*)NULL);
  CHECK_EQ(s.size(), 0);

  s.set(NULL);
  CHECK_EQ(s.data(), (const char*)NULL);
  CHECK_EQ(s.size(), 0);
}

static void CheckComparisons2() {
  StringPiece abc("abcdefghijklmnopqrstuvwxyz");

  // check comparison operations on strings longer than 4 bytes.
  CHECK(abc == StringPiece("abcdefghijklmnopqrstuvwxyz"));
  CHECK(abc.compare(StringPiece("abcdefghijklmnopqrstuvwxyz")) == 0);

  CHECK(abc < StringPiece("abcdefghijklmnopqrstuvwxzz"));
  CHECK(abc.compare(StringPiece("abcdefghijklmnopqrstuvwxzz")) < 0);

  CHECK(abc > StringPiece("abcdefghijklmnopqrstuvwxyy"));
  CHECK(abc.compare(StringPiece("abcdefghijklmnopqrstuvwxyy")) > 0);

  // starts_with
  CHECK(abc.starts_with(abc));
  CHECK(abc.starts_with("abcdefghijklm"));
  CHECK(!abc.starts_with("abcdefguvwxyz"));

  // ends_with
  CHECK(abc.ends_with(abc));
  CHECK(!abc.ends_with("abcdefguvwxyz"));
  CHECK(abc.ends_with("nopqrstuvwxyz"));
}

TEST(ComparisonOpsTest, StringCompareNotAmbiguous) {
  EXPECT_TRUE("hello" == string("hello"));
  EXPECT_TRUE("hello" < string("world"));
}

TEST(ComparisonOpsTest, HeterogenousStringPieceEquals) {
  EXPECT_TRUE(StringPiece("hello") == string("hello"));
  EXPECT_TRUE("hello" == StringPiece("hello"));
}
