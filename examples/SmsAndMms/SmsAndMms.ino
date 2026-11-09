// KihMMS example: SmsAndMms
//
// Everything with one object (KihMms) on a LilyGO T-A7670:
//   - SMS that arrive are printed (æøå, €, emoji, long SMS joined).
//   - MMS that arrive are fetched by themselves over PPP; the parts are listed, and the sender gets an SMS back saying what came.
//   - In the Serial Monitor (115200, newline):  sms +4712345678 Hello 😀   sends an SMS
//                                              mms +4712345678             sends an MMS with a picture the ESP32 draws itself
//
// The MMS settings (APN, MMSC, proxy) are found from the SIM card for the operators in extras/docs/OPERATORS.md. For others:
//   modem.setOperator("apn", "http://mmsc", "proxy", 8080);
// Tested on a LilyGO T-A7670G R2 with an Ice SIM card (Norway). Board settings: PSRAM enabled (an MMS can be several hundred KB).
#include <KihMMS.h>

#define MODEM_RX 27
#define MODEM_TX 26
#define MODEM_PWRKEY 4
#define MODEM_POWER 12

KihMms modem;

// A 96 × 96 PNG drawn in code (a smiley), so the example needs no SD card. Uncompressed ("stored" deflate): only CRC32 and
// Adler-32 are needed.
static uint32_t crc32(const uint8_t *p, int n) {
  uint32_t c = 0xFFFFFFFF;
  for (int i = 0; i < n; i++) { c ^= p[i]; for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (0 - (c & 1))); }
  return ~c;
}
static int drawPng(uint8_t *out) {
  const int W = 96, H = 96, raw = H * (1 + W * 3);
  int n = 0;
  auto u32 = [&](uint32_t v) { out[n++] = v >> 24; out[n++] = v >> 16; out[n++] = v >> 8; out[n++] = v; };
  const uint8_t sig[8] = { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };
  memcpy(out, sig, 8); n = 8;
  u32(13); int s = n; memcpy(out + n, "IHDR", 4); n += 4; u32(W); u32(H);
  out[n++] = 8; out[n++] = 2; out[n++] = 0; out[n++] = 0; out[n++] = 0;    // 8 bit RGB
  u32(crc32(out + s, n - s));
  u32(2 + 5 + raw + 4); s = n; memcpy(out + n, "IDAT", 4); n += 4;
  out[n++] = 0x78; out[n++] = 0x01;                                         // zlib header
  out[n++] = 0x01; out[n++] = raw & 0xFF; out[n++] = raw >> 8; out[n++] = ~raw & 0xFF; out[n++] = (~raw >> 8) & 0xFF;
  uint32_t a = 1, b = 0;
  for (int y = 0; y < H; y++)
    for (int x = -1; x < W; x++) {
      uint8_t px[3] = { 0, 0, 0 };
      int count = 1;                                                        // x = -1: the filter byte of the row
      if (x >= 0) {
        float dx = x - 48, dy = y - 48, r = sqrtf(dx * dx + dy * dy);
        bool eye = sqrtf((dx + 13) * (dx + 13) + (dy + 10) * (dy + 10)) < 5 || sqrtf((dx - 13) * (dx - 13) + (dy + 10) * (dy + 10)) < 5;
        if (eye || (r > 18 && r < 22 && dy > 5) || (r >= 35 && r < 38)) { px[0] = px[1] = px[2] = 30; }
        else if (r < 35) { px[0] = 255; px[1] = 205; px[2] = 40; }
        else { px[0] = 40 + x; px[1] = 60 + y; px[2] = 200 - x; }
        count = 3;
      }
      for (int k = 0; k < count; k++) { out[n++] = px[k]; a = (a + px[k]) % 65521; b = (b + a) % 65521; }
    }
  u32((b << 16) | a);
  u32(crc32(out + s, n - s));
  u32(0); s = n; memcpy(out + n, "IEND", 4); n += 4; u32(crc32(out + s, 4));
  return n;
}

static uint8_t picture[30000];

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("KihMMS " KIHMMS_VERSION " – SmsAndMms");

  modem.onSms = [](const char *from, const char *text) { Serial.printf("SMS from %s: %s\n", from, text); };

  modem.onMms = [](const KihMmsReceived &m) {
    Serial.printf("MMS from %s, subject \"%s\", text \"%s\"\n", m.from.c_str(), m.subject.c_str(), m.text.c_str());
    String reply = "Thanks, I got your MMS:";
    for (int i = 0; i < m.count; i++) {
      Serial.printf("  part %d: %s, %d bytes\n", i + 1, m.parts[i].type, m.parts[i].len);
      if (strcmp(m.parts[i].type, "application/smil")) reply += String("\n- ") + m.parts[i].type + " (" + (m.parts[i].len + 512) / 1024 + " KB)";
      // m.partData(i) points to the bytes, e.g. to save a picture on an SD card.
    }
    String to = m.from;                                   // some operators give the MMS sender without "+"
    if (to.length() > 8 && isdigit((unsigned char)to[0])) to = "+" + to;
    modem.sendSms(to.c_str(), reply.c_str());
  };

  modem.onMmsSent = [](bool ok, const char *status) { Serial.printf("MMS %s: %s\n", ok ? "sent" : "NOT sent", status); };

  modem.begin(Serial1, MODEM_RX, MODEM_TX, MODEM_PWRKEY, MODEM_POWER);
  Serial.println("Starting the modem …");
}

void loop() {
  modem.loop();                                           // never blocks

  static bool announced = false;
  if (!announced && modem.ready() && modem.networkCode()[0]) {
    announced = true;
    Serial.printf("Ready (network %s). Send an SMS or MMS to the board, or type: sms <number> <text> / mms <number>\n", modem.networkCode());
  }

  if (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    line.trim();
    if (line.startsWith("sms ")) {
      int sp = line.indexOf(' ', 4);
      String to = line.substring(4, sp), text = line.substring(sp + 1);
      modem.sendSms(to.c_str(), text.c_str(), [](bool ok) { Serial.println(ok ? "SMS sent" : "SMS NOT sent"); });
    } else if (line.startsWith("mms ")) {
      String to = line.substring(4);
      int n = drawPng(picture);
      bool queued = modem.sendMms(to.c_str(), "Hello from KihMMS 😀", "This picture was drawn by the ESP32 and sent as MMS over PPP.",
                                  picture, n, "image/png", "smiley.png");
      Serial.println(queued ? "MMS queued" : "MMS NOT queued (one is already waiting)");
    }
  }
}
