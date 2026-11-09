// KihMMS – a data connection through the modem with PPP: the ESP32 gets an IP address in the mobile network and does TCP itself.
//
//   KihPpp ppp(at, Serial1, 27, 26);       // the AT layer, the modem's UART and its RX/TX pins
//   ppp.run("mms.ice.net", [] { /* runs in its own task while connected: NetworkClient, HTTP … */ },
//           [](bool ok) { /* back in loop(): the AT layer works again */ });
//   void loop() { at.loop(); ppp.loop(); }
//
// Why PPP? Some SIM cards keep the modem's own TCP stack (AT+NETOPEN) from working (on Ice the SIM toolkit's traffic does, and the
// modem answers ERROR), and the A76xx has no MMS commands at all. With PPP the modem is just a pipe, and the ESP32 core's PPP
// (PPP.h, model SIM7600 for the A76xx) gives a normal network interface.
// While PPP runs it owns the UART: the AT layer is paused, and SMS that arrive meanwhile wait on the SIM (KihSms stores them there).
// Afterwards the UART is given back, the modem is taken out of data mode, and the AT layer continues where it was.
// The connection is made the default route while it lasts (a board that is also on WiFi would otherwise send the traffic there),
// and the previous default is restored. Connecting and the work run in a FreeRTOS task, so loop() never waits.
#pragma once
#include <Arduino.h>
#include <PPP.h>
#include <functional>
#include "kih_at.h"

class KihPpp {
 public:
  KihPpp(KihAt &at, HardwareSerial &port, int rx, int tx, uint32_t baud = 115200) : at(at), port(port), rx(rx), tx(tx), baud(baud) {}

  // Starts a session: connect with «apn», run «work» while connected, disconnect, give the UART back, then done(connected) in loop().
  // Returns false when a session is already running. Starts when the AT layer has finished the command it is on.
  bool run(const char *apn, std::function<void()> work, std::function<void(bool connected)> done = nullptr) {
    if (state != IDLE) return false;
    this->apn = apn;
    this->work = work;
    this->done = done;
    state = WAITING;
    return true;
  }

  bool active() const { return state != IDLE; }
  const char *status() const { return info; }
  IPAddress localIP() const { return ip; }

  void loop() {
    if (state == WAITING && !at.busy()) {
      at.pause(true);
      port.end();
      state = CONNECTING;
      xTaskCreatePinnedToCore(task, "kih_ppp", 8192, this, 1, nullptr, 1);
    } else if (state == RETURNED) {
      // Back in command mode: the modem may still be in data mode, so "+++" (with a second of silence on both sides), then AT
      // until it answers.
      port.begin(baud, SERIAL_8N1, rx, tx);
      t = millis();
      state = GUARD1;
    } else if (state == GUARD1 && millis() - t > 1100) {
      port.print("+++");
      t = millis();
      state = GUARD2;
    } else if (state == GUARD2 && millis() - t > 1100) {
      while (port.available()) port.read();
      at.pause(false);
      tries = 0;
      state = CHECKING;
      check();
    }
  }

 private:
  enum State { IDLE, WAITING, CONNECTING, RETURNED, GUARD1, GUARD2, CHECKING };
  uint32_t t = 0;
  KihAt &at;
  HardwareSerial &port;
  int rx, tx;
  uint32_t baud;
  String apn;
  std::function<void()> work;
  std::function<void(bool)> done;
  volatile State state = IDLE;
  bool connected = false;
  int tries = 0;
  const char *info = "";
  IPAddress ip;

  void check() {
    at.send("AT", [this](bool ok, const char *) {
      if (ok || ++tries >= 10) {
        state = IDLE;
        if (!ok) info = "the modem did not answer after PPP";
        if (done) done(connected);
        return;
      }
      check();
    }, 1000);
  }

  static void task(void *arg) {
    KihPpp &p = *(KihPpp *)arg;
    p.connected = false;
    PPP.setApn(p.apn.c_str());
    PPP.setPins(p.tx, p.rx);
    bool started = PPP.begin(PPP_MODEM_SIM7600, 1, p.baud);
    if (started) {
      uint32_t t = millis();
      while (!PPP.attached() && millis() - t < 60000) delay(250);
      PPP.mode(ESP_MODEM_MODE_DATA);
      t = millis();
      while (!PPP.connected() && millis() - t < 60000) delay(250);
    }
    NetworkInterface *before = Network.getDefaultInterface();
    if (started && PPP.connected()) {
      p.connected = true;
      p.ip = PPP.localIP();
      p.info = "connected";
      PPP.setDefault();
      if (p.work) p.work();
    } else {
      p.info = started ? "no PPP connection within 60 s (APN?)" : "PPP did not start";
    }
    PPP.end();
    if (before && before != &PPP) before->setDefault();
    delay(1500);
    p.state = RETURNED;
    vTaskDelete(nullptr);
  }
};
