// KihMMS – OTAMMS, part 1: the piece format and the signature (plain C++, no Arduino, testable on a PC).
//
// A new program for the board is split into pieces that travel as MMS attachments. Each piece starts with a header:
//
//   "BXFW2"      5 bytes  marker (format 2: with a version)
//   id           4        the same for all pieces of one program (so pieces of different programs are never mixed)
//   version      4        must be higher than the version the board runs (protection against going back to an older program)
//   part, count  2 + 2    this piece (1..count) and the number of pieces
//   total        4        the size of the whole program
//   start, len   4 + 4    where this piece goes in the program, and its length
//   crc32        4        CRC-32 of this piece's data
//   hmac        32        HMAC-SHA256(key, version (4 bytes) + whole program), the same in every piece
//   data       len
//
// All numbers are little endian. The piece can sit anywhere inside an attachment (the sender app wraps it in a playable MIDI
// file), so KihFwPiece::find() searches for the marker. It can also arrive as text: "BXFWB64:" followed by base64 (in a contact
// card, for messaging apps that refuse unknown attachments); KihFwPiece::decodeText() turns that back into bytes.
// The signature uses a secret key that only the sender and the board know: without it, nobody can make a program the board accepts.
#pragma once
#include <stdint.h>
#include <string.h>

// SHA-256 (FIPS 180-4) and HMAC-SHA256 (RFC 2104).
struct KihSha256 {
  uint32_t h[8];
  uint8_t block[64];
  uint64_t bits;
  int used;

  static uint32_t ror(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

  void begin() {
    static const uint32_t init[8] = { 0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19 };
    memcpy(h, init, sizeof h);
    bits = 0;
    used = 0;
  }

  void compress() {
    static const uint32_t k[64] = {
      0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
      0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
      0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
      0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
      0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
      0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2 };
    uint32_t w[64];
    for (int i = 0; i < 16; i++) w[i] = (uint32_t)block[i * 4] << 24 | (uint32_t)block[i * 4 + 1] << 16 | (uint32_t)block[i * 4 + 2] << 8 | block[i * 4 + 3];
    for (int i = 16; i < 64; i++) {
      uint32_t s0 = ror(w[i - 15], 7) ^ ror(w[i - 15], 18) ^ (w[i - 15] >> 3);
      uint32_t s1 = ror(w[i - 2], 17) ^ ror(w[i - 2], 19) ^ (w[i - 2] >> 10);
      w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
    for (int i = 0; i < 64; i++) {
      uint32_t t1 = hh + (ror(e, 6) ^ ror(e, 11) ^ ror(e, 25)) + ((e & f) ^ (~e & g)) + k[i] + w[i];
      uint32_t t2 = (ror(a, 2) ^ ror(a, 13) ^ ror(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
      hh = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
    }
    h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
  }

  void add(const uint8_t *p, size_t n) {
    bits += (uint64_t)n * 8;
    while (n--) { block[used++] = *p++; if (used == 64) { compress(); used = 0; } }
  }

  void end(uint8_t out[32]) {
    uint64_t total = bits;
    uint8_t one = 0x80, zero = 0;
    add(&one, 1);
    while (used != 56) add(&zero, 1);
    for (int i = 7; i >= 0; i--) { block[used++] = (uint8_t)(total >> (i * 8)); }
    compress();
    for (int i = 0; i < 8; i++) { out[i * 4] = (uint8_t)(h[i] >> 24); out[i * 4 + 1] = (uint8_t)(h[i] >> 16); out[i * 4 + 2] = (uint8_t)(h[i] >> 8); out[i * 4 + 3] = (uint8_t)h[i]; }
  }

  // HMAC-SHA256(key, a + b): «a» and «b» are hashed one after the other (here: the version, then the program).
  static void hmac(const uint8_t *key, size_t keyLen, const uint8_t *a, size_t aLen, const uint8_t *b, size_t bLen, uint8_t out[32]) {
    uint8_t k[64] = { 0 };
    if (keyLen > 64) { KihSha256 s; s.begin(); s.add(key, keyLen); s.end(k); } else memcpy(k, key, keyLen);
    uint8_t pad[64], inner[32];
    for (int i = 0; i < 64; i++) pad[i] = k[i] ^ 0x36;
    KihSha256 s; s.begin(); s.add(pad, 64); s.add(a, aLen); s.add(b, bLen); s.end(inner);
    for (int i = 0; i < 64; i++) pad[i] = k[i] ^ 0x5c;
    s.begin(); s.add(pad, 64); s.add(inner, 32); s.end(out);
  }
};

struct KihFwPiece {
  static const int HEADER = 5 + 4 + 4 + 2 + 2 + 4 + 4 + 4 + 4 + 32;
  uint32_t id, version, total, start, len, crc;
  int part, count;
  uint8_t hmac[32];
  const uint8_t *data;              // points into the attachment the piece was found in

  static uint32_t crc32(const uint8_t *p, size_t n, uint32_t c = 0) {
    c = ~c;
    while (n--) { c ^= *p++; for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1))); }
    return ~c;
  }
  static uint32_t u32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
  static int u16(const uint8_t *p) { return p[0] | p[1] << 8; }

  // Finds a piece anywhere in «d». Returns false when there is none, or when it is cut off.
  bool find(const uint8_t *d, size_t n) {
    for (size_t i = 0; i + HEADER <= n; i++) {
      if (memcmp(d + i, "BXFW2", 5)) continue;
      const uint8_t *p = d + i + 5;
      id = u32(p); version = u32(p + 4); part = u16(p + 8); count = u16(p + 10);
      total = u32(p + 12); start = u32(p + 16); len = u32(p + 20); crc = u32(p + 24);
      memcpy(hmac, p + 28, 32);
      data = d + i + HEADER;
      if (i + HEADER + len <= n && part >= 1 && part <= count && start + len <= total) return true;
    }
    return false;
  }

  // Is this piece's data what the sender sent (CRC-32)?
  bool dataOk() const { return crc32(data, len) == crc; }

  // "BXFWB64:" + base64 somewhere in «text» → the bytes in «out». Returns the number of bytes (0 = not found).
  // Line breaks inside the base64 (vCard folding: CR LF + space) are skipped.
  static size_t decodeText(const char *text, size_t n, uint8_t *out, size_t max) {
    const char *m = nullptr;
    for (size_t i = 0; i + 8 <= n; i++) if (!memcmp(text + i, "BXFWB64:", 8)) { m = text + i + 8; break; }
    if (!m) return 0;
    uint32_t acc = 0;
    int bitsHeld = 0;
    size_t o = 0;
    for (const char *p = m; p < text + n && o < max; p++) {
      char c = *p;
      int v;
      if (c >= 'A' && c <= 'Z') v = c - 'A';
      else if (c >= 'a' && c <= 'z') v = c - 'a' + 26;
      else if (c >= '0' && c <= '9') v = c - '0' + 52;
      else if (c == '+') v = 62;
      else if (c == '/') v = 63;
      else if (c == '\r' || c == '\n' || c == ' ') continue;
      else break;                                          // '=' or the end of the note
      acc = acc << 6 | v;
      bitsHeld += 6;
      if (bitsHeld >= 8) { bitsHeld -= 8; out[o++] = (uint8_t)(acc >> bitsHeld); }
    }
    return o;
  }

  // Does the whole program (all pieces put together) carry the right signature for «key»?
  bool signatureOk(const uint8_t *program, const char *key) const {
    uint8_t v[4] = { (uint8_t)version, (uint8_t)(version >> 8), (uint8_t)(version >> 16), (uint8_t)(version >> 24) };
    uint8_t h[32];
    KihSha256::hmac((const uint8_t *)key, strlen(key), v, 4, program, total, h);
    uint8_t diff = 0;
    for (int i = 0; i < 32; i++) diff |= h[i] ^ hmac[i];  // compare without stopping at the first difference
    return diff == 0;
  }
};
