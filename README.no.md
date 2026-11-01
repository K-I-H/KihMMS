<p align="center"><img src="extras/logo.svg" alt="KihMMS-logo" width="560"></p>

# KihMMS

**MMS og SMS for ESP32 med SIMCom A76xx 4G-modem** – et Arduino-bibliotek.

> 🚧 **Under arbeid.** Repoet inneholder foreløpig kjernen på byte-nivå (lage MMS, dekode SMS i PDU-modus, lese
> MMS-varsler og -meldinger). Modemdriveren, PPP-overføringen og eksemplene flyttes inn fra prosjektet der alt ble
> laget og prøvd på ekte maskinvare. Se [Status](#status).

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
| `src/kih_pdu.h` | `KihMmsBuilder` (M-Send.req), `KihSmsPdu` (SMS-dekoder), `KihMmsParser` (varsel, hentet melding, deler). Ren C++ (uten Arduino-typer). |
| `extras/docs/FINDINGS.md` | Hva som stopper MMS på disse modemene, og hvordan det ble løst (på engelsk). |
| `extras/docs/OPERATORS.md` | MMS-innstillinger per operatør, og hvilke som er prøvd. |

## OTAMMS – nytt program over MMS

Programfila deles i biter, hver bit signeres (HMAC-SHA256 med en hemmelig nøkkel) og sendes som MMS-vedlegg. Kortet samler
bitene, sjekker CRC og signaturen, skriver programmet med `Update` og starter på nytt, og svarer på SMS for hver bit. En
Android-app sender bitene direkte. Prøvd: 620 KB i 3 MMS-er, ca. 2 minutter fra første melding til det nye programmet kjører.
Kommer hit sammen med driveren.

## Status

- [x] MMS-pakken, SMS-dekoderen, lesing av MMS-varsler og -meldinger
- [ ] Modemdriveren (`KihModem`)
- [ ] MMS over PPP (sende med vedlegg, hente og lagre på SD)
- [ ] Eksempler
- [ ] OTAMMS-mottaker, Python-pakker og Android-appen

## Lisens

MIT – se [LICENSE](LICENSE).
