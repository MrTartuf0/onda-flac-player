// Impostazioni: uscita audio, rilevamento cuffie, luminosità, libreria.
#include "ui_internal.h"
#include <stdio.h>
#include "../lib/library.h"
#include "../audio/audio_out.h"
#include "../audio/player.h"
#include "../platform.h"

namespace {

class SettingsPage : public Page {
 public:
  lv_obj_t *col, *out_val, *jack_val;

  SettingsPage() { build(); }

  static void back_cb(lv_event_t *) { ui_pop(); }
  static void out_cb(lv_event_t *) { ui_device_sheet(); }
  static void rescan_cb(lv_event_t *) { ui_request_rescan(); }
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
  static void bright_cb(lv_event_t *e) {
    lv_obj_t *s = lv_event_get_target_obj(e);
    ui_set_brightness(lv_slider_get_value(s));
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
    lv_slider_set_value(sl, plat_kv_get("bright", 80), LV_ANIM_OFF);
    lv_obj_set_ext_click_area(sl, 14);
    lv_obj_set_style_bg_color(sl, lv_color_hex(0x5A5A5A), LV_PART_MAIN);
    lv_obj_set_style_bg_color(sl, C_GREEN, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(sl, C_TEXT, LV_PART_KNOB);
    lv_obj_set_style_pad_all(sl, 5, LV_PART_KNOB);
    lv_obj_add_event_cb(sl, bright_cb, LV_EVENT_VALUE_CHANGED, this);

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
