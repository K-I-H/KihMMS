// KihMMS – everything together: one object for SMS and MMS on a SIMCom A76xx modem.
//
//   KihMms modem;
//   void setup() {
//     modem.onSms = [](const char *from, const char *text) { ... };
//     modem.onMms = [](const KihMmsReceived &m) { ... m.from, m.subject, m.text, m.parts … };
//     modem.begin(Serial1, 27, 26, 4, 12);              // UART, RX, TX, PWRKEY, power enable (LilyGO T-A7670)
//   }
//   void loop() { modem.loop(); }                       // never blocks
//   modem.sendSms("+4712345678", "Hei 😀");
//   modem.sendMms("+4712345678", "Subject", "Text", jpeg, len);   // the attachment must stay valid until onMmsSent
//
// Incoming MMS are fetched by themselves when the notification arrives. The MMS settings (APN, MMSC, proxy) are found from the SIM
// card's network code (AT+CIMI) for the operators in OPERATORS.md; set them with setOperator() for others. Only one PPP session runs
// at a time; MMS waiting to be fetched or sent are queued (4 fetches, 1 send).
#pragma once
#include <Arduino.h>
#include <functional>
#include "kih_at.h"
#include "kih_power.h"
#include "kih_sms.h"
#include "kih_ppp.h"
#include "kih_mmsget.h"
#include "kih_mmssend.h"

class KihMms {
 public:
  std::function<void(const char *from, const char *text)> onSms;
  std::function<void(const KihMmsReceived &mms)> onMms;
  std::function<void(bool ok, const char *status)> onMmsSent;
  std::function<void(const char *line)> onUrc;     // anything else the modem says by itself (RING, +CGEV …)

  KihAt at;

  KihMms() : power(at), sms(at), ppp(nullptr) {}
  ~KihMms() { delete ppp; }

  void begin(HardwareSerial &port, int rx, int tx, int pwrkey, int powerEnable) {
    ppp = new KihPpp(at, port, rx, tx);
    at.onUrc = [this](const char *l) { if (onUrc) onUrc(l); };
    sms.onSms = [this](const char *f, const char *t) { if (onSms) onSms(f, t); };
    sms.onBinary = [this](const KihSmsPdu &p) { notice(p); };
    power.begin(port, rx, tx, pwrkey, powerEnable);
  }

  // MMS settings by hand (when the operator is not in the table, or to override it).
  void setOperator(const char *apn, const char *mmsc, const char *proxy, int port) {
    opApn = apn; opMmsc = mmsc; opProxy = proxy; op = { opApn.c_str(), opMmsc.c_str(), opProxy.c_str(), port };
    haveOp = true;
  }

  bool ready() const { return power.ready(); }
  bool busy() const { return ppp && ppp->active(); }
  const char *networkCode() const { return mccmnc; }
  // Where an MMS transfer is right now (for logging): PPP step and HTTP step.
  const char *step() const { return ppp ? ppp->step() : ""; }

  bool sendSms(const char *number, const char *text, std::function<void(bool ok)> done = nullptr) {
    if (!power.ready()) return false;
    return sms.send(number, text, done);
  }

  // Builds and sends an MMS. The attachment is optional (nullptr): a JPEG unless «type» says otherwise ("image/png" …).
  bool sendMms(const char *number, const char *subject, const char *text, const uint8_t *att = nullptr, int attLen = 0,
               const char *type = nullptr, const char *name = nullptr) {
    if (pendingSend) return false;
    int size = attLen + strlen(text) + strlen(subject) + 512;
    if (sendBuf) free(sendBuf);
    sendBuf = (uint8_t *)(psramFound() ? ps_malloc(size) : malloc(size));
    if (!sendBuf) return false;
    KihMmsBuilder b(sendBuf, size);
    char id[24];
    snprintf(id, sizeof id, "KIH%lu", (unsigned long)millis());
    sendLen = b.build(number, subject, text, att, attLen, id, type, name);
    if (sendLen < 0) return false;
    pendingSend = true;
    return true;
  }

  void loop() {
    at.loop();
    power.loop();
    if (!power.ready()) return;
    if (!started) { started = true; sms.begin(); }
    // The network code from the SIM (AT+CIMI). Right after start-up the SIM may still be busy: ask again every 2 s.
    if (!mccmnc[0] && !askingCimi && millis() - cimiAt > 2000) {
      askingCimi = true;
      cimiAt = millis();
      at.send("AT+CIMI", [this](bool ok, const char *a) {
        askingCimi = false;
        if (!ok) return;
        while (*a && !isdigit((unsigned char)*a)) a++;
        if (strlen(a) < 5) return;
        strncpy(mccmnc, a, 5); mccmnc[5] = 0;
        if (!haveOp) findOperator();
      });
    }
    sms.loop();
    if (!ppp) return;
    ppp->loop();
    if (ppp->active() || !haveOp) return;
    if (fetchCount > 0) {
      current = fetches[0];
      for (int i = 1; i < fetchCount; i++) fetches[i - 1] = fetches[i];
      fetchCount--;
      ppp->run(op.apn, [this] { KihMmsGet::fetch(op, current.url.c_str(), current.tid.c_str(), received); },
               [this](bool) {
                 sms.readStored();
                 if (received.count > 0 || received.text.length()) { if (onMms) onMms(received); }
               });
    } else if (pendingSend) {
      ppp->run(op.apn, [this] { sentOk = KihMmsSend::post(op, sendBuf, sendLen, sendStatus); }, [this](bool connected) {
        pendingSend = false;
        sms.readStored();
        if (!connected) { sentOk = false; sendStatus = String("no PPP connection: ") + ppp->status(); }
        if (onMmsSent) onMmsSent(sentOk, sendStatus.c_str());
      });
    }
  }

 private:
  struct Fetch { String url, tid; };
  KihPower power;
  KihSms sms;
  KihPpp *ppp;
  bool started = false, haveOp = false, pendingSend = false, sentOk = false, askingCimi = false;
  uint32_t cimiAt = 0;
  char mccmnc[6] = "";
  String opApn, opMmsc, opProxy, sendStatus;
  KihMmsOperator op = { "", "", "", 80 };
  Fetch fetches[4], current;
  int fetchCount = 0;
  KihMmsReceived received;
  uint8_t *sendBuf = nullptr;
  int sendLen = 0;

  void notice(const KihSmsPdu &p) {
    static KihMmsParser n;
    if (!n.notification(p.data, p.dataLen)) return;
    for (int i = 0; i < fetchCount; i++) if (fetches[i].url == n.url) return;      // the same notification twice
    if (fetchCount < 4) fetches[fetchCount++] = { String(n.url), String(n.tid) };
  }

  void findOperator() {
    // MCC+MNC → APN | MMSC | proxy | port (see extras/docs/OPERATORS.md; only Ice is tested).
    static const char *t[][5] = {
      { "24214", "mms.ice.net", "http://mms", "10.10.10.10", "80" }, { "24206", "mms.ice.net", "http://mms", "10.10.10.10", "80" },
      { "24201", "telenor.mms", "http://mmsc", "mms-proxy.telenor.no", "8080" }, { "24212", "telenor.mms", "http://mmsc", "mms-proxy.telenor.no", "8080" },
      { "24202", "mms", "http://mmss/", "193.209.134.132", "8080" }, { "24208", "mms", "http://mmss/", "193.209.134.132", "8080" },
      { "24001", "mms.telia.se", "http://mmss", "193.209.134.132", "8080" }, { "24007", "mms.tele2.se", "http://mmsc.tele2.se", "130.244.202.30", "8080" },
      { "24008", "services.telenor.se", "http://mms", "172.30.253.241", "8080" }, { "24002", "data.tre.se", "http://mms.tre.se", "193.209.134.132", "8080" },
      { "23801", "mms", "http://mms.tdc.dk:8002", "194.182.251.15", "8080" }, { "23820", "www.mms.mobil.telenor.dk", "http://mms.telenor.dk", "212.88.64.8", "8080" },
      { "23802", "telenor", "http://mms.telenor.dk", "212.88.64.199", "80" }, { "23830", "www.mms.mobil.telia.dk", "http://mms.telia.dk", "193.209.134.131", "8080" } };
    for (auto &e : t)
      if (!strcmp(mccmnc, e[0])) { setOperator(e[1], e[2], e[3], atoi(e[4])); return; }
  }
};
