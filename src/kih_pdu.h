// KihMMS – the byte-level parts: building an MMS (M-Send.req), decoding received SMS in PDU mode, and reading MMS headers/parts.
//
// Everything in this file is plain C/C++ (no Arduino, no String), so it can be compiled and tested on a PC
// (see extras/test/pdu_test.cpp). The modem driver (kih_modem.h) and the PPP transfer (kih_ppp.h) use these.
//
// References: OMA-WAP-MMS-ENC (MMS encapsulation, v1.2), WAP-230-WSP (WSP encoding of headers),
// 3GPP TS 23.040 (SMS PDU) and 3GPP TS 23.038 (GSM 7-bit alphabet).
#pragma once
#include <stdint.h>
#include <string.h>
#include <stdio.h>

// ---------------------------------------------------------------------------------------------------------------------------------
// KihMmsBuilder: an M-Send.req, the binary message that is HTTP-POSTed to the operator's MMSC.
//
// Why build it ourselves? The A76xx modems on the LilyGO boards have no AT+CMMS* commands (we tried: ERROR), so there is nothing
// in the modem that makes MMS for us. An MMS is "just" a WSP-encoded header block followed by a multipart body, so we write the
// bytes directly into a buffer. Layout:
//   8C 80                 X-Mms-Message-Type = m-send-req
//   98 "id" 00            X-Mms-Transaction-ID (any unique text)
//   8D 92                 X-Mms-MMS-Version = 1.2
//   89 01 81              From = insert-address-token (the MMSC fills in our number)
//   97 "+47.../TYPE=PLMN" To
//   96 ...                Subject (encoded-string-value, UTF-8 when not plain ASCII)
//   84 A3                 Content-Type = application/vnd.wap.multipart.mixed
//   uintvar(parts) then for each part: uintvar(header length) uintvar(data length) header data
// ---------------------------------------------------------------------------------------------------------------------------------
struct KihMmsBuilder {
  uint8_t *b;
  int n, max;
  KihMmsBuilder(uint8_t *buf, int m) : b(buf), n(0), max(m) {}

  void u8(uint8_t x) { if (n < max) b[n++] = x; }
  void text(const char *s) { while (*s) u8((uint8_t)*s++); u8(0); }

  // WSP uintvar: 7 bits per byte, most significant first, high bit set on all but the last byte.
  void uintvar(uint32_t v) {
    uint8_t t[5];
    int k = 0;
    do { t[k++] = v & 0x7F; v >>= 7; } while (v);
    while (k--) u8(t[k] | (k ? 0x80 : 0));
  }

  static bool ascii(const char *s) { for (; *s; s++) if ((uint8_t)*s >= 0x80) return false; return true; }

  // Encoded-string-value: plain ASCII as-is. Anything else gets a value-length and the charset UTF-8 (MIBenum 106 = 0xEA as a short
  // integer) in front, otherwise phones show æøå and emoji as garbage in the subject.
  void encodedText(const char *s) {
    if (ascii(s)) { text(s); return; }
    int l = (int)strlen(s) + 2;
    if (l < 31) u8((uint8_t)l); else { u8(31); uintvar((uint32_t)l); }
    u8(0xEA);
    text(s);
  }

  // Builds the whole message. The text part is always there (UTF-8). The attachment is optional: a JPEG by default
  // (well-known content type 0x9E), or any type given as text, e.g. "image/png", "audio/midi" or "application/octet-stream".
  // Returns the length, or -1 when the buffer was too small.
  int build(const char *to, const char *subject, const char *body, const uint8_t *att, int attLen, const char *id,
            const char *type = nullptr, const char *name = nullptr) {
    n = 0;
    u8(0x8C); u8(0x80);
    u8(0x98); text(id);
    u8(0x8D); u8(0x92);
    u8(0x89); u8(0x01); u8(0x81);
    char a[80];
    snprintf(a, sizeof(a), "%s/TYPE=PLMN", to);
    u8(0x97); text(a);
    if (subject && *subject) { u8(0x96); encodedText(subject); }
    u8(0x84); u8(0xA3);
    uintvar(att && attLen > 0 ? 2 : 1);
    // Text part header: value-length 3, text/plain (0x83), charset (0x81) = UTF-8 (0xEA), then Content-Location (0x8E) "text.txt".
    int tl = (int)strlen(body);
    static const uint8_t hdr[] = { 0x03, 0x83, 0x81, 0xEA, 0x8E, 't', 'e', 'x', 't', '.', 't', 'x', 't', 0 };
    uintvar(sizeof(hdr));
    uintvar((uint32_t)tl);
    for (unsigned i = 0; i < sizeof(hdr); i++) u8(hdr[i]);
    for (int i = 0; i < tl; i++) u8((uint8_t)body[i]);
    if (att && attLen > 0) {
      const char *file = name ? name : "picture.jpg";
      int hl = (type ? (int)strlen(type) + 1 : 1) + 1 + (int)strlen(file) + 1;
      uintvar((uint32_t)hl);
      uintvar((uint32_t)attLen);
      if (type) text(type); else u8(0x9E);
      u8(0x8E); text(file);
      for (int i = 0; i < attLen; i++) u8(att[i]);
    }
    return n < max ? n : -1;
  }
};

// ---------------------------------------------------------------------------------------------------------------------------------
// KihSmsPdu: one received SMS in PDU mode (the hex line after +CMT/+CMGR/+CMGL), decoded to UTF-8.
//
// Why PDU mode and not the friendlier text mode (AT+CMGF=1)? Because MMS notifications are binary SMS (WAP push, 8-bit data on
// port 2948). In text mode the modem turns them into an unreadable mess and the MMS address is lost. In PDU mode we get every byte,
// and then we must decode ordinary text messages ourselves too: the GSM 7-bit alphabet (with æøå and the extension table for € [ ] …),
// UCS2 (emoji arrive as UTF-16 surrogate pairs) and 8-bit data.
// ---------------------------------------------------------------------------------------------------------------------------------
struct KihSmsPdu {
  char from[48];
  char text[640];
  uint8_t data[200];
  int dataLen;
  bool binary;
  int ref, count, part;                      // long SMS (UDH IE 00 or 08): reference, number of parts, this part (1..count)

  static int hx(char c) { return c <= '9' ? c - '0' : (c | 32) - 'a' + 10; }

  static void utf8(char *u, int &n, int max, uint32_t cp) {
    char t[4]; int k = 0;
    if (cp < 0x80) t[k++] = (char)cp;
    else if (cp < 0x800) { t[k++] = (char)(0xC0 | (cp >> 6)); t[k++] = (char)(0x80 | (cp & 0x3F)); }
    else if (cp < 0x10000) { t[k++] = (char)(0xE0 | (cp >> 12)); t[k++] = (char)(0x80 | ((cp >> 6) & 0x3F)); t[k++] = (char)(0x80 | (cp & 0x3F)); }
    else { t[k++] = (char)(0xF0 | (cp >> 18)); t[k++] = (char)(0x80 | ((cp >> 12) & 0x3F)); t[k++] = (char)(0x80 | ((cp >> 6) & 0x3F)); t[k++] = (char)(0x80 | (cp & 0x3F)); }
    if (n + k >= max) return;
    for (int i = 0; i < k; i++) u[n++] = t[i];
    u[n] = 0;
  }

  // GSM 03.38 default alphabet. Note that it is NOT ASCII: '@' is 0x00, '$' is 0x02, 'Å' is 0x0E and so on.
  static uint16_t gsm(int c) {
    static const uint16_t t[128] = {
      0x40, 0xA3, 0x24, 0xA5, 0xE8, 0xE9, 0xF9, 0xEC, 0xF2, 0xC7, 0x0A, 0xD8, 0xF8, 0x0D, 0xC5, 0xE5,
      0x394, 0x5F, 0x3A6, 0x393, 0x39B, 0x3A9, 0x3A0, 0x3A8, 0x3A3, 0x398, 0x39E, 0x1B, 0xC6, 0xE6, 0xDF, 0xC9,
      0x20, 0x21, 0x22, 0x23, 0xA4, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
      0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
      0xA1, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
      0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0xC4, 0xD6, 0xD1, 0xDC, 0xA7,
      0xBF, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F,
      0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0xE4, 0xF6, 0xF1, 0xFC, 0xE0 };
    return t[c & 0x7F];
  }
  // The extension table: ESC (0x1B) followed by one of these.
  static uint16_t gsmExt(int c) {
    switch (c) { case 0x0A: return 0x0C; case 0x14: return '^'; case 0x28: return '{'; case 0x29: return '}'; case 0x2F: return '\\';
      case 0x3C: return '['; case 0x3D: return '~'; case 0x3E: return ']'; case 0x40: return '|'; case 0x65: return 0x20AC; }
    return '?';
  }
  // Septets number «from» up to «to» of packed GSM text in b → UTF-8.
  static void gsmText(const uint8_t *b, int from, int to, char *u, int max) {
    int n = 0; u[0] = 0;
    bool esc = false;
    for (int k = from; k < to; k++) {
      int bit = k * 7, i = bit >> 3, s = bit & 7;
      int c = ((b[i] | (b[i + 1] << 8)) >> s) & 0x7F;
      if (esc) { utf8(u, n, max, gsmExt(c)); esc = false; }
      else if (c == 0x1B) esc = true;
      else utf8(u, n, max, gsm(c));
    }
  }

  // Parses one SMS-DELIVER PDU given as hex. Returns false for anything too short to be one.
  bool parse(const char *h) {
    static uint8_t b[200];
    int n = 0;
    while (h[0] && h[1] && n < 199) { b[n++] = (uint8_t)(hx(h[0]) * 16 + hx(h[1])); h += 2; }
    b[n] = 0;
    from[0] = 0; text[0] = 0; dataLen = 0; binary = false;
    if (n < 12) return false;
    int i = 1 + b[0];                        // skip the SMSC address
    int fo = b[i++];                         // first octet: bit 6 = UDHI (a user data header is present)
    int oaLen = b[i++], toa = b[i++], oaB = (oaLen + 1) / 2;
    if ((toa & 0x70) == 0x50) gsmText(b + i, 0, oaLen * 4 / 7, from, sizeof(from));   // alphanumeric sender, e.g. "Ice"
    else {
      int k = 0;
      if ((toa & 0x70) == 0x10) from[k++] = '+';                                     // international number
      for (int d = 0; d < oaLen && k < 46; d++) { int x = d & 1 ? b[i + d / 2] >> 4 : b[i + d / 2] & 0x0F; if (x < 10) from[k++] = (char)('0' + x); }
      from[k] = 0;
    }
    i += oaB;
    i++;                                     // PID
    int dcs = b[i++];
    i += 7;                                  // service centre time stamp
    int udl = b[i++];
    int alpha = 0;                           // 0 GSM 7-bit, 1 8-bit, 2 UCS2
    if ((dcs & 0xC0) == 0 || (dcs & 0xC0) == 0x40) alpha = (dcs >> 2) & 3;
    else if ((dcs & 0xF0) == 0xF0) alpha = (dcs & 4) ? 1 : 0;
    else if ((dcs & 0xF0) == 0xE0) alpha = 2;
    if (alpha == 3) alpha = 1;
    int udh = (fo & 0x40) ? b[i] + 1 : 0;
    ref = 0; count = 1; part = 1;
    for (int k = 1; k + 1 < udh; k += 2 + b[i + k + 1]) {
      int ie = b[i + k], l = b[i + k + 1];
      if (ie == 0x00 && l == 3) { ref = b[i + k + 2]; count = b[i + k + 3]; part = b[i + k + 4]; }
      if (ie == 0x08 && l == 4) { ref = (b[i + k + 2] << 8) | b[i + k + 3]; count = b[i + k + 4]; part = b[i + k + 5]; }
    }
    // With GSM 7-bit, UDL counts septets and the header is padded to a septet boundary, hence (udh*8+6)/7.
    if (alpha == 0) gsmText(b + i, (udh * 8 + 6) / 7, udl, text, sizeof(text));
    else if (alpha == 1) {
      binary = true;
      for (int k = udh; k < udl && i + k < n && dataLen < (int)sizeof(data); k++) data[dataLen++] = b[i + k];
    } else {
      int u = 0;
      for (int k = udh; k + 1 < udl && i + k + 1 < n; k += 2) {
        uint32_t c = (b[i + k] << 8) | b[i + k + 1];
        if (c >= 0xD800 && c <= 0xDBFF && k + 3 < udl) {          // surrogate pair = emoji
          uint32_t l = (b[i + k + 2] << 8) | b[i + k + 3];
          c = 0x10000 + (((c & 0x3FF) << 10) | (l & 0x3FF));
          k += 2;
        }
        utf8(text, u, sizeof(text), c);
      }
    }
    return true;
  }
};

// ---------------------------------------------------------------------------------------------------------------------------------
// KihMmsParser: the headers of an MMS notification (m-notification-ind, inside the WAP push SMS) and of a fetched MMS
// (m-retrieve-conf), plus the list of parts (content type, name and where the bytes are).
//
// WSP header values carry their own length (short integer, text, value-length …), so unknown headers can be skipped safely.
// ---------------------------------------------------------------------------------------------------------------------------------
struct KihMmsPart { char type[48]; char name[64]; int start, len; };

struct KihMmsParser {
  char from[48], subject[120], url[200], tid[64];
  int type;                                  // 0x82 notification, 0x84 retrieved message
  int body;                                  // first byte after Content-Type (the parts), or -1

  static uint32_t uintvar(const uint8_t *p, int n, int &i) { uint32_t v = 0; uint8_t x; do { if (i >= n) return v; x = p[i++]; v = (v << 7) | (x & 0x7F); } while (x & 0x80); return v; }
  static void copy(char *out, int max, const uint8_t *p, int n, int &i) {
    int k = 0;
    if (i < n && p[i] == 0x7F) i++;          // quote character in front of texts that start with a high byte
    while (i < n && p[i]) { if (k < max - 1) out[k++] = (char)p[i]; i++; }
    out[k] = 0; i++;
  }
  static void skip(const uint8_t *p, int n, int &i) {
    if (i >= n) return;
    uint8_t b = p[i];
    if (b >= 0x80) { i++; return; }                                        // short integer
    if (b <= 30) { i += b + 1; return; }                                   // value-length
    if (b == 31) { i++; uint32_t l = uintvar(p, n, i); i += (int)l; return; }
    while (i < n && p[i]) i++;                                             // text
    i++;
  }
  // Encoded-string-value: text, or value-length + charset + text.
  static void encoded(char *out, int max, const uint8_t *p, int n, int &i) {
    if (i < n && p[i] < 0x20) {
      int l;
      if (p[i] == 31) { i++; l = (int)uintvar(p, n, i); } else l = p[i++];
      int end = i + l;
      if (i < n && p[i] >= 0x80) i++; else skip(p, n, i);                  // the charset
      copy(out, max, p, n, i);
      i = end;
      return;
    }
    copy(out, max, p, n, i);
  }
  static void typeText(char *out, int max, int code) {
    const char *t = "application/octet-stream";
    switch (code) { case 0x03: t = "text/plain"; break; case 0x1D: t = "image/gif"; break; case 0x1E: t = "image/jpeg"; break;
      case 0x20: t = "image/png"; break; case 0x23: t = "multipart/mixed"; break; case 0x33: t = "multipart/related"; break; }
    snprintf(out, max, "%s", t);
  }
  static void readType(char *out, int max, const uint8_t *p, int n, int &i) {
    if (i >= n) { out[0] = 0; return; }
    uint8_t b = p[i];
    if (b >= 0x80) { typeText(out, max, b & 0x7F); i++; return; }
    if (b < 32) {
      int l;
      if (b == 31) { i++; l = (int)uintvar(p, n, i); } else { i++; l = b; }
      int s = i;
      readType(out, max, p, s + l < n ? s + l : n, i);
      i = s + l;
      return;
    }
    copy(out, max, p, n, i);
  }
  bool parse(const uint8_t *p, int n, int from0) {
    from[0] = subject[0] = url[0] = tid[0] = 0; type = 0; body = -1;
    int i = from0;
    while (i < n) {
      uint8_t f = p[i++];
      if (f == 0x8C) { type = i < n ? p[i++] : 0; continue; }
      if (f == 0x98) { copy(tid, sizeof(tid), p, n, i); continue; }
      if (f == 0x83) { copy(url, sizeof(url), p, n, i); continue; }
      if (f == 0x96) { encoded(subject, sizeof(subject), p, n, i); continue; }
      if (f == 0x89) {
        int l;
        if (i < n && p[i] == 31) { i++; l = (int)uintvar(p, n, i); } else l = i < n ? p[i++] : 0;
        int end = i + l;
        if (i < n && p[i] == 0x80) { i++; encoded(from, sizeof(from), p, n, i); }
        char *s = strstr(from, "/TYPE=");
        if (s) *s = 0;
        i = end;
        continue;
      }
      if (f == 0x84) { char t[48]; readType(t, sizeof(t), p, n, i); body = i; break; }
      if (f >= 0x80) skip(p, n, i);
      else { while (i < n && p[i]) i++; i++; skip(p, n, i); }              // unknown header with a text name
    }
    return type != 0;
  }
  // The notification sits in the user data after a WSP push header we don't need: look for m-notification-ind (8C 82).
  bool notification(const uint8_t *p, int n) {
    for (int k = 0; k + 1 < n; k++) if (p[k] == 0x8C && p[k + 1] == 0x82) return parse(p, n, k) && url[0];
    return false;
  }
  // The parts of a fetched message (after parse()).
  int parts(const uint8_t *p, int n, KihMmsPart *out, int max) const {
    if (body < 0) return 0;
    int i = body;
    int cnt = (int)uintvar(p, n, i), k = 0;
    for (; k < cnt && k < max && i < n; k++) {
      int hl = (int)uintvar(p, n, i), dl = (int)uintvar(p, n, i);
      int h = i;
      readType(out[k].type, sizeof(out[k].type), p, h + hl, i);
      out[k].name[0] = 0;
      // The name: Content-Location (0x8E), Content-ID (0xC0) or name (0x85) – the first readable one is good enough.
      for (int j = i; j < h + hl; j++) if (p[j] == 0x8E || p[j] == 0x85 || p[j] == 0xC0) {
        int a = j + 1;
        if (a < h + hl && p[a] == '"') a++;
        if (a < h + hl && p[a] == '<') a++;
        int m = 0;
        while (a < h + hl && p[a] && p[a] != '>' && m < 63) out[k].name[m++] = (char)p[a++];
        out[k].name[m] = 0;
        if (m) break;
      }
      out[k].start = h + hl;
      out[k].len = dl;
      i = h + hl + dl;
    }
    return k;
  }
};
