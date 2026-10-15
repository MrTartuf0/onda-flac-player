// Orologio. La scheda non ha una batteria tampone per l'ora: la prende dal
// telefono/computer quando apri la pagina del trasferimento file, o da internet
// (NTP) se il lettore è collegato al Wi-Fi di casa. Poi la tiene il chip anche
// a schermo spento e da spento (deep sleep); si perde solo staccando la batteria.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <time.h>

enum ClockSource : uint8_t { CLOCK_NONE = 0, CLOCK_BROWSER, CLOCK_NTP };

void clock_begin();                        // fuso orario (all'avvio)
bool clock_valid();                        // l'ora è stata impostata
bool clock_local(struct tm *out);          // ora locale; false se non impostata
void clock_set(int64_t unix_ms, ClockSource src);
ClockSource clock_source();
int64_t clock_synced_at();                 // quando (unix, s) è stata impostata l'ultima volta
void clock_ntp_start();                    // con il Wi-Fi di casa collegato
void clock_ntp_stop();

// "21:50", "giovedì 15 ottobre 2026"
void clock_fmt_time(const struct tm &t, char *buf, size_t n);
void clock_fmt_date(const struct tm &t, char *buf, size_t n);
