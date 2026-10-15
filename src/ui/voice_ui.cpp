// Ricerca vocale: schermata a tutto schermo aperta dal microfono in Cerca.
// Mentre ascolta la musica va in pausa (il microfono usa lo stesso I2S
// dell'uscita) e riprende chiudendo, a meno di aver scelto un brano.
#include "ui_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../voice/voice.h"
#include "../lib/library.h"
#include "../audio/player.h"
#include "../platform.h"

namespace {

constexpr int MIC = 76;            // pulsante del microfono
constexpr int MIC_Y = 186;         // centro del microfono
constexpr int RES_Y = 92;          // inizio dei risultati
// il primo risultato si apre da solo se è abbastanza netto
constexpr float kAutoProb = 0.50f;
constexpr float kAutoGap = 0.15f;
constexpr uint32_t kAutoMs = 900;

lv_obj_t *s_root = nullptr;
lv_obj_t *s_title, *s_sub, *s_ring, *s_ring2, *s_mic, *s_mic_ic, *s_note, *s_res, *s_retry;
lv_obj_t *s_rows[3];
lv_timer_t *s_tm = nullptr;
bool s_open = false;
bool s_was_playing = false;
VoiceState s_shown = VOICE_IDLE;
int s_shown_n = -1;
VoiceStatus s_vs;
int s_lv = 0;
bool s_auto = false;
uint32_t s_done_ms = 0;
char s_example[96];

void tick_cb(lv_timer_t *);

void set_mic(lv_color_t bg, lv_color_t fg) {
  lv_obj_set_style_bg_color(s_mic, bg, 0);
  lv_obj_set_style_text_color(s_mic_ic, fg, 0);
}

void show_mic_area(bool on) {
  lv_obj_t *o[] = {s_mic, s_ring, s_ring2, s_note};
  for (auto x : o) {
    if (on) lv_obj_remove_flag(x, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(x, LV_OBJ_FLAG_HIDDEN);
  }
  if (on) lv_obj_add_flag(s_res, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_remove_flag(s_res, LV_OBJ_FLAG_HIDDEN);
}

void close_now(bool resume) {
  if (!s_open) return;
  s_open = false;
  voice_cancel();
  if (s_tm) {
    lv_timer_delete(s_tm);
    s_tm = nullptr;
  }
  lv_obj_add_flag(s_root, LV_OBJ_FLAG_HIDDEN);
  if (resume && s_was_playing) player_resume();
  s_was_playing = false;
}

void act(int i) {
  if (i < 0 || i >= s_vs.n) return;
  VoiceMatch m = s_vs.m[i];
  plat_log("[voice] scelto %d (tipo %d, indice %d)", i + 1, m.kind, m.index);
  if (m.kind == VK_TRACK) {
    close_now(false);
    const Track &t = lib_tracks()[m.index];
    PlayContext c = lib_ctx_album(t.album);
    std::vector<uint16_t> tr;
    lib_context_tracks(c, tr);
    int start = 0;
    for (size_t k = 0; k < tr.size(); k++)
      if (tr[k] == m.index) start = k;
    ui_play_context(c, start);
    ui_nowplaying_open();
    return;
  }
  close_now(true);
  switch (m.kind) {
    case VK_ARTIST: ui_open_artist(m.index); break;
    case VK_ALBUM: ui_open_album(m.index); break;
    case VK_PLAYLIST: ui_open_context(lib_ctx_playlist(m.index)); break;
    case VK_GENRE: ui_open_context(lib_ctx_genre(m.index)); break;
    default: break;
  }
}

void close_cb(lv_event_t *) { close_now(true); }
void row_cb(lv_event_t *e) { act(ui_row_index(lv_event_get_current_target_obj(e))); }

void listen_again() {
  s_shown = VOICE_IDLE;
  s_shown_n = -1;
  s_auto = false;
  voice_listen();
}

void mic_cb(lv_event_t *) {
  if (s_vs.state == VOICE_LISTENING) voice_stop();     // ho finito di parlare
  else if (s_vs.state != VOICE_LOADING) listen_again();
}
void retry_cb(lv_event_t *) { listen_again(); }

void bind_row(int i) {
  const VoiceMatch &m = s_vs.m[i];
  RowParts p = ui_row_parts(s_rows[i]);
  char sub[160];
  lv_obj_set_style_radius(p.cover, m.kind == VK_ARTIST ? LV_RADIUS_CIRCLE : 3, 0);
  const char *title = "";
  switch (m.kind) {
    case VK_ARTIST:
      ui_cover_set(p.cover, COVER_ARTIST, m.index);
      title = lib_artists()[m.index].name;
      snprintf(sub, sizeof sub, "%s", TR("Artista"));
      break;
    case VK_ALBUM: {
      const Album &a = lib_albums()[m.index];
      ui_cover_set(p.cover, COVER_ALBUM, m.index);
      title = a.title;
      snprintf(sub, sizeof sub, TR("Album \xE2\x80\xA2 %s"), a.artist);
      break;
    }
    case VK_PLAYLIST: {
      const Playlist &pl = lib_playlists()[m.index];
      ui_cover_set(p.cover, pl.tracks.empty() ? COVER_PLAYLIST : COVER_ALBUM,
                   pl.tracks.empty() ? -1 : lib_tracks()[pl.tracks[0]].album);
      title = pl.name;
      snprintf(sub, sizeof sub, "%s", TR("Playlist"));
      break;
    }
    case VK_GENRE:
      ui_cover_set(p.cover, COVER_GENRE, m.index);
      title = lib_genres()[m.index].name;
      snprintf(sub, sizeof sub, "%s", TR("Genere"));
      break;
    case VK_TRACK: {
      const Track &t = lib_tracks()[m.index];
      ui_cover_set(p.cover, COVER_ALBUM, t.album);
      title = t.title;
      snprintf(sub, sizeof sub, TR("Brano \xE2\x80\xA2 %s"), t.artist);
      break;
    }
  }
  lv_label_set_text(p.title, title);
  lv_label_set_text(p.sub, sub);
  lv_obj_set_style_text_color(p.title, (s_auto && i == 0) ? C_GREEN : C_TEXT, 0);
}

void pick_example() {
  s_example[0] = 0;
  const auto &ar = lib_artists();
  const auto &al = lib_albums();
  char tmp[64];
  for (int tries = 0; tries < 12; tries++) {
    bool artist = (rand() & 1) && !ar.empty();
    const char *n = artist ? ar[rand() % ar.size()].name : (al.empty() ? nullptr : al[rand() % al.size()].title);
    if (!n || !strcmp(n, TR("Artista sconosciuto")) || !strcmp(n, TR("Brani singoli"))) continue;
    if (strlen(n) > 28 || !voice_phrase_text(n, true, tmp, sizeof tmp)) continue;
    snprintf(s_example, sizeof s_example, TR("Prova: \xE2\x80\x9C%s\xE2\x80\x9D"), n);
    return;
  }
}

void update_state() {
  const VoiceStatus &v = s_vs;
  bool changed = v.state != s_shown || (v.state == VOICE_DONE && v.n != s_shown_n);
  if (!changed) return;
  s_shown = v.state;
  s_shown_n = v.n;
  switch (v.state) {
    case VOICE_IDLE:
    case VOICE_LOADING:
      show_mic_area(true);
      lv_label_set_text(s_title, TR("Un attimo\xE2\x80\xA6"));
      lv_label_set_text(s_sub, TR("La prima volta prepara i nomi della libreria: qualche secondo"));
      set_mic(C_TILE, C_SUB);
      lv_obj_add_flag(s_ring, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(s_ring2, LV_OBJ_FLAG_HIDDEN);
      break;
    case VOICE_LISTENING: {
      show_mic_area(true);
      lv_label_set_text(s_title, TR("Ti ascolto\xE2\x80\xA6"));
      char sub[200];
      snprintf(sub, sizeof sub, "%s%s%s", TR("Dì il nome di un artista, un album, una playlist o un brano"),
               s_example[0] ? "\n" : "", s_example);
      lv_label_set_text(s_sub, sub);
      set_mic(C_GREEN, C_BLACK);
      break;
    }
    case VOICE_DONE:
      if (v.n == 0) {
        show_mic_area(true);
        lv_label_set_text(s_title, TR("Non ho capito"));
        lv_label_set_text(s_sub, TR("Tocca il microfono e riprova, scandendo bene il nome"));
        set_mic(C_TEXT, C_BLACK);
        lv_obj_add_flag(s_ring, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_ring2, LV_OBJ_FLAG_HIDDEN);
        break;
      }
      s_auto = v.m[0].prob >= kAutoProb && (v.n == 1 || v.m[0].prob - v.m[1].prob >= kAutoGap);
      s_done_ms = plat_millis();
      show_mic_area(false);
      lv_label_set_text(s_title, s_auto ? TR("Trovato") : TR("Forse cercavi"));
      lv_label_set_text(s_sub, s_auto ? TR("Apro\xE2\x80\xA6") : TR("Tocca quello giusto"));
      for (int i = 0; i < 3; i++) {
        if (i < v.n) {
          bind_row(i);
          lv_obj_remove_flag(s_rows[i], LV_OBJ_FLAG_HIDDEN);
        } else {
          lv_obj_add_flag(s_rows[i], LV_OBJ_FLAG_HIDDEN);
        }
      }
      break;
    case VOICE_ERROR:
      show_mic_area(true);
      lv_label_set_text(s_title, TR("Ricerca vocale non disponibile"));
      lv_label_set_text(s_sub, v.error && v.error[0] ? v.error : "");
      set_mic(C_TILE, C_SUB);
      lv_obj_add_flag(s_ring, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(s_ring2, LV_OBJ_FLAG_HIDDEN);
      break;
  }
}

void tick_cb(lv_timer_t *) {
  if (!s_open) return;
  s_vs = voice_status();
  update_state();
  if (s_vs.state == VOICE_LISTENING) {
    // anelli che seguono la voce (addolciti)
    int target = s_vs.level < 25 ? 0 : (s_vs.level - 25) * 100 / 75;
    s_lv = target > s_lv ? (s_lv + target) / 2 : s_lv * 3 / 4;
    int d1 = MIC + 10 + s_lv * 44 / 100, d2 = MIC + 18 + s_lv * 80 / 100;
    lv_obj_set_size(s_ring, d1, d1);
    lv_obj_set_size(s_ring2, d2, d2);
    lv_obj_align(s_ring, LV_ALIGN_TOP_MID, 0, MIC_Y - d1 / 2);
    lv_obj_align(s_ring2, LV_ALIGN_TOP_MID, 0, MIC_Y - d2 / 2);
    lv_obj_remove_flag(s_ring, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(s_ring2, LV_OBJ_FLAG_HIDDEN);
  }
  if (s_vs.state == VOICE_DONE && s_auto && s_vs.n > 0 && plat_millis() - s_done_ms >= kAutoMs) act(0);
}

lv_obj_t *circle(lv_obj_t *parent, int d, lv_color_t c, lv_opa_t opa) {
  lv_obj_t *o = ui_box(parent);
  lv_obj_set_size(o, d, d);
  lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(o, c, 0);
  lv_obj_set_style_bg_opa(o, opa, 0);
  lv_obj_align(o, LV_ALIGN_TOP_MID, 0, MIC_Y - d / 2);
  return o;
}

void build() {
  s_root = ui_box(lv_layer_top());
  lv_obj_set_size(s_root, SCR_W, SCR_H);
  lv_obj_set_style_bg_color(s_root, C_BG, 0);
  lv_obj_set_style_bg_grad_color(s_root, lv_color_hex(0x0D2A19), 0);
  lv_obj_set_style_bg_grad_dir(s_root, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_bg_main_stop(s_root, 150, 0);
  lv_obj_set_style_bg_opa(s_root, LV_OPA_COVER, 0);
  lv_obj_add_flag(s_root, LV_OBJ_FLAG_CLICKABLE);   // niente tocchi alle pagine sotto

  lv_obj_t *x = ui_icon_btn(s_root, &icons_18, IC_CLOSE, C_TEXT, close_cb);
  lv_obj_set_pos(x, 14, 14);
  lv_obj_set_ext_click_area(x, 14);

  lv_obj_t *head = ui_box(s_root);
  lv_obj_set_size(head, SCR_W - 32, LV_SIZE_CONTENT);
  lv_obj_align(head, LV_ALIGN_TOP_MID, 0, 44);
  lv_obj_set_flex_flow(head, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(head, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_gap(head, 6, 0);
  s_title = ui_label(head, &font_b20, C_TEXT, "");
  lv_obj_set_width(s_title, lv_pct(100));
  lv_obj_set_style_text_align(s_title, LV_TEXT_ALIGN_CENTER, 0);
  s_sub = ui_label(head, &font_r12, C_SUB, "");
  lv_obj_set_width(s_sub, lv_pct(100));
  lv_obj_set_style_text_align(s_sub, LV_TEXT_ALIGN_CENTER, 0);

  s_ring2 = circle(s_root, MIC + 18, C_GREEN, 40);
  s_ring = circle(s_root, MIC + 10, C_GREEN, 80);
  s_mic = circle(s_root, MIC, C_GREEN, LV_OPA_COVER);
  lv_obj_add_flag(s_mic, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_transform_scale(s_mic, 236, LV_STATE_PRESSED);
  lv_obj_set_style_transform_pivot_x(s_mic, MIC / 2, 0);
  lv_obj_set_style_transform_pivot_y(s_mic, MIC / 2, 0);
  lv_obj_set_ext_click_area(s_mic, 16);
  lv_obj_add_event_cb(s_mic, mic_cb, LV_EVENT_CLICKED, nullptr);
  s_mic_ic = ui_icon(s_mic, &icons_28, IC_MIC, C_BLACK);
  lv_obj_center(s_mic_ic);

  s_note = ui_label(s_root, &font_r10, C_DIM, TR("Senza internet \xE2\x80\xA2 capisce la pronuncia inglese"));
  lv_obj_set_width(s_note, SCR_W - 32);
  lv_obj_set_style_text_align(s_note, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(s_note, LV_ALIGN_BOTTOM_MID, 0, -14);

  s_res = ui_box(s_root);
  lv_obj_set_size(s_res, SCR_W, LV_SIZE_CONTENT);
  lv_obj_set_y(s_res, RES_Y);
  lv_obj_set_flex_flow(s_res, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(s_res, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  for (int i = 0; i < 3; i++) {
    s_rows[i] = ui_row(s_res, 54, 44, false, false);
    ui_row_set_index(s_rows[i], i);
    lv_obj_add_event_cb(s_rows[i], row_cb, LV_EVENT_CLICKED, nullptr);
  }
  s_retry = ui_box(s_res);
  lv_obj_set_size(s_retry, LV_SIZE_CONTENT, 36);
  lv_obj_set_style_pad_hor(s_retry, 18, 0);
  lv_obj_set_style_margin_top(s_retry, 10, 0);
  lv_obj_set_style_radius(s_retry, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_border_width(s_retry, 1, 0);
  lv_obj_set_style_border_color(s_retry, C_DIM, 0);
  lv_obj_set_flex_flow(s_retry, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(s_retry, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_gap(s_retry, 8, 0);
  ui_pressable(s_retry);
  lv_obj_add_event_cb(s_retry, retry_cb, LV_EVENT_CLICKED, nullptr);
  ui_icon(s_retry, &icons_14, IC_MIC, C_TEXT);
  ui_label(s_retry, &font_b14, C_TEXT, TR("Riprova"));
  lv_obj_add_flag(s_res, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(s_root, LV_OBJ_FLAG_HIDDEN);
}

}  // namespace

void ui_voice_open() {
  if (s_open || !voice_supported()) return;
  if (!s_root) build();
  s_open = true;
  s_was_playing = ui_is_playing();
  if (s_was_playing) player_pause();
  pick_example();
  s_lv = 0;
  s_vs = VoiceStatus();
  s_vs.state = VOICE_LOADING;
  s_shown = VOICE_IDLE;
  s_shown_n = -1;
  s_auto = false;
  update_state();
  lv_obj_move_to_index(s_root, -1);
  lv_obj_remove_flag(s_root, LV_OBJ_FLAG_HIDDEN);
  voice_listen();
  s_tm = lv_timer_create(tick_cb, 40, nullptr);
}

bool ui_voice_visible() { return s_open; }

bool ui_voice_close() {
  if (!s_open) return false;
  close_now(true);
  return true;
}
