// KihMMS – MMS and SMS for ESP32 with SIMCom A76xx 4G modems.
//
// This is the header to include in your sketch:   #include <KihMMS.h>
//
// The easy way: one object.
//   KihMms         SMS and MMS in and out, incoming MMS fetched by themselves, operator settings from the SIM (kih_mms.h)
//
// The parts it is made of (use them on their own if you want):
//   KihAt          one AT command at a time from a queue, answers read without waiting, unsolicited lines to onUrc
//   KihPower       switches the modem on (PWRKEY) and waits until it answers
//   KihSms         SMS in (stored on the SIM, long SMS joined) and out (KihSmsSubmit: GSM 7-bit or UCS2, long SMS split)
//   KihPpp         a data connection through the modem with PPP (the ESP32 does TCP itself)
//   KihMmsGet      fetches an MMS over PPP;  KihMmsSend posts one
//   KihGnss        position from the modem's GNSS receiver (AT+CGNSSINFO)
// Byte level, plain C++ (no Arduino), testable on a PC:
//   KihMmsBuilder  builds an MMS (M-Send.req)
//   KihSmsPdu      decodes a received SMS in PDU mode (GSM 7-bit with æøå/€, UCS2 with emoji, 8-bit, long SMS)
//   KihMmsParser   reads MMS notifications and fetched messages, and lists their parts
#pragma once
#define KIHMMS_VERSION "0.3.0"
#include "kih_pdu.h"
#include "kih_smsout.h"
#include "kih_gnss.h"
#include "kih_fwpiece.h"
#ifdef ARDUINO
#include "kih_mms.h"
#include "kih_otamms.h"
#endif
