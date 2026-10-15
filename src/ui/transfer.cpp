// Schermata "Trasferimento file": Wi-Fi + server web per gestire la microSD
// dal browser. Mentre è aperta la musica è ferma e l'uscita audio sospesa
// (il Wi-Fi ha bisogno della RAM interna occupata dal DMA dell'I2S).
#include "ui_internal.h"
#include "../i18n.h"
#include <stdio.h>
#include <string.h>
#include "../net/xfer.h"
#include "../audio/player.h"
#include "../audio/audio_out.h"
#include "../voice/voice.h"

namespace {

enum View : uint8_t { V_MAIN, V_NETS, V_PASS };

lv_obj_t *xf = nullptr;                          // schermata intera
lv_obj_t *v_main, *v_nets, *v_pass;              // viste
lv_obj_t *m_wait, *m_wait_lbl, *m_ready, *m_head, *m_qr, *m_big, *m_small, *m_step2;
lv_obj_t *m_status, *m_bar, *m_net_val;
lv_obj_t *n_list;
lv_obj_t *p_title, *p_ta, *p_kb;
View s_view = V_MAIN;
bool s_open = false;
XferState s_shown = XF_OFF;
char s_qr_data[128] = "";
char s_pass_ssid[33] = "";
bool s_scanning = false;
std::vector<XferNet> s_nets;

// --------------------------------------------------------- utilità ----
lv_obj_t *topbar(lv_obj_t *parent, const char *title, lv_event_cb_t back) {
  lv_obj_t *top = ui_box(parent);
  lv_obj_set_size(top, SCR_W, 44);
  lv_obj_t *b = ui_icon_btn(top, &icons_18, IC_BACK, C_TEXT, back);
  lv_obj_align(b, LV_ALIGN_LEFT_MID, 14, 0);
  lv_obj_t *t = ui_label(top, &font_b16, C_TEXT, title);
  ui_dot(t);
  lv_obj_set_width(t, SCR_W - 100);
  lv_obj_set_style_text_align(t, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_center(t);
  return t;
}

lv_obj_t *view(lv_obj_t *parent) {
  lv_obj_t *v = ui_box(parent);
  lv_obj_set_size(v, SCR_W, SCR_H);
  lv_obj_add_flag(v, LV_OBJ_FLAG_HIDDEN);
  return v;
}

lv_obj_t *center_label(lv_obj_t *p, const lv_font_t *f, lv_color_t c, int y) {
  lv_obj_t *l = ui_label(p, f, c, "");
  lv_obj_set_width(l, SCR_W - 24);
  lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_pos(l, 12, y);
  return l;
}

void show_view(View v) {
  s_view = v;
  lv_obj_t *all[] = {v_main, v_nets, v_pass};
  for (int i = 0; i < 3; i++) {
    if (i == v) lv_obj_remove_flag(all[i], LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(all[i], LV_OBJ_FLAG_HIDDEN);
  }
}

// aggiorna solo se cambia: la schermata viene rinfrescata 10 volte al secondo
void set_text(lv_obj_t *l, const char *t) {
  if (strcmp(lv_label_get_text(l), t)) lv_label_set_text(l, t);
}

void fmt_size(uint64_t b, char *out, size_t n) {
  if (b >= 1024ULL * 1024 * 1024) snprintf(out, n, "%.1f GB", b / 1073741824.0);
  else if (b >= 1024 * 1024) snprintf(out, n, "%.0f MB", b / 1048576.0);
  else snprintf(out, n, "%.0f KB", b / 1024.0);
}

// ------------------------------------------------------ vista reti ----
void nets_rebuild();

void pass_open(const char *ssid) {
  snprintf(s_pass_ssid, sizeof s_pass_ssid, "%s", ssid);
  lv_label_set_text(p_title, ssid);
  lv_textarea_set_text(p_ta, "");
  show_view(V_PASS);
  lv_obj_add_state(p_ta, LV_STATE_FOCUSED);
}

void net_row_cb(lv_event_t *e) {
  int i = (int)(intptr_t)lv_event_get_user_data(e);
  if (i < 0) {   // hotspot del lettore
    xfer_use_hotspot();
    show_view(V_MAIN);
    return;
  }
  if (i >= (int)s_nets.size()) return;
  if (s_nets[i].secure) {
    pass_open(s_nets[i].ssid);
  } else {
    xfer_connect(s_nets[i].ssid, "");
    show_view(V_MAIN);
  }
}

lv_obj_t *net_row(const char *ic, const char *title, const char *sub, bool lock, bool check, int idx) {
  lv_obj_t *r = ui_box(n_list);
  lv_obj_set_size(r, SCR_W, sub ? 52 : 44);
  ui_pressable(r);
  lv_obj_add_event_cb(r, net_row_cb, LV_EVENT_CLICKED, (void *)(intptr_t)idx);
  lv_obj_t *i = ui_icon(r, &icons_18, ic, check ? C_GREEN : C_SUB);
  lv_obj_align(i, LV_ALIGN_LEFT_MID, 16, 0);
  lv_obj_t *t = ui_label(r, &font_r14, check ? C_GREEN : C_TEXT, title);
  ui_dot(t);
  lv_obj_set_width(t, SCR_W - 100);
  lv_obj_align(t, LV_ALIGN_LEFT_MID, 48, sub ? -9 : 0);
  if (sub) {
    lv_obj_t *s = ui_label(r, &font_r12, C_SUB, sub);
    ui_dot(s);
    lv_obj_set_width(s, SCR_W - 100);
    lv_obj_align(s, LV_ALIGN_LEFT_MID, 48, 9);
  }
  if (lock) {
    lv_obj_t *l = ui_icon(r, &icons_14, IC_LOCK, C_DIM);
    lv_obj_align(l, LV_ALIGN_RIGHT_MID, check ? -40 : -16, 0);
  }
  if (check) {
    lv_obj_t *c = ui_icon(r, &icons_14, IC_CHECK, C_GREEN);
    lv_obj_align(c, LV_ALIGN_RIGHT_MID, -16, 0);
  }
  return r;
}

void rescan_cb(lv_event_t *) {
  s_scanning = true;
  xfer_scan_start();
  nets_rebuild();
}

void nets_rebuild() {
  lv_obj_clean(n_list);
  char saved[33];
  xfer_saved_network(saved, sizeof saved);
  net_row(IC_WIFI, TR("Hotspot del lettore"), TR("Nessuna rete: collegati direttamente a onda"), false, !saved[0], -1);
  lv_obj_t *h = ui_label(n_list, &font_b14, C_TEXT, TR("Reti disponibili"));
  lv_obj_set_style_pad_left(h, 16, 0);
  lv_obj_set_style_pad_top(h, 14, 0);
  lv_obj_set_style_pad_bottom(h, 4, 0);
  if (s_scanning) {
    lv_obj_t *l = ui_label(n_list, &font_r12, C_SUB, TR("Ricerca delle reti in corso..."));
    lv_obj_set_style_pad_left(l, 16, 0);
    lv_obj_set_style_pad_top(l, 8, 0);
    return;
  }
  for (size_t i = 0; i < s_nets.size(); i++) {
    const XferNet &n = s_nets[i];
    const char *q = n.rssi > -60 ? TR("Segnale ottimo") : n.rssi > -72 ? TR("Segnale buono") : TR("Segnale debole");
    net_row(IC_WIFI, n.ssid, q, n.secure, !strcmp(n.ssid, saved), (int)i);
  }
  if (s_nets.empty()) {
    lv_obj_t *l = ui_label(n_list, &font_r12, C_SUB, TR("Nessuna rete trovata."));
    lv_obj_set_style_pad_left(l, 16, 0);
    lv_obj_set_style_pad_top(l, 8, 0);
  }
  lv_obj_t *again = ui_box(n_list);
  lv_obj_set_size(again, SCR_W, 44);
  ui_pressable(again);
  lv_obj_add_event_cb(again, rescan_cb, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *i = ui_icon(again, &icons_14, IC_REFRESH, C_TEXT);
  lv_obj_align(i, LV_ALIGN_LEFT_MID, 18, 0);
  lv_obj_t *t = ui_label(again, &font_b14, C_TEXT, TR("Cerca di nuovo"));
  lv_obj_align(t, LV_ALIGN_LEFT_MID, 48, 0);
}

void nets_open_cb(lv_event_t *) {
  show_view(V_NETS);
  s_scanning = true;
  s_nets.clear();
  xfer_scan_start();
  nets_rebuild();
}

// ------------------------------------------------- vista password ----
void pass_ready_cb(lv_event_t *e) {
  lv_event_code_t c = lv_event_get_code(e);
  if (c == LV_EVENT_READY) {
    xfer_connect(s_pass_ssid, lv_textarea_get_text(p_ta));
    show_view(V_MAIN);
  } else if (c == LV_EVENT_CANCEL) {
    show_view(V_NETS);
  }
}

// ---------------------------------------------------- vista principale ----
void back_cb(lv_event_t *) { ui_transfer_back(); }

void set_qr(const char *data) {
  if (!strcmp(data, s_qr_data)) return;
  snprintf(s_qr_data, sizeof s_qr_data, "%s", data);
  lv_qrcode_update(m_qr, data, strlen(data));
}

void main_update(const XferStatus &st) {
  bool ready = st.state == XF_STA || st.state == XF_AP;
  if (ready) {
    lv_obj_add_flag(m_wait, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(m_ready, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_remove_flag(m_wait, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(m_ready, LV_OBJ_FLAG_HIDDEN);
  }
  char b[128];
  if (st.state == XF_CONNECTING) {
    snprintf(b, sizeof b, TR("Connessione a \"%s\"..."), st.ssid[0] ? st.ssid : TR("Wi-Fi"));
    set_text(m_wait_lbl, b);
  } else if (st.state == XF_ERROR) {
    set_text(m_wait_lbl, TR("Impossibile avviare il Wi-Fi.\nRiprova tra poco."));
  }
  if (st.state == XF_STA) {
    set_text(m_head, TR("Apri questo indirizzo nel browser,\ndal computer o dal telefono"));
    snprintf(b, sizeof b, "http://%s", st.ip);
    set_qr(b);
    set_text(m_big, APP_NAME ".local");
    snprintf(b, sizeof b, TR("oppure http://%s"), st.ip);
    set_text(m_small, b);
    lv_obj_add_flag(m_step2, LV_OBJ_FLAG_HIDDEN);
  } else if (st.state == XF_AP) {
    set_text(m_head, TR("1. Collegati al Wi-Fi del lettore\n(inquadra il codice col telefono)"));
    snprintf(b, sizeof b, "WIFI:T:WPA;S:%s;P:%s;;", st.ssid, st.ap_pass);
    set_qr(b);
    set_text(m_big, st.ssid);
    snprintf(b, sizeof b, TR("Password: %s"), st.ap_pass);
    set_text(m_small, b);
    snprintf(b, sizeof b, TR("2. Apri http://%s"), st.ip);
    set_text(m_step2, b);
    lv_obj_remove_flag(m_step2, LV_OBJ_FLAG_HIDDEN);
  }
  // stato dei trasferimenti
  if (st.cur_name[0] && st.cur_total) {
    unsigned pct = (unsigned)((uint64_t)st.cur_done * 100 / st.cur_total);
    snprintf(b, sizeof b, "%u%% \xE2\x80\xA2 %s", pct, st.cur_name);
    set_text(m_status, b);
    lv_obj_set_style_text_color(m_status, C_TEXT, 0);
    lv_obj_remove_flag(m_bar, LV_OBJ_FLAG_HIDDEN);
    lv_bar_set_value(m_bar, pct, LV_ANIM_OFF);
  } else {
    lv_obj_add_flag(m_bar, LV_OBJ_FLAG_HIDDEN);
    if (st.files_in) {
      char sz[16];
      fmt_size(st.bytes_in, sz, sizeof sz);
      snprintf(b, sizeof b, TR("%u file ricevuti \xE2\x80\xA2 %s"), (unsigned)st.files_in, sz);
      set_text(m_status, b);
      lv_obj_set_style_text_color(m_status, C_GREEN, 0);
    } else if (st.note[0]) {
      set_text(m_status, st.note);
      lv_obj_set_style_text_color(m_status, lv_color_hex(0xF0B232), 0);
    } else {
      set_text(m_status, TR("La musica resta in pausa finch\xC3\xA9 sei qui"));
      lv_obj_set_style_text_color(m_status, C_DIM, 0);
    }
  }
  char saved[33];
  xfer_saved_network(saved, sizeof saved);
  set_text(m_net_val, saved[0] ? saved : TR("Hotspot del lettore"));
  s_shown = st.state;
}

void build() {
  xf = ui_box(lv_screen_active());
  lv_obj_set_size(xf, SCR_W, SCR_H);
  lv_obj_set_style_bg_color(xf, C_BG, 0);
  lv_obj_set_style_bg_opa(xf, LV_OPA_COVER, 0);
  lv_obj_add_flag(xf, LV_OBJ_FLAG_CLICKABLE);   // blocca i tocchi verso ciò che sta sotto

  // --- principale
  v_main = view(xf);
  topbar(v_main, TR("Trasferimento file"), back_cb);
  m_wait = ui_box(v_main);
  lv_obj_set_size(m_wait, SCR_W, 200);
  lv_obj_set_y(m_wait, 60);
  lv_obj_t *wi = ui_icon(m_wait, &icons_28, IC_WIFI, C_GREEN);
  lv_obj_align(wi, LV_ALIGN_TOP_MID, 0, 40);
  m_wait_lbl = center_label(m_wait, &font_b14, C_TEXT, 92);
  lv_obj_t *wsub = center_label(m_wait, &font_r12, C_SUB, 132);
  lv_label_set_text(wsub, TR("Il lettore si sta collegando alla rete"));

  m_ready = ui_box(v_main);
  lv_obj_set_size(m_ready, SCR_W, 230);
  lv_obj_set_y(m_ready, 44);
  m_head = center_label(m_ready, &font_r12, C_SUB, 0);
  lv_obj_t *qbg = ui_box(m_ready);
  lv_obj_set_size(qbg, 112, 112);
  lv_obj_align(qbg, LV_ALIGN_TOP_MID, 0, 32);
  lv_obj_set_style_bg_color(qbg, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_bg_opa(qbg, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(qbg, 8, 0);
  m_qr = lv_qrcode_create(qbg);
  lv_qrcode_set_size(m_qr, 100);
  lv_qrcode_set_dark_color(m_qr, lv_color_hex(0x000000));
  lv_qrcode_set_light_color(m_qr, lv_color_hex(0xFFFFFF));
  lv_obj_center(m_qr);
  m_big = center_label(m_ready, &font_b20, C_TEXT, 150);
  m_small = center_label(m_ready, &font_r12, C_SUB, 176);
  m_step2 = center_label(m_ready, &font_b12, C_TEXT, 194);

  m_status = center_label(v_main, &font_r12, C_DIM, SCR_H - 62);
  ui_dot(m_status);
  m_bar = lv_bar_create(v_main);
  lv_obj_set_size(m_bar, SCR_W - 48, 4);
  lv_obj_set_pos(m_bar, 24, SCR_H - 45);
  lv_bar_set_range(m_bar, 0, 100);
  lv_obj_set_style_bg_color(m_bar, lv_color_hex(0x404040), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(m_bar, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_color(m_bar, C_GREEN, LV_PART_INDICATOR);
  lv_obj_add_flag(m_bar, LV_OBJ_FLAG_HIDDEN);

  lv_obj_t *nr = ui_box(v_main);
  lv_obj_set_size(nr, SCR_W, 40);
  lv_obj_set_y(nr, SCR_H - 40);
  lv_obj_set_style_bg_color(nr, C_BG2, 0);
  lv_obj_set_style_bg_opa(nr, LV_OPA_COVER, 0);
  ui_pressable(nr);
  lv_obj_add_event_cb(nr, nets_open_cb, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *ni = ui_icon(nr, &icons_14, IC_WIFI, C_SUB);
  lv_obj_align(ni, LV_ALIGN_LEFT_MID, 16, 0);
  lv_obj_t *nl = ui_label(nr, &font_b12, C_TEXT, TR("Rete Wi-Fi"));
  lv_obj_align(nl, LV_ALIGN_LEFT_MID, 40, 0);
  m_net_val = ui_label(nr, &font_r12, C_SUB, "");
  ui_dot(m_net_val);
  lv_obj_set_width(m_net_val, 100);
  lv_obj_set_style_text_align(m_net_val, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_align(m_net_val, LV_ALIGN_RIGHT_MID, -32, 0);
  lv_obj_t *ch = ui_icon(nr, &icons_14, IC_RIGHT, C_SUB);
  lv_obj_align(ch, LV_ALIGN_RIGHT_MID, -14, 0);

  // --- elenco reti
  v_nets = view(xf);
  topbar(v_nets, TR("Rete Wi-Fi"), back_cb);
  n_list = ui_box(v_nets);
  lv_obj_set_size(n_list, SCR_W, SCR_H - 44);
  lv_obj_set_y(n_list, 44);
  lv_obj_set_flex_flow(n_list, LV_FLEX_FLOW_COLUMN);
  lv_obj_add_flag(n_list, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(n_list, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(n_list, LV_SCROLLBAR_MODE_OFF);

  // --- password
  v_pass = view(xf);
  p_title = topbar(v_pass, "", back_cb);
  lv_obj_t *pl = ui_label(v_pass, &font_r12, C_SUB, TR("Password della rete"));
  lv_obj_set_pos(pl, 16, 50);
  p_ta = lv_textarea_create(v_pass);
  lv_textarea_set_one_line(p_ta, true);
  lv_textarea_set_password_mode(p_ta, false);   // si vede cosa scrivi: niente errori di battitura
  lv_obj_set_size(p_ta, SCR_W - 32, 40);
  lv_obj_set_pos(p_ta, 16, 70);
  lv_obj_set_style_bg_color(p_ta, lv_color_hex(0x2A2A2A), 0);
  lv_obj_set_style_border_width(p_ta, 0, 0);
  lv_obj_set_style_radius(p_ta, 6, 0);
  lv_obj_set_style_text_font(p_ta, &font_r14, 0);
  lv_obj_set_style_text_color(p_ta, C_TEXT, 0);
  lv_obj_add_event_cb(p_ta, pass_ready_cb, LV_EVENT_ALL, nullptr);
  lv_obj_t *ph = ui_label(v_pass, &font_r10, C_DIM, TR("Poi premi il tasto in basso a destra"));
  lv_obj_set_pos(ph, 16, 116);
  p_kb = lv_keyboard_create(v_pass);
  lv_obj_set_size(p_kb, SCR_W, 150);
  lv_obj_align(p_kb, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_keyboard_set_textarea(p_kb, p_ta);
  lv_obj_set_style_bg_color(p_kb, lv_color_hex(0x1A1A1A), 0);
  lv_obj_set_style_text_font(p_kb, &lv_font_montserrat_14, LV_PART_ITEMS);

  show_view(V_MAIN);
  lv_obj_add_flag(xf, LV_OBJ_FLAG_HIDDEN);
}

}  // namespace

// ================================================================= API ==

bool ui_transfer_visible() { return s_open; }

void ui_transfer_open() {
  if (s_open) return;
  if (!xf) build();
  // il Wi-Fi vuole tutta la RAM interna possibile: via il modello vocale
  ui_voice_close();
  voice_release();
  ui_session_save();
  // musica ferma e file chiusi: dal browser si può cancellare anche il brano in ascolto
  player_stop();
  for (int i = 0; i < 40 && player_status().track >= 0; i++) lv_delay_ms(25);
  player_hold_output(true);
  audio_out_suspend();
  ui_nowplaying_close();
  s_open = true;
  s_qr_data[0] = 0;
  s_shown = XF_OFF;
  show_view(V_MAIN);
  xfer_start();
  main_update(xfer_status());
  lv_obj_move_to_index(xf, -1);
  lv_obj_remove_flag(xf, LV_OBJ_FLAG_HIDDEN);
}

void ui_transfer_close() {
  if (!s_open) return;
  XferStatus st = xfer_status();
  xfer_stop();
  audio_out_resume();
  player_hold_output(false);
  s_open = false;
  lv_obj_add_flag(xf, LV_OBJ_FLAG_HIDDEN);
  ui_after_transfer(st.changes > 0);
}

bool ui_transfer_back() {
  if (!s_open) return false;
  if (s_view == V_PASS) show_view(V_NETS);
  else if (s_view == V_NETS) show_view(V_MAIN);
  else ui_transfer_close();
  return true;
}

void ui_transfer_tick() {
  if (!s_open) return;
  XferStatus st = xfer_status();
  if (st.done_req) {   // dal browser: "Aggiorna libreria"
    ui_transfer_close();
    return;
  }
  main_update(st);
  if (s_view == V_NETS && s_scanning && xfer_scan_result(s_nets)) {
    s_scanning = false;
    nets_rebuild();
  }
}
