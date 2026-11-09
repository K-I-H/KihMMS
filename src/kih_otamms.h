// KihMMS – OTAMMS, part 2: the receiver. Collects the pieces of a new program from MMS, checks them, installs and restarts.
//
//   KihMms modem;
//   KihOtaMms ota("my secret key", "+4712345678");      // the key the sender signs with, and the only number allowed to send
//   ota.onMessage = [](const char *text) { Serial.println(text); /* e.g. reply by SMS */ };
//   modem.onMms = [](const KihMmsReceived &m) { if (ota.offer(m)) return; /* an ordinary MMS */ };
//
// What it checks before anything is written:
//   - the sender is the allowed number (the last 8 digits; operators write it with or without +country code),
//   - every piece's CRC-32,
//   - the version is higher than the one installed (kept in Preferences "kihota"/"ver"), so an older, validly signed program can
//     not be sent again,
//   - the HMAC-SHA256 over version + the whole program, with the secret key.
// Only then is the program written with Update (from KihOtaMms::loop(), 10 s later) and the board restarted. Pieces of a program are kept in memory (PSRAM when the
// board has it) until all have arrived; pieces from another program (another id) start over. The pieces can come in any order.
#pragma once
#include <Arduino.h>
#include <Update.h>
#include <Preferences.h>
#include <functional>
#include "kih_fwpiece.h"
#include "kih_mmsget.h"

class KihOtaMms {
 public:
  // Status lines for the user ("piece 2 of 3 ✔ (missing 3)", "installing …", errors). Called from loop() context.
  std::function<void(const char *text)> onMessage;
  // Called just before the restart (to close files, say goodbye …).
  std::function<void()> onRestart;

  KihOtaMms(const char *key, const char *allowedNumber) : key(key), allowed(allowedNumber) {}

  // The version this board runs (0 = never updated over MMS).
  static uint32_t installedVersion() {
    Preferences p;
    p.begin("kihota", true);
    uint32_t v = p.getUInt("ver", 0);
    p.end();
    return v;
  }

  // Gives the receiver an MMS. Returns true when it held a program piece (then it is not an ordinary MMS).
  bool offer(const KihMmsReceived &m) {
    bool any = false;
    for (int i = 0; i < m.count; i++) {
      const uint8_t *d = m.partData(i);
      size_t n = m.parts[i].len;
      KihFwPiece p;
      if (p.find(d, n)) { any = true; take(p, m.from.c_str()); continue; }
      // The old format without a version (an old sender app): say so instead of treating it as an ordinary MMS.
      if (n >= 5 && !memcmp(d, "BXFW1", 5)) { any = true; say("old piece format (BXFW1): update the sender app"); continue; }
      // A contact card with the piece as base64 text.
      if (n > 8 && memmem(d, n, "BXFWB64:", 8)) {
        size_t max = n * 3 / 4 + 4;
        uint8_t *raw = (uint8_t *)(psramFound() ? ps_malloc(max) : malloc(max));
        if (!raw) { say("not enough memory for the piece"); any = true; continue; }
        size_t k = KihFwPiece::decodeText((const char *)d, n, raw, max);
        if (k && p.find(raw, k)) { any = true; take(p, m.from.c_str()); }
        free(raw);
      }
    }
    return any;
  }

  // Call in loop(): installs a complete, checked program 10 s after the last piece (time for a reply SMS to go out).
  // canSend: whether onMessage can deliver now (e.g. modem.ready()). After an update over MMS the new program says
  // "new firmware is running" once, as soon as it can.
  void loop(bool canSend = true) {
    if (installAt && millis() - installAt > 10000) install();
    if (canSend && !bootChecked) {
      bootChecked = true;
      Preferences pr;
      pr.begin("kihota", false);
      if (pr.getBool("new", false)) {
        pr.putBool("new", false);
        uint32_t v = pr.getUInt("ver", 0);
        pr.end();
        say(String("new firmware is running (version ") + v + ")");
        return;
      }
      pr.end();
    }
  }

  // How far a program has come: pieces received and needed (0/0 when nothing is going on).
  int received() const { return got; }
  int needed() const { return count; }

 private:
  const char *key;
  const char *allowed;
  uint32_t id = 0, version = 0, total = 0;
  int count = 0, got = 0;
  bool have[64] = { false };
  uint8_t *program = nullptr;
  uint8_t hmac[32];
  uint32_t installAt = 0;
  bool bootChecked = false;

  void say(const String &s) { if (onMessage) onMessage(s.c_str()); }

  static bool sameNumber(const char *a, const char *b) {
    char x[24] = "", y[24] = "";
    int nx = 0, ny = 0;
    for (; *a && nx < 23; a++) if (isdigit((unsigned char)*a)) x[nx++] = *a;
    for (; *b && ny < 23; b++) if (isdigit((unsigned char)*b)) y[ny++] = *b;
    int n = min(8, min(nx, ny));
    return n >= 8 && !memcmp(x + nx - n, y + ny - n, n);
  }

  void reset() {
    if (program) free(program);
    program = nullptr;
    id = version = total = 0;
    count = got = 0;
    memset(have, 0, sizeof have);
  }

  void take(const KihFwPiece &p, const char *from) {
    if (!sameNumber(from, allowed)) { say("program piece from an unknown number: ignored"); return; }
    if (!p.dataOk()) { say(String("piece ") + p.part + " of " + p.count + " is damaged (CRC): send it again"); return; }
    if (p.version <= installedVersion()) { say("this program is not newer than the one installed: ignored"); return; }
    if (p.count > 64) { say("too many pieces"); return; }
    if (p.id != id) {                                    // a new program: start over
      reset();
      program = (uint8_t *)(psramFound() ? ps_malloc(p.total) : malloc(p.total));
      if (!program) { say("not enough memory for the program"); return; }
      id = p.id; version = p.version; total = p.total; count = p.count;
      memcpy(hmac, p.hmac, 32);
    }
    if (p.total != total || p.count != count || p.version != version || memcmp(p.hmac, hmac, 32)) { say("the pieces do not belong together"); return; }
    if (!have[p.part - 1]) {
      memcpy(program + p.start, p.data, p.len);
      have[p.part - 1] = true;
      got++;
    }
    if (got < count) {
      String missing;
      for (int i = 0; i < count; i++) if (!have[i]) missing += (missing.length() ? ", " : "") + String(i + 1);
      say(String("got piece ") + p.part + " of " + count + " ✔ (missing " + missing + ")");
      return;
    }
    if (!p.signatureOk(program, key)) { say("all pieces are here, but the signature is wrong: not installed"); reset(); return; }
    say(String("all ") + count + " pieces are here and the signature is right: installing in a moment");
    installAt = millis();                                // in loop(), a little later, so the message above can be sent first
  }

  void install() {
    installAt = 0;
    if (!Update.begin(total)) { say(String("Update could not start: ") + Update.errorString()); reset(); return; }
    size_t w = Update.write(program, total);
    if (w != total || !Update.end(true)) { say(String("writing the program failed: ") + Update.errorString()); reset(); return; }
    Preferences pr;
    pr.begin("kihota", false);
    pr.putUInt("ver", version);
    pr.putBool("new", true);                             // the new program reports when it runs (loop())
    pr.end();
    say("installed, restarting");
    if (onRestart) onRestart();
    delay(1500);
    ESP.restart();
  }
};
