# A76xx AT commands for SMS – a cheat sheet

The commands KihMMS and its examples use, with the settings that turned out to work on A7670SA-FASE (LilyGO T-A7670G),
and why. Every command ends with `\r\n`; the modem answers `OK` or `ERROR`/`+CME ERROR:`/`+CMS ERROR:`.

## Getting started

| Command | What | Notes |
|---|---|---|
| `AT` | Is the modem alive? | Ask *before* pulsing PWRKEY: a pulse switches a running modem **off**. |
| `ATE0` | No echo | Otherwise every command comes back as a line before the answer. |
| `AT+CMEE=2` | Error messages as text | Not supported on every firmware; harmless if it fails. |
| `AT+CPIN?` / `AT+CPIN="1234"` | SIM status / enter PIN | `+CPIN: READY` when unlocked. |
| `AT+CEREG?` | On the LTE network? | `+CEREG: n,stat`: stat 1 = home network, 5 = roaming. As an unsolicited line (`AT+CEREG=1`) it is just `+CEREG: stat`. |
| `AT+CSQ` | Signal | `+CSQ: rssi,ber`, rssi 0–31 → dBm = −113 + 2·rssi (99 = unknown). |
| `AT+COPS=3,0` then `AT+COPS?` | Operator name | Without `COPS=3,0` the name may come as digits (e.g. `24214`). |
| `AT+CIMI` | IMSI | First five digits = country + network code: find the operator (and its MMS settings) automatically. |

Wait for **`SMS DONE`** (and `PB DONE`) after power-on before sending SMS. Being registered on the network is not enough:
an SMS sent too early ends with `+CME ERROR: AT command timeout` a minute or two later.

## Receiving SMS

| Command | What | Notes |
|---|---|---|
| `AT+CMGF=0` | PDU mode | Needed to receive MMS notifications (binary SMS). Text mode destroys them. |
| `AT+CNMI=2,1,0,0,0` | Store new SMS, announce with `+CMTI: "SM",<index>` | Safer than `2,2` (direct delivery as `+CMT:`), which loses messages while the modem is busy in data mode (PPP). |
| `AT+CMGR=<index>` | Read one | `+CMGR: <stat>,,<length>` then the PDU as hex on the next line. |
| `AT+CMGD=<index>` | Delete one | Delete after reading, or the SIM fills up (often only 20 places). |
| `AT+CMGL=4` | List all (PDU mode) | `+CMGL: <index>,<stat>,,<length>` + PDU line for each. Do it at start-up and after every data session. |
| `AT+CMGD=1,4` | Delete all | The index is ignored with delete flag 4. |
| `AT+CPMS?` | Storages and how full they are | `"SM"` = SIM (e.g. 20 places), `"ME"` = modem memory (e.g. 180). `AT+CPMS="ME","ME","ME"` switches. |

## Sending SMS

| Command | What | Notes |
|---|---|---|
| `AT+CMGF=1` | Text mode (for sending) | Switch back to `AT+CMGF=0` afterwards if you receive in PDU mode. |
| `AT+CSCS="GSM"` + `AT+CSMP=17,167,0,0` | Plain text, up to 160 characters | The GSM alphabet is not ASCII: `@ $ _` and the extension characters (`€ [ ] { } \ ~ ^ \|`) need care. |
| `AT+CSCS="UCS2"` + `AT+CSMP=17,167,2,25` | Unicode: æøå, any symbol, emoji – up to 70 characters | Number and text are then given as UTF-16 hex; emoji are surrogate pairs (e.g. 😀 = `D83DDE00`). |
| `AT+CMGS="<number>"` | Send | Wait for the `>` prompt, send the text, end with Ctrl+Z (0x1A). Answer: `+CMGS: <ref>` then `OK`. Allow up to 60 s. |

## GNSS (A7670G and other models with GPS)

| Command | What | Notes |
|---|---|---|
| `AT+CGNSSPWR=1` | GNSS on | Answers `+CGNSSPWR: READY!` when ready. |
| `AT+CGNSSTST=0` + `AT+CGNSSPORTSWITCH=1,0` | No raw NMEA on the AT port | Otherwise `$GPGGA…` lines flood the AT channel and end up in your SMS texts. |
| `AT+CGNSSINFO` | Position | The number of satellite fields varies between firmwares: find the `N`/`S` field and count from there. Speed is in knots. |

## Lines that are not answers to anything

| Line | Meaning |
|---|---|
| `+CMTI: "SM",3` | A new SMS stored at index 3 |
| `+CLIP: "+47…",145` | Someone is calling (with `AT+CLIP=1`) |
| `+MSTK: …` | SIM toolkit traffic. Some SIM cards (Ice) send these all the time; ignore them. |
| `+CGEV: …` | A data context went up or down |
| `SMS DONE`, `PB DONE` | The SIM's SMS storage / phone book is ready |
