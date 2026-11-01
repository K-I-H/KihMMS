// KihMMS example: BuildMms
//
// Builds an MMS (M-Send.req) with a subject, a text and a small attachment, and prints the size and the bytes in hex.
// This is the message that is HTTP-POSTed to the operator's MMSC. Runs on any ESP32, no modem needed.
//
// Why build it on the ESP32? The A76xx modems have no MMS commands (AT+CMMS* answers ERROR), so the message must be
// made by the program. See extras/docs/FINDINGS.md.
#include <KihMMS.h>

static uint8_t buffer[2048];

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("KihMMS " KIHMMS_VERSION " – BuildMms");

  // A tiny "attachment" (any bytes). With a real picture you would read a JPEG from the SD card.
  uint8_t attachment[64];
  for (int i = 0; i < 64; i++) attachment[i] = (uint8_t)i;

  KihMmsBuilder mms(buffer, sizeof(buffer));
  // Subject and text may contain æøå and emoji: they are sent as UTF-8.
  int n = mms.build("+4700000000",                 // the receiver (replace with a real number)
                    "Hei fra ESP32 😀",             // subject
                    "Hilsen fra KihMMS – æøå 👍",   // text
                    attachment, sizeof(attachment),
                    "BX1",                          // transaction id: any unique text
                    "application/octet-stream",     // attachment type (nullptr = JPEG)
                    "data.bin");                    // attachment name
  if (n < 0) { Serial.println("The buffer was too small."); return; }

  Serial.printf("MMS built: %d bytes\n", n);
  for (int i = 0; i < n; i++) {
    Serial.printf("%02X ", buffer[i]);
    if (i % 24 == 23) Serial.println();
  }
  Serial.println();

  // Read it back with the parser, to see the headers.
  KihMmsParser p;
  p.parse(buffer, n, 0);
  Serial.printf("Type 0x%02X, subject \"%s\"\n", p.type, p.subject);
  KihMmsPart parts[4];
  int count = p.parts(buffer, n, parts, 4);
  for (int i = 0; i < count; i++) Serial.printf("Part %d: %s, %d bytes, name \"%s\"\n", i + 1, parts[i].type, parts[i].len, parts[i].name);
}

void loop() {}
