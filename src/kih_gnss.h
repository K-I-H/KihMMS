// KihMMS – GNSS position from the modem's built-in receiver (AT+CGNSSINFO).
//
// Plain C/C++ (no Arduino, no String), so it can be tested on a PC. Feed it the response line:
//   KihGnss g;
//   if (g.parse(line) && g.fix) { ... g.lat, g.lon, g.speedKmh ... }
//
// The A76xx answers  +CGNSSINFO: <mode>,<sats…>,<lat>,<N/S>,<lon>,<E/W>,<date>,<time>,<alt>,<speed>,<course>,<pdop>,<hdop>,<vdop>
// The number of satellite fields after <mode> differs between firmware versions (GPS/GLONASS/BEIDOU/GALILEO counts), so the
// parser finds the N/S field and reads everything relative to it. Latitude/longitude come as decimal degrees on most firmware,
// but some versions send ddmm.mmmm like NMEA: values above 180 are converted. Speed is in knots. Without a fix the modem sends
// "+CGNSSINFO: ,,,,,,,," (empty fields).
#pragma once
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

struct KihGnss {
  bool fix = false;
  int mode = 0;            // 2 = 2D fix, 3 = 3D fix
  int satellites = 0;      // sum of the satellite fields
  double lat = NAN, lon = NAN;
  float altitude = NAN;    // metres
  float speedKmh = NAN;
  float course = NAN;      // degrees
  float hdop = NAN;
  char date[11] = "";      // "dd.mm.yyyy" (UTC)
  char time[9] = "";       // "hh:mm:ss" (UTC)

  // Field i (0-based) of a comma-separated list, copied into out (max outLen-1 chars). Returns false when there is no such field.
  static bool field(const char *s, int i, char *out, int outLen) {
    while (i > 0 && *s) { if (*s++ == ',') i--; }
    if (i > 0) return false;
    int n = 0;
    while (*s && *s != ',' && *s != '\r' && *s != '\n' && n < outLen - 1) out[n++] = *s++;
    out[n] = 0;
    return true;
  }

  static double toDegrees(const char *v, const char *hemi) {
    if (!*v) return NAN;
    double x = atof(v);
    if (fabs(x) > 180) { double d = floor(x / 100); x = d + (x - d * 100) / 60; }   // ddmm.mmmm
    if (*hemi == 'S' || *hemi == 'W') x = -x;
    return x;
  }

  // Parses one "+CGNSSINFO: …" line. Returns true when the line was a CGNSSINFO response (with or without a fix).
  bool parse(const char *line) {
    const char *p = strstr(line, "+CGNSSINFO:");
    if (!p) return false;
    p += 11;
    while (*p == ' ') p++;
    char f[24];
    fix = false; satellites = 0;
    lat = lon = NAN; altitude = speedKmh = course = hdop = NAN;
    date[0] = time[0] = 0;
    field(p, 0, f, sizeof f);
    mode = atoi(f);
    int h = -1;
    for (int i = 1; i < 20 && field(p, i, f, sizeof f); i++)
      if (!strcmp(f, "N") || !strcmp(f, "S")) { h = i; break; }
    if (h < 2) return true;                                     // no fix yet
    for (int i = 1; i < h - 1; i++) { field(p, i, f, sizeof f); satellites += atoi(f); }
    char v[24], hemi[4];
    field(p, h - 1, v, sizeof v); field(p, h, hemi, sizeof hemi); lat = toDegrees(v, hemi);
    field(p, h + 1, v, sizeof v); field(p, h + 2, hemi, sizeof hemi); lon = toDegrees(v, hemi);
    if (field(p, h + 3, f, sizeof f) && strlen(f) >= 6)        // ddmmyy
      snprintf(date, sizeof date, "%.2s.%.2s.20%.2s", f, f + 2, f + 4);
    if (field(p, h + 4, f, sizeof f) && strlen(f) >= 6)        // hhmmss.s
      snprintf(time, sizeof time, "%.2s:%.2s:%.2s", f, f + 2, f + 4);
    if (field(p, h + 5, f, sizeof f) && *f) altitude = (float)atof(f);
    if (field(p, h + 6, f, sizeof f) && *f) speedKmh = (float)atof(f) * 1.852f;
    if (field(p, h + 7, f, sizeof f) && *f) course = (float)atof(f);
    if (field(p, h + 9, f, sizeof f) && *f) hdop = (float)atof(f);
    fix = mode >= 2 && !isnan(lat) && !isnan(lon);
    return true;
  }
};
