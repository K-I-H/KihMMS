<p align="center"><img src="extras/logo.svg" alt="KihMMS logo" width="560"></p>

# KihMMS

[![Compile examples](https://github.com/K-I-H/KihMMS/actions/workflows/compile.yml/badge.svg)](https://github.com/K-I-H/KihMMS/actions/workflows/compile.yml) [![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE) ![ESP32](https://img.shields.io/badge/ESP32-arduino--esp32%203.x-orange) ![Modem](https://img.shields.io/badge/modem-SIMCom%20A76xx-green)


**MMS and SMS for ESP32 with SIMCom A76xx 4G modems** – an Arduino library.

```cpp
#include <KihMMS.h>
KihMms modem;

void setup() {
  modem.onSms = [](const char *from, const char *text) { Serial.printf("%s: %s
", from, text); };
  modem.onMms = [](const KihMmsReceived &m) { Serial.printf("MMS from %s: %s (%d parts)
", m.from.c_str(), m.text.c_str(), m.count); };
  modem.begin(Serial1, 27, 26, 4, 12);                   // LilyGO T-A7670: RX, TX, PWRKEY, power
}
void loop() { modem.loop(); }                            // never blocks
// modem.sendSms("+4712345678", "Hei 😀");   modem.sendMms("+4712345678", "Subject", "Text", jpeg, len);
```

🇳🇴 [Norsk versjon](README.no.md)

## Why?

Sending an MMS from a microcontroller is usually considered a dead end: most A76xx modems have no MMS commands, and the
modem's own TCP stack can be blocked by the SIM card. KihMMS gets around both:

- **The ESP32 builds the MMS itself** (OMA MMS 1.2 / WSP encoding) – text, emoji, pictures, sound or any binary attachment.
- **MMS goes over PPP**: the ESP32 takes the data connection through the modem and does TCP with its own network stack,
  so it works even when `AT+NETOPEN` only answers `ERROR`.
- **SMS in PDU mode**, decoded on the ESP32: the GSM 7-bit alphabet with æøå and the extension table (€ [ ] { } …), UCS2
  with emoji (surrogate pairs), long SMS joined, and binary SMS – which is how MMS notifications (WAP push) arrive.

Everything has been tried on a real board with a Norwegian SIM card (Ice): sending text and pictures, fetching received MMS
(GIF, JPEG, ~300 KB), replying automatically, and **updating the board's firmware over MMS** (OTAMMS, see below).

## Install

**Arduino IDE:** download [KihMMS-0.3.0.zip](https://github.com/K-I-H/KihMMS/archive/refs/tags/v0.3.0.zip), then
*Sketch → Include Library → Add .ZIP Library…* and pick the file. (Newest from `main`:
[main.zip](https://github.com/K-I-H/KihMMS/archive/refs/heads/main.zip).)

**arduino-cli:**

```bash
arduino-cli config set library.enable_unsafe_install true
arduino-cli lib install --git-url https://github.com/K-I-H/KihMMS.git
```

Then `#include <KihMMS.h>`. Requires the ESP32 core (arduino-esp32 3.x). Enable PSRAM in the board settings (an MMS can be
several hundred KB).

## Hardware

| | |
|---|---|
| **Modem** | SIMCom **A76xx** family: A7670E, A7670G, A7670SA (tested: A7670SA-FASE, firmware A011B09A7670M7) |
| **Board** | **LilyGO T-A7670E/G/SA** (tested: T-A7670G R2, ESP32-WROVER-E with 8 MB PSRAM) – or any ESP32 wired to an A76xx |
| **ESP32 core** | arduino-esp32 **3.x** (uses the built-in `PPP` library) |
| **Recommended** | PSRAM (an MMS can be 300 KB), an SD card for pictures, a 18650 battery or a good 5 V supply (the modem draws up to 2 A) |

Default pins (LilyGO T-A7670): modem TX/RX on GPIO 26/27, PWRKEY 4, modem power 12, SD card CS 13 / SCK 14 / MISO 2 / MOSI 15.

## What's in the box

| File | What |
|---|---|
| `src/kih_mms.h` | **`KihMms`**: everything in one object – SMS and MMS in and out, incoming MMS fetched by themselves, operator settings from the SIM. |
| `src/kih_at.h` | `KihAt`: one AT command at a time from a queue, answers read in `loop()` without waiting, unsolicited lines to `onUrc`. |
| `src/kih_power.h` | `KihPower`: switches the modem on (asks first, so a running modem is not switched off by PWRKEY). |
| `src/kih_sms.h`, `src/kih_smsout.h` | `KihSms`: SMS stored on the SIM, long SMS joined; `KihSmsSubmit`: GSM 7-bit or UCS2, long SMS split (emoji never broken). |
| `src/kih_ppp.h` | `KihPpp`: a PPP data connection through the modem; the AT layer pauses and continues afterwards. |
| `src/kih_mmsget.h`, `src/kih_mmssend.h` | `KihMmsGet` / `KihMmsSend`: fetch and send MMS through the operator's MMS proxy. |
| `src/kih_gnss.h` | `KihGnss`: position, speed, time and satellites from `AT+CGNSSINFO`. Plain C++. |
| `src/kih_pdu.h` | `KihMmsBuilder` (M-Send.req), `KihSmsPdu` (SMS-DELIVER decoder), `KihMmsParser` (notification, retrieve-conf, parts). Plain C++ (no Arduino types). |
| `extras/docs/FINDINGS.md` | What blocks MMS on these modems and how it was solved – the notes we wish we had found. |
| `extras/docs/OPERATORS.md` | MMS settings (APN, MMSC, proxy) per operator, and which are tested. |
| `extras/docs/AT-COMMANDS.md` | Cheat sheet: the A76xx AT commands for SMS (and GNSS), with the settings that work and why. |

## Examples

Open them in the Arduino IDE under *File → Examples → KihMMS*.

| Example | What it shows | Needs |
|---|---|---|
| **BuildMms** | Build an MMS with subject, text (æøå, emoji) and an attachment; print it and read it back | any ESP32 |
| **DecodeSmsPdu** | Decode SMS in PDU mode: GSM 7-bit with æøå and €, UCS2 with emoji, sender with letters | any ESP32 |
| **ParseMmsNotification** | Read an MMS notification: sender, subject and the URL where the MMS is fetched | any ESP32 |
| **SendSms** | Switch on the modem and send an SMS with AT commands ✅ *tested on T-A7670G* | LilyGO T-A7670 + SIM |
| **ReceiveSms** | Receive SMS (stored on the SIM, read in PDU mode) and decode them; recognise MMS notifications ✅ *tested on T-A7670G* | LilyGO T-A7670 + SIM |
| **SmsAndMms** | `KihMms`: SMS and MMS in and out, reply to an MMS by SMS, send a picture the ESP32 draws itself ✅ *tested on T-A7670G* | LilyGO T-A7670 + SIM |
| **GnssPosition** | Position from the modem's GNSS receiver every 5 s, with a map link ✅ *tested on T-A7670G* | LilyGO T-A7670G + GPS antenna |

## OTAMMS – firmware over MMS

A new program for the board, delivered as MMS: the program file is split into pieces, each piece is signed (HMAC-SHA256
with a secret key) and sent as an MMS attachment. The board collects the pieces, checks the CRC of each piece and the
signature of the whole program, writes it with `Update` and restarts. The board replies by SMS for each piece. An Android
app sends the pieces directly (ordinary messaging apps filter unknown attachments). Tested: 620 KB in 3 MMS, about 2 minutes
from the first message to the new program running. Not part of this library (yet).

## Status

- [x] MMS encoder, SMS PDU decoder, MMS notification/message parser
- [x] Modem driver: AT queue, power-on, SMS in and out (SIM storage, long SMS, emoji), GNSS
- [x] MMS over PPP: fetch and send, with automatic fetching and operator settings from the SIM (`KihMms`)
- [x] Examples: build MMS, decode SMS, parse MMS notification, send SMS, receive SMS, SMS and MMS, GNSS position
- [ ] Examples: picture from SD card, save received MMS to SD
- [ ] OTAMMS receiver, Python packer and the Android sender app

## License

MIT – see [LICENSE](LICENSE).
