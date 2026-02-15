#pragma once

#include <cstdint>

static inline int utf8SequenceLen(unsigned char c) {
  if (c < 0x80) return 1;
  if ((c >> 5) == 0x6) return 2;
  if ((c >> 4) == 0xE) return 3;
  if ((c >> 3) == 0x1E) return 4;
  return 0; // invalid lead byte
}

static inline uint32_t utf8NextCodepoint(const unsigned char** s) {
    if (!s || !*s || **s == 0) return 0;

    const unsigned char* p = *s;

    if (*p >= 0xF5 || (*p >= 0xC0 && *p <= 0xC1)) {
        (*s)++;
        return 0xFFFD;
    }

    int len = utf8SequenceLen(*p);

    if (len == 0 || len > 4) {
        (*s)++;
        return 0xFFFD;
    }

    for (int i = 1; i < len; i++) {
        if (p[i] == 0 || (p[i] & 0xC0) != 0x80) {
            (*s)++;
            return 0xFFFD;
        }
    }

    uint32_t cp = (len == 1) ? p[0] : (p[0] & (0x7F >> len));
    for (int i = 1; i < len; i++)
        cp = (cp << 6) | (p[i] & 0x3F);

    if ((len == 2 && cp < 0x80) ||
        (len == 3 && cp < 0x800) ||
        (len == 4 && cp < 0x10000)) {
        *s += len;  // ✅ Must consume the overlong sequence
        return 0xFFFD;
    }

    if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) {
        *s += len;  // ✅ Must consume the invalid sequence
        return 0xFFFD;
    }

    *s += len;
    return cp;
}