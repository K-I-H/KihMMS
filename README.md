<p align="center"><img src="extras/logo.svg" alt="KihMMS logo" width="560"></p>

# KihMMS

[![Compile examples](https://github.com/K-I-H/KihMMS/actions/workflows/compile.yml/badge.svg)](https://github.com/K-I-H/KihMMS/actions/workflows/compile.yml) [![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE) ![ESP32](https://img.shields.io/badge/ESP32-arduino--esp32%203.x-orange) ![Modem](https://img.shields.io/badge/modem-SIMCom%20A76xx-green)

**Send and receive MMS and SMS from an ESP32 with a SIMCom A76xx 4G modem** (LilyGO T-A7670) – an Arduino library with no
other dependencies.

```cpp
#include <KihMMS.h>
#include <SD.h>
KihMms modem;

void setup() {
  Serial.begin(115200);
  SPI.begin(KIH_A7670_SD_SCK, KIH_A7670_SD_MISO, KIH_A7670_SD_MOSI);
  SD.begin(KIH_A7670_SD_CS);
  modem.onReady = [] { modem.sendMmsFile("+4712345678", "Hello", "A picture from my ESP32", SD, "/photo.jpg"); };
  modem.onSms = [](const char *from, const char *text) { Serial.printf("SMS from %s: %s\n", from, text); };
  modem.onMms = [](const KihMmsReceived &m) { m.save(m.findPart("image/"), SD, "/received.jpg"); };
  modem.begin();                       // LilyGO T-A7670 pins
}
void loop() { modem.loop(); }          // never blocks
```

🇳🇴 [Norsk versjon](README.no.md)

## Quick start (5 minutes)

1. **Board:** a LilyGO T-A7670E/G/SA with a SIM card that has mobile data and MMS on the subscription.
2. **Arduino IDE:** install *esp32 by Espressif* (3.x) in the Boards Manager, and KihMMS (see [Install](#install)).
3. **Board settings:** *Tools → Board → ESP32 Dev Module* (or *ESP32 Wrover Module*), *Tools → PSRAM → Enabled*.
4. Open *File → Examples → KihMMS → **SendMms***, write your phone number in `TO_NUMBER`, and upload.
5. Open the Serial Monitor (115200). After 10–30 s: `✅ MMS sent` – and the picture arrives on your phone.

The MMS settings (APN, MMSC, proxy) are found from the SIM card for the operators in
[OPERATORS.md](extras/docs/OPERATORS.md). For others, add one line: `modem.setOperator("apn", "http://mmsc", "proxy", 8080);`

## Examples

Open them in the Arduino IDE under *File → Examples → KihMMS*. **Start with SendMms.**

| Example | What it does | Needs |
|---|---|---|
| ⭐ **SendMms** | The shortest way: sends one MMS with a subject, a text and a picture (built in, no SD card) | T-A7670 + SIM |
| 📷 **MmsFromSdCard** | SMS "photo" to the board → it answers with an MMS of `/photo.jpg` from the SD card; "list" and "send &lt;file&gt;" too | T-A7670 + SIM + SD card |
| 💾 **SaveMmsToSd** | Every picture, sound or video that arrives by MMS is saved on the SD card, and the sender gets an SMS receipt | T-A7670 + SIM + SD card |
| 💬 **SmsAndMms** | SMS and MMS in and out, reply to an MMS by SMS, commands from the Serial Monitor, a picture the ESP32 draws itself ✅ *tested* | T-A7670 + SIM |
| 📨 **OtaMms** | OTAMMS: a new program for the board over MMS, with signature check and confirmation ✅ *tested* | T-A7670 + SIM, the OTAMMS app |
| 📍 **GnssPosition** | Position from the modem's GNSS receiver every 5 s, with a map link ✅ *tested* | T-A7670G + GPS antenna |
| ✉️ **SendSms** | Switch on the modem and send an SMS, step by step with the low-level parts ✅ *tested* | T-A7670 + SIM |
| 📥 **ReceiveSms** | Receive SMS (stored on the SIM, PDU mode) and recognise MMS notifications, with the low-level parts ✅ *tested* | T-A7670 + SIM |
| 🧱 **BuildMms** | Build an MMS byte by byte and read it back – no modem needed | any ESP32 |
| 🔍 **DecodeSmsPdu** | Decode SMS in PDU mode: æøå, €, emoji, sender with letters – no modem needed | any ESP32 |
| 🔔 **ParseMmsNotification** | Read an MMS notification: sender, subject, where to fetch it – no modem needed | any ESP32 |

✅ *tested* = run on a LilyGO T-A7670G R2 with a Norwegian SIM card (Ice). The others compile for ESP32 in CI.

## The API in one table

Everything goes through one object, `KihMms modem;`. `loop()` never waits, so your own code keeps running.

| Call | What |
|---|---|
| `modem.begin()` | Start with the LilyGO T-A7670 pins. Other boards: `modem.begin(Serial1, rx, tx, pwrkey, powerEnable)` |
| `modem.loop()` | Call in `loop()` – does all the work, never blocks |
| `modem.onReady = [] { … }` | The modem is on and the SIM has been read: you can send now |
| `modem.sendSms(number, text)` | SMS with æøå, € and emoji; long texts are split. Optional 3rd argument: `[](bool ok) { … }` |
| `modem.sendMms(number, subject, text)` | MMS with text only |
| `modem.sendMms(number, subject, text, data, len, "image/jpeg", "photo.jpg")` | MMS with an attachment from memory (it is copied) |
| `modem.sendMmsFile(number, subject, text, SD, "/photo.jpg")` | MMS with a file from SD, SD_MMC or LittleFS; the type comes from the extension |
| `modem.onMmsSent = [](bool ok, const char *status) { … }` | The result of the MMS |
| `modem.onSms = [](const char *from, const char *text) { … }` | An SMS arrived |
| `modem.onMms = [](const KihMmsReceived &m) { … }` | An MMS arrived and has been fetched: `m.from`, `m.subject`, `m.text`, `m.count`, `m.parts[i].type`/`.len`, `m.partData(i)` |
| `m.findPart("image/")`, `m.save(i, SD, "/x.jpg")`, `m.extension(i)` | Find a part, save it as a file, a fitting file extension |
| `modem.lastError()` | Why the last `send…` returned `false` |
| `modem.mmsReady()`, `modem.networkCode()` | MMS settings known? The SIM's network code (e.g. `24214`) |
| `modem.setOperator(apn, mmsc, proxy, port)` | MMS settings by hand |

One MMS is sent at a time: wait for `onMmsSent` before the next one. Most operators accept up to about 300 KB per MMS.

## Troubleshooting

| Problem | Try this |
|---|---|
| Nothing happens after `Starting the modem` | The modem needs a good supply (up to 2 A): use a charged 18650 battery or a strong USB port. The first start can take 30 s. |
| `MMS settings … unknown` | Your operator is not in the table: find the MMS settings on the operator's web page and use `modem.setOperator(…)` (and please tell us in an issue!). |
| `MMS NOT sent: no PPP connection` | Mobile data is off on the subscription, or there is no coverage. Check that SMS works first. |
| `not enough memory` | *Tools → PSRAM → Enabled*. |
| An MMS arrives on the phone, but without the picture | The picture is too big for the operator: keep it under ~300 KB. |
| Upload fails with *Wrong boot mode* | The SD card holds GPIO 2 high on the T-A7670: take it out while uploading, or hold the BOOT button. |
| The sender is `47…` without `+` | Fixed by the library: `m.from` can be answered directly. |

More background: [FINDINGS.md](extras/docs/FINDINGS.md) (what blocks MMS on these modems and how it was solved) and
[AT-COMMANDS.md](extras/docs/AT-COMMANDS.md).

## Install

**Arduino IDE:** download [KihMMS-0.4.0.zip](https://github.com/K-I-H/KihMMS/archive/refs/tags/v0.4.0.zip), then
*Sketch → Include Library → Add .ZIP Library…* and pick the file. (Newest from `main`:
[main.zip](https://github.com/K-I-H/KihMMS/archive/refs/heads/main.zip).)

**arduino-cli:**

```bash
arduino-cli config set library.enable_unsafe_install true
arduino-cli lib install --git-url https://github.com/K-I-H/KihMMS.git
```

**PlatformIO:** `lib_deps = https://github.com/K-I-H/KihMMS.git` (with the arduino-esp32 3.x platform, and PSRAM enabled).

Then `#include <KihMMS.h>`. Requires the ESP32 core (arduino-esp32 3.x).

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

## Hardware

| | |
|---|---|
| **Modem** | SIMCom **A76xx** family: A7670E, A7670G, A7670SA (tested: A7670SA-FASE, firmware A011B09A7670M7) |
| **Board** | **LilyGO T-A7670E/G/SA** (tested: T-A7670G R2, ESP32-WROVER-E with 8 MB PSRAM) – or any ESP32 wired to an A76xx |
| **ESP32 core** | arduino-esp32 **3.x** (uses the built-in `PPP` library) |
| **Recommended** | PSRAM (an MMS can be 300 KB), an SD card for pictures, a 18650 battery or a good 5 V supply (the modem draws up to 2 A) |

Pins on the LilyGO T-A7670 (also as constants): modem RX/TX `KIH_A7670_RX` 27 / `KIH_A7670_TX` 26, `KIH_A7670_PWRKEY` 4,
modem power `KIH_A7670_POWER` 12, SD card `KIH_A7670_SD_CS` 13 / `_SCK` 14 / `_MISO` 2 / `_MOSI` 15.

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
| `src/kih_otamms.h`, `src/kih_fwpiece.h` | `KihOtaMms`: OTAMMS receiver (number, CRC, version, signature, Update, confirmation); `KihFwPiece` / `KihSha256`: the piece format and HMAC-SHA256, plain C++. |
| `src/kih_pdu.h` | `KihMmsBuilder` (M-Send.req), `KihSmsPdu` (SMS-DELIVER decoder), `KihMmsParser` (notification, retrieve-conf, parts). Plain C++ (no Arduino types). |
| `extras/docs/FINDINGS.md` | What blocks MMS on these modems and how it was solved – the notes we wish we had found. |
| `extras/docs/OPERATORS.md` | MMS settings (APN, MMSC, proxy) per operator, and which are tested. |
| `extras/docs/AT-COMMANDS.md` | Cheat sheet: the A76xx AT commands for SMS (and GNSS), with the settings that work and why. |

## OTAMMS – firmware over MMS

A new program for the board, delivered as MMS: the program file is split into pieces, each piece is signed (HMAC-SHA256
with a secret key) and sent as an MMS attachment. The board collects the pieces, checks the CRC of each piece and the
signature of the whole program, writes it with `Update` and restarts. The board replies by SMS for each piece. An Android
app sends the pieces directly (ordinary messaging apps filter unknown attachments). The version in each package is the time it
was signed: a newer package is always accepted (also with an older program), but an old package can not be sent again. When the
new program runs, it says so by SMS. The receiver is `KihOtaMms` (example **OtaMms**). Tested: 1 MB in 5 MMS, about 5 minutes
from the first message to the confirmation. The Android app is not in this repository yet.

## Status

- [x] MMS encoder, SMS PDU decoder, MMS notification/message parser
- [x] Modem driver: AT queue, power-on, SMS in and out (SIM storage, long SMS, emoji), GNSS
- [x] MMS over PPP: fetch and send, with automatic fetching and operator settings from the SIM (`KihMms`)
- [x] Simple API: `begin()` with the board's pins, `onReady`, `sendMmsFile`, `m.save`, `lastError`
- [x] Examples: send MMS, MMS from SD card, save received MMS to SD, SMS and MMS, OTAMMS, GNSS, and the low-level ones
- [x] OTAMMS receiver (`KihOtaMms`) with version check and confirmation
- [ ] OTAMMS Python packer and the Android sender app
- [ ] More operators tested (please open an issue with yours!)

## License

MIT – see [LICENSE](LICENSE).
