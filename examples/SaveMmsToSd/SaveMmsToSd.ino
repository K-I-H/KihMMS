// KihMMS example: SaveMmsToSd – every picture (and sound, video …) that arrives by MMS is saved on the SD card.
//
// Send an MMS with a picture to the board. It is fetched by itself, each part is saved as /mms/<number>_<part>.<type>
// (e.g. /mms/0007_1.jpg), the text is added to /mms/log.txt, and the sender gets an SMS saying what was saved.
//
//   Tools → PSRAM: "Enabled" (an MMS can be several hundred KB). Upload, and open the Serial Monitor at 115200.
//
// NB on the LilyGO T-A7670: the SD card holds GPIO 2 high, which can stop uploading ("Wrong boot mode"). Take the card out
// while uploading, or hold the BOOT button.
#include <KihMMS.h>
#include <SPI.h>
#include <SD.h>
#include <Preferences.h>

KihMms modem;
Preferences prefs;
bool sdOk = false;

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("KihMMS " KIHMMS_VERSION " – SaveMmsToSd");

  SPI.begin(KIH_A7670_SD_SCK, KIH_A7670_SD_MISO, KIH_A7670_SD_MOSI);
  sdOk = SD.begin(KIH_A7670_SD_CS, SPI, 10000000);
  if (sdOk) SD.mkdir("/mms");
  Serial.println(sdOk ? "SD card OK" : "No SD card: the MMS are only listed");
  prefs.begin("kihmms");                                     // the MMS number is kept over restarts, so no file is overwritten

  modem.onReady = [] { Serial.println("Ready: send an MMS with a picture to the board."); };

  modem.onMms = [](const KihMmsReceived &m) {
    int nr = prefs.getInt("nr", 0) + 1;
    prefs.putInt("nr", nr);
    Serial.printf("MMS %d from %s: \"%s\" \"%s\", %d parts\n", nr, m.from.c_str(), m.subject.c_str(), m.text.c_str(), m.count);

    String reply = "Got it 👍";
    for (int i = 0; i < m.count; i++) {
      if (!strcmp(m.parts[i].type, "application/smil")) continue;      // only layout, not worth keeping
      if (!strncmp(m.parts[i].type, "text/plain", 10)) continue;       // the text is already in m.text
      char path[40];
      snprintf(path, sizeof path, "/mms/%04d_%d.%s", nr, i + 1, m.extension(i));
      bool saved = sdOk && m.save(i, SD, path);
      Serial.printf("  %s, %d bytes → %s\n", m.parts[i].type, m.parts[i].len, saved ? path : "not saved");
      reply += String("\n") + (saved ? path : m.parts[i].type) + " (" + (m.parts[i].len + 512) / 1024 + " KB)";
    }

    if (sdOk) {
      File log = SD.open("/mms/log.txt", FILE_APPEND);
      if (log) { log.printf("%04d\t%s\t%s\t%s\n", nr, m.from.c_str(), m.subject.c_str(), m.text.c_str()); log.close(); }
    }
    modem.sendSms(m.from.c_str(), reply.c_str());
  };

  modem.begin();
}

void loop() {
  modem.loop();
}
