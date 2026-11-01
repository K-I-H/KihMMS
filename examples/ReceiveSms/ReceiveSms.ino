// KihMMS example: ReceiveSms
//
// Receives SMS on a LilyGO T-A7670 in PDU mode and decodes them with KihSmsPdu: æøå, €, emoji and senders with letters
// all come out as UTF-8. Binary SMS are recognised too, and if one is an MMS notification, the sender, subject and the
// address of the MMS are printed.
//
// Why store on the SIM (AT+CNMI=2,1) instead of direct delivery? While the modem is busy in data mode (e.g. fetching an MMS
// over PPP) directly delivered messages can be lost. Stored ones wait on the SIM until we read them.
#include <KihMMS.h>

#define SIM_PIN ""
#define MODEM_TX 26
#define MODEM_RX 27
#define MODEM_PWRKEY 4
#define MODEM_POWER 12

HardwareSerial modem(1);
static KihSmsPdu sms;
static KihMmsParser mms;
String line;
bool pduNext = false;

void cmd(const String &c) { modem.print(c); modem.print("\r\n"); delay(300); while (modem.available()) modem.read(); }

void setup() {
  Serial.begin(115200);
  modem.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);
  pinMode(MODEM_POWER, OUTPUT); digitalWrite(MODEM_POWER, HIGH);
  pinMode(MODEM_PWRKEY, OUTPUT);
  digitalWrite(MODEM_PWRKEY, HIGH); delay(1000); digitalWrite(MODEM_PWRKEY, LOW);
  Serial.println("Waiting for the modem …");
  for (int i = 0; i < 30; i++) {
    modem.print("AT\r\n"); delay(500);
    String r; while (modem.available()) r += (char)modem.read();
    if (r.indexOf("OK") >= 0) break;
  }
  cmd("ATE0");                               // no echo
  if (strlen(SIM_PIN)) cmd(String("AT+CPIN=\"") + SIM_PIN + "\"");
  cmd("AT+CMGF=0");                          // PDU mode
  cmd("AT+CNMI=2,1,0,0,0");                  // store on the SIM, tell us with +CMTI
  modem.print("AT+CMGL=4\r\n");              // and read what is already stored
  Serial.println("Ready – send an SMS to the board.");
}

void handle(const String &l) {
  if (l.startsWith("+CMTI:")) {              // +CMTI: "SM",3 → read message 3, then delete it
    String i = l.substring(l.lastIndexOf(',') + 1);
    modem.print("AT+CMGR=" + i + "\r\n");
    delay(200);
    modem.print("AT+CMGD=" + i + "\r\n");
    return;
  }
  if (l.startsWith("+CMGR:") || l.startsWith("+CMGL:")) { pduNext = true; return; }
  if (!pduNext) return;
  pduNext = false;
  if (!sms.parse(l.c_str())) return;
  if (!sms.binary) {
    Serial.printf("SMS from %s: %s", sms.from, sms.text);
    if (sms.count > 1) Serial.printf("  (part %d of %d)", sms.part, sms.count);
    Serial.println();
  } else if (mms.notification(sms.data, sms.dataLen)) {
    Serial.printf("MMS from %s, subject \"%s\", fetch at %s\n", mms.from, mms.subject, mms.url);
  } else {
    Serial.printf("Binary SMS from %s (%d bytes)\n", sms.from, sms.dataLen);
  }
}

void loop() {
  while (modem.available()) {
    char c = modem.read();
    if (c == '\n') { line.trim(); if (line.length()) handle(line); line = ""; }
    else if (c != '\r') line += c;
  }
}
