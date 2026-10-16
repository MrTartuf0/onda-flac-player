// Schermata "Disco USB": col cavo USB-C la microSD appare sul computer come una
// chiavetta. Mentre è aperta la musica è ferma e il lettore non tocca la scheda;
// quando il computer la espelle (o tocchi Fine) il lettore riavvia e la libreria
// si aggiorna con i file nuovi.
#include "ui_internal.h"
#include "../i18n.h"
#include <stdio.h>
#include <string.h>
#include "../hal/usb_disk.h"
#include "../lib/library.h"
#include "../audio/player.h"
#include "../audio/audio_out.h"
#include "../voice/voice.h"
#include "../platform.h"
#include "../power.h"

namespace {

enum View : uint8_t {
  V_NONE,
  V_NOSD,      // nessuna microSD
  V_BUSY,      // la libreria si sta aggiornando
  V_PLUG,      // in attesa del cavo (disco non ancora avviato)
  V_OPENING,   // disco avviato, il computer non l'ha ancora aperto
  V_MOUNTED,   // aperto sul computer
  V_AWAY,      // era aperto, ora il computer è scollegato o in standby
  V_EJECTED,   // espulso dal computer: si riavvia da soli
  V_LEAVING,   // riavvio in corso
};

constexpr int ICON_D = 72, ICON_Y = 46;
const lv_color_t C_RED = lv_color_hex(0xF15E6C);
const lv_color_t C_WARN = lv_color_hex(0xF0B232);

lv_obj_t *s_root = nullptr;
lv_obj_t *s_close, *s_ring, *s_icon_bg, *s_icon, *s_title, *s_sub, *s_stats, *s_hint, *s_btn, *s_btn_l;
lv_timer_t *s_tm = nullptr;
bool s_open = false;
bool s_started = false;      // disco avviato: si esce solo riavviando
bool s_was_mounted = false;
bool s_held = false;         // musica fermata e uscita audio sospesa da questa schermata
View s_view = V_NONE;
uint32_t s_host_since = 0;   // da quando si vede il computer (V_PLUG)
uint32_t s_confirm_until = 0;   // "Esci lo stesso" valido fino a...
uint32_t s_eject_t = 0;
uint32_t s_pulse = 0;

void set_text(lv_obj_t *l, const char *t) {
  if (strcmp(lv_label_get_text(l), t)) lv_label_set_text(l, t);
}

void fmt_size(uint64_t b, char *out, size_t n) {
  if (b >= 1024ULL * 1024 * 1024) snprintf(out, n, "%.1f GB", b / 1073741824.0);
  else if (b >= 10 * 1024 * 1024) snprintf(out, n, "%.0f MB", b / 1048576.0);
  else if (b >= 1024 * 1024) snprintf(out, n, "%.1f MB", b / 1048576.0);
  else snprintf(out, n, "%.0f KB", b / 1024.0);
  if (i18n_lang() == LANG_IT)
    for (char *p = out; *p; p++)
      if (*p == '.') *p = ',';
}

void leave_now();

// ---------------------------------------------------------- aspetto ----
void set_icon(const char *ic, lv_color_t bg, lv_color_t fg) {
  lv_obj_set_style_bg_color(s_icon_bg, bg, 0);
  lv_obj_set_style_text_color(s_icon, fg, 0);
  if (strcmp(lv_label_get_text(s_icon), ic)) lv_label_set_text(s_icon, ic);
}

// riga dell'avanzamento: nascosta quando è vuota (non lascia un buco nella colonna)
void set_stats(const char *t, lv_color_t c) {
  set_text(s_stats, t);
  lv_obj_set_style_text_color(s_stats, c, 0);
  if (t[0]) lv_obj_remove_flag(s_stats, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(s_stats, LV_OBJ_FLAG_HIDDEN);
}

void set_button(const char *txt, bool primary, bool risky = false) {
  set_text(s_btn_l, txt);
  lv_obj_set_style_bg_color(s_btn, primary ? C_GREEN : C_TILE, 0);
  lv_obj_set_style_text_color(s_btn_l, primary ? C_BLACK : risky ? C_WARN : C_TEXT, 0);
}

void show(View v) {
  if (v == s_view) return;
  s_view = v;
  bool can_close = !s_started;   // prima del disco si esce e basta, dopo solo riavviando
  if (can_close) lv_obj_remove_flag(s_close, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(s_close, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(s_ring, LV_OBJ_FLAG_HIDDEN);
  lv_obj_remove_flag(s_btn, LV_OBJ_FLAG_HIDDEN);
  set_stats("", C_SUB);
  lv_obj_set_style_text_color(s_hint, C_DIM, 0);
  switch (v) {
    case V_NOSD:
      set_icon(IC_SDCARD, C_RED, C_BLACK);
      set_text(s_title, TR("Nessuna microSD"));
      set_text(s_sub, TR("Inserisci la scheda e riprova"));
      set_text(s_hint, "");
      set_button(TR("Chiudi"), false);
      break;
    case V_BUSY:
      set_icon(IC_REFRESH, C_TILE, C_SUB);
      set_text(s_title, TR("Un momento"));
      set_text(s_sub, TR("La libreria si sta aggiornando: riprova appena ha finito"));
      set_text(s_hint, "");
      set_button(TR("Chiudi"), false);
      break;
    case V_PLUG:
      set_icon(IC_LAPTOP, C_TILE, C_SUB);
      set_text(s_title, TR("Collega il computer"));
      set_text(s_sub, TR("Con un cavo USB-C che trasferisce dati: con uno solo da ricarica il computer non vede niente"));
      set_text(s_hint, TR("La musica resta in pausa finch\xC3\xA9 sei qui"));
      set_button(TR("Annulla"), false);
      break;
    case V_OPENING:
      set_icon(IC_LAPTOP, C_TILE, C_TEXT);
      set_text(s_title, TR("Apertura in corso..."));
      set_text(s_sub, TR("La microSD apparir\xC3\xA0 come un disco esterno"));
      set_text(s_hint, TR("Quando hai finito espelli il disco dal computer"));
      set_button(TR("Fine"), false);
      break;
    case V_MOUNTED:
      set_icon(IC_LAPTOP, C_GREEN, C_BLACK);
      set_text(s_title, TR("Aperta sul computer"));
      set_text(s_sub, TR("Copia la musica nella microSD come su una chiavetta"));
      set_text(s_hint, TR("Finito? Espelli il disco dal computer e il lettore torna da solo alla musica"));
      set_button(TR("Fine"), false);
      break;
    case V_AWAY:
      set_icon(IC_LAPTOP, C_TILE, C_SUB);
      set_text(s_title, TR("Computer scollegato"));
      set_text(s_sub, TR("Ricollega il cavo, oppure torna alla musica"));
      set_text(s_hint, "");
      set_button(TR("Torna alla musica"), true);
      break;
    case V_EJECTED:
      set_icon(IC_CHECK, C_GREEN, C_BLACK);
      set_text(s_title, TR("Disco espulso"));
      set_text(s_sub, TR("Puoi staccare il cavo. Torno alla musica..."));
      set_text(s_hint, "");
      lv_obj_add_flag(s_btn, LV_OBJ_FLAG_HIDDEN);
      break;
    case V_LEAVING:
      set_icon(IC_MUSIC, C_GREEN, C_BLACK);
      set_text(s_title, TR("Torno alla musica..."));
      set_text(s_sub, TR("La libreria si aggiorna con i file nuovi"));
      set_text(s_hint, "");
      lv_obj_add_flag(s_btn, LV_OBJ_FLAG_HIDDEN);
      break;
    default: break;
  }
}

// ------------------------------------------------------------ uscita ----
// Prima del disco: si chiude e si riprende da dove eri
void close_plain() {
  if (!s_open || s_started) return;
  if (s_tm) lv_timer_delete(s_tm);
  s_tm = nullptr;
  s_open = false;
  lv_obj_add_flag(s_root, LV_OBJ_FLAG_HIDDEN);
  if (!s_held) return;
  s_held = false;
  audio_out_resume();
  player_hold_output(false);
  ui_after_transfer(false);
}

// Dopo il disco: riavvio (la porta USB torna la seriale, la libreria si rilegge)
void leave_now() {
  show(V_LEAVING);
  lv_refr_now(nullptr);
  usb_disk_finish();
  // simulatore: niente riavvio, si chiude e si riscansiona come dopo il Wi-Fi
  s_started = false;
  if (s_tm) lv_timer_delete(s_tm);
  s_tm = nullptr;
  s_open = false;
  s_held = false;
  lv_obj_add_flag(s_root, LV_OBJ_FLAG_HIDDEN);
  audio_out_resume();
  player_hold_output(false);
  ui_after_transfer(true);
}

void btn_cb(lv_event_t *) {
  switch (s_view) {
    case V_NOSD:
    case V_BUSY:
    case V_PLUG: close_plain(); break;
    case V_MOUNTED: {
      // Il modo giusto è "Espelli" dal computer; uscire senza può lasciare a metà
      // l'ultimo file copiato: la prima volta lo si dice, la seconda si esce.
      UsbDiskStatus st = usb_disk_status();
      if (plat_millis() > s_confirm_until && (st.bytes_written > 0 || st.idle_ms < 3000)) {
        s_confirm_until = plat_millis() + 5000;
        set_text(s_hint, TR("Prima espelli il disco dal computer, o gli ultimi file copiati potrebbero rovinarsi"));
        lv_obj_set_style_text_color(s_hint, C_WARN, 0);
        set_button(TR("Esci lo stesso"), false, true);
        return;
      }
      leave_now();
      break;
    }
    default: leave_now(); break;
  }
}

void close_cb(lv_event_t *) { close_plain(); }

// --------------------------------------------------------- avanzamento ----
void stats_update(const UsbDiskStatus &st) {
  char b[96], w[16], r[16];
  if (st.error) {
    set_stats(TR("Errore di lettura o scrittura della microSD"), C_RED);
    return;
  }
  if (!st.bytes_written && !st.bytes_read) {
    set_stats("", C_SUB);
    return;
  }
  fmt_size(st.bytes_read, r, sizeof r);
  if (st.bytes_written) {
    fmt_size(st.bytes_written, w, sizeof w);
    snprintf(b, sizeof b, TR("%s ricevuti \xE2\x80\xA2 %s letti"), w, r);
  } else {
    snprintf(b, sizeof b, TR("%s letti"), r);
  }
  set_stats(b, C_SUB);
}

void tick_cb(lv_timer_t *) {
  if (!s_open || s_view == V_LEAVING) return;
  uint32_t now = plat_millis();
  if (!s_started) {
    if (s_view != V_PLUG) return;
    // cavo collegato a un computer da mezzo secondo: si apre il disco
    if (!usb_disk_host_present()) {
      s_host_since = 0;
      return;
    }
    if (!s_host_since) s_host_since = now ? now : 1;
    if (now - s_host_since < 500) return;
    if (player_status().track >= 0) {   // non deve succedere, ma la scheda dev'essere libera
      player_stop();
      return;
    }
    if (!usb_disk_start()) {
      show(V_NOSD);
      return;
    }
    s_started = true;
    show(V_OPENING);
    return;
  }
  UsbDiskStatus st = usb_disk_status();
  if (st.state == UD_EJECTED) {
    if (s_view != V_EJECTED) {
      show(V_EJECTED);
      s_eject_t = now;
    } else if (now - s_eject_t > 1800) {
      leave_now();
    }
    return;
  }
  if (st.state == UD_MOUNTED) {
    s_was_mounted = true;
    if (s_view != V_MOUNTED) show(V_MOUNTED);
    if (s_confirm_until && now > s_confirm_until) {   // conferma scaduta: si torna come prima
      s_confirm_until = 0;
      s_view = V_NONE;
      show(V_MOUNTED);
    }
  } else {
    show(s_was_mounted ? V_AWAY : V_OPENING);
  }
  stats_update(st);
  // anello che pulsa mentre il computer legge o scrive
  bool busy = st.state == UD_MOUNTED && st.idle_ms < 400;
  if (busy) {
    s_pulse = (s_pulse + 1) % 10;
    int d = ICON_D + 8 + (s_pulse < 5 ? s_pulse : 10 - s_pulse) * 3;
    lv_obj_set_size(s_ring, d, d);
    lv_obj_align(s_ring, LV_ALIGN_TOP_MID, 0, ICON_Y + ICON_D / 2 - d / 2);
    lv_obj_set_style_bg_color(s_ring, st.writing ? C_GREEN : C_TEXT, 0);
    lv_obj_remove_flag(s_ring, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(s_ring, LV_OBJ_FLAG_HIDDEN);
  }
}

// --------------------------------------------------------- costruzione ----
lv_obj_t *text_label(lv_obj_t *p, const lv_font_t *f, lv_color_t c) {
  lv_obj_t *l = ui_label(p, f, c, "");
  lv_obj_set_width(l, lv_pct(100));
  lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_WRAP);
  lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
  return l;
}

void build() {
  s_root = ui_box(lv_layer_top());
  lv_obj_set_size(s_root, SCR_W, SCR_H);
  lv_obj_set_style_bg_color(s_root, C_BG, 0);
  lv_obj_set_style_bg_grad_color(s_root, lv_color_hex(0x0B2233), 0);
  lv_obj_set_style_bg_grad_dir(s_root, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_bg_main_stop(s_root, 140, 0);
  lv_obj_set_style_bg_opa(s_root, LV_OPA_COVER, 0);
  lv_obj_add_flag(s_root, LV_OBJ_FLAG_CLICKABLE);   // niente tocchi alle pagine sotto

  s_close = ui_icon_btn(s_root, &icons_18, IC_CLOSE, C_TEXT, close_cb);
  lv_obj_set_pos(s_close, 14, 14);
  lv_obj_set_ext_click_area(s_close, 14);
  lv_obj_t *t = ui_label(s_root, &font_b14, C_SUB, TR("Disco USB"));
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 15);

  s_ring = ui_box(s_root);
  lv_obj_set_style_radius(s_ring, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(s_ring, 60, 0);
  lv_obj_add_flag(s_ring, LV_OBJ_FLAG_HIDDEN);
  s_icon_bg = ui_box(s_root);
  lv_obj_set_size(s_icon_bg, ICON_D, ICON_D);
  lv_obj_set_style_radius(s_icon_bg, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(s_icon_bg, LV_OPA_COVER, 0);
  lv_obj_align(s_icon_bg, LV_ALIGN_TOP_MID, 0, ICON_Y);
  s_icon = ui_icon(s_icon_bg, &icons_28, IC_LAPTOP, C_TEXT);
  lv_obj_center(s_icon);

  // titolo, spiegazione e avanzamento in colonna: i testi vanno a capo senza sovrapporsi
  lv_obj_t *col = ui_box(s_root);
  lv_obj_set_size(col, SCR_W - 24, LV_SIZE_CONTENT);
  lv_obj_align(col, LV_ALIGN_TOP_MID, 0, ICON_Y + ICON_D + 14);
  lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(col, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(col, 6, 0);
  s_title = text_label(col, &font_b20, C_TEXT);
  s_sub = text_label(col, &font_r12, C_SUB);
  s_stats = text_label(col, &font_b12, C_SUB);

  s_btn = ui_box(s_root);
  lv_obj_set_size(s_btn, SCR_W - 48, 40);
  lv_obj_align(s_btn, LV_ALIGN_BOTTOM_MID, 0, -14);
  lv_obj_set_style_radius(s_btn, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(s_btn, LV_OPA_COVER, 0);
  ui_pressable(s_btn);
  lv_obj_add_event_cb(s_btn, btn_cb, LV_EVENT_CLICKED, nullptr);
  s_btn_l = ui_label(s_btn, &font_b14, C_TEXT, "");
  lv_obj_center(s_btn_l);

  s_hint = ui_label(s_root, &font_r10, C_DIM, "");
  lv_obj_set_width(s_hint, SCR_W - 40);
  lv_label_set_long_mode(s_hint, LV_LABEL_LONG_MODE_WRAP);
  lv_obj_set_style_text_align(s_hint, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(s_hint, LV_ALIGN_BOTTOM_MID, 0, -62);
  lv_obj_add_flag(s_root, LV_OBJ_FLAG_HIDDEN);
}

void open_cb() { ui_usbdisk_open(); }

}  // namespace

// ================================================================= API ==

void ui_usbdisk_open() {
  if (s_open) return;
  if (!s_root) build();
  ui_cc_close();
  ui_voice_close();
  ui_timer_close();
  if (ui_transfer_visible()) ui_transfer_close();
  s_open = true;
  s_view = V_NONE;
  s_host_since = 0;
  s_confirm_until = 0;
  s_was_mounted = false;
  lv_obj_move_to_index(s_root, -1);
  lv_obj_remove_flag(s_root, LV_OBJ_FLAG_HIDDEN);
  s_tm = lv_timer_create(tick_cb, 100, nullptr);
  LibState ls = lib_progress().state;
  if (ls == LIB_NO_SD || ls == LIB_IDLE) {
    show(V_NOSD);
    return;
  }
  if (ls != LIB_READY) {
    show(V_BUSY);
    return;
  }
  // Il computer scrive direttamente sui settori: il lettore deve lasciare la
  // scheda del tutto. Musica ferma e file chiusi, conteggi salvati adesso.
  voice_release();
  ui_session_save();
  player_stop();
  for (int i = 0; i < 40 && player_status().track >= 0; i++) lv_delay_ms(25);
  player_hold_output(true);
  audio_out_suspend();
  ui_nowplaying_close();
  lib_save_dirty();
  s_held = true;
  show(V_PLUG);
  tick_cb(nullptr);   // già collegato: si parte subito
}

bool ui_usbdisk_visible() { return s_open; }

bool ui_usbdisk_back() {
  if (!s_open) return false;
  if (!s_started) close_plain();
  return true;   // col disco aperto il gesto "indietro" non fa niente
}

// Chiamata da ui_tick: true = la schermata è aperta e il resto dell'interfaccia
// non deve toccare la scheda (salvataggi, copertine, timer...).
bool ui_usbdisk_tick() { return s_open; }

// Computer appena collegato al cavo: lo si propone, una volta per collegamento.
// Non se era già collegato all'accensione (sviluppo col monitor seriale).
void ui_usbdisk_watch() {
  static int8_t s_host = -1;
  static uint32_t s_since = 0;
  static bool s_offered = false;
  bool h = usb_disk_host_present();
  uint32_t now = plat_millis();
  if (s_host < 0) {
    s_host = h;
    s_offered = h;
    return;
  }
  if (h != (bool)s_host) {
    s_host = h;
    s_since = now;
    if (!h) s_offered = false;
    return;
  }
  if (!h || s_offered || now - s_since < 1500) return;
  s_offered = true;
  if (!power_screen_on() || ui_transfer_visible() || ui_timer_visible() || ui_voice_visible() || ui_cc_visible())
    return;
  ui_toast_action(TR("Computer collegato: tocca qui per aprire la microSD come disco USB"), open_cb);
}
