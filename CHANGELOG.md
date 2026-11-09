# Changelog

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
