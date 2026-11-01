// KihMMS example: DecodeSmsPdu
//
// Decodes received SMS in PDU mode – the hex line the modem gives after +CMT, +CMGR or +CMGL when AT+CMGF=0.
// The three samples below are made-up messages (the number +4700000000 is not real). Runs on any ESP32, no modem needed.
//
// Why PDU mode? MMS notifications are binary SMS, and text mode destroys them. In PDU mode we get every byte, but then the
// ESP32 must decode ordinary texts itself: the GSM 7-bit alphabet (with æøå and the extension table for € [ ] { }), UCS2
// (emoji are UTF-16 surrogate pairs) and 8-bit data. KihSmsPdu does all of that and gives UTF-8.
#include <KihMMS.h>

static const char *samples[] = {
  // GSM 7-bit: "Hei æøå ÆØÅ - pris 100 € [ok] {x}"
  "00040A91740000000000006211102053254026C8721AD4613C409C8503D402C1E5E939280683813665D086F75E6F7CA00D0ABF4901",
  // UCS2 with emoji: "Hei 😀👍 fra telefonen"
  "00040A9174000000000008621110205325402C0048006500690020D83DDE00D83DDC4D0020006600720061002000740065006C00650066006F006E0065006E",
  // Sender with letters ("Ice"): "Koden din er 1234"
  "000406D0C9711900006211102053254011CB37B9EC0691D36E50590E8AC96634",
};

static KihSmsPdu sms;   // static: it holds a 640-byte text buffer

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("KihMMS " KIHMMS_VERSION " – DecodeSmsPdu");
  for (const char *hex : samples) {
    if (!sms.parse(hex)) { Serial.println("Not an SMS-DELIVER PDU"); continue; }
    if (sms.binary) Serial.printf("From %s: %d bytes of binary data\n", sms.from, sms.dataLen);
    else Serial.printf("From %s: %s\n", sms.from, sms.text);
    // A long SMS arrives as several PDUs: join the texts of parts 1..count that have the same ref.
    if (sms.count > 1) Serial.printf("  (part %d of %d, ref %d)\n", sms.part, sms.count, sms.ref);
  }
}

void loop() {}
