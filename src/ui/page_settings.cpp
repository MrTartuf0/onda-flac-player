// Impostazioni: batteria, uscita audio, rilevamento cuffie, luminosità, libreria.
#include "ui_internal.h"
#include <stdio.h>
#include "../lib/library.h"
#include "../audio/audio_out.h"
#include "../audio/player.h"
#include "../platform.h"
#include "../hal/battery.h"

namespace {

class SettingsPage : public Page {
 public:
  lv_obj_t *col, *out_val, *jack_val;
  lv_obj_t *bat_lvl = nullptr, *bat_state = nullptr, *bat_icon = nullptr, *bat_load = nullptr;
  lv_timer_t *bat_timer = nullptr;

  SettingsPage() { build(); }
  ~SettingsPage() override {
    if (bat_timer) lv_timer_delete(bat_timer);
  }

  static void back_cb(lv_event_t *) { ui_pop(); }
  static void out_cb(lv_event_t *) { ui_device_sheet(); }
  static void rescan_cb(lv_event_t *) { ui_request_rescan(); }
  static void xfer_cb(lv_event_t *) { ui_transfer_open(); }
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
  static void bat_timer_cb(lv_timer_t *t) { ((SettingsPage *)lv_timer_get_user_data(t))->bat_update(); }

  void bat_update() {
    BatteryInfo b = battery_info();
    char buf[64];
    if (b.state == BAT_NONE || b.pct < 0) {
      snprintf(buf, sizeof buf, "Non rilevata (%d,%02d V)", b.mv / 1000, b.mv % 1000 / 10);
      lv_label_set_text(bat_lvl, buf);
      lv_label_set_text(bat_state, "Collega una LiPo 3,7 V al connettore BAT");
      lv_obj_add_flag(bat_icon, LV_OBJ_FLAG_HIDDEN);
      return;
    }
    snprintf(buf, sizeof buf, "%d%% \xE2\x80\xA2 %d,%02d V", b.pct, b.mv / 1000, b.mv % 1000 / 10);
    lv_label_set_text(bat_lvl, buf);
    char t[24];
    if (b.state == BAT_CHARGING && b.minutes_to_full > 0) {
      ui_fmt_minutes(b.minutes_to_full, t, sizeof t);
      snprintf(buf, sizeof buf, "In carica \xE2\x80\xA2 piena tra %s", t);
      lv_label_set_text(bat_state, buf);
    } else if (b.state == BAT_DISCHARGING && b.minutes_left >= 0) {
      ui_fmt_minutes(b.minutes_left, t, sizeof t);
      snprintf(buf, sizeof buf, "%s \xE2\x80\xA2 %s rimasti", ui_battery_state_text(b), t);
      lv_label_set_text(bat_state, buf);
    } else {
      lv_label_set_text(bat_state, ui_battery_state_text(b));
    }
    if (b.load_ma > 0) {
      snprintf(buf, sizeof buf, "~%d mA", (b.load_ma + 5) / 10 * 10);
      lv_label_set_text(bat_load, buf);
    } else {
      lv_label_set_text(bat_load, "-");
    }
    bool chg = b.state == BAT_CHARGING || b.state == BAT_FULL;
    ui_battery_icon_set(bat_icon, b.pct, chg ? C_GREEN : (b.low ? lv_color_hex(0xF15E6C) : C_TEXT));
    lv_obj_remove_flag(bat_icon, LV_OBJ_FLAG_HIDDEN);
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
      case OUT_SPEAKER: return "Altoparlante integrato";
      case OUT_HEADPHONES: return "Cuffie (PCM5102)";
      default: return "Automatico";
    }
  }

  const char *jack_name() {
    if (!audio_out_hp_detect_available()) return "Non configurato (HP_DETECT_PIN = -1)";
    return audio_out_hp_present() ? "Cuffie collegate" : "Cuffie non collegate";
  }

  void build() {
    lv_obj_t *r = ui_page_scroll(this);
    col = ui_box(r);
    lv_obj_set_size(col, SCR_W, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_bottom(col, BOTTOM_PAD, 0);

    lv_obj_t *top = ui_box(col);
    lv_obj_set_size(top, SCR_W, 46);
    lv_obj_t *b = ui_icon_btn(top, &icons_18, IC_BACK, C_TEXT, back_cb);
    lv_obj_align(b, LV_ALIGN_LEFT_MID, 14, 0);
    lv_obj_t *t = ui_label(top, &font_b16, C_TEXT, "Impostazioni");
    lv_obj_center(t);

    section("Batteria");
    lv_obj_t *bl = row("Livello", "-", nullptr);
    bat_lvl = lv_obj_get_child(bl, 1);
    bat_icon = ui_battery_icon(bl, 26, 13);
    lv_obj_align(bat_icon, LV_ALIGN_RIGHT_MID, -16, 0);
    lv_obj_t *bs = row("Stato", "-", nullptr);
    bat_state = lv_obj_get_child(bs, 1);
    lv_obj_t *bc = row("Consumo stimato", "-", nullptr);
    bat_load = lv_obj_get_child(bc, 1);
    lv_obj_set_width(bat_load, SCR_W - 28);
    lv_obj_set_width(bat_state, SCR_W - 28);
    bat_update();
    bat_timer = lv_timer_create(bat_timer_cb, 1000, this);

    section("Audio");
    lv_obj_t *o = row("Uscita audio", out_name(), out_cb);
    out_val = lv_obj_get_child(o, 1);
    lv_obj_t *j = row("Jack cuffie", jack_name(), nullptr);
    jack_val = lv_obj_get_child(j, 1);
    toggle("Pausa se scolleghi le cuffie", "Come sul telefono", audio_out_pause_on_unplug(), pause_cb);
    toggle("Inverti rilevamento cuffie", "Se il jack funziona al contrario", audio_out_detect_inverted(), invert_cb);

    section("Schermo");
    lv_obj_t *br = row("Luminosità", nullptr, nullptr);
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

    section("Wi-Fi");
    row("Trasferimento file", "Carica e gestisci la musica dal browser", xfer_cb);

    section("Libreria");
    char buf[96];
    snprintf(buf, sizeof buf, "%u brani \xE2\x80\xA2 %u album \xE2\x80\xA2 %u artisti", (unsigned)lib_tracks().size(),
             (unsigned)lib_albums().size(), (unsigned)lib_artists().size());
    row("Aggiorna libreria", buf, rescan_cb);
    row("Cartella cache", APP_DIR, nullptr);

    section("Informazioni");
    row(APP_NAME " 1.0", "Lettore FLAC per Freenove ESP32-S3 Display", nullptr);
    row("Formati", "FLAC 8-32 bit, fino a 96 kHz (oltre: decimato)", nullptr);
  }

  void refresh(uint32_t) override {
    lv_label_set_text(out_val, out_name());
    lv_label_set_text(jack_val, jack_name());
  }
};

}  // namespace

Page *page_settings_create() { return new SettingsPage(); }
