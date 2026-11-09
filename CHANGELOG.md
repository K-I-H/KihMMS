# Changelog

## 0.4.0
- OTAMMS: a new program over MMS. `KihFwPiece` (the piece format "BXFW2", also as base64 in a contact card) and `KihSha256`
  (SHA-256, HMAC-SHA256) in plain C++; `KihOtaMms` takes pieces only from one number, checks each piece (CRC-32), the version
  (newer than the one installed, so an old signed package can not be sent again) and the signature over version + program,
  installs with `Update`, restarts, and the new program reports "new firmware is running". An old sender app (format "BXFW1")
  gets a clear answer.
- Example: OtaMms.
- Easier to use: `modem.begin()` with the LilyGO T-A7670 pins (also as `KIH_A7670_…` constants, SD card included),
  `onReady` (the modem is on and the SIM has been read), `mmsReady()`, `sendMmsFile()` (a file from SD/LittleFS, the type
  from the extension), `lastError()` (why a send returned false), and on a received MMS `findPart()`, `save()` and
  `extension()`. Numbers without "+" from the operator ("4712345678") get one, so `m.from` can be answered directly.
- New examples: SendMms (start here: one MMS with a built-in picture), MmsFromSdCard (SMS "photo" → MMS from the SD card),
  SaveMmsToSd (received pictures saved on the SD card, with a receipt by SMS).
- README: quick start, the API in one table, troubleshooting; the code at the top had broken "\n" strings.
- keywords.txt for the Arduino IDE.
- Tested on a LilyGO T-A7670G R2 with an Ice SIM card and the OTAMMS app 1.8: 1 070 576 bytes in 5 MMS, installed, restarted
  and confirmed by SMS. The piece format and the signature are also checked on a PC against Python's hashlib/hmac and RFC 4231.

## 0.3.0
- The modem driver, in small parts that can be used on their own: `KihAt` (AT command queue, answers read without waiting,
  unsolicited lines), `KihPower` (PWRKEY, waits until the modem answers), `KihSms` (SMS stored on the SIM, long SMS joined,
  sending with `KihSmsSubmit`: GSM 7-bit or UCS2, long SMS split without breaking emoji), `KihPpp` (PPP data connection),
  `KihMmsGet` / `KihMmsSend` (MMS fetched and sent over PPP), `KihGnss` (position from AT+CGNSSINFO).
- `KihMms`: everything in one object – `onSms`, `onMms` (fetched by themselves), `sendSms`, `sendMms`, operator settings
  found from the SIM card (AT+CIMI).
- Examples: SmsAndMms and GnssPosition.
- Tested on a LilyGO T-A7670G R2 with an Ice SIM card: SMS in and out with æøå, € and emoji (a 300-character SMS in five
  parts arrived as one message), MMS fetched (PNG 189 KB, JPEG 253 KB) and sent (PNG 43 KB), automatic SMS reply to an MMS,
  GNSS position with 14–21 satellites.

## 0.2.1
- Examples: pulse PWRKEY only when the modem does not answer (a pulse switches a running modem off), wait for the
  mobile network, and in SendSms wait for `SMS DONE` and retry when sending fails.
- SendSms and ReceiveSms tested on a LilyGO T-A7670G R2 (A7670SA-FASE) with æøå and emoji.

## 0.2.0
- Five examples: BuildMms, DecodeSmsPdu, ParseMmsNotification, SendSms, ReceiveSms. All compile for ESP32.

## 0.1.0
- First draft: `KihMmsBuilder`, `KihSmsPdu`, `KihMmsParser`, README in English and Norwegian, findings and operator table.
