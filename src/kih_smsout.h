// KihMMS – building outgoing SMS (SMS-SUBMIT PDUs) from UTF-8 text.
//
// Plain C/C++ (no Arduino), so it can be tested on a PC. The text is sent in the GSM 7-bit alphabet when every character is in it
// (æøå, ÆØÅ, € and [ ] { } via the extension table included: 160 characters per SMS), otherwise as UCS2 (emoji and everything
// else: 70 characters per SMS). Longer texts become a concatenated SMS (UDH with reference, count and part number: 153 or 67
// characters per part) that the phone shows as one message. An emoji (a UTF-16 surrogate pair) or an ESC + character pair is
// never split between two parts.
//
//   KihSmsSubmit s;
//   int parts = s.build("+4712345678", "Hei 😀", ref);
//   for (int i = 0; i < parts; i++) { s.hex(i) is the PDU in hex, s.length(i) is the number for AT+CMGS=<length> }
#pragma once
#include <stdint.h>
#include <string.h>
#include "kih_pdu.h"

struct KihSmsSubmit {
  static const int MAXPARTS = 6;
  char pdus[MAXPARTS][2 * 176 + 1];
  int lens[MAXPARTS];
  int count = 0;
  bool ucs2 = false;

  const char *hex(int i) const { return pdus[i]; }
  int length(int i) const { return lens[i]; }

  // UTF-8 → code points. Returns the number of code points (at most max).
  static int codepoints(const char *s, uint32_t *cp, int max) {
    int n = 0;
    const uint8_t *p = (const uint8_t *)s;
    while (*p && n < max) {
      uint32_t c = *p++;
      int more = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : c >= 0xC0 ? 1 : 0;
      if (more) c &= (0x3F >> more);
      while (more-- && (*p & 0xC0) == 0x80) c = (c << 6) | (*p++ & 0x3F);
      cp[n++] = c;
    }
    return n;
  }

  // The GSM septet(s) for a code point: 1 (basic table), 2 (ESC + extension) or 0 (not in GSM).
  static int toGsm(uint32_t c, uint8_t out[2]) {
    for (int i = 0; i < 128; i++)
      if (i != 0x1B && KihSmsPdu::gsm(i) == c) { out[0] = (uint8_t)i; return 1; }
    for (int i = 0; i < 128; i++)
      if (KihSmsPdu::gsmExt(i) == c && c != '?') { out[0] = 0x1B; out[1] = (uint8_t)i; return 2; }
    return 0;
  }

  // Builds all parts. ref: the reference for a long SMS (0–255, a new one per message). Returns the number of parts (0 = too long).
  int build(const char *number, const char *text, uint8_t ref) {
    static uint32_t cp[1000];
    int n = codepoints(text, cp, 1000);
    // GSM 7-bit or UCS2?
    static uint8_t units[2000];     // septets (GSM) or UTF-16 units as pairs of bytes (UCS2)
    int u = 0;
    ucs2 = false;
    for (int i = 0; i < n && !ucs2; i++) { uint8_t g[2]; int k = toGsm(cp[i], g); if (!k) ucs2 = true; }
    // Units with "may split before this unit" marks: an ESC pair or a surrogate pair is kept together.
    static bool canSplit[2000];
    if (!ucs2) {
      for (int i = 0; i < n; i++) {
        uint8_t g[2];
        int k = toGsm(cp[i], g);
        for (int j = 0; j < k && u < 2000; j++) { canSplit[u] = j == 0; units[u++] = g[j]; }
      }
    } else {
      for (int i = 0; i < n && u < 1996; i++) {
        uint32_t c = cp[i];
        if (c >= 0x10000) {
          c -= 0x10000;
          uint16_t hi = 0xD800 | (c >> 10), lo = 0xDC00 | (c & 0x3FF);
          canSplit[u / 2] = true; units[u++] = hi >> 8; units[u++] = hi & 0xFF;
          canSplit[u / 2] = false; units[u++] = lo >> 8; units[u++] = lo & 0xFF;
        } else {
          canSplit[u / 2] = true; units[u++] = c >> 8; units[u++] = c & 0xFF;
        }
      }
    }
    int total = ucs2 ? u / 2 : u;                       // characters as the network counts them
    int single = ucs2 ? 70 : 160, per = ucs2 ? 67 : 153;
    // Split points.
    int starts[MAXPARTS + 1], parts = 0, at = 0;
    if (total <= single) { starts[0] = 0; parts = 1; }
    else
      while (at < total) {
        if (parts == MAXPARTS) return count = 0;
        starts[parts++] = at;
        int end = at + per;
        if (end < total) while (end > at + 1 && !canSplit[end]) end--;
        at = end < total ? end : total;
      }
    starts[parts] = total;
    for (int p = 0; p < parts; p++) part(p, number, units, starts[p], starts[p + 1], parts, ref, parts > 1);
    return count = parts;
  }

 private:
  void part(int p, const char *number, const uint8_t *units, int from, int to, int parts, uint8_t ref, bool udh) {
    uint8_t b[176];
    int n = 0;
    b[n++] = udh ? 0x51 : 0x11;                         // SMS-SUBMIT, relative validity period, (UDH present)
    b[n++] = 0x00;                                      // message reference: the modem sets it
    const char *d = number;
    bool intl = *d == '+';
    if (intl) d++;
    int digits = 0;
    for (const char *q = d; *q; q++) if (*q >= '0' && *q <= '9') digits++;
    b[n++] = (uint8_t)digits;
    b[n++] = intl ? 0x91 : 0x81;
    int k = 0;
    uint8_t cur = 0;
    for (const char *q = d; *q; q++) {
      if (*q < '0' || *q > '9') continue;
      if (k & 1) { b[n++] = cur | ((*q - '0') << 4); } else cur = (uint8_t)(*q - '0');
      k++;
    }
    if (k & 1) b[n++] = cur | 0xF0;
    b[n++] = 0x00;                                      // protocol identifier
    b[n++] = ucs2 ? 0x08 : 0x00;                        // data coding: UCS2 or GSM 7-bit
    b[n++] = 0xAA;                                      // valid for 4 days
    uint8_t h[6] = { 5, 0x00, 3, ref, (uint8_t)parts, (uint8_t)(p + 1) };
    if (ucs2) {
      int bytes = (to - from) * 2;
      b[n++] = (uint8_t)(bytes + (udh ? 6 : 0));
      if (udh) for (int i = 0; i < 6; i++) b[n++] = h[i];
      for (int i = from * 2; i < to * 2; i++) b[n++] = units[i];
    } else {
      // GSM: the user data length counts septets; the 6-byte UDH takes 7 septets (one fill bit), and the text is packed after it.
      int skip = udh ? 7 : 0;
      int septets = skip + (to - from);
      b[n++] = (uint8_t)septets;
      int start = n;
      int bytes = (septets * 7 + 7) / 8;
      memset(b + start, 0, bytes);
      if (udh) for (int i = 0; i < 6; i++) b[start + i] = h[i];
      for (int i = 0; i < to - from; i++) {
        int bit = (skip + i) * 7, idx = start + (bit >> 3), s = bit & 7;
        b[idx] |= (uint8_t)(units[from + i] << s);
        if (s > 1) b[idx + 1] |= (uint8_t)(units[from + i] >> (8 - s));
      }
      n = start + bytes;
    }
    // Hex with the SMSC byte "00" (use the SIM's service centre) in front; AT+CMGS counts only the TPDU.
    static const char *H = "0123456789ABCDEF";
    char *o = pdus[p];
    *o++ = '0'; *o++ = '0';
    for (int i = 0; i < n; i++) { *o++ = H[b[i] >> 4]; *o++ = H[b[i] & 15]; }
    *o = 0;
    lens[p] = n;
  }
};
