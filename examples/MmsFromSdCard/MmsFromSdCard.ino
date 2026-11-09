// KihMMS example: MmsFromSdCard – send a picture from the SD card when someone asks for it by SMS.
//
// Send an SMS with the word  photo  to the board. It answers with an MMS with /photo.jpg from the SD card.
// Other words: "list" answers with the files on the card, "send <file>" sends that file (e.g. "send /song.mp3").
//
//   1. Copy a picture to the SD card as photo.jpg (FAT32; under ~300 KB, most operators refuse larger MMS).
//   2. Write your phone number below: only that number gets answers.
//   3. Tools → PSRAM: "Enabled". Upload, and open the Serial Monitor at 115200.
//
// NB on the LilyGO T-A7670: the SD card holds GPIO 2 high, which can stop uploading ("Wrong boot mode"). Take the card out
// while uploading, or hold the BOOT button.
#include <KihMMS.h>
#include <SPI.h>
#include <SD.h>

#define MY_NUMBER "+4712345678"     // ← your phone number: only this number gets answers

KihMms modem;
bool sdOk = false;

// Compares the last 8 digits, because operators write the same number in different ways (+47…, 47…, 0047…).
bool sameNumber(String a, String b) {
  a.replace(" ", ""); b.replace(" ", "");
  return a.length() >= 8 && b.length() >= 8 && a.substring(a.length() - 8) == b.substring(b.length() - 8);
}

void sendFile(const String &path) {
  if (!sdOk) { modem.sendSms(MY_NUMBER, "No SD card 😕"); return; }
  if (!SD.exists(path)) { modem.sendSms(MY_NUMBER, ("Can't find " + path).c_str()); return; }
  if (modem.sendMmsFile(MY_NUMBER, "From the SD card", path.c_str(), SD, path.c_str()))
    Serial.printf("Sending %s …\n", path.c_str());
  else
    modem.sendSms(MY_NUMBER, (String("Could not send: ") + modem.lastError()).c_str());
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("KihMMS " KIHMMS_VERSION " – MmsFromSdCard");

  SPI.begin(KIH_A7670_SD_SCK, KIH_A7670_SD_MISO, KIH_A7670_SD_MOSI);
  sdOk = SD.begin(KIH_A7670_SD_CS, SPI, 10000000);
  Serial.println(sdOk ? "SD card OK" : "No SD card");

  modem.onReady = [] { Serial.println("Ready: send \"photo\" by SMS to the board."); };

  modem.onSms = [](const char *from, const char *text) {
    Serial.printf("SMS from %s: %s\n", from, text);
    if (!sameNumber(from, MY_NUMBER)) return;
    String t = String(text);
    t.trim();
    String word = t;
    word.toLowerCase();
    if (word == "photo") sendFile("/photo.jpg");
    else if (word.startsWith("send ")) { String p = t.substring(5); p.trim(); sendFile(p.startsWith("/") ? p : "/" + p); }
    else if (word == "list") {
      String answer = "Files:";
      File dir = SD.open("/");
      for (File f = dir.openNextFile(); f && answer.length() < 600; f = dir.openNextFile())
        if (!f.isDirectory()) answer += String("\n") + f.name() + " (" + (f.size() + 512) / 1024 + " KB)";
      modem.sendSms(MY_NUMBER, sdOk ? answer.c_str() : "No SD card 😕");
    } else modem.sendSms(MY_NUMBER, "Send \"photo\", \"list\" or \"send <file>\"");
  };

  modem.onMmsSent = [](bool ok, const char *status) { Serial.printf(ok ? "✅ MMS sent\n" : "❌ MMS NOT sent: %s\n", status); };

  modem.begin();
}

void loop() {
  modem.loop();
}
