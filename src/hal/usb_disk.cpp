#include "usb_disk.h"

#ifdef ARDUINO
#include <Arduino.h>
#include <SD_MMC.h>
#include <USB.h>
#include <USBMSC.h>
#include <sdmmc_cmd.h>
#include <driver/gpio.h>
#include <soc/rtc_cntl_reg.h>
#include <soc/usb_serial_jtag_reg.h>
#include <soc/usb_serial_jtag_struct.h>
#include "tusb.h"
#include "../platform.h"

namespace {

USBMSC *s_msc = nullptr;   // creato solo quando serve: fino ad allora la porta resta la seriale
sdmmc_card_t *s_card = nullptr;
bool s_started = false;
volatile bool s_mounted = false;    // il computer ha configurato il dispositivo (e non è in standby)
volatile bool s_ejected = false;
volatile bool s_error = false;
volatile bool s_last_wr = false;
volatile uint32_t s_rd_sec = 0, s_wr_sec = 0;   // settori da 512 byte (32 bit: niente letture a metà)
volatile uint32_t s_last_io = 0;

// La scheda di SD_MMC è un membro protetto: lo si raggiunge da una sottoclasse
// (mai istanziata) con un puntatore a membro.
struct CardPeek : fs::SDMMCFS {
  static sdmmc_card_t *get(fs::SDMMCFS &fs) { return fs.*(&CardPeek::_card); }
};

// Il computer legge e scrive a blocchi fino a 4 KB (CONFIG_TINYUSB_MSC_BUFSIZE):
// un solo comando multi-blocco alla scheda invece di uno per settore.
int32_t on_read(uint32_t lba, uint32_t offset, void *buf, uint32_t n) {
  if (!s_card || offset || n % 512) return -1;
  if (sdmmc_read_sectors(s_card, buf, lba, n / 512) != ESP_OK) {
    s_error = true;
    return -1;
  }
  s_rd_sec += n / 512;
  s_last_wr = false;
  s_last_io = millis();
  return n;
}

int32_t on_write(uint32_t lba, uint32_t offset, uint8_t *buf, uint32_t n) {
  if (!s_card || offset || n % 512) return -1;
  if (sdmmc_write_sectors(s_card, buf, lba, n / 512) != ESP_OK) {
    s_error = true;
    return -1;
  }
  s_wr_sec += n / 512;
  s_last_wr = true;
  s_last_io = millis();
  return n;
}

// "Espelli" dal computer (Finder, Esplora risorse, eject): START STOP UNIT con
// LoEj. Da lì in poi la scheda risulta assente, così non viene rimontata.
bool on_start_stop(uint8_t, bool start, bool load_eject) {
  if (load_eject && !start) {
    s_ejected = true;
    if (s_msc) s_msc->mediaPresent(false);
  }
  return true;
}

void on_usb_event(void *, esp_event_base_t base, int32_t id, void *) {
  if (base != ARDUINO_USB_EVENTS) return;
  switch (id) {
    case ARDUINO_USB_STARTED_EVENT:
    case ARDUINO_USB_RESUME_EVENT: s_mounted = true; break;
    // standby del computer o cavo staccato (la scheda non vede la tensione
    // del cavo: le due cose si somigliano)
    case ARDUINO_USB_STOPPED_EVENT:
    case ARDUINO_USB_SUSPEND_EVENT: s_mounted = false; break;
    default: break;
  }
}

}  // namespace

bool usb_disk_host_present() {
  if (s_started) return s_mounted;
  // Il computer manda un SOF ogni millisecondo e la seriale USB/JTAG ne conta il
  // numero (11 bit): se cambia tra due chiamate c'è un computer.
  static uint32_t last_f = 0, last_t = 0;
  static bool host = false;
  uint32_t now = millis();
  if (last_t == 0) {   // la prima volta: due letture a poco più di un millisecondo
    last_f = USB_SERIAL_JTAG.fram_num.sof_frame_index;
    delayMicroseconds(1100);
  }
  if (last_t == 0 || now - last_t >= 20) {
    uint32_t f = USB_SERIAL_JTAG.fram_num.sof_frame_index;
    host = f != last_f;
    last_f = f;
    last_t = now ? now : 1;
  }
  return host;
}

bool usb_disk_start() {
  if (s_started) return true;
  s_card = CardPeek::get(SD_MMC);
  int secs = SD_MMC.numSectors(), ss = SD_MMC.sectorSize();
  if (!s_card || secs <= 0 || ss != 512) {
    plat_event("disco usb: nessuna scheda");
    return false;
  }
  s_msc = new USBMSC();
  s_msc->vendorID("onda");
  s_msc->productID("FLAC Player");
  s_msc->productRevision("1.0");
  s_msc->onRead(on_read);
  s_msc->onWrite(on_write);
  s_msc->onStartStop(on_start_stop);
  s_msc->isWritable(true);
  s_msc->mediaPresent(true);
  if (!s_msc->begin((uint32_t)secs, (uint16_t)ss)) {
    plat_event("disco usb: msc non avviato");
    return false;
  }
  USB.onEvent(on_usb_event);
  USB.manufacturerName("onda");
  USB.productName("onda FLAC Player");
  Serial.println("[usb] disco USB: la seriale si scollega fino al riavvio");
  Serial.flush();
  // da qui la porta passa al controller USB-OTG: il computer vede sparire la
  // seriale e comparire il disco
  if (!USB.begin()) {
    plat_event("disco usb: usb non avviato");
    return false;
  }
  s_started = true;
  s_last_io = millis();
  plat_event("disco usb: avviato");
  return true;
}

bool usb_disk_active() { return s_started; }

UsbDiskStatus usb_disk_status() {
  UsbDiskStatus s;
  if (!s_started) return s;
  s.state = s_ejected ? UD_EJECTED : s_mounted ? UD_MOUNTED : UD_WAITING;
  s.bytes_read = (uint64_t)s_rd_sec * 512;
  s.bytes_written = (uint64_t)s_wr_sec * 512;
  s.idle_ms = millis() - s_last_io;
  s.writing = s_last_wr;
  s.error = s_error;
  return s;
}

void usb_disk_release() {
  if (!s_started) return;
  if (s_msc) s_msc->mediaPresent(false);
  // il computer vede staccarsi il disco subito (e non al timeout dopo il riavvio)
  tud_disconnect();
  delay(150);
  // Come usb_switch_to_cdc_jtag() del core: la porta torna alla seriale
  // USB/JTAG. Questi bit stanno nel dominio RTC, che il riavvio e il deep sleep
  // non azzerano: senza, dopo il riavvio non ci sarebbe più la seriale (né il
  // caricamento del firmware) fino a quando si toglie l'alimentazione.
  CLEAR_PERI_REG_MASK(RTC_CNTL_USB_CONF_REG, RTC_CNTL_SW_HW_USB_PHY_SEL | RTC_CNTL_SW_USB_PHY_SEL | RTC_CNTL_USB_PAD_ENABLE);
  CLEAR_PERI_REG_MASK(USB_SERIAL_JTAG_CONF0_REG, USB_SERIAL_JTAG_PHY_SEL);
  s_started = false;
  plat_event("disco usb: staccato");
}

void usb_disk_finish() {
  usb_disk_release();
  plat_restart();
}

#else  // ------------------------------------------------- simulatore (PC) ----
#include "../platform.h"

namespace {
bool s_on = false;
uint32_t s_t0 = 0;
}  // namespace

// Nel simulatore il "computer" è sempre collegato e copia ~1 MB/s per 20 s.
bool usb_disk_host_present() { return true; }
bool usb_disk_start() {
  s_on = true;
  s_t0 = plat_millis();
  return true;
}
bool usb_disk_active() { return s_on; }
UsbDiskStatus usb_disk_status() {
  UsbDiskStatus s;
  if (!s_on) return s;
  uint32_t t = plat_millis() - s_t0;
  s.state = t < 1500 ? UD_WAITING : UD_MOUNTED;
  uint32_t busy = t > 1500 ? (t < 21500 ? t - 1500 : 20000) : 0;
  s.bytes_written = (uint64_t)busy * 1024;
  s.bytes_read = 3 * 1024 * 1024 + busy * 40ULL;
  s.writing = true;
  s.idle_ms = t < 21500 ? 0 : t - 21500;
  return s;
}
void usb_disk_release() { s_on = false; }
void usb_disk_finish() { s_on = false; }
#endif
