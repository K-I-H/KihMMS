// KihMMS example: ParseMmsNotification
//
// When someone sends an MMS to the board, the operator first sends a binary SMS (a WAP push on port 2948) with an
// m-notification-ind inside. It says who sent it, the subject, and the address (URL) where the message can be fetched.
// This example reads such a notification. The sample is made up (the number +4700000000 is not real). No modem needed.
//
// Fetching the message itself is an HTTP GET of that URL through the operator's MMS proxy, over the MMS APN.
#include <KihMMS.h>

static const char *notification =
  "00440A917400000000000462111020532540660605040B8423F0010603BEAF848C8298543432008D928917802B343730303030303030302F"
  "545950453D504C4D4E009642696C6465206672612066657269656E008A808E03012A3888058103093A8083687474703A2F2F6D6D732F4D4D"
  "313F41626331323300";

static KihSmsPdu sms;
static KihMmsParser mms;

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("KihMMS " KIHMMS_VERSION " – ParseMmsNotification");
  if (!sms.parse(notification) || !sms.binary) { Serial.println("Not a binary SMS"); return; }
  if (!mms.notification(sms.data, sms.dataLen)) { Serial.println("Not an MMS notification"); return; }
  Serial.printf("MMS from:  %s\n", mms.from);       // note: no "+" here, unlike in SMS – compare the last digits
  Serial.printf("Subject:   %s\n", mms.subject);
  Serial.printf("Fetch at:  %s\n", mms.url);
  Serial.printf("Transaction id: %s\n", mms.tid);
}

void loop() {}
