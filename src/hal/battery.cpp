#include "battery.h"
#include "../config.h"

// ------------------------------------------------------------ stima ----
// Curva tipica di una cella LiPo a riposo (tensione a vuoto -> %).
int battery_pct_from_mv(int mv) {
  static const int16_t kTab[][2] = {
    {4200, 100}, {4150, 95}, {4110, 90}, {4080, 85}, {4020, 80}, {3980, 75}, {3950, 70},
    {3910, 65},  {3870, 60}, {3850, 55}, {3840, 50}, {3820, 45}, {3800, 40}, {3790, 35},
    {3770, 30},  {3750, 25}, {3730, 20}, {3710, 15}, {3690, 10}, {3610, 5},  {3300, 0},
  };
  const int n = sizeof kTab / sizeof kTab[0];
  if (mv >= kTab[0][0]) return 100;
  if (mv <= kTab[n - 1][0]) return 0;
  for (int i = 1; i < n; i++) {
    if (mv >= kTab[i][0]) {
      int v0 = kTab[i][0], v1 = kTab[i - 1][0], p0 = kTab[i][1], p1 = kTab[i - 1][1];
      return p0 + (mv - v0) * (p1 - p0) / (v1 - v0);
    }
  }
  return 0;
}

namespace {
constexpr int kPresentMv = 2800;     // sotto: nessuna batteria
constexpr int kCvMv = 4180;          // il caricatore tiene la batteria a 4,2 V (fase finale)
constexpr uint32_t kCvFullMs = 40 * 60 * 1000;   // durata tipica della fase finale
constexpr int kStepMv = 40;          // salto improvviso = cavo collegato/staccato
constexpr int kTrendUpMv = 35;       // salita lenta in 6 minuti (acceso già col cavo collegato)
constexpr int kTrendDownMv = 30;     // discesa lenta in 6 minuti mentre risultava in carica
constexpr int kLoadDropMv = 40;      // a batteria la tensione è più bassa di quella a vuoto
constexpr int kChargeRiseMv = 60;    // in carica è più alta (valore iniziale, poi misurato)
constexpr uint32_t kVerifyMs = 20 * 60 * 1000;
constexpr int kVerifyRiseMv = 4;     // in carica, in 20 minuti, la tensione sale almeno di tanto
}  // namespace

void BatteryEstimator::clear_history() {
  hist_n_ = 0;
  hist_i_ = 0;
  coarse_n_ = 0;
}

void BatteryEstimator::set_ext(bool on, uint32_t now, bool verify) {
  if (on == ext_) return;
  ext_ = on;
  clear_history();
  cv_since_ = 0;
  full_ = false;
  verify_at_ = verify ? (now + kVerifyMs ? now + kVerifyMs : 1) : 0;
  verify_mv_ = -1;
  load_ref_ = -1;
  step_pre_ = slow_;          // livello prima del salto (il filtro lento non l'ha ancora seguito)
  allow_drop_ = false;
  hold_until_ = now + 8000;   // la tensione si assesta: per qualche secondo la % resta ferma
  resync_ = true;             // poi il filtro lento riparte dal nuovo livello
}

void BatteryEstimator::load_changed(uint32_t now) {
  if (!init_) return;
  uint32_t until = now + 12000;
  if ((int32_t)(until - hold_until_) > 0) hold_until_ = until;
  if (load_ref_ < 0) load_ref_ = fast_;
  resync_ = true;
  hist_n_ = 0;   // il salto non deve contare come andamento
  hist_i_ = 0;
}

void BatteryEstimator::sample(uint32_t now, int mv, bool usb_host) {
  if (!init_) {
    init_ = true;
    fast_ = slow_ = mv;
    ext_ = usb_host;
    usb_prev_ = usb_host;
    last_hist_ = now;
    last_coarse_ = now;
  }
  fast_ += (mv - fast_) / 3.0f;     // ~3 s
  slow_ += (mv - slow_) / 40.0f;    // ~40 s
  bool holding = (int32_t)(now - hold_until_) < 0;
  if (!holding && resync_) {
    resync_ = false;
    if (verify_at_) {
      if (verify_mv_ < 0) verify_mv_ = fast_;                    // livello appena dopo il collegamento
      else if (load_ref_ > 0) verify_mv_ += fast_ - load_ref_;   // salto di consumo: si sposta il riferimento
    }
    load_ref_ = -1;
    if (step_pre_ > 0) {
      // salto misurato = consumo a batteria + effetto della carica
      int rise = (int)(fast_ - step_pre_) - kLoadDropMv;
      if (ext_ && rise > 0) charge_rise_ = rise > 160 ? 160 : rise;
      step_pre_ = -1;
    }
    slow_ = fast_;
  }
  int f = (int)(fast_ + 0.5f), s = (int)(slow_ + 0.5f);

  // --- c'è corrente dal cavo? ---
  if (usb_host) {
    set_ext(true, now);
  } else if (usb_prev_) {
    set_ext(false, now);   // il computer è stato scollegato
  } else if (!holding && !resync_) {
    if (verify_at_ && verify_mv_ >= 0) {
      if (cv_since_ || s - verify_mv_ >= 15) verify_at_ = 0;   // sta caricando davvero
      else if ((int32_t)(now - verify_at_) >= 0) {
        if (s - verify_mv_ < kVerifyRiseMv) set_ext(false, now);   // era solo meno consumo
        else verify_at_ = 0;
      }
    }
    if (holding || resync_) {
      // appena cambiato
    } else if (f - s > kStepMv) set_ext(true, now, !usb_host);
    else if (s - f > kStepMv) set_ext(false, now);
    else if (hist_n_ == kHist) {
      int oldest = hist_[hist_i_];   // il più vecchio (il buffer è pieno)
      // salita costante sotto i 4,1 V: in carica (es. acceso col cavo già collegato)
      if (!ext_ && s < 4100 && f - oldest > kTrendUpMv) {
        set_ext(true, now);
        step_pre_ = -1;
        charge_rise_ = kChargeRiseMv;
        allow_drop_ = true;
      }
      // in carica (prima della fase finale) la tensione non scende: se scende, il cavo non c'è più
      else if (ext_ && !cv_since_ && oldest - f > kTrendDownMv) set_ext(false, now);
    }
    if (coarse_n_ == kCoarse) {
      // 20 minuti di salita (o discesa) costante, a ogni passo: una pausa della
      // musica fa salire la tensione una volta sola, la carica la fa salire sempre
      int up = 0, down = 0;
      for (int k = 1; k < kCoarse; k++) {
        int d = coarse_[k] - coarse_[k - 1];
        if (d >= 2) up++;
        if (d <= -2) down++;
      }
      int tot = coarse_[kCoarse - 1] - coarse_[0];
      if (!ext_ && s < 4150 && up == kCoarse - 1 && tot >= 20) {
        set_ext(true, now);
        step_pre_ = -1;
        charge_rise_ = kChargeRiseMv;
        allow_drop_ = true;
      }
      else if (ext_ && !cv_since_ && down == kCoarse - 1 && tot <= -15) set_ext(false, now);
      // carica finita: la tensione si assesta un po', ma se continua a scendere il cavo non c'è più
      else if (ext_ && cv_since_ && down == kCoarse - 1 && tot <= -30) set_ext(false, now);
    }
    // col cavo, dopo la fase finale, il caricatore riparte sotto ~4,05 V: più in basso si è a batteria
    if (ext_ && !resync_ && cv_since_ && s < 3990) set_ext(false, now);
  }
  usb_prev_ = usb_host;
  holding = (int32_t)(now - hold_until_) < 0 || resync_;
  if (!holding && now - last_coarse_ >= 5 * 60 * 1000) {
    last_coarse_ = now;
    if (coarse_n_ == kCoarse) {
      for (int k = 1; k < kCoarse; k++) coarse_[k - 1] = coarse_[k];
      coarse_n_--;
    }
    coarse_[coarse_n_++] = s;
  }
  if (!holding && now - last_hist_ >= 10000) {   // durante l'assestamento non si registra
    last_hist_ = now;
    hist_[hist_i_] = f;
    hist_i_ = (hist_i_ + 1) % kHist;
    if (hist_n_ < kHist) hist_n_++;
  }

  // --- stato e percentuale ---
  info_.mv = s;
  if (s < kPresentMv) {
    info_ = BatteryInfo();
    info_.mv = s;
    return;
  }
  holding = (int32_t)(now - hold_until_) < 0 || resync_;
  if (ext_ && !holding) {
    // fase finale: la tensione resta a 4,2 V mentre la corrente cala; quando la
    // carica termina la tensione si assesta un po' sotto (o passa il tempo tipico)
    if (!cv_since_ && s >= kCvMv) {
      cv_since_ = now ? now : 1;
      cv_p0_ = info_.pct >= 0 ? info_.pct : 85;
      if (cv_p0_ > 95) cv_p0_ = 95;
    }
    if (cv_since_ && !full_ &&
        (now - cv_since_ >= kCvFullMs || (now - cv_since_ > 5 * 60 * 1000 && s < kCvMv - 15)))
      full_ = true;
  }
  BatState st = !ext_ ? BAT_DISCHARGING : full_ ? BAT_FULL : BAT_CHARGING;
  int p;
  if (st == BAT_FULL) {
    p = 100;
  } else if (st == BAT_CHARGING && cv_since_) {
    uint32_t el = now - cv_since_;
    p = cv_p0_ + (int)((99 - cv_p0_) * (uint64_t)(el < kCvFullMs ? el : kCvFullMs) / kCvFullMs);
  } else {
    int ocv = s + (st == BAT_DISCHARGING ? kLoadDropMv : -charge_rise_);
    p = battery_pct_from_mv(ocv);
    if (st == BAT_CHARGING && p > 99) p = 99;
  }
  int prev = info_.pct;
  if (prev >= 0 && st != BAT_FULL) {
    if (holding) {
      p = prev;   // subito dopo aver collegato/staccato il cavo la tensione salta: si aspetta
    } else if (st == BAT_DISCHARGING && p > prev && p < prev + 15) {
      p = prev;   // a batteria la percentuale non risale (se non di molto: batteria cambiata)
    } else if (st == BAT_CHARGING && p < prev && !(allow_drop_ && p < prev - 15)) {
      p = prev;   // in carica non scende
    }
  }
  info_.state = st;
  info_.pct = p;
  info_.minutes_to_full =
      st == BAT_CHARGING ? (100 - p) * BAT_CAPACITY_MAH * 60 / 100 / BAT_CHARGE_MA * 115 / 100 : -1;

  // --- batteria scarica ---
  info_.low = st == BAT_DISCHARGING && p <= 10;
  if (st == BAT_DISCHARGING && s < BAT_SHUTDOWN_MV) {
    if (!low_since_) low_since_ = now ? now : 1;
    info_.critical = now - low_since_ >= 30000;
  } else {
    low_since_ = 0;
    info_.critical = false;
  }
}

// ------------------------------------------------------------ lettura ----
#ifdef ARDUINO
#include <Arduino.h>
#include <soc/soc_caps.h>
#if SOC_USB_SERIAL_JTAG_SUPPORTED
#include <driver/usb_serial_jtag.h>
#endif

namespace {
BatteryEstimator s_est;
uint32_t s_last = 0, s_sig = 0;
bool s_sig_init = false;
portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
BatteryInfo s_info;
}  // namespace

void battery_begin() {
#if BAT_ADC_PIN >= 0
  pinMode(BAT_ADC_PIN, INPUT);
  analogSetPinAttenuation(BAT_ADC_PIN, ADC_11db);
#endif
}

int battery_read_mv() {
#if BAT_ADC_PIN >= 0
  // 10 letture distanziate; si scartano le 2 più basse e le 2 più alte (picchi di
  // consumo di Wi-Fi e amplificatore)
  int v[10];
  for (int i = 0; i < 10; i++) {
    v[i] = analogReadMilliVolts(BAT_ADC_PIN);
    if (i < 9) delayMicroseconds(500);
  }
  for (int i = 1; i < 10; i++)
    for (int j = i; j > 0 && v[j] < v[j - 1]; j--) {
      int t = v[j];
      v[j] = v[j - 1];
      v[j - 1] = t;
    }
  int sum = 0;
  for (int i = 2; i < 8; i++) sum += v[i];
  int mv = sum / 6 * BAT_DIVIDER_X100 / 100 + BAT_OFFSET_MV;
  return mv < 0 ? 0 : mv;
#else
  return 0;
#endif
}

void battery_poll(uint32_t load_sig) {
#if BAT_ADC_PIN >= 0
  uint32_t now = millis();
  if (s_sig_init && load_sig != s_sig) s_est.load_changed(now);
  s_sig = load_sig;
  s_sig_init = true;
  if (s_last && now - s_last < 1000) return;
  s_last = now;
  bool host = false;
#if SOC_USB_SERIAL_JTAG_SUPPORTED
  host = usb_serial_jtag_is_connected();   // cavo verso un computer (non un caricatore)
#endif
  s_est.sample(now, battery_read_mv(), host);
  BatteryInfo i = s_est.info();
  portENTER_CRITICAL(&s_mux);
  s_info = i;
  portEXIT_CRITICAL(&s_mux);
#else
  (void)load_sig;
#endif
}

BatteryInfo battery_info() {
  portENTER_CRITICAL(&s_mux);
  BatteryInfo i = s_info;
  portEXIT_CRITICAL(&s_mux);
  return i;
}
#endif
