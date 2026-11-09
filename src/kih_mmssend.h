// KihMMS – sending an MMS over PPP.
//
// The message (M-Send.req, built with KihMmsBuilder) is HTTP-POSTed to the operator's MMSC through the MMS proxy. The MMSC answers
// with an m-send-conf: response status 0x80 means accepted, and it carries the message ID. Call KihMmsSend::post() from the work
// function of KihPpp::run().
//
//   static uint8_t buf[200000];
//   KihMmsBuilder b(buf, sizeof buf);
//   int n = b.build("+4712345678", "Subject", "Text", jpeg, jpegLen, "KIH1");
//   ppp.run(op.apn, [&] { ok = KihMmsSend::post(op, buf, n, status); });
#pragma once
#include <Arduino.h>
#include <NetworkClient.h>
#include "kih_mmsget.h"

class KihMmsSend {
 public:
  // Posts the message. Returns true when the MMSC accepted it; «status» says what happened (and the message ID when accepted).
  static bool post(const KihMmsOperator &op, const uint8_t *msg, int len, String &status) {
    NetworkClient c;
    KihMmsGet::step = "connecting to the proxy";
    if (!KihMmsGet::connect(c, op)) { status = "could not connect to the MMS proxy"; KihMmsGet::step = ""; return false; }
    String mmsc = op.mmsc;
    if (mmsc.indexOf('/', 8) < 0) mmsc += "/";
    KihMmsGet::step = "POST";
    c.print("POST " + mmsc + " HTTP/1.1\r\nHost: " + KihMmsGet::host(mmsc) +
            "\r\nContent-Type: application/vnd.wap.mms-message\r\nAccept: */*, application/vnd.wap.mms-message\r\nUser-Agent: KihMMS\r\nContent-Length: " +
            String(len) + "\r\nConnection: close\r\n\r\n");
    for (int i = 0; i < len;) {                        // in pieces, so a slow link does not stall one big write
      int k = c.write(msg + i, min(1024, len - i));
      if (k <= 0) { status = "the connection broke while sending"; c.stop(); KihMmsGet::step = ""; return false; }
      i += k;
    }
    KihMmsGet::step = "reading the answer";
    uint8_t *buf = nullptr;
    int body = 0;
    int n = KihMmsGet::readAll(c, buf, 4096, body);
    c.stop();
    KihMmsGet::step = "";
    if (!buf) { status = "no answer from the MMSC"; return false; }
    int code = n > 12 ? atoi((const char *)buf + 9) : 0;
    bool ok = false;
    if (code != 200) status = String("the MMSC answered HTTP ") + code;
    else {
      // m-send-conf: 8C 81 …, 92 <status> (0x80 = ok), 8B "message id".
      // Walk the headers (the version header 8D has the value 0x92 too, so a plain byte search would read the wrong one).
      int st = -1;
      String id;
      for (int i = body; i + 1 < n;) {
        int h = buf[i++];
        if (h == 0x98 || h == 0x8B) {                    // transaction ID, message ID: text
          String t;
          while (i < n && buf[i]) t += (char)buf[i++];
          i++;
          if (h == 0x8B) id = t;
        } else if (h == 0x8C || h == 0x8D || h == 0x92) { // message type, version, response status: one byte
          if (h == 0x92) st = buf[i];
          i++;
        } else if (h == 0x93) {                          // response text: skip it
          while (i < n && buf[i]) i++;
          i++;
        } else break;
      }
      ok = st == 0x80;
      status = ok ? "accepted by the MMSC (id " + id + ")" : String("refused by the MMSC, status 0x") + String(st, HEX);
    }
    free(buf);
    return ok;
  }
};
