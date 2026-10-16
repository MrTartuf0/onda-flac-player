// Impostazioni: batteria, uscita audio, rilevamento cuffie, luminosità, libreria.
#include "ui_internal.h"
#include "../i18n.h"
#include <stdio.h>
#include <string.h>
#include "../lib/library.h"
#include "../audio/audio_out.h"
#include "../audio/player.h"
#include "../platform.h"
#include "../hal/battery.h"
#include "../power.h"
#include "../clock.h"
#include "../session.h"
#include "../hal/usb_disk.h"

namespace {

class SettingsPage : public Page {
 public:
  lv_obj_t *col, *out_val, *jack_val;
  lv_obj_t *bat_lvl = nullptr, *bat_state = nullptr, *bat_icon = nullptr, *bat_load = nullptr;
  lv_timer_t *bat_timer = nullptr;
  lv_obj_t *scr_val = nullptr, *off_val = nullptr;
  lv_obj_t *clk = nullptr, *date_t = nullptr, *date_s = nullptr;
  lv_obj_t *tm_sub = nullptr;
  lv_obj_t *usb_sub = nullptr;

  SettingsPage() { build(); }
  ~SettingsPage() override {
    if (bat_timer) lv_timer_delete(bat_timer);
  }

  static void out_cb(lv_event_t *) { ui_device_sheet(); }
  static void rescan_cb(lv_event_t *) { ui_request_rescan(); }
  // cambio lingua: si salva e si riavvia (riparte dal brano in cui eri, in pausa)
  static void lang_cb(lv_event_t *) {
    Lang l = i18n_lang() == LANG_IT ? LANG_EN : LANG_IT;
    plat_kv_set("lang", l);
    i18n_set(l);
    ui_toast(TR("Riavvio per cambiare lingua..."));
    ui_session_save();
    lv_timer_create([](lv_timer_t *) { plat_restart(); }, 700, nullptr);
  }
  static void xfer_cb(lv_event_t *) { ui_transfer_open(); }
  static void usb_cb(lv_event_t *) { ui_usbdisk_open(); }
  // computer collegato o no: si vede subito se il cavo è quello giusto
  void usb_update() {
    bool host = usb_disk_host_present();
    const char *t = host ? TR("Computer collegato: tocca qui") : TR("Col cavo USB-C, come una chiavetta");
    if (strcmp(lv_label_get_text(usb_sub), t)) lv_label_set_text(usb_sub, t);
    lv_obj_set_style_text_color(usb_sub, host ? C_GREEN : C_SUB, 0);
  }
  static void pause_cb(lv_event_t *e) {
    bool on = lv_obj_has_state(lv_event_get_target_obj(e), LV_STATE_CHECKED);
    audio_out_set_pause_on_unplug(on);
    plat_kv_set("p_unpl", on);
  }
  static void invert_cb(lv_event_t *e) {
    bool on = lv_obj_has_state(lv_event_get_target_obj(e), LV_STATE_CHECKED);
    audio_out_set_detect_inverted(on);
    plat_kv_set("hp_inv", on);
  }
  static void bat_timer_cb(lv_timer_t *t) {
    SettingsPage *p = (SettingsPage *)lv_timer_get_user_data(t);
    p->bat_update();
    p->clock_update();
    p->timer_update();
    p->usb_update();
  }

  // ---- timer: una riga che apre la sua schermata, con lo stato sotto
  static void tm_open_cb(lv_event_t *) { ui_timer_open(); }
  void timer_update() {
    static const lv_color_t red = lv_color_hex(0xF15E6C);
    char b[64], t[16];
    lv_color_t c = C_SUB;
    int32_t rem = session_remaining_ms();
    if (!session_running()) {
      uint32_t d = session_duration_s();
      session_fmt(d, t, sizeof t);
      if (d) snprintf(b, sizeof b, TR("Fermo \xE2\x80\xA2 %s"), t);
      else snprintf(b, sizeof b, "%s", TR("Fermo \xE2\x80\xA2 cronometro"));
    } else if (!session_run_duration_s()) {
      session_fmt(session_elapsed_ms() / 1000, t, sizeof t);
      snprintf(b, sizeof b, session_paused() ? TR("Cronometro in pausa \xE2\x80\xA2 %s") : TR("Cronometro \xE2\x80\xA2 %s"), t);
      if (!session_paused()) c = C_GREEN;
    } else if (session_over()) {
      session_fmt((uint32_t)(-rem) / 1000, t, sizeof t);
      snprintf(b, sizeof b, TR("Scaduto da %s"), t);
      c = red;
    } else {
      session_fmt((uint32_t)(rem + 999) / 1000, t, sizeof t);
      snprintf(b, sizeof b, session_paused() ? TR("In pausa \xE2\x80\xA2 %s rimasti") : TR("%s rimasti"), t);
      if (!session_paused()) c = C_GREEN;
    }
    if (strcmp(lv_label_get_text(tm_sub), b)) lv_label_set_text(tm_sub, b);
    lv_obj_set_style_text_color(tm_sub, c, 0);
  }

  void timer_build() {
    lv_obj_t *r = ui_box(col);
    lv_obj_set_size(r, SCR_W, 60);
    lv_obj_set_style_margin_top(r, 6, 0);
    ui_pressable(r);
    lv_obj_add_event_cb(r, tm_open_cb, LV_EVENT_CLICKED, this);
    lv_obj_t *ic = ui_box(r);
    lv_obj_set_size(ic, 36, 36);
    lv_obj_set_style_radius(ic, 9, 0);
    lv_obj_set_style_bg_color(ic, lv_color_hex(0xF5A623), 0);
    lv_obj_set_style_bg_opa(ic, LV_OPA_COVER, 0);
    lv_obj_align(ic, LV_ALIGN_LEFT_MID, 14, 0);
    lv_obj_center(ui_icon(ic, &icons_18, IC_STOPWATCH, C_BLACK));
    lv_obj_t *t = ui_label(r, &font_r14, C_TEXT, TR("Timer"));
    lv_obj_align(t, LV_ALIGN_LEFT_MID, 62, -9);
    tm_sub = ui_label(r, &font_r12, C_SUB, "");
    lv_obj_set_width(tm_sub, SCR_W - 62 - 36);
    ui_dot(tm_sub);
    lv_obj_align(tm_sub, LV_ALIGN_LEFT_MID, 62, 10);
    lv_obj_t *ch = ui_icon(r, &icons_14, IC_RIGHT, C_DIM);
    lv_obj_align(ch, LV_ALIGN_RIGHT_MID, -16, 0);
    timer_update();
  }

  // ora in alto a destra e riga "Data e ora"
  void clock_update() {
    struct tm t;
    char b[64], hm[8];
    if (!clock_local(&t)) {
      lv_label_set_text(clk, "");
      lv_label_set_text(date_t, TR("Ora non impostata"));
      lv_label_set_text(date_s, TR("Si prende da sola dal telefono o dal computer aprendo Trasferimento file"));
      return;
    }
    clock_fmt_time(t, hm, sizeof hm);
    lv_label_set_text(clk, hm);
    clock_fmt_date(t, b, sizeof b);
    if (b[0] >= 'a' && b[0] <= 'z') b[0] -= 32;   // "Giovedì 15 ottobre 2026"
    lv_label_set_text(date_t, b);
    time_t at = (time_t)clock_synced_at();
    struct tm st;
    localtime_r(&at, &st);
    char when[24];
    clock_fmt_time(st, when, sizeof when);
    if (st.tm_yday != t.tm_yday || st.tm_year != t.tm_year)
      snprintf(when + strlen(when), sizeof when - strlen(when), TR(" del %d/%d"), st.tm_mday, st.tm_mon + 1);
    snprintf(b, sizeof b, TR("%s alle %s"), clock_source() == CLOCK_NTP ? TR("Da internet") : TR("Dal telefono/computer"), when);
    lv_label_set_text(date_s, b);
  }

  void bat_update() {
    BatteryInfo b = battery_info();
    char buf[64];
    if (b.state == BAT_NONE || b.pct < 0) {
      snprintf(buf, sizeof buf, TR("Non rilevata (%d,%02d V)"), b.mv / 1000, b.mv % 1000 / 10);
      lv_label_set_text(bat_lvl, buf);
      lv_label_set_text(bat_state, TR("Collega una LiPo 3,7 V al connettore BAT"));
      lv_obj_add_flag(bat_icon, LV_OBJ_FLAG_HIDDEN);
      return;
    }
    snprintf(buf, sizeof buf, TR("%d%% \xE2\x80\xA2 %d,%02d V"), b.pct, b.mv / 1000, b.mv % 1000 / 10);
    lv_label_set_text(bat_lvl, buf);
    char t[24];
    if (b.state == BAT_CHARGING && b.minutes_to_full > 0) {
      ui_fmt_minutes(b.minutes_to_full, t, sizeof t);
      snprintf(buf, sizeof buf, TR("In carica \xE2\x80\xA2 piena tra %s"), t);
      lv_label_set_text(bat_state, buf);
    } else if (b.state == BAT_DISCHARGING && b.minutes_left >= 0) {
      ui_fmt_minutes(b.minutes_left, t, sizeof t);
      snprintf(buf, sizeof buf, TR("%s \xE2\x80\xA2 %s rimasti"), ui_battery_state_text(b), t);
      lv_label_set_text(bat_state, buf);
    } else {
      lv_label_set_text(bat_state, ui_battery_state_text(b));
    }
    if (b.load_ma > 0) {
      snprintf(buf, sizeof buf, TR("~%d mA"), (b.load_ma + 5) / 10 * 10);
      lv_label_set_text(bat_load, buf);
    } else {
      lv_label_set_text(bat_load, "-");
    }
    bool chg = b.state == BAT_CHARGING || b.state == BAT_FULL;
    ui_battery_icon_set(bat_icon, b.pct, chg ? C_GREEN : (b.low ? lv_color_hex(0xF15E6C) : C_TEXT));
    lv_obj_remove_flag(bat_icon, LV_OBJ_FLAG_HIDDEN);
  }

  // tocco sulla riga: passa al valore successivo
  static void scr_cb(lv_event_t *e) {
    static const int opts[] = {15, 30, 60, 120, 300, 0};
    int cur = power_screen_timeout_s(), n = sizeof opts / sizeof opts[0], i = 0;
    while (i < n && opts[i] != cur) i++;
    power_set_screen_timeout_s(opts[(i + 1) % n]);
    ((SettingsPage *)lv_event_get_user_data(e))->power_labels();
  }
  static void off_cb(lv_event_t *e) {
    static const int opts[] = {5, 10, 30, 60, 0};
    int cur = power_auto_off_min(), n = sizeof opts / sizeof opts[0], i = 0;
    while (i < n && opts[i] != cur) i++;
    power_set_auto_off_min(opts[(i + 1) % n]);
    ((SettingsPage *)lv_event_get_user_data(e))->power_labels();
  }
  static void touchwake_cb(lv_event_t *e) {
    power_set_touch_wake(lv_obj_has_state(lv_event_get_target_obj(e), LV_STATE_CHECKED));
  }
  static void poweroff_cb(lv_event_t *) { power_off_later(); }

  void power_labels() {
    char b[48];
    int s = power_screen_timeout_s();
    if (!s) snprintf(b, sizeof b, "%s", TR("Mai"));
    else if (s < 60) snprintf(b, sizeof b, TR("Dopo %d secondi"), s);
    else if (s == 60) snprintf(b, sizeof b, "%s", TR("Dopo 1 minuto"));
    else snprintf(b, sizeof b, TR("Dopo %d minuti"), s / 60);
    lv_label_set_text(scr_val, b);
    int m = power_auto_off_min();
    if (!m) snprintf(b, sizeof b, "%s", TR("Mai"));
    else snprintf(b, sizeof b, TR("Dopo %d min in pausa"), m);
    lv_label_set_text(off_val, b);
  }

  static void bright_cb(lv_event_t *e) {
    lv_obj_t *s = lv_event_get_target_obj(e);
    // mentre trascini cambia e basta; si salva al rilascio (niente scritture in flash a ogni passo)
    ui_set_brightness(lv_slider_get_value(s), lv_event_get_code(e) == LV_EVENT_RELEASED);
  }

  void section(const char *t) {
    lv_obj_t *l = ui_label(col, &font_b16, C_TEXT, t);
    lv_obj_set_style_pad_left(l, 14, 0);
    lv_obj_set_style_pad_top(l, 16, 0);
    lv_obj_set_style_pad_bottom(l, 4, 0);
  }

  lv_obj_t *row(const char *title, const char *sub, lv_event_cb_t cb) {
    lv_obj_t *r = ui_box(col);
    lv_obj_set_size(r, SCR_W, sub ? 52 : 44);
    if (cb) {
      ui_pressable(r);
      lv_obj_add_event_cb(r, cb, LV_EVENT_CLICKED, this);
    }
    lv_obj_t *t = ui_label(r, &font_r14, C_TEXT, title);
    lv_obj_set_width(t, SCR_W - 80);
    ui_dot(t);
    lv_obj_align(t, LV_ALIGN_LEFT_MID, 14, sub ? -9 : 0);
    if (sub) {
      lv_obj_t *s = ui_label(r, &font_r12, C_SUB, sub);
      lv_obj_set_width(s, SCR_W - 80);
      ui_dot(s);
      lv_obj_align(s, LV_ALIGN_LEFT_MID, 14, 9);
    }
    return r;
  }

  lv_obj_t *toggle(const char *title, const char *sub, bool on, lv_event_cb_t cb) {
    lv_obj_t *r = row(title, sub, nullptr);
    lv_obj_t *sw = lv_switch_create(r);
    lv_obj_set_size(sw, 40, 22);
    lv_obj_align(sw, LV_ALIGN_RIGHT_MID, -14, 0);
    lv_obj_set_style_bg_color(sw, lv_color_hex(0x535353), 0);
    lv_obj_set_style_bg_color(sw, C_GREEN, LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(sw, C_TEXT, LV_PART_KNOB);
    if (on) lv_obj_add_state(sw, LV_STATE_CHECKED);
    lv_obj_add_event_cb(sw, cb, LV_EVENT_VALUE_CHANGED, this);
    return r;
  }

  const char *out_name() {
    switch (audio_out_mode()) {
      case OUT_SPEAKER: return TR("Altoparlante integrato");
      case OUT_HEADPHONES: return TR("Cuffie (PCM5102)");
      default: return TR("Automatico");
    }
  }

  const char *jack_name() {
    if (!audio_out_hp_detect_available()) return TR("Non configurato (HP_DETECT_PIN = -1)");
    return audio_out_hp_present() ? TR("Cuffie collegate") : TR("Cuffie non collegate");
  }

  void build() {
    lv_obj_t *r = ui_page_scroll(this);
    col = ui_box(r);
    lv_obj_set_size(col, SCR_W, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_bottom(col, BOTTOM_PAD, 0);

    // scheda principale (barra in basso): titolo grande come le altre schede
    lv_obj_t *hd = ui_box(col);
    lv_obj_set_size(hd, SCR_W, LV_SIZE_CONTENT);
    lv_obj_t *t = ui_label(hd, &font_b20, C_TEXT, TR("Impostazioni"));
    lv_obj_set_style_pad_left(t, 12, 0);
    lv_obj_set_style_pad_top(t, 12, 0);
    clk = ui_label(hd, &font_b16, C_SUB, "");   // orologio
    lv_obj_align(clk, LV_ALIGN_TOP_RIGHT, -14, 16);

    timer_build();

    section(TR("Batteria"));
    lv_obj_t *bl = row(TR("Livello"), "-", nullptr);
    bat_lvl = lv_obj_get_child(bl, 1);
    bat_icon = ui_battery_icon(bl, 26, 13);
    lv_obj_align(bat_icon, LV_ALIGN_RIGHT_MID, -16, 0);
    lv_obj_t *bs = row(TR("Stato"), "-", nullptr);
    bat_state = lv_obj_get_child(bs, 1);
    lv_obj_t *bc = row(TR("Consumo stimato"), "-", nullptr);
    bat_load = lv_obj_get_child(bc, 1);
    lv_obj_set_width(bat_load, SCR_W - 28);
    lv_obj_set_width(bat_state, SCR_W - 28);
    bat_update();

    section(TR("Data e ora"));
    // come le altre righe, ma alta quanto serve: la nota può andare a capo
    lv_obj_t *dr = ui_box(col);
    lv_obj_set_size(dr, SCR_W, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(dr, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_hor(dr, 14, 0);
    lv_obj_set_style_pad_ver(dr, 8, 0);
    lv_obj_set_style_pad_row(dr, 2, 0);
    date_t = ui_label(dr, &font_r14, C_TEXT, "");
    date_s = ui_label(dr, &font_r12, C_SUB, "");
    lv_obj_set_width(date_s, SCR_W - 28);
    clock_update();
    bat_timer = lv_timer_create(bat_timer_cb, 1000, this);

    section(TR("Audio"));
    lv_obj_t *o = row(TR("Uscita audio"), out_name(), out_cb);
    out_val = lv_obj_get_child(o, 1);
    lv_obj_t *j = row(TR("Jack cuffie"), jack_name(), nullptr);
    jack_val = lv_obj_get_child(j, 1);
    toggle(TR("Pausa se scolleghi le cuffie"), TR("Come sul telefono"), audio_out_pause_on_unplug(), pause_cb);
    toggle(TR("Inverti rilevamento cuffie"), TR("Se il jack funziona al contrario"), audio_out_detect_inverted(), invert_cb);

    section(TR("Lingua"));
    // sempre in entrambe le lingue: chi non capisce quella attuale la trova lo stesso
    row("Lingua \xC2\xB7 Language", i18n_lang() == LANG_EN ? "English" : "Italiano", lang_cb);

    section(TR("Schermo"));
    lv_obj_t *br = row(TR("Luminosità"), nullptr, nullptr);
    lv_obj_t *sl = lv_slider_create(br);
    lv_obj_set_size(sl, 110, 4);
    lv_obj_align(sl, LV_ALIGN_RIGHT_MID, -20, 0);
    lv_slider_set_range(sl, 10, 100);
    lv_slider_set_value(sl, ui_brightness(), LV_ANIM_OFF);
    lv_obj_set_ext_click_area(sl, 14);
    lv_obj_set_style_bg_color(sl, lv_color_hex(0x5A5A5A), LV_PART_MAIN);
    lv_obj_set_style_bg_color(sl, C_GREEN, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(sl, C_TEXT, LV_PART_KNOB);
    lv_obj_set_style_pad_all(sl, 5, LV_PART_KNOB);
    lv_obj_add_event_cb(sl, bright_cb, LV_EVENT_VALUE_CHANGED, this);
    lv_obj_add_event_cb(sl, bright_cb, LV_EVENT_RELEASED, this);

    lv_obj_t *so = row(TR("Spegni lo schermo"), "-", scr_cb);
    scr_val = lv_obj_get_child(so, 1);
    toggle(TR("Riattiva toccando"), TR("Se lo schermo si è spento da solo"), power_touch_wake(), touchwake_cb);

    section(TR("Tasto BOOT"));
    lv_obj_t *hint = ui_label(col, &font_r12, C_SUB,
                              TR("1 clic: schermo spento o acceso, la musica continua\n"
                              "2 clic: play / pausa\n"
                              "3 clic: avvia o ferma il timer\n"
                              "Tenuto premuto: spegne il lettore (si riaccende col tasto)"));
    lv_obj_set_width(hint, SCR_W - 28);
    lv_obj_set_style_pad_left(hint, 14, 0);
    lv_obj_set_style_pad_bottom(hint, 6, 0);
    lv_obj_t *ao = row(TR("Spegnimento automatico"), "-", off_cb);
    off_val = lv_obj_get_child(ao, 1);
    row(TR("Spegni ora"), TR("Si riaccende col tasto BOOT"), poweroff_cb);
    power_labels();

    section(TR("Aggiungere musica"));
    lv_obj_t *ur = row(TR("Disco USB"), "-", usb_cb);
    usb_sub = lv_obj_get_child(ur, 1);
    lv_obj_set_width(usb_sub, SCR_W - 28);
    usb_update();
    lv_obj_t *xr = row(TR("Trasferimento file"), TR("Carica e gestisci la musica dal browser"), xfer_cb);
    lv_obj_set_width(lv_obj_get_child(xr, 1), SCR_W - 28);

    section(TR("Libreria"));
    char buf[96];
    snprintf(buf, sizeof buf, TR("%u brani \xE2\x80\xA2 %u album \xE2\x80\xA2 %u artisti"), (unsigned)lib_tracks().size(),
             (unsigned)lib_albums().size(), (unsigned)lib_artists().size());
    row(TR("Aggiorna libreria"), buf, rescan_cb);
    row(TR("Cartella cache"), APP_DIR, nullptr);

    section(TR("Informazioni"));
    row(APP_NAME " 1.0", TR("Lettore FLAC per Freenove ESP32-S3 Display"), nullptr);
    row(TR("Formati"), TR("FLAC 8-32 bit, fino a 96 kHz (oltre: decimato)"), nullptr);
  }

  void refresh(uint32_t) override {
    lv_label_set_text(out_val, out_name());
    lv_label_set_text(jack_val, jack_name());
  }
};

}  // namespace

Page *page_settings_create() { return new SettingsPage(); }
