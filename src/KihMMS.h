// KihMMS – MMS and SMS for ESP32 with SIMCom A76xx 4G modems.
//
// This is the header to include in your sketch:   #include <KihMMS.h>
//
// Version 0.1.0 (work in progress) contains the byte-level core only:
//   KihMmsBuilder  builds an MMS (M-Send.req) with text and an optional attachment
//   KihSmsPdu      decodes a received SMS in PDU mode (GSM 7-bit with æøå/€, UCS2 with emoji, 8-bit, long SMS)
//   KihMmsParser   reads MMS notifications and fetched messages, and lists their parts
// The modem driver, MMS over PPP and the examples follow in later versions. See README.md.
#pragma once
#define KIHMMS_VERSION "0.1.0"
#include "kih_pdu.h"
