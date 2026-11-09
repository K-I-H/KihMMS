// KihMMS – turning the modem on and keeping it answering.
//
//   KihAt at;
//   KihPower power(at);
//   power.begin(Serial1, 27, 26, 4, 12);   // RX, TX, PWRKEY, power enable (LilyGO T-A7670: 27, 26, 4, 12; -1 = not used)
//   void loop() { at.loop(); power.loop(); if (power.ready()) { ... } }
//
// Sequence: power enable high, one PWRKEY pulse of 1 s, then "AT" about once a second until the modem answers OK (it takes
// 5–10 s after the pulse). A modem that was already on is switched OFF by the same pulse, so when there is no answer after 15 s
// the pulse is given once more (at most three times). The 1 s pulse is done in one go with delay(): if loop() is held up in the
// middle of it (a slow SD card did this), a too long pulse switches the modem off again. It only happens at start-up.
// When the modem answers, the basic setup is queued: no echo, readable errors, full function, network registration messages,
// caller ID and network time.
#pragma once
#include <Arduino.h>
#include "kih_at.h"

class KihPower {
 public:
  explicit KihPower(KihAt &at) : at(at) {}

  void begin(HardwareSerial &port, int rx, int tx, int pwrkey, int powerEnable, uint32_t baud = 115200) {
    port.begin(baud, SERIAL_8N1, rx, tx);
    at.begin(port);
    pwr = pwrkey;
    if (powerEnable >= 0) { pinMode(powerEnable, OUTPUT); digitalWrite(powerEnable, HIGH); }
    if (pwr >= 0) { pinMode(pwr, OUTPUT); digitalWrite(pwr, LOW); }
    state = PULSE;
    t = millis();
    pulses = 0;
  }

  bool ready() const { return state == READY; }
  int pulsesGiven() const { return pulses; }

  void loop() {
    uint32_t n = millis();
    if (state == PULSE && n - t > 100) {
      if (pwr >= 0) { digitalWrite(pwr, HIGH); delay(1000); digitalWrite(pwr, LOW); }
      pulses++;
      state = WAIT;
      t = millis();
    } else if (state == WAIT) {
      if (!at.busy() && at.queued() == 0)
        at.send("AT", [this](bool ok, const char *) { if (ok && state == WAIT) onAnswer(); }, 1000);
      if (n - t > 15000) {
        if (pulses < 3) { state = PULSE; t = n; }
        else state = FAILED;
      }
    }
  }

  bool failed() const { return state == FAILED; }

 private:
  enum State { OFF, PULSE, WAIT, READY, FAILED };
  KihAt &at;
  int pwr = -1;
  State state = OFF;
  uint32_t t = 0;
  int pulses = 0;

  void onAnswer() {
    state = READY;
    for (const char *c : { "ATE0", "AT+CMEE=2", "AT+CFUN=1", "AT+CEREG=1", "AT+CLIP=1", "AT+CTZU=1" }) at.send(c);
  }
};
