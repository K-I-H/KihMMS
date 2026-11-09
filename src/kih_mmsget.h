// KihMMS – fetching an MMS over PPP.
//
// An incoming MMS is announced by a binary SMS (WAP push with m-notification-ind) that holds the address of the message at the
// operator's MMSC. To get the message:
//   1. GET that address through the operator's MMS proxy (the MMSC is only reachable inside the operator's MMS network),
//   2. read the m-retrieve-conf: sender, subject and the parts (text/plain, image/jpeg …),
//   3. POST an m-notifyresp-ind ("retrieved") to the MMSC, so it does not send the notification again.
// Call KihMmsGet::fetch() from the work function of KihPpp::run() (it blocks while it talks to the MMSC, in the PPP task).
//
//   KihMmsOperator op = { "mms.ice.net", "http://mms", "10.10.10.10", 80 };   // APN, MMSC, proxy, proxy port (Ice, Norway)
//   KihMmsReceived m;
//   bool ok = KihMmsGet::fetch(op, notificationUrl, transactionId, m);   // m.from, m.subject, m.text, m.part(i) …
#pragma once
#include <Arduino.h>
#include <NetworkClient.h>
#include "kih_pdu.h"

struct KihMmsOperator {
  const char *apn;      // the MMS APN
  const char *mmsc;     // the MMSC address (the notification response is posted here)
  const char *proxy;    // the MMS proxy (WAP gateway), IP address or name
  int port;             // the proxy port
};

// A fetched MMS. The whole message stays in «data» (PSRAM when the board has it); the parts point into it.
struct KihMmsReceived {
  String from, subject, text, status;
  uint8_t *data = nullptr;
  int len = 0, body = 0, count = 0;
  KihMmsPart parts[8];

  const uint8_t *partData(int i) const { return data + body + parts[i].start; }
  ~KihMmsReceived() { clear(); }
  void clear() { if (data) free(data); data = nullptr; len = body = count = 0; from = subject = text = ""; }
};

class KihMmsGet {
 public:
  // Where fetch() is right now (for logging).
  static inline const char *volatile step = "";

  static bool fetch(const KihMmsOperator &op, const char *url, const char *tid, KihMmsReceived &m, int maxBytes = 0) {
    m.clear();
    if (maxBytes <= 0) maxBytes = psramFound() ? 1500000 : 120000;
    NetworkClient c;
    step = "connecting to the proxy";
    if (!connect(c, op)) { m.status = "could not connect to the MMS proxy"; step = ""; return false; }
    step = "GET";
    c.print(String("GET ") + url + " HTTP/1.1\r\nHost: " + host(url) +
            "\r\nAccept: */*, application/vnd.wap.mms-message\r\nUser-Agent: KihMMS\r\nConnection: close\r\n\r\n");
    int body = 0;
    step = "reading the answer";
    int n = readAll(c, m.data, maxBytes, body);
    c.stop();
    step = "";
    if (!m.data) { m.status = body ? "not enough memory for the MMS" : "no answer from the MMSC"; return false; }
    int code = n > 12 ? atoi((const char *)m.data + 9) : 0;
    if (code != 200 || body <= 0) { m.status = String("the MMSC answered ") + code; return false; }
    m.len = n;
    m.body = body;
    static KihMmsParser p;
    if (!p.parse(m.data + body, n - body, 0)) { m.status = "not an MMS"; return false; }
    m.from = p.from;
    m.subject = p.subject;
    m.count = p.parts(m.data + body, n - body, m.parts, 8);
    // m.parts[i].start is relative to the message (after the HTTP header).
    for (int i = 0; i < m.count; i++)
      if (!strcmp(m.parts[i].type, "text/plain")) {
        if (m.text.length()) m.text += "\n";
        m.text += String((const char *)m.partData(i), m.parts[i].len);
      }
    m.status = String("fetched (") + (n - body) + " bytes, " + m.count + " parts)";
    if (tid && *tid) { step = "notify retrieved"; notifyRetrieved(op, tid); step = ""; }
    return true;
  }

  // m-notifyresp-ind with status "retrieved".
  static void notifyRetrieved(const KihMmsOperator &op, const char *tid) {
    uint8_t r[80];
    int k = 0;
    r[k++] = 0x8C; r[k++] = 0x83;                                   // m-notifyresp-ind
    r[k++] = 0x98;                                                  // transaction ID
    for (const char *s = tid; *s && k < 70; s++) r[k++] = (uint8_t)*s;
    r[k++] = 0;
    r[k++] = 0x8D; r[k++] = 0x92;                                   // MMS version 1.2
    r[k++] = 0x95; r[k++] = 0x81;                                   // status: retrieved
    NetworkClient c;
    if (!connect(c, op)) return;
    String mmsc = op.mmsc;
    if (mmsc.indexOf('/', 8) < 0) mmsc += "/";
    c.print("POST " + mmsc + " HTTP/1.1\r\nHost: " + host(mmsc) + "\r\nContent-Type: application/vnd.wap.mms-message\r\nContent-Length: " +
            String(k) + "\r\nConnection: close\r\n\r\n");
    c.write(r, k);
    uint32_t t = millis();
    while (c.connected() && millis() - t < 10000) { while (c.available()) c.read(); delay(10); }
    c.stop();
  }

  static String host(const String &url) {
    int h = url.indexOf("://");
    String v = url.substring(h < 0 ? 0 : h + 3);
    int s = v.indexOf('/');
    return s < 0 ? v : v.substring(0, s);
  }

  static bool connect(NetworkClient &c, const KihMmsOperator &op) {
    IPAddress ip;
    int port = op.port > 0 ? op.port : 80;
    if (ip.fromString(op.proxy)) return c.connect(ip, port, 20000);
    return c.connect(op.proxy, port, 20000);
  }

  // The whole answer (header + body) into a buffer; returns its length, and «body» is where the body starts. The header is read
  // first, so the buffer can be made as big as Content-Length says (PSRAM when the board has it), not a fixed guess.
  static int readAll(NetworkClient &c, uint8_t *&buf, int max, int &body) {
    buf = nullptr;
    body = 0;
    String head;
    uint32_t t = millis(), start = t;
    while ((c.connected() || c.available()) && millis() - t < 30000 && !head.endsWith("\r\n\r\n") && head.length() < 4096) {
      if (c.available()) { head += (char)c.read(); t = millis(); } else delay(5);
    }
    if (!head.endsWith("\r\n\r\n")) return 0;
    int want = max;
    String low = head;
    low.toLowerCase();
    int cl = low.indexOf("content-length:");
    if (cl >= 0) want = min(max, (int)head.substring(cl + 15).toInt());
    int size = head.length() + want;
    buf = (uint8_t *)(psramFound() ? ps_malloc(size) : malloc(size));
    if (!buf) return 0;
    memcpy(buf, head.c_str(), head.length());
    body = head.length();
    int n = body;
    t = millis();
    while ((c.connected() || c.available()) && millis() - t < 60000 && millis() - start < 180000 && n < size) {
      while (c.available() && n < size) { buf[n++] = (uint8_t)c.read(); t = millis(); }
      delay(5);
    }
    return n;
  }
};
