#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <string>
class __FlashStringHelper;
#define F(s) ((const __FlashStringHelper *)(s))

class String {
public:
  std::string s;
  String(const char *c = "") : s(c ? c : "") {}
  String(const std::string &x) : s(x) {}
  const char *c_str() const { return s.c_str(); }
  unsigned length() const { return (unsigned)s.size(); }
  bool startsWith(const char *p) const { return s.rfind(p, 0) == 0; }
  bool operator==(const char *o) const { return s == o; }
  String substring(unsigned a, unsigned b = 0xFFFFFFFF) const {
    if (a > s.size()) return String("");
    return String(s.substr(a, b == 0xFFFFFFFF ? std::string::npos : b - a));
  }
  int toInt() const { return atoi(s.c_str()); }
  int indexOf(char c) const { size_t p = s.find(c); return p == std::string::npos ? -1 : (int)p; }
  int indexOf(const char *c) const { size_t p = s.find(c); return p == std::string::npos ? -1 : (int)p; }
  int lastIndexOf(char c) const { size_t p = s.rfind(c); return p == std::string::npos ? -1 : (int)p; }
  void trim() {
    size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
    s = a == std::string::npos ? "" : s.substr(a, b - a + 1);
  }
};
inline String operator+(const char *a, const String &b) { return String(std::string(a) + b.s); }

class Print {
public:
  virtual ~Print() {}
  virtual size_t write(uint8_t) = 0;
  virtual size_t write(const uint8_t *b, size_t n) { size_t k = 0; while (n--) k += write(*b++); return k; }
  size_t write(const char *s) { return s ? write((const uint8_t *)s, strlen(s)) : 0; }
  size_t print(const char *s) { return write(s); }
  size_t print(char c) { return write((uint8_t)c); }
  size_t print(int v) { char b[16]; snprintf(b, sizeof(b), "%d", v); return write(b); }
  size_t print(unsigned v) { char b[16]; snprintf(b, sizeof(b), "%u", v); return write(b); }
  size_t println(const char *s = "") { size_t n = write(s); return n + write("\n"); }
  size_t print(const __FlashStringHelper *s) { return write((const char *)s); }
  size_t print(const String &s) { return write(s.c_str()); }
  virtual void flush() {}
};
