<p align="center"><img src="extras/logo.svg" alt="KihMMS-logo" width="560"></p>

# KihMMS

[![Compile examples](https://github.com/K-I-H/KihMMS/actions/workflows/compile.yml/badge.svg)](https://github.com/K-I-H/KihMMS/actions/workflows/compile.yml) [![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE) ![ESP32](https://img.shields.io/badge/ESP32-arduino--esp32%203.x-orange) ![Modem](https://img.shields.io/badge/modem-SIMCom%20A76xx-green)


**MMS og SMS for ESP32 med SIMCom A76xx 4G-modem** – et Arduino-bibliotek.

```cpp
#include <KihMMS.h>
KihMms modem;

void setup() {
  modem.onSms = [](const char *fra, const char *tekst) { Serial.printf("%s: %s
", fra, tekst); };
  modem.onMms = [](const KihMmsReceived &m) { Serial.printf("MMS fra %s: %s (%d deler)
", m.from.c_str(), m.text.c_str(), m.count); };
  modem.begin(Serial1, 27, 26, 4, 12);                   // LilyGO T-A7670: RX, TX, PWRKEY, strøm
}
void loop() { modem.loop(); }                            // venter aldri
// modem.sendSms("+4712345678", "Hei 😀");   modem.sendMms("+4712345678", "Emne", "Tekst", jpeg, lengde);
```

🇬🇧 [English version](README.md)

## Hvorfor?

MMS fra en mikrokontroller regnes som en blindvei: de fleste A76xx-modemer har ingen MMS-kommandoer, og modemets egen
TCP kan være sperret av SIM-kortet. KihMMS går rundt begge deler:

- **ESP32 lager MMS-en selv** (OMA MMS 1.2 / WSP) – tekst, emoji, bilder, lyd eller hvilket som helst vedlegg.
- **MMS går over PPP**: ESP32 tar datatilkoblingen gjennom modemet og gjør TCP med sin egen nettstakk, så det virker selv
  når `AT+NETOPEN` bare svarer `ERROR`.
- **SMS i PDU-modus**, dekodet på ESP32: GSM-alfabetet med æøå og utvidelsestabellen (€ [ ] { } …), UCS2 med emoji, lange
  SMS-er satt sammen, og binære SMS-er – som er slik MMS-varsler (WAP push) kommer.

Alt er prøvd på et ekte kort med norsk SIM-kort (Ice): sende tekst og bilder, hente mottatte MMS (GIF, JPEG, ~300 KB), svare
automatisk, og **oppdatere programmet på kortet over MMS** (OTAMMS, se under).

## Installering

**Arduino IDE:** last ned [KihMMS-0.3.0.zip](https://github.com/K-I-H/KihMMS/archive/refs/tags/v0.3.0.zip), og velg
*Skisse → Inkluder bibliotek → Legg til .ZIP-bibliotek …* og fila. (Nyeste fra `main`:
[main.zip](https://github.com/K-I-H/KihMMS/archive/refs/heads/main.zip).)

**arduino-cli:**

```bash
arduino-cli config set library.enable_unsafe_install true
arduino-cli lib install --git-url https://github.com/K-I-H/KihMMS.git
```

Deretter `#include <KihMMS.h>`. Krever ESP32-kjernen (arduino-esp32 3.x). Slå på PSRAM i kortinnstillingene
(en MMS kan være flere hundre KB).

## Maskinvare

| | |
|---|---|
| **Modem** | SIMCom **A76xx**: A7670E, A7670G, A7670SA (prøvd: A7670SA-FASE, fastvare A011B09A7670M7) |
| **Kort** | **LilyGO T-A7670E/G/SA** (prøvd: T-A7670G R2, ESP32-WROVER-E med 8 MB PSRAM) – eller en ESP32 koblet til et A76xx |
| **ESP32-kjerne** | arduino-esp32 **3.x** (bruker det innebygde `PPP`-biblioteket) |
| **Anbefalt** | PSRAM (en MMS kan være 300 KB), SD-kort til bilder, et 18650-batteri eller god 5 V (modemet trekker opptil 2 A) |

Standardpinner (LilyGO T-A7670): modem TX/RX på GPIO 26/27, PWRKEY 4, strøm til modemet 12, SD-kort CS 13 / SCK 14 / MISO 2 / MOSI 15.

## Innhold

| Fil | Hva |
|---|---|
| `src/kih_mms.h` | **`KihMms`**: alt i ett objekt – SMS og MMS inn og ut, innkommende MMS hentes av seg selv, operatøren fra SIM-kortet. |
| `src/kih_at.h` | `KihAt`: én AT-kommando om gangen fra en kø, svarene leses i `loop()` uten å vente, meldinger fra modemet til `onUrc`. |
| `src/kih_power.h` | `KihPower`: slår på modemet (spør først, så et modem som går, ikke slås av av PWRKEY). |
| `src/kih_sms.h`, `src/kih_smsout.h` | `KihSms`: SMS lagret på SIM-kortet, lange SMS satt sammen; `KihSmsSubmit`: GSM 7-bit eller UCS2, lange SMS delt (emoji deles aldri). |
| `src/kih_ppp.h` | `KihPpp`: datatilkobling gjennom modemet med PPP; AT-laget tar pause og fortsetter etterpå. |
| `src/kih_mmsget.h`, `src/kih_mmssend.h` | `KihMmsGet` / `KihMmsSend`: hente og sende MMS gjennom operatørens MMS-proxy. |
| `src/kih_gnss.h` | `KihGnss`: posisjon, fart, tid og satellitter fra `AT+CGNSSINFO`. Ren C++. |
| `src/kih_pdu.h` | `KihMmsBuilder` (M-Send.req), `KihSmsPdu` (SMS-dekoder), `KihMmsParser` (varsel, hentet melding, deler). Ren C++ (uten Arduino-typer). |
| `extras/docs/FINDINGS.md` | Hva som stopper MMS på disse modemene, og hvordan det ble løst (på engelsk). |
| `extras/docs/OPERATORS.md` | MMS-innstillinger per operatør, og hvilke som er prøvd. |
| `extras/docs/AT-COMMANDS.md` | Jukselapp: A76xx-kommandoene for SMS (og GNSS), med innstillingene som virker og hvorfor (på engelsk). |

## Eksempler

Åpnes i Arduino IDE under *Fil → Eksempler → KihMMS*.

| Eksempel | Hva det viser | Trenger |
|---|---|---|
| **BuildMms** | Lag en MMS med emne, tekst (æøå, emoji) og vedlegg; skriv den ut og les den tilbake | hvilken som helst ESP32 |
| **DecodeSmsPdu** | Dekod SMS i PDU-modus: GSM-alfabetet med æøå og €, UCS2 med emoji, avsender med bokstaver | hvilken som helst ESP32 |
| **ParseMmsNotification** | Les et MMS-varsel: avsender, emne og adressen der MMS-en hentes | hvilken som helst ESP32 |
| **SendSms** | Slå på modemet og send en SMS med AT-kommandoer ✅ *prøvd på T-A7670G* | LilyGO T-A7670 + SIM |
| **ReceiveSms** | Motta SMS (lagret på SIM-kortet, lest i PDU-modus) og dekod dem; kjenn igjen MMS-varsler ✅ *prøvd på T-A7670G* | LilyGO T-A7670 + SIM |
| **SmsAndMms** | `KihMms`: SMS og MMS inn og ut, svar på MMS med SMS, send et bilde ESP32-en tegner selv ✅ *prøvd på T-A7670G* | LilyGO T-A7670 + SIM |
| **GnssPosition** | Posisjon fra modemets GNSS-mottaker hvert 5. sekund, med kartlenke ✅ *prøvd på T-A7670G* | LilyGO T-A7670G + GPS-antenne |

## OTAMMS – nytt program over MMS

Programfila deles i biter, hver bit signeres (HMAC-SHA256 med en hemmelig nøkkel) og sendes som MMS-vedlegg. Kortet samler
bitene, sjekker CRC og signaturen, skriver programmet med `Update` og starter på nytt, og svarer på SMS for hver bit. En
Android-app sender bitene direkte. Prøvd: 620 KB i 3 MMS-er, ca. 2 minutter fra første melding til det nye programmet kjører.
Ikke en del av biblioteket (ennå).

## Status

- [x] MMS-pakken, SMS-dekoderen, lesing av MMS-varsler og -meldinger
- [x] Modemdriveren: AT-kø, påslåing, SMS inn og ut (lagret på SIM, lange SMS, emoji), GNSS
- [x] MMS over PPP: hente og sende, med automatisk henting og operatøren fra SIM-kortet (`KihMms`)
- [x] Eksempler: lag MMS, dekod SMS, les MMS-varsel, send SMS, motta SMS, SMS og MMS, GNSS-posisjon
- [ ] Eksempler: bilde fra SD-kortet, lagre mottatt MMS på SD
- [ ] OTAMMS-mottaker, Python-pakker og Android-appen

## Lisens

MIT – se [LICENSE](LICENSE).
