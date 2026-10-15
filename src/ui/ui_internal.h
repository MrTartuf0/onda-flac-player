// Interni dell'interfaccia: tema, widget riutilizzabili, navigazione.
#pragma once
#include <functional>
#include <vector>
#include "../lvgl/lvgl.h"
#include "../fonts/fonts.h"
#include "../types.h"
#include "../i18n.h"
#include "../config.h"

// ------------------------------------------------------------------ tema ----
#define C_BG        lv_color_hex(0x121212)
#define C_BG2       lv_color_hex(0x181818)
#define C_TILE      lv_color_hex(0x2A2A2A)
#define C_ELEV      lv_color_hex(0x282828)
#define C_PRESS     lv_color_hex(0x2E2E2E)
#define C_TEXT      lv_color_hex(0xFFFFFF)
#define C_SUB       lv_color_hex(0xB3B3B3)
#define C_DIM       lv_color_hex(0x7A7A7A)
#define C_GREEN     lv_color_hex(0x1ED760)
#define C_BLACK     lv_color_hex(0x000000)

constexpr int SCR_W = LCD_W;
constexpr int SCR_H = LCD_H;
constexpr int NAV_H = 48;
constexpr int MINI_H = 50;
constexpr int PAGE_H = SCR_H - NAV_H;
constexpr int BOTTOM_PAD = MINI_H + 14;   // spazio per il mini player sopra la barra

lv_color_t col565(uint16_t c);
lv_color_t col_darken(lv_color_t c, uint8_t keep);   // keep: 0..255
lv_color_t album_color(int album);                   // colore dominante (o grigio)
lv_color_t genre_color(int genre);

// --------------------------------------------------------- widget base ----
lv_obj_t *ui_box(lv_obj_t *parent);                                  // contenitore "pulito"
lv_obj_t *ui_label(lv_obj_t *parent, const lv_font_t *f, lv_color_t c, const char *txt = "");
lv_obj_t *ui_icon(lv_obj_t *parent, const lv_font_t *f, const char *ic, lv_color_t c);
lv_obj_t *ui_icon_btn(lv_obj_t *parent, const lv_font_t *f, const char *ic, lv_color_t c,
                      lv_event_cb_t cb, void *ud = nullptr);
lv_obj_t *ui_play_btn(lv_obj_t *parent, int size, lv_event_cb_t cb, void *ud = nullptr);
void ui_play_btn_set(lv_obj_t *btn, bool playing);
lv_obj_t *ui_chip(lv_obj_t *parent, const char *txt, lv_event_cb_t cb, void *ud);
void ui_chip_set(lv_obj_t *chip, bool on);
void ui_pressable(lv_obj_t *o);                      // effetto pressione standard
void ui_dot(lv_obj_t *label);                        // una riga con "..."

enum CoverKind : uint8_t { COVER_ALBUM, COVER_LIKED, COVER_ARTIST, COVER_PLAYLIST, COVER_GENRE, COVER_ALL };
lv_obj_t *ui_cover(lv_obj_t *parent, int size, int radius);
void ui_cover_set(lv_obj_t *cover, CoverKind kind, int index);       // index = album/genere

// ---------------------------------------------------- lista virtuale ----
// Crea solo le righe visibili e le ricicla durante lo scroll: liste lunghe
// (centinaia di album o migliaia di brani) senza esaurire la memoria.
struct VList {
  lv_obj_t *scroll = nullptr;
  int y0 = 0;
  int row_h = 56;
  int count = 0;
  std::vector<lv_obj_t *> pool;
  std::vector<int> bound;
  lv_obj_t *spacer = nullptr;
  std::function<lv_obj_t *(lv_obj_t *)> create;
  std::function<void(lv_obj_t *, int)> bind;
  void init(lv_obj_t *scroll_parent, int start_y, int row_height);
  void set_count(int n);
  void set_y0(int y);
  void refresh(bool force = false);
  void rebind_all() { refresh(true); }
};

// Riga standard (copertina + titolo + sottotitolo + "...")
struct RowParts {
  lv_obj_t *cover, *title, *sub, *eq, *more;
};
lv_obj_t *ui_row(lv_obj_t *parent, int h, int cover_size, bool cover_round, bool with_more);
RowParts ui_row_parts(lv_obj_t *row);
void ui_row_set_index(lv_obj_t *row, int idx);
int ui_row_index(lv_obj_t *row);

// ------------------------------------------------------------ pagine ----
enum RefreshWhat : uint32_t {
  RF_PLAYER = 1 << 0,    // brano/stato cambiato
  RF_LIKED = 1 << 1,
  RF_LIBRARY = 1 << 2,
  RF_ALL = 0xFFFFFFFF,
};

class Page {
 public:
  lv_obj_t *root = nullptr;
  virtual ~Page() {
    if (root) lv_obj_delete(root);
  }
  virtual void refresh(uint32_t what) { (void)what; }
  virtual void on_show() {}
  virtual bool on_back() { return false; }   // gesto "indietro": true = gestito dalla pagina
  // cosa mostra (album, artista, playlist...): 0 = niente di identificabile.
  // Serve a non aprire due volte di fila la stessa pagina.
  virtual uint64_t ident() const { return 0; }
};

enum Tab : uint8_t { TAB_HOME = 0, TAB_SEARCH = 1, TAB_SETTINGS = 2 };

lv_obj_t *ui_page_root();            // dove creare le pagine
lv_obj_t *ui_page_scroll(Page *p);   // radice scrollabile standard di una pagina
void ui_push(Page *p);
void ui_pop();
void ui_switch_tab(Tab t);
void ui_set_chrome_visible(bool vis);   // nasconde barra e mini player (tastiera ricerca)

// Pagine
Page *page_home_create();
Page *page_search_create();
Page *page_collection_create(const PlayContext &ctx);
Page *page_artist_create(int artist);
Page *page_settings_create();

// Azioni comuni
void ui_open_album(int album);
void ui_open_artist(int artist);
void ui_open_context(const PlayContext &ctx);
void ui_play_context(const PlayContext &ctx, int start_index, bool force_shuffle = false);
void ui_toggle_context(const PlayContext &ctx);       // play/pausa se è il contesto attivo
bool ui_context_is_current(const PlayContext &ctx);
bool ui_is_playing();
int ui_current_track();
void ui_toast(const char *msg);
void ui_track_menu(int track);
void ui_device_sheet();
void ui_queue_open();
bool ui_queue_visible();
void ui_queue_close();
bool ui_sheet_close();                       // chiude il foglio aperto; false se non ce n'era
void ui_back();                              // "indietro": foglio, coda, in riproduzione, pagina
bool ui_nowplaying_blocks_edge(int x, int y);   // zone dove il gesto dal bordo è disattivato
void ui_nowplaying_open();
void ui_nowplaying_close();
bool ui_nowplaying_visible();
void ui_nowplaying_refresh(uint32_t what);
void ui_nowplaying_tick();
void ui_nowplaying_build(lv_obj_t *scr);
void ui_set_brightness(uint8_t pct, bool save = true);   // save: scrive in flash
uint8_t ui_brightness();
void ui_pref_set(const char *key, int32_t v);   // salva in flash (rimandato durante la riproduzione)
void ui_request_rescan(bool force = true);
// Trasferimento file via Wi-Fi (src/ui/transfer.cpp)
void ui_transfer_open();
void ui_transfer_close();
bool ui_transfer_visible();
bool ui_transfer_back();      // gesto/tasto indietro dentro la schermata
void ui_transfer_tick();
void ui_session_save();                  // salva contesto, brano e posizione
void ui_after_transfer(bool changed);    // dopo il trasferimento: riscansione o ripristino
const char *ctx_type_label(CtxType t);
// Batteria (src/ui/battery_ui.cpp)
struct BatteryInfo;
void ui_battery_tick();                  // avvisi di batteria scarica
lv_obj_t *ui_battery_icon(lv_obj_t *parent, int w, int h);
void ui_battery_icon_set(lv_obj_t *icon, int pct, lv_color_t c);
const char *ui_battery_state_text(const BatteryInfo &b);
void ui_fmt_minutes(int m, char *buf, size_t n);   // durata stimata arrotondata
// Ricerca vocale (src/ui/voice_ui.cpp): schermata del microfono
void ui_voice_open();
bool ui_voice_visible();
bool ui_voice_close();                   // false se non era aperta
// Centro di controllo (src/ui/control_center.cpp): scorri su dal bordo in basso
void ui_cc_build();
bool ui_cc_visible();
bool ui_cc_close();                      // false se non era aperto
void ui_cc_drag(int dist);               // gesto dal bordo in basso in corso
void ui_cc_drag_end(bool commit, int speed);
bool ui_cc_blocks_edge(int x, int y);
