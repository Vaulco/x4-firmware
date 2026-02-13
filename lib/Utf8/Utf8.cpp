#include "Utf8.h"

static int utf8SequenceLen(unsigned char c) {
  if (c < 0x80) return 1;
  if ((c >> 5) == 0x6) return 2;
  if ((c >> 4) == 0xE) return 3;
  if ((c >> 3) == 0x1E) return 4;
  return 0; // invalid lead byte
}

uint32_t utf8NextCodepoint(const unsigned char** s) {
  if (!s || !*s || **s == 0) return 0;

  const unsigned char* p = *s;
  int len = utf8SequenceLen(*p);

  // Validate length
  if (len == 0 || len > 4) return 0xFFFD;

  // Validate continuation bytes
  for (int i = 1; i < len; i++) {
    if (p[i] == 0 || (p[i] & 0xC0) != 0x80) return 0xFFFD;
  }

  // Decode codepoint - extract bits from first byte
  uint32_t cp = (len == 1) ? p[0] : (p[0] & (0x7F >> len));
  for (int i = 1; i < len; i++) {
    cp = (cp << 6) | (p[i] & 0x3F);
  }

  // Reject overlong encodings
  if ((len == 2 && cp < 0x80) ||
      (len == 3 && cp < 0x800) ||
      (len == 4 && cp < 0x10000)) return 0xFFFD;

  // Reject surrogates and out-of-range
  if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) return 0xFFFD;

  *s += len;
  return cp;
}