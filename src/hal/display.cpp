#include "display.h"
#include <Arduino.h>
#include <Wire.h>
#include <driver/spi_master.h>
#include <driver/gpio.h>
#include <esp_lcd_panel_io.h>
#include <esp_heap_caps.h>
#include "../lvgl/lvgl.h"
#include "../config.h"
#include "../platform.h"
#include "edge_swipe.h"
#include "../power.h"

namespace {

esp_lcd_panel_io_handle_t s_io = nullptr;
lv_display_t *s_disp = nullptr;

// Sequenza di init ILI9341 (variante "ILI9341_2" usata da Freenove con TFT_eSPI)
struct InitCmd {
  uint8_t cmd;
  uint8_t len;
  uint8_t data[15];
  uint16_t delay_ms;
};
const InitCmd kInit[] = {
  {0x01, 0, {0}, 120},                                   // reset software
  {0xCF, 3, {0x00, 0xC1, 0x30}, 0},
  {0xED, 4, {0x64, 0x03, 0x12, 0x81}, 0},
  {0xE8, 3, {0x85, 0x00, 0x78}, 0},
  {0xCB, 5, {0x39, 0x2C, 0x00, 0x34, 0x02}, 0},
  {0xF7, 1, {0x20}, 0},
  {0xEA, 2, {0x00, 0x00}, 0},
  {0xC0, 1, {0x10}, 0},                                  // power control 1
  {0xC1, 1, {0x00}, 0},                                  // power control 2
  {0xC5, 2, {0x30, 0x30}, 0},                            // VCOM 1
  {0xC7, 1, {0xB7}, 0},                                  // VCOM 2
  {0x3A, 1, {0x55}, 0},                                  // 16 bit/pixel
  {0x36, 1, {0x48}, 0},                                  // MADCTL: MX | BGR (verticale)
  {0xB1, 2, {0x00, LCD_FRAME_RTNA}, 0},                  // frequenza del quadro (vedi config.h)
  {0xB4, 1, {LCD_FRAME_INVERSION ? 0x07 : 0x02}, 0},     // inversione di quadro / di riga
  {0xB6, 3, {0x08, 0x82, 0x27}, 0},
  {0xF2, 1, {0x00}, 0},
  {0x26, 1, {0x01}, 0},
  {0xE0, 15, {0x0F, 0x2A, 0x28, 0x08, 0x0E, 0x08, 0x54, 0xA9, 0x43, 0x0A, 0x0F, 0x00, 0x00, 0x00, 0x00}, 0},
  {0xE1, 15, {0x00, 0x15, 0x17, 0x07, 0x11, 0x06, 0x2B, 0x56, 0x3C, 0x05, 0x10, 0x0F, 0x3F, 0x3F, 0x0F}, 0},
  {0x21, 0, {0}, 0},                                     // inversione colori ON (come Freenove)
  {0x11, 0, {0}, 120},                                   // sleep out
  {0x29, 0, {0}, 20},                                    // display on
};

volatile bool s_dma_busy = false;

// Quanto si ridisegna: oltre kBoostPx pixel in 200 ms (pagina nuova, scorrimento,
// animazioni) il processore torna a 240 MHz; la barra di avanzamento e l'orario
// che cambiano ogni secondo restano molto sotto
constexpr uint32_t kBoostPx = 15000;
uint32_t s_px = 0, s_px_t = 0;

// Chiamata dall'interrupt SPI (anche con la cache flash disattivata, es. durante
// scritture NVS): qui NON si può chiamare codice in flash come LVGL.
bool IRAM_ATTR flush_done(esp_lcd_panel_io_handle_t, esp_lcd_panel_io_event_data_t *, void *) {
  s_dma_busy = false;
  return false;
}

// LVGL chiama wait_cb solo quando gli serve di nuovo il buffer appena inviato
// (con due buffer, nel frattempo disegna nell'altro). Deve tornare a DMA finito.
void wait_cb(lv_display_t *) {
  while (s_dma_busy) {
  }
}

void flush_cb(lv_display_t *, const lv_area_t *a, uint8_t *px) {
  // LVGL produce RGB565 little-endian, l'ILI9341 vuole big-endian
  uint32_t n = (a->x2 - a->x1 + 1) * (a->y2 - a->y1 + 1);
  uint32_t now = millis();
  if (now - s_px_t > 200) {
    s_px_t = now;
    s_px = 0;
  }
  s_px += n;
  if (s_px > kBoostPx) power_cpu_boost();
  lv_draw_sw_rgb565_swap(px, n);
  uint8_t ca[4] = {(uint8_t)(a->x1 >> 8), (uint8_t)a->x1, (uint8_t)(a->x2 >> 8), (uint8_t)a->x2};
  uint8_t ra[4] = {(uint8_t)(a->y1 >> 8), (uint8_t)a->y1, (uint8_t)(a->y2 >> 8), (uint8_t)a->y2};
  esp_lcd_panel_io_tx_param(s_io, 0x2A, ca, 4);
  esp_lcd_panel_io_tx_param(s_io, 0x2B, ra, 4);
  s_dma_busy = true;
  esp_lcd_panel_io_tx_color(s_io, 0x2C, px, n * 2);   // asincrono: vedi wait_cb
}

// ------------------------------------------------------------ touch ----
bool tp_read(uint16_t &x, uint16_t &y) {
  Wire.beginTransmission(TP_I2C_ADDR);
  Wire.write(0x02);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((uint16_t)TP_I2C_ADDR, (uint8_t)5) != 5) return false;
  uint8_t d[5];
  for (int i = 0; i < 5; i++) d[i] = Wire.read();
  uint8_t n = d[0] & 0x0F;
  if (n == 0 || n > 2) return false;
  x = ((d[1] & 0x0F) << 8) | d[2];
  y = ((d[3] & 0x0F) << 8) | d[4];
  return x < LCD_W && y < LCD_H;
}

void touch_cb(lv_indev_t *, lv_indev_data_t *data) {
  uint16_t x = 0, y = 0;
  // a schermo spento col tasto il touch non si legge nemmeno; a schermo spento
  // da solo un tocco lo riaccende ma non arriva all'interfaccia
  bool down = power_touch_needed() && tp_read(x, y);
  if (!power_touch_filter(down)) down = false;
  // il filtro intercetta il gesto "indietro" dal bordo sinistro
  int ox, oy;
  bool pressed = edge_swipe_filter(down, x, y, &ox, &oy, millis());
  data->state = pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
  data->point.x = ox;
  data->point.y = oy;
}

uint32_t tick_cb() { return millis(); }

}  // namespace

static uint8_t s_bright = 0;    // luminosità scelta
static uint8_t s_cur = 0;       // quella che c'è davvero (attenuata, in dissolvenza...)
static bool s_dim = false;
static uint32_t s_fade_t = 0;
static bool s_panel_on = true;

static void bl_write(uint8_t pct) {
  s_cur = pct;
  if (s_panel_on) ledcWrite(LCD_PIN_BL, (uint32_t)pct * ((1u << LCD_BL_PWM_BITS) - 1) / 100);
}

// attenuata: un quarto della luminosità scelta, almeno il 5%
static uint8_t bl_target() {
  if (!s_dim) return s_bright;
  uint8_t d = s_bright / 4;
  if (d < 5) d = 5;
  return d < s_bright ? d : s_bright;
}

void display_set_brightness(uint8_t pct) {
  if (pct < 5) pct = 5;
  s_bright = pct;
  s_dim = false;
  bl_write(pct);
}

void display_dim(bool on) {
  if (on == s_dim) return;
  s_dim = on;
  if (!on) bl_write(s_bright);   // si torna subito; si attenua piano (display_tick)
}

void display_tick(uint32_t now) {
  uint8_t want = bl_target();
  if (!s_panel_on || s_cur == want || now - s_fade_t < 12) return;
  s_fade_t = now;
  bl_write(s_cur > want ? s_cur - 1 : s_cur + 1);
}

uint8_t display_brightness() { return s_panel_on ? s_cur : 0; }
uint8_t display_brightness_setting() { return s_bright; }

void display_panel(bool on) {
  if (on == s_panel_on || !s_io) return;
  if (!on) {
    ledcWrite(LCD_PIN_BL, 0);
    for (int i = 0; i < 50 && s_dma_busy; i++) delay(1);
    esp_lcd_panel_io_tx_param(s_io, 0x28, nullptr, 0);   // display off
    esp_lcd_panel_io_tx_param(s_io, 0x10, nullptr, 0);   // sleep in: il pannello consuma pochissimo
    s_panel_on = false;
  } else {
    esp_lcd_panel_io_tx_param(s_io, 0x11, nullptr, 0);   // sleep out
    delay(120);
    esp_lcd_panel_io_tx_param(s_io, 0x29, nullptr, 0);   // display on (retroilluminazione: dopo il ridisegno)
    s_panel_on = true;
  }
}

void display_sleep() {
  ledcWrite(LCD_PIN_BL, 0);
  for (int i = 0; i < 50 && s_dma_busy; i++) delay(1);
  if (s_io) {
    esp_lcd_panel_io_tx_param(s_io, 0x28, nullptr, 0);   // display off
    esp_lcd_panel_io_tx_param(s_io, 0x10, nullptr, 0);   // sleep in
    delay(10);
  }
  // nel deep sleep i pin restano come sono: retroilluminazione spenta
  ledcDetach(LCD_PIN_BL);
  pinMode(LCD_PIN_BL, OUTPUT);
  digitalWrite(LCD_PIN_BL, LOW);
  gpio_hold_en((gpio_num_t)LCD_PIN_BL);
}

bool display_begin() {
  // sopra i 20 kHz: niente fischio (vedi config.h). Al 100% il core tiene il pin
  // sempre acceso (255 su 8 bit = pieno), quindi lì il PWM non c'è proprio.
  if (!ledcAttach(LCD_PIN_BL, LCD_BL_PWM_HZ, LCD_BL_PWM_BITS)) Serial.println("[lcd] PWM retroilluminazione non disponibile");
  ledcWrite(LCD_PIN_BL, 0);

  spi_bus_config_t bus = {};
  bus.mosi_io_num = LCD_PIN_MOSI;
  bus.miso_io_num = LCD_PIN_MISO;
  bus.sclk_io_num = LCD_PIN_SCLK;
  bus.quadwp_io_num = -1;
  bus.quadhd_io_num = -1;
  bus.max_transfer_sz = LCD_W * LCD_BUF_LINES * 2 + 16;
  plat_mark(0, "display: bus SPI");
  esp_err_t e = spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO);
  if (e != ESP_OK) {
    Serial.printf("[lcd] spi_bus_initialize: %s\n", esp_err_to_name(e));
    return false;
  }

  esp_lcd_panel_io_spi_config_t io = {};
  io.cs_gpio_num = LCD_PIN_CS;
  io.dc_gpio_num = LCD_PIN_DC;
  io.spi_mode = 0;
  io.pclk_hz = LCD_SPI_HZ;
  io.trans_queue_depth = 10;
  io.lcd_cmd_bits = 8;
  io.lcd_param_bits = 8;
  io.on_color_trans_done = flush_done;
  io.user_ctx = nullptr;
  e = esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &io, &s_io);
  if (e != ESP_OK) {
    Serial.printf("[lcd] panel io: %s\n", esp_err_to_name(e));
    return false;
  }

  plat_mark(0, "display: init ILI9341");
  for (auto &c : kInit) {
    esp_lcd_panel_io_tx_param(s_io, c.cmd, c.len ? c.data : nullptr, c.len);
    if (c.delay_ms) delay(c.delay_ms);
  }

  plat_mark(0, "display: touch");
  // touch FT6336U: reset hardware
  pinMode(TP_PIN_RST, OUTPUT);
  digitalWrite(TP_PIN_RST, LOW);
  delay(10);
  digitalWrite(TP_PIN_RST, HIGH);
  delay(200);
  pinMode(TP_PIN_INT, INPUT_PULLUP);

  plat_mark(0, "display: init LVGL");
  lv_init();
  lv_tick_set_cb(tick_cb);
  size_t bsz = LCD_W * LCD_BUF_LINES;
  uint8_t *b1 = (uint8_t *)heap_caps_aligned_alloc(4, bsz * 2, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
  uint8_t *b2 = (uint8_t *)heap_caps_aligned_alloc(4, bsz * 2, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
  if (!b1 || !b2) return false;
  // Test: schermo grigio scuro + retroilluminazione accesa, prima ancora di LVGL.
  // Se resta nero qui, il problema è display/retroilluminazione, non il firmware.
  for (size_t i = 0; i < bsz; i++) ((uint16_t *)b1)[i] = 0x8210;   // 0x1082 byte-swapped
  for (int y = 0; y < LCD_H; y += LCD_BUF_LINES) {
    uint8_t ca[4] = {0, 0, (uint8_t)((LCD_W - 1) >> 8), (uint8_t)(LCD_W - 1)};
    uint8_t ra[4] = {(uint8_t)(y >> 8), (uint8_t)y, (uint8_t)((y + LCD_BUF_LINES - 1) >> 8), (uint8_t)(y + LCD_BUF_LINES - 1)};
    esp_lcd_panel_io_tx_param(s_io, 0x2A, ca, 4);
    esp_lcd_panel_io_tx_param(s_io, 0x2B, ra, 4);
    esp_lcd_panel_io_tx_color(s_io, 0x2C, b1, bsz * 2);
    delay(8);
  }
  ledcWrite(LCD_PIN_BL, (1u << LCD_BL_PWM_BITS) * 3 / 4);
  Serial.println("[lcd] pannello inizializzato");
  s_disp = lv_display_create(LCD_W, LCD_H);
  lv_display_set_color_format(s_disp, LV_COLOR_FORMAT_RGB565);
  lv_display_set_flush_cb(s_disp, flush_cb);
  lv_display_set_flush_wait_cb(s_disp, wait_cb);
  lv_display_set_buffers(s_disp, b1, b2, bsz * 2, LV_DISPLAY_RENDER_MODE_PARTIAL);

  lv_indev_t *indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, touch_cb);
  lv_indev_set_display(indev, s_disp);
  return true;
}
