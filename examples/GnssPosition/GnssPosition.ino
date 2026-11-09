// KihMMS example: GnssPosition
//
// The A76xx modems (A7670G/E/SA) have a GNSS receiver. This example switches it on and prints the position every 5 seconds,
// with a map link. Indoors, or on a cold start, the first position can take a few minutes; until then the fields are empty.
// Needs the GNSS antenna connected (the LilyGO T-A7670G has a separate GPS antenna socket).
#include <KihMMS.h>

KihAt at;
KihPower power(at);
KihGnss gnss;

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("KihMMS " KIHMMS_VERSION " – GnssPosition");
  at.onUrc = [](const char *line) { if (!strncmp(line, "+CGNSSPWR:", 10)) Serial.println(line); };   // "+CGNSSPWR: READY!"
  power.begin(Serial1, 27, 26, 4, 12);   // RX, TX, PWRKEY, power enable (LilyGO T-A7670)
}

void loop() {
  at.loop();
  power.loop();
  if (!power.ready()) return;

  static bool on = false;
  if (!on) {
    on = true;
    at.send("AT+CGNSSPWR=1", [](bool ok, const char *) { Serial.println(ok ? "GNSS switched on, waiting for a position …" : "GNSS did not switch on"); });
  }

  static uint32_t last = 0;
  if (millis() - last < 5000) return;
  last = millis();
  at.send("AT+CGNSSINFO", [](bool ok, const char *answer) {
    if (!ok || !gnss.parse(answer)) return;
    if (!gnss.fix) { Serial.println("No position yet"); return; }
    Serial.printf("%s %s UTC  %.6f, %.6f  %.0f m  %.1f km/h  %d satellites  HDOP %.1f\n", gnss.date, gnss.time, gnss.lat, gnss.lon,
                  gnss.altitude, isnan(gnss.speedKmh) ? 0.0f : gnss.speedKmh, gnss.satellites, gnss.hdop);
    Serial.printf("  https://maps.google.com/?q=%.6f,%.6f\n", gnss.lat, gnss.lon);
  });
}
