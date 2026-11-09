// KihMMS – receiving SMS: stored on the SIM, read and deleted, long SMS joined.
//
//   KihAt at;  KihPower power(at);  KihSms sms(at);
//   sms.onSms = [](const char *from, const char *text) { ... };     // UTF-8: æøå, €, emoji
//   sms.onBinary = [](const KihSmsPdu &p) { ... };                  // binary SMS (e.g. MMS notifications)
//   in loop():  if (power.ready()) sms.begin();  (once)  and  at.loop(); power.loop(); sms.loop();
//
// Messages are stored on the SIM (AT+CNMI=2,1) and announced with "+CMTI: "SM",<index>". We read that one (AT+CMGR) and delete it
// (AT+CMGD). Stored messages are not lost while the modem is busy (for instance in data mode for MMS); direct delivery (+CMT) can be.
// At start everything already on the SIM is read (AT+CMGL=4) and then deleted.
// PDU mode (AT+CMGF=0): KihSmsPdu decodes GSM 7-bit with æøå and the extension table, UCS2 with emoji, and 8-bit.
// Long SMS arrive as parts with the same reference; they are kept until all parts are here (or 60 s have passed, then the
// parts that came are delivered with "…" where something is missing).
#pragma once
#include <Arduino.h>
#include <functional>
#include "kih_at.h"
#include "kih_pdu.h"

class KihSms {
 public:
  std::function<void(const char *from, const char *text)> onSms;
  std::function<void(const KihSmsPdu &pdu)> onBinary;

  explicit KihSms(KihAt &at) : at(at) {}

  // Call once when the modem is ready. Chains onto the AT layer's URC handler (keeps one that was set before).
  void begin() {
    if (started) return;
    started = true;
    auto before = at.onUrc;
    at.onUrc = [this, before](const char *l) {
      if (!strncmp(l, "+CMTI:", 6)) { onCmti(l); return; }
      if (before) before(l);
    };
    at.send("AT+CMGF=0");
    at.send("AT+CNMI=2,1,0,0,0");
    at.send("AT+CMGL=4", [this](bool ok, const char *a) { if (ok) readList(a); }, 15000);
    at.send("AT+CMGD=1,4", nullptr, 15000);   // delete everything that has been read
  }

  void loop() {
    for (Long &l : parts)
      if (l.ref >= 0 && millis() - l.first > 60000) deliver(l);
  }

 private:
  static const int MAXPARTS = 8;
  struct Long {
    int ref = -1, count = 0, got = 0;
    uint32_t first = 0;
    char from[48];
    String text[MAXPARTS];
  };
  KihAt &at;
  bool started = false;
  KihSmsPdu pdu;
  Long parts[3];

  void onCmti(const char *l) {
    const char *c = strrchr(l, ',');
    if (!c) return;
    int i = atoi(c + 1);
    char cmd[24];
    snprintf(cmd, sizeof cmd, "AT+CMGR=%d", i);
    at.send(cmd, [this](bool ok, const char *a) { if (ok) readList(a); });
    snprintf(cmd, sizeof cmd, "AT+CMGD=%d", i);
    at.send(cmd);
  }

  // The answer of CMGR/CMGL: a "+CMGR: …" / "+CMGL: …" line followed by the PDU in hex, possibly many of them.
  void readList(const char *a) {
    bool next = false;
    char line[700];
    while (*a) {
      int n = 0;
      while (*a && *a != '\n' && n < (int)sizeof line - 1) line[n++] = *a++;
      line[n] = 0;
      if (*a == '\n') a++;
      if (!strncmp(line, "+CMGR:", 6) || !strncmp(line, "+CMGL:", 6)) { next = true; continue; }
      if (next && n > 0) { next = false; handlePdu(line); }
    }
  }

  void handlePdu(const char *hex) {
    if (!pdu.parse(hex)) return;
    if (pdu.binary) { if (onBinary) onBinary(pdu); return; }
    if (pdu.count <= 1) { if (onSms) onSms(pdu.from, pdu.text); return; }
    Long *l = nullptr;
    for (Long &x : parts) if (x.ref == pdu.ref && !strcmp(x.from, pdu.from)) l = &x;
    if (!l) for (Long &x : parts) if (x.ref < 0) { l = &x; break; }
    if (!l) { deliver(parts[0]); l = &parts[0]; }              // all slots busy: give the oldest away as it is
    if (l->ref < 0) {
      l->ref = pdu.ref; l->count = min(pdu.count, MAXPARTS); l->got = 0; l->first = millis();
      strncpy(l->from, pdu.from, sizeof l->from - 1); l->from[sizeof l->from - 1] = 0;
      for (String &t : l->text) t = "";
    }
    int i = pdu.part - 1;
    if (i >= 0 && i < l->count && !l->text[i].length()) { l->text[i] = pdu.text; l->got++; }
    if (l->got >= l->count) deliver(*l);
  }

  void deliver(Long &l) {
    String all;
    for (int i = 0; i < l.count; i++) all += l.text[i].length() ? l.text[i] : String("…");
    l.ref = -1;
    if (onSms) onSms(l.from, all.c_str());
  }
};
