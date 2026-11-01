# Findings: MMS and SMS on SIMCom A76xx with an ESP32

Notes from getting MMS working on a LilyGO T-A7670G R2 (A7670SA-FASE) with a Norwegian Ice SIM card, November 2026.
Each item is something that cost hours – written down so it doesn't have to cost them again.

## 1. The modem has no MMS commands

`AT+CMMSINIT` → `ERROR` on A7670SA-FASE. There is nothing in the modem that builds or sends MMS. Solution: build the
M-Send.req ourselves (it is a WSP-encoded header block plus a multipart body) and POST it to the operator's MMSC.

## 2. `AT+NETOPEN` answers `ERROR` – the SIM card is busy

The modem's own TCP stack (`AT+NETOPEN`, `AT+CIPOPEN`) only returned `ERROR`, with every APN and every order of commands.
The MMS PDP context itself was fine (`AT+CGACT?` active, `AT+CGPADDR` had an address). The log was flooded with `+MSTK:`
lines: the SIM toolkit was running a BIP data channel (SEND DATA / RECEIVE DATA proactive commands) all the time, and the
modem refused to open its own socket. `AT+MSTK=0` and `AT+STK` are not supported on this firmware.

**Solution: PPP.** arduino-esp32 3.x has a `PPP` library (`PPP.begin(PPP_MODEM_SIM7600, …)`). The ESP32 dials up through
the modem on the MMS APN and does TCP with its own lwIP stack, then talks HTTP to the MMS proxy. The modem's TCP stack is
not involved at all.

## 3. While PPP runs, SMS notifications get lost

During PPP the UART is in data mode. MMS notifications (binary SMS) that arrive meanwhile were simply gone, so pieces of a
multi-MMS transfer never showed up. **Solution:** let the modem store every SMS on the SIM (`AT+CNMI=2,1` → `+CMTI:`), read
and delete them (`AT+CMGR`, `AT+CMGD`), and after every PPP session list what is stored (`AT+CMGL=4`, then `AT+CMGD=1,4`).
Do the same every couple of minutes as a safety net.

## 4. Text mode ruins MMS notifications

In text mode (`AT+CMGF=1`) the WAP push SMS is turned into nonsense and the MMS address is lost. Use PDU mode
(`AT+CMGF=0`) and decode SMS yourself: GSM 7-bit (not ASCII: `@` is 0x00, `Å` is 0x0E …) with the ESC extension table,
UCS2 with surrogate pairs for emoji, and 8-bit for the binary ones. Long SMS come as separate parts (UDH IE 00 or 08) and
can arrive in any order.

## 5. The sender number differs between SMS and MMS

SMS arrive from `+4712345678`, but the `From` header of an MMS from the same phone was `4712345678/TYPE=PLMN` (no `+`).
A filter like `from.endsWith("+4712345678")` silently rejects every MMS. Compare the last 8 digits instead.

## 6. MMS settings for Ice (Norway)

APN `mms.ice.net`, MMSC `http://mms`, proxy `10.10.10.10` port 80. The proxy is not reachable over the ordinary internet
APN (HTTP error 714), only over the MMS APN.

## 7. The response status byte

In the M-Send.conf, `0x8D 0x92` is the MMS version (1.2), not the response status. The response status is the byte after
`0x92` when it is *not* preceded by `0x8D`. Our first test reported "failed (0x92)" while the MMS had arrived fine.

## 8. Phones filter what you can attach

Google Messages on a Motorola refused to attach `.mid` and `.bin` files picked from storage ("couldn't add attachment"),
although the same MIDI file received as MMS could be played and saved. Contact cards (`.vcf`) are always accepted – a piece
of data as base64 in the `NOTE` field gets through. A small Android app that builds the MMS itself and sends it with
`SmsManager.sendMultimediaMessage` (works without being the default SMS app on Android 14) avoids the filter completely.

WAV audio could be played but not saved on that phone; MIDI (`audio/midi`) could be both. The phone renamed it to `.rtx`.

## 9. LilyGO T-A7670: the SD card blocks uploading

The TF card uses GPIO 2 as MISO, a strapping pin. With a card inserted, `esptool` fails with "Wrong boot mode detected (0xa)".
Take the card out (or hold BOOT) when uploading over USB. A normal restart (e.g. after an OTA update) works fine with the card in.

## 10. A blocking SD card can switch the modem off

The PWRKEY pulse was timed in `loop()` (high, then low after 1 s). A failing `SD.begin()` blocked `loop()` for seconds, the
pulse became too long, and a long PWRKEY press switches the A76xx *off*. Do the 1 s pulse in one go, and retry a missing SD
card only every few seconds.
