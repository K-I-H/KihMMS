// KihMMS example: SendSms
//
// Switches on the A76xx modem of a LilyGO T-A7670 and sends one SMS with plain AT commands.
// Put a SIM card (without PIN, or set SIM_PIN) and the antenna in, and replace NUMBER with your own number.
//
// Pins (LilyGO T-A7670): ESP32 TX 26 → modem RX, ESP32 RX 27 ← modem TX, PWRKEY 4, modem power 12.
#include <KihMMS.h>

#define NUMBER  "+4700000000"   // replace with your mobile number
#define SIM_PIN ""              // e.g. "1234", or "" when the SIM has no PIN
#define MODEM_TX 26
#define MODEM_RX 27
#define MODEM_PWRKEY 4
#define MODEM_POWER 12

HardwareSerial modem(1);

// Sends a command and returns everything the modem answers within «ms» milliseconds (blocking: fine for a demo).
String at(const String &cmd, uint32_t ms = 1000) {
  modem.print(cmd); modem.print("\r\n");
  String svar;
  uint32_t t = millis();
  while (millis() - t < ms) while (modem.available()) svar += (char)modem.read();
  Serial.print("> " + cmd + "\n" + svar);
  return svar;
}

void setup() {
  Serial.begin(115200);
  modem.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);
  pinMode(MODEM_POWER, OUTPUT); digitalWrite(MODEM_POWER, HIGH);
  // PWRKEY: one pulse of about 1 s switches the modem on. Do it in one go: a much longer press switches it off again.
  pinMode(MODEM_PWRKEY, OUTPUT);
  digitalWrite(MODEM_PWRKEY, HIGH); delay(1000); digitalWrite(MODEM_PWRKEY, LOW);

  Serial.println("Waiting for the modem …");
  for (int i = 0; i < 30 && at("AT", 500).indexOf("OK") < 0; i++) delay(500);
  if (strlen(SIM_PIN)) at(String("AT+CPIN=\"") + SIM_PIN + "\"", 3000);

  Serial.println("Waiting for the mobile network …");
  for (int i = 0; i < 60; i++) {
    String r = at("AT+CEREG?", 500);
    if (r.indexOf(",1") >= 0 || r.indexOf(",5") >= 0) break;   // 1 = home network, 5 = roaming
    delay(1000);
  }

  // Text mode with the GSM character set: fine for plain letters and digits. For æøå and emoji, use UCS2
  // (AT+CSCS="UCS2", AT+CSMP=17,167,2,25 and the text as UTF-16 hex) – the full driver in KihMMS will do that for you.
  at("AT+CMGF=1");
  at("AT+CSCS=\"GSM\"");
  modem.print("AT+CMGS=\"" NUMBER "\"\r\n");
  delay(500);                                   // wait for the '>' prompt
  modem.print("Hello from ESP32 and KihMMS");
  modem.write(0x1A);                            // Ctrl+Z ends the message
  at("", 15000);                                // +CMGS: <ref> and OK when sent
}

void loop() {
  while (modem.available()) Serial.write(modem.read());   // show whatever the modem says
}
