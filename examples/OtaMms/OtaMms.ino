// KihMMS example: OtaMms
//
// A new program for the board over MMS (OTAMMS). The OTAMMS Android app splits the program file (.bin) into pieces, signs it
// with your secret key and sends the pieces as MMS. The board:
//   - takes pieces only from your phone number, checks each piece (CRC-32) and replies by SMS ("got piece 2 of 5 ✔ …"),
//   - when all are here: checks the version (newer than the one installed) and the signature (HMAC-SHA256 with the key),
//   - installs with Update, restarts, and the new program replies "new firmware is running (version …)".
//
// Put the same key in the app and here. Keep it secret: whoever has it can make programs the board accepts.
// The new program must also contain the receiver (this example), or the board can not be updated over MMS again.
// Board settings: PSRAM enabled, and a partition scheme with OTA space (e.g. "Minimal SPIFFS" for programs over 1.2 MB).
// Tested on a LilyGO T-A7670G R2 with an Ice SIM card: 1 MB in 5 MMS, about 5 minutes from the first MMS to the confirmation.
#include <KihMMS.h>

#define OTA_KEY "change-me-to-a-long-random-secret"   // the same key as in the OTAMMS app
#define MY_NUMBER "+4712345678"                         // the only number allowed to send programs (and that gets the replies)
#define FIRMWARE "1"                                    // change it to see which program is running

KihMms modem;
KihOtaMms ota(OTA_KEY, MY_NUMBER);

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.printf("KihMMS " KIHMMS_VERSION " – OtaMms, firmware %s, installed OTAMMS version %u\n", FIRMWARE, (unsigned)KihOtaMms::installedVersion());

  ota.onMessage = [](const char *text) {
    Serial.printf("OTAMMS: %s\n", text);
    modem.sendSms(MY_NUMBER, (String("OTAMMS: ") + text).c_str());
  };

  modem.onMms = [](const KihMmsReceived &m) {
    if (ota.offer(m)) return;                           // a program piece
    Serial.printf("MMS from %s: %s\n", m.from.c_str(), m.text.c_str());
  };
  modem.onSms = [](const char *from, const char *text) { Serial.printf("SMS from %s: %s\n", from, text); };

  modem.begin(Serial1, 27, 26, 4, 12);                 // LilyGO T-A7670: RX, TX, PWRKEY, power
}

void loop() {
  modem.loop();
  ota.loop(modem.ready() && modem.networkCode()[0]);  // installs when complete; reports a new program once it can send
}
