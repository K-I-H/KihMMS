// KihMMS – everything together: one object for SMS and MMS on a SIMCom A76xx modem.
//
//   KihMms modem;
//   void setup() {
//     modem.onSms = [](const char *from, const char *text) { ... };
//     modem.onMms = [](const KihMmsReceived &m) { ... m.from, m.subject, m.text, m.parts … };
//     modem.onReady = [] { modem.sendMms("+4712345678", "Hello", "Sent from my ESP32"); };
//     modem.begin();                                    // LilyGO T-A7670 pins; or begin(Serial1, rx, tx, pwrkey, powerEnable)
//   }
//   void loop() { modem.loop(); }                       // never blocks
//   modem.sendSms("+4712345678", "Hei 😀");
//   modem.sendMms("+4712345678", "Subject", "Text", jpeg, len);          // the attachment is copied, it can be freed afterwards
//   modem.sendMmsFile("+4712345678", "Subject", "Text", SD, "/photo.jpg"); // a file from SD / LittleFS, type from the extension
//
// Incoming MMS are fetched by themselves when the notification arrives. The MMS settings (APN, MMSC, proxy) are found from the SIM
// card's network code (AT+CIMI) for the operators in OPERATORS.md; set them with setOperator() for others. Only one PPP session runs
// at a time; MMS waiting to be fetched or sent are queued (4 fetches, 1 send).
#pragma once
#include <Arduino.h>
#include <functional>
#include <FS.h>
#include "kih_at.h"
#include "kih_power.h"
#include "kih_sms.h"
#include "kih_ppp.h"
#include "kih_mmsget.h"
#include "kih_mmssend.h"

// The pins of the LilyGO T-A7670E/G/SA (R2). Other boards: pass your own pins to begin().
#define KIH_A7670_RX 27
#define KIH_A7670_TX 26
#define KIH_A7670_PWRKEY 4
#define KIH_A7670_POWER 12
#define KIH_A7670_SD_CS 13
#define KIH_A7670_SD_SCK 14
#define KIH_A7670_SD_MISO 2
#define KIH_A7670_SD_MOSI 15

class KihMms {
 public:
  // Called once, when the modem is on and the SIM card has been read. Then SMS can be sent, and MMS too if mmsReady().
  std::function<void()> onReady;
  std::function<void(const char *from, const char *text)> onSms;
  std::function<void(const KihMmsReceived &mms)> onMms;
  std::function<void(bool ok, const char *status)> onMmsSent;
  std::function<void(const char *line)> onUrc;     // anything else the modem says by itself (RING, +CGEV …)

  KihAt at;

  KihMms() : power(at), sms(at), ppp(nullptr) {}
  ~KihMms() { delete ppp; }

  // The LilyGO T-A7670 pins on Serial1.
  void begin() { begin(Serial1, KIH_A7670_RX, KIH_A7670_TX, KIH_A7670_PWRKEY, KIH_A7670_POWER); }

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
  // MMS can be sent and fetched: the operator's MMS settings are known (from the SIM card, or setOperator()).
  bool mmsReady() const { return power.ready() && haveOp; }
  // Why the last sendSms/sendMms/sendMmsFile returned false (or "" when it did not).
  const char *lastError() const { return error.c_str(); }
  bool busy() const { return ppp && ppp->active(); }
  const char *networkCode() const { return mccmnc; }
  // Where an MMS transfer is right now (for logging): PPP step and HTTP step.
  const char *step() const { return ppp ? ppp->step() : ""; }

  bool sendSms(const char *number, const char *text, std::function<void(bool ok)> done = nullptr) {
    error = "";
    if (!power.ready()) { error = "the modem is not ready yet (wait for onReady)"; return false; }
    String nr = fixNumber(number);
    if (!sms.send(nr.c_str(), text, done)) { error = "the SMS could not be queued"; return false; }
    return true;
  }

  // Builds and sends an MMS. The attachment is optional (nullptr): a JPEG unless «type» says otherwise ("image/png" …).
  bool sendMms(const char *number, const char *subject, const char *text, const uint8_t *att = nullptr, int attLen = 0,
               const char *type = nullptr, const char *name = nullptr) {
    error = "";
    if (pendingSend) { error = "an MMS is already waiting to be sent (wait for onMmsSent)"; return false; }
    if (!subject) subject = "";
    if (!text) text = "";
    int size = attLen + strlen(text) + strlen(subject) + 512;
    if (sendBuf) free(sendBuf);
    sendBuf = (uint8_t *)(psramFound() ? ps_malloc(size) : malloc(size));
    if (!sendBuf) { error = "not enough memory (enable PSRAM in the board settings)"; return false; }
    KihMmsBuilder b(sendBuf, size);
    char id[24];
    snprintf(id, sizeof id, "KIH%lu", (unsigned long)millis());
    String nr = fixNumber(number);
    sendLen = b.build(nr.c_str(), subject, text, att, attLen, id, type, name);
    if (sendLen < 0) { error = "the MMS could not be built"; return false; }
    pendingSend = true;
    return true;
  }

  // Sends a file as an MMS attachment: a picture, a sound … The type comes from the extension (.jpg .png .gif .mp3 .amr .wav
  // .mid .txt .vcf). Most operators take up to about 300 KB per MMS.
  bool sendMmsFile(const char *number, const char *subject, const char *text, fs::FS &fs, const char *path) {
    error = "";
    if (pendingSend) { error = "an MMS is already waiting to be sent (wait for onMmsSent)"; return false; }
    File f = fs.open(path, FILE_READ);
    if (!f || f.isDirectory()) { error = String("could not open ") + path; return false; }
    int n = f.size();
    uint8_t *buf = (uint8_t *)(psramFound() ? ps_malloc(n ? n : 1) : malloc(n ? n : 1));
    if (!buf) { f.close(); error = "not enough memory for the file (enable PSRAM in the board settings)"; return false; }
    int got = f.read(buf, n);
    f.close();
    if (got != n) { free(buf); error = String("could not read ") + path; return false; }
    const char *name = strrchr(path, '/');
    name = name ? name + 1 : path;
    bool ok = sendMms(number, subject, text, buf, n, typeFor(name), name);
    free(buf);
    return ok;
  }

  // The MIME type for a file name ("photo.jpg" → "image/jpeg"; "application/octet-stream" when unknown).
  static const char *typeFor(const char *name) {
    const char *dot = strrchr(name, '.');
    if (!dot) return "application/octet-stream";
    String e = String(dot + 1);
    e.toLowerCase();
    static const char *t[][2] = { { "jpg", "image/jpeg" }, { "jpeg", "image/jpeg" }, { "png", "image/png" }, { "gif", "image/gif" },
                                  { "bmp", "image/bmp" }, { "txt", "text/plain" }, { "vcf", "text/x-vcard" }, { "amr", "audio/amr" },
                                  { "mp3", "audio/mpeg" }, { "wav", "audio/wav" }, { "mid", "audio/midi" }, { "midi", "audio/midi" },
                                  { "mp4", "video/mp4" }, { "3gp", "video/3gpp" } };
    for (auto &x : t) if (e == x[0]) return x[1];
    return "application/octet-stream";
  }

  // Some operators give the sender without "+" ("4712345678"): a long number of digits gets a "+" in front, so it can be
  // answered. Numbers with "+", short numbers and numbers that start with 0 are left as they are.
  static String fixNumber(const char *number) {
    String s = number ? String(number) : String();
    s.trim();
    bool digits = s.length() >= 10 && s[0] != '0';
    for (unsigned i = 0; i < s.length() && digits; i++) if (!isdigit((unsigned char)s[i])) digits = false;
    return digits ? "+" + s : s;
  }

  void loop() {
    at.loop();
    power.loop();
    if (!power.ready()) return;
    if (!started) { started = true; readyAt = millis(); sms.begin(); }
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
    // onReady once the SIM has answered, or after 30 s without an answer (SMS can still be sent).
    if (!announced && (mccmnc[0] || millis() - readyAt > 30000)) {
      announced = true;
      if (onReady) onReady();
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
                 received.from = fixNumber(received.from.c_str());     // so m.from can be answered directly
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
  bool started = false, announced = false, haveOp = false, pendingSend = false, sentOk = false, askingCimi = false;
  uint32_t cimiAt = 0, readyAt = 0;
  char mccmnc[6] = "";
  String opApn, opMmsc, opProxy, sendStatus, error;
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
