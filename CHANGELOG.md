# Changelog

## 0.2.1
- Examples: pulse PWRKEY only when the modem does not answer (a pulse switches a running modem off), wait for the
  mobile network, and in SendSms wait for `SMS DONE` and retry when sending fails.
- SendSms and ReceiveSms tested on a LilyGO T-A7670G R2 (A7670SA-FASE) with æøå and emoji.

## 0.2.0
- Five examples: BuildMms, DecodeSmsPdu, ParseMmsNotification, SendSms, ReceiveSms. All compile for ESP32.

## 0.1.0
- First draft: `KihMmsBuilder`, `KihSmsPdu`, `KihMmsParser`, README in English and Norwegian, findings and operator table.
