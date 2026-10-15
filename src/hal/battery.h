// Batteria LiPo: livello (%) e stato di ricarica.
//
// Hardware (Freenove FNK0104): la tensione della batteria arriva su BAT_ADC_PIN
// tramite un partitore 200k/200k; il caricatore TP4054 (circa 300 mA) non porta
// il suo segnale CHRG a nessun pin, e nemmeno la presenza del cavo USB. Lo stato
// di ricarica si ricava quindi dalla tensione: collegando il cavo sale di colpo
// (la batteria smette di alimentare la scheda e riceve corrente), staccandolo
// scende di colpo. Se il cavo va a un computer lo dice anche la USB del chip.
#pragma once
#include <stdint.h>

enum BatState : uint8_t {
  BAT_NONE,          // nessuna batteria (o tensione non plausibile)
  BAT_DISCHARGING,   // in uso a batteria
  BAT_CHARGING,      // cavo collegato, in carica
  BAT_FULL,          // cavo collegato, carica completa
};

struct BatteryInfo {
  BatState state = BAT_NONE;
  int pct = -1;            // 0..100, -1 = non ancora noto
  int mv = 0;              // tensione filtrata della batteria
  bool low = false;        // a batteria e sotto il 10%
  bool critical = false;   // a batteria e sotto la soglia di spegnimento da un po'
  int minutes_to_full = -1;
};

void battery_begin();
// da chiamare spesso (loop): campiona una volta al secondo. load_sig riassume ciò
// che consuma di più (play/pausa, volume, uscita, Wi-Fi, luminosità): quando cambia,
// il salto di tensione che segue non viene scambiato per il cavo collegato/staccato.
void battery_poll(uint32_t load_sig);
BatteryInfo battery_info();
int battery_read_mv();          // lettura immediata (mV), 0 se non configurata

// ---- logica di stima, separata dall'hardware (provata anche sul PC) ----
class BatteryEstimator {
 public:
  // un campione al secondo: tensione della batteria (mV) e "computer collegato via USB"
  void sample(uint32_t now_ms, int mv, bool usb_host);
  // il consumo è cambiato di colpo (play/pausa, volume, Wi-Fi, luminosità): il salto
  // di tensione che segue non va scambiato per il cavo collegato o staccato
  void load_changed(uint32_t now_ms);
  BatteryInfo info() const { return info_; }
  bool external_power() const { return ext_; }

 private:
  static constexpr int kHist = 37;   // una voce ogni 10 s: 6 minuti
  BatteryInfo info_;
  bool init_ = false, ext_ = false, usb_prev_ = false;
  float fast_ = 0, slow_ = 0;
  int hist_[kHist] = {};
  int hist_n_ = 0, hist_i_ = 0;
  uint32_t last_hist_ = 0, low_since_ = 0, hold_until_ = 0;
  uint32_t cv_since_ = 0;   // inizio della fase finale della carica (4,2 V costanti)
  int cv_p0_ = 85;
  bool full_ = false, resync_ = false;
  // andamento lento (una voce ogni 5 minuti, 20 minuti in tutto): riconosce la
  // carica anche quando la scheda viene accesa col cavo già collegato
  static constexpr int kCoarse = 5;
  int coarse_[kCoarse] = {};
  int coarse_n_ = 0;
  uint32_t last_coarse_ = 0;
  // carica riconosciuta solo da un salto di tensione: confermata se nei 20 minuti
  // successivi la tensione sale davvero, altrimenti era un calo di consumo
  uint32_t verify_at_ = 0;
  float verify_mv_ = -1;
  float load_ref_ = -1;     // tensione appena prima dell'ultimo cambio di consumo
  // quanto la carica alza la tensione rispetto a quella a vuoto: si misura dal salto
  // al momento del collegamento (dipende da batteria, cavi e consumo della scheda)
  float step_pre_ = -1;
  int charge_rise_ = 60;
  bool allow_drop_ = false; // carica riconosciuta dall'andamento: la % letta prima era gonfiata
  void set_ext(bool on, uint32_t now, bool verify = false);
  void clear_history();
};

int battery_pct_from_mv(int ocv_mv);   // curva tensione a vuoto -> % di una LiPo
