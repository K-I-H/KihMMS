// KihMMS – the AT layer: one command at a time to the modem, answers read without waiting.
//
//   KihAt at;
//   at.begin(Serial1);
//   at.onUrc = [](const char *line) { ... };          // unsolicited lines: +CMTI, RING, +CGNSSINFO …
//   at.send("AT+CSQ", [](bool ok, const char *answer) { ... });
//   void loop() { at.loop(); }                         // never blocks
//
// Commands wait in a small queue. The next one is sent when the modem has answered OK or ERROR (or the timeout passes), so the
// answers never get mixed up. Every line that arrives while a command is running is added to its answer; lines that arrive when
// nothing is running go to onUrc. Raw NMEA sentences ($GPGGA,…*4F) that some firmware prints on the AT port are dropped, but only
// real NMEA sentences: a wider filter once removed SMS lines too.
#pragma once
#include <Arduino.h>
#include <functional>

class KihAt {
 public:
  using Done = std::function<void(bool ok, const char *answer)>;
  std::function<void(const char *line)> onUrc;

  void begin(Stream &port) { s = &port; }

  // Queues a command. timeoutMs: how long to wait for OK/ERROR. Returns false when the queue is full.
  bool send(const char *cmd, Done done = nullptr, uint32_t timeoutMs = 5000) { return sendWithData(cmd, nullptr, done, timeoutMs); }

  // A command that answers with the prompt "> " (AT+CMGS): «data» is written after the prompt, ended with Ctrl-Z.
  bool sendWithData(const char *cmd, const char *data, Done done = nullptr, uint32_t timeoutMs = 5000) {
    if (count >= QUEUE) return false;
    Item &it = queue[(head + count++) % QUEUE];
    strncpy(it.cmd, cmd, sizeof it.cmd - 1);
    it.cmd[sizeof it.cmd - 1] = 0;
    it.data = data ? String(data) : String();
    it.done = done;
    it.timeout = timeoutMs;
    return true;
  }

  bool busy() const { return running; }
  int queued() const { return count; }

  void loop() {
    if (!s) return;
    while (s->available()) {
      char c = (char)s->read();
      if (running && c == '>' && len == 0 && queue[head].data.length()) {   // the prompt: no newline after it
        s->print(queue[head].data);
        s->write(0x1A);
        queue[head].data = String();
        continue;
      }
      if (c == '\r') continue;
      if (c != '\n') { if (len < (int)sizeof line - 1) line[len++] = c; continue; }
      line[len] = 0;
      if (len > 0) handle(line);
      len = 0;
    }
    if (running && millis() - sentAt > queue[head].timeout) finish(false);
    if (!running && count > 0) {
      s->print(queue[head].cmd);
      s->print("\r");
      answer[0] = 0;
      sentAt = millis();
      running = true;
    }
  }

  // A raw NMEA sentence: '$', five capital letters, a comma, and '*' near the end.
  static bool isNmea(const char *l) {
    if (l[0] != '$' || strlen(l) < 10 || !strchr(l + 6, '*')) return false;
    for (int i = 1; i <= 5; i++) if (l[i] < 'A' || l[i] > 'Z') return false;
    return l[6] == ',';
  }

 private:
  static const int QUEUE = 16;
  struct Item { char cmd[160]; String data; Done done; uint32_t timeout; };
  Stream *s = nullptr;
  Item queue[QUEUE];
  int head = 0, count = 0;
  bool running = false;
  uint32_t sentAt = 0;
  char line[700];
  int len = 0;
  char answer[4096];

  void handle(const char *l) {
    if (isNmea(l)) return;
    if (running) {
      if (!strcmp(l, "OK")) { finish(true); return; }
      if (!strcmp(l, "ERROR") || !strncmp(l, "+CME ERROR", 10) || !strncmp(l, "+CMS ERROR", 10)) {
        append(l);
        finish(false);
        return;
      }
      if (!strcmp(l, queue[head].cmd)) return;                 // the modem's echo of our command
      if (!isUrcFor(l, queue[head].cmd)) { append(l); return; }
    }
    if (onUrc) onUrc(l);
  }

  // A line that is not the answer to the running command: RING, or "+XYZ: …" where +XYZ is not in the command
  // (a +CMTI that arrives while AT+CSQ runs must still reach onUrc).
  static bool isUrcFor(const char *l, const char *cmd) {
    if (!strcmp(l, "RING")) return true;
    if (l[0] != '+') return false;
    const char *colon = strchr(l, ':');
    if (!colon) return false;
    char prefix[24];
    int n = (int)(colon - l);
    if (n >= (int)sizeof prefix) return false;
    memcpy(prefix, l, n);
    prefix[n] = 0;
    return strstr(cmd, prefix) == nullptr;
  }

  void append(const char *l) {
    size_t n = strlen(answer);
    if (n && n < sizeof answer - 1) answer[n++] = '\n';
    strncpy(answer + n, l, sizeof answer - 1 - n);
    answer[sizeof answer - 1] = 0;
  }

  void finish(bool ok) {
    Done d = queue[head].done;
    queue[head].done = nullptr;
    queue[head].data = String();
    head = (head + 1) % QUEUE;
    count--;
    running = false;
    if (d) d(ok, answer);
  }
};
