// KihMMS example: SendMms – the shortest way to send an MMS.
//
// When the modem is ready, the board sends ONE MMS with a subject, a text and a picture (picture.h, so no SD card is needed).
//
//   1. Put a SIM card in the LilyGO T-A7670 (with mobile data and MMS on the subscription).
//   2. Write your own phone number below, with country code.
//   3. Tools → PSRAM: "Enabled". Upload, and open the Serial Monitor at 115200.
//
// The MMS settings (APN, MMSC, proxy) are found from the SIM card for the operators in extras/docs/OPERATORS.md. If yours is not
// there, add one line in setup():  modem.setOperator("apn", "http://mmsc", "proxy", 8080);
#include <KihMMS.h>
#include "picture.h"

#define TO_NUMBER "+4712345678"     // ← your phone number

KihMms modem;

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("KihMMS " KIHMMS_VERSION " – SendMms. Starting the modem (10–20 s) …");

  modem.onReady = [] {
    if (!modem.mmsReady()) {
      Serial.printf("The modem is on, but the MMS settings for network %s are unknown: use modem.setOperator(…)\n", modem.networkCode());
      return;
    }
    Serial.println("Ready. Sending the MMS …");
    if (!modem.sendMms(TO_NUMBER, "Hello 👋", "This MMS was sent by an ESP32 with KihMMS.", picture_jpg, sizeof picture_jpg,
                       "image/jpeg", "hello.jpg"))
      Serial.printf("Could not send: %s\n", modem.lastError());
  };

  modem.onMmsSent = [](bool ok, const char *status) {
    Serial.printf(ok ? "✅ MMS sent (%s)\n" : "❌ MMS NOT sent: %s\n", status);
  };

  modem.begin();                    // LilyGO T-A7670 pins. Other boards: modem.begin(Serial1, RX, TX, PWRKEY, POWER);
}

void loop() {
  modem.loop();                     // never blocks: keep your own code here too, without long delay()s
}
