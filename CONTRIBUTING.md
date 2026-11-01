# Contributing

Thanks for wanting to help! The most valuable contributions right now:

- **Tested MMS settings** for your operator (APN, MMSC, proxy) – add a row to `extras/docs/OPERATORS.md`.
- **Reports from other A76xx modems and boards** – which model, firmware (`AT+SIMCOMATI`) and what worked.
- **Bug fixes and examples** – keep examples small, commented (explain *why*, not only *what*) and compiling for ESP32.

## Guidelines

- No personal data in issues, code or logs: replace phone numbers, IMEI, ICCID and keys with placeholders like `+4700000000`.
- `loop()` should never wait for the modem; the examples are allowed to keep it simple.
- Pull requests are compiled for ESP32 automatically (see `.github/workflows/compile.yml`).
