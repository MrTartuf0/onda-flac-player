// Piccolo strato di astrazione: permette di compilare libreria, copertine e UI
// anche su PC (simulatore/test), oltre che sull'ESP32-S3.
#pragma once
#include <stddef.h>
#include <stdint.h>

void *plat_alloc_big(size_t n);          // PSRAM su ESP32
void *plat_alloc_fast(size_t n);         // RAM interna
void plat_free(void *p);
uint32_t plat_millis();
void plat_yield();                       // lascia respirare gli altri task
void plat_log(const char *fmt, ...);

// Impostazioni persistenti (NVS su ESP32)
int32_t plat_kv_get(const char *key, int32_t def);
void plat_kv_set(const char *key, int32_t v);
// stringhe (es. rete Wi-Fi): out deve contenere almeno n byte
void plat_kv_get_str(const char *key, char *out, size_t n);
void plat_kv_set_str(const char *key, const char *v);

// Diagnostica di avvio: "briciole" in memoria RTC che sopravvivono a un crash.
// slot 0 = interfaccia/loop, 1 = scansione SD, 2 = player
void plat_mark(int slot, const char *what, const char *detail = nullptr);
const char *plat_mark_get(int slot);
const char *plat_panic_text();   // motivo + backtrace dell'ultimo crash (o "")
// Da chiamare all'inizio di setup(): ritorna il numero di crash consecutivi
uint32_t plat_boot_check(const char **reason);
void plat_boot_ok();   // l'avvio è andato bene: azzera il contatore
void plat_heap_check(); // registra il minimo di RAM interna libera (da chiamare spesso)
void plat_monitor_start(); // sorveglia il core 0 dal core 1 (diagnostica watchdog)
// Scatola nera (sopravvive al riavvio): ultimi eventi e ultimo "battito" di ogni task.
void plat_event(const char *what, const char *detail = nullptr);
void plat_beat(int who);   // 0 interfaccia, 1 uscita audio, 2 decoder, 3 monitor
void plat_where(int who, uint8_t code);   // punto del codice raggiunto (numeri nei sorgenti)
const char *plat_blackbox_text();
