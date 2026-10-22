// Trasferimento file via Wi-Fi: rete (STA o hotspot), mDNS "onda.local" e
// server HTTP (esp_http_server) con le API usate dalla pagina web.
#include "xfer.h"

#ifdef ARDUINO
#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <SD_MMC.h>
#include <esp_http_server.h>
#include <esp_mac.h>
#include <esp_random.h>
#include <esp_heap_caps.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <string>
#include <algorithm>
#include <ctype.h>
#include <lwip/sockets.h>
#include "../config.h"
#include "../platform.h"
#include "web_index.h"
#include "captive_dns.h"
#include "../clock.h"
#include "../i18n.h"
#include "../lib/library.h"
#include "../lib/artwork.h"
#include "../lib/flac_tags.h"

namespace {

// ------------------------------------------------------------- stato ----
portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
XferStatus s_st;
TaskHandle_t s_task = nullptr;
QueueHandle_t s_q = nullptr;
httpd_handle_t s_http = nullptr;
volatile bool s_scan_done = false;
std::vector<XferNet> s_scan;
SemaphoreHandle_t s_scan_mtx = nullptr;   // i vector allocano memoria: niente sezioni critiche

enum CmdKind : uint8_t { K_START, K_CONNECT, K_HOTSPOT, K_SCAN, K_STOP };
struct Cmd {
  CmdKind k;
  char ssid[33];
  char pass[65];
};

template <typename F>
void locked(F f) {
  portENTER_CRITICAL(&s_mux);
  f(s_st);
  portEXIT_CRITICAL(&s_mux);
}

void set_note(const char *n) {
  char tmp[sizeof s_st.note];
  snprintf(tmp, sizeof tmp, "%s", n);
  locked([&](XferStatus &s) { memcpy(s.note, tmp, sizeof tmp); });
}

// --------------------------------------------------------- percorsi ----
// Le API ricevono percorsi relativi alla scheda ("/Musica/Album"). Niente "..".
bool url_decode(char *s) {
  char *o = s;
  for (char *p = s; *p; p++) {
    if (*p == '+') {
      *o++ = ' ';
    } else if (*p == '%' && p[1] && p[2]) {
      char h[3] = {p[1], p[2], 0};
      *o++ = (char)strtol(h, nullptr, 16);
      p += 2;
    } else {
      *o++ = *p;
    }
  }
  *o = 0;
  return true;
}

bool safe_rel(const char *p) {
  if (p[0] != '/') return false;
  if (strstr(p, "/..") || strchr(p, '\\')) return false;
  for (const unsigned char *c = (const unsigned char *)p; *c; c++)
    if (*c < 0x20) return false;
  return strlen(p) < 240;
}

// Legge il parametro "key" della query, decodificato, come percorso completo sulla SD.
bool query_path(httpd_req_t *req, const char *key, std::string &full, std::string *rel = nullptr) {
  size_t ql = httpd_req_get_url_query_len(req);
  if (!ql) return false;
  std::string q(ql + 1, '\0');
  if (httpd_req_get_url_query_str(req, &q[0], ql + 1) != ESP_OK) return false;
  char v[512];
  if (httpd_query_key_value(q.c_str(), key, v, sizeof v) != ESP_OK) return false;
  url_decode(v);
  if (!safe_rel(v)) return false;
  // niente "/" finale (tranne la radice)
  size_t n = strlen(v);
  while (n > 1 && v[n - 1] == '/') v[--n] = 0;
  if (rel) *rel = v;
  full = std::string(SD_MOUNT) + (strcmp(v, "/") ? v : "");
  return true;
}

bool query_flag(httpd_req_t *req, const char *key) {
  size_t ql = httpd_req_get_url_query_len(req);
  if (!ql) return false;
  std::string q(ql + 1, '\0');
  char v[8];
  if (httpd_req_get_url_query_str(req, &q[0], ql + 1) != ESP_OK) return false;
  return httpd_query_key_value(q.c_str(), key, v, sizeof v) == ESP_OK && v[0] == '1';
}

void mkdirs(const std::string &dir) {
  for (size_t i = strlen(SD_MOUNT) + 1; i <= dir.size(); i++) {
    if (i == dir.size() || dir[i] == '/') mkdir(dir.substr(0, i).c_str(), 0775);
  }
}

std::string parent_of(const std::string &p) {
  size_t k = p.rfind('/');
  return k == std::string::npos ? p : p.substr(0, k);
}

bool rm_rf(const std::string &p) {
  struct stat st;
  if (stat(p.c_str(), &st) != 0) return false;
  if (!S_ISDIR(st.st_mode)) return unlink(p.c_str()) == 0;
  std::vector<std::string> names;
  if (DIR *d = opendir(p.c_str())) {
    while (struct dirent *e = readdir(d)) {
      if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
      names.push_back(e->d_name);
    }
    closedir(d);
  }
  bool ok = true;
  for (auto &n : names) ok &= rm_rf(p + "/" + n);
  return rmdir(p.c_str()) == 0 && ok;
}

void json_str(std::string &o, const char *s) {
  o += '"';
  for (const unsigned char *c = (const unsigned char *)s; *c; c++) {
    if (*c == '"' || *c == '\\') {
      o += '\\';
      o += (char)*c;
    } else if (*c < 0x20) {
      char b[8];
      snprintf(b, sizeof b, "\\u%04x", *c);
      o += b;
    } else {
      o += (char)*c;
    }
  }
  o += '"';
}

esp_err_t send_json(httpd_req_t *req, const std::string &s) {
  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  return httpd_resp_send(req, s.c_str(), s.size());
}

esp_err_t ok(httpd_req_t *req) { return send_json(req, "{\"ok\":true}"); }

esp_err_t fail(httpd_req_t *req, const char *status, const char *msg) {
  httpd_resp_set_status(req, status);
  std::string s = "{\"ok\":false,\"error\":";
  json_str(s, msg);
  s += "}";
  return send_json(req, s);
}

void changed() {
  locked([](XferStatus &s) { s.changes++; });
}

// ---------------------------------------------------- captive portal ----
// Con l'hotspot il lettore fa anche da DNS e risponde a ogni nome col proprio
// indirizzo; le pagine che il telefono usa per controllare internet (e qualunque
// indirizzo sconosciuto) rimandano alla pagina del lettore: telefoni e computer
// la aprono da soli appena collegati. Con la rete di casa niente di tutto questo.
volatile bool s_captive = false;
char s_home_url[32] = "";       // "http://192.168.4.1/"
char s_ip_txt[16] = "";
int s_dns = -1;
uint8_t s_dns_ip[4];
uint8_t *s_dns_buf = nullptr;   // domanda + risposta, in PSRAM solo finché c'è l'hotspot
constexpr size_t kDnsMax = 512;

void dns_stop() {
  s_captive = false;
  if (s_dns >= 0) close(s_dns);
  s_dns = -1;
  heap_caps_free(s_dns_buf);
  s_dns_buf = nullptr;
}

void dns_start(IPAddress ip) {
  dns_stop();
  for (int i = 0; i < 4; i++) s_dns_ip[i] = ip[i];
  snprintf(s_ip_txt, sizeof s_ip_txt, "%s", ip.toString().c_str());
  snprintf(s_home_url, sizeof s_home_url, "http://%s/", s_ip_txt);
  s_captive = true;   // il rimando delle pagine funziona anche se il DNS non parte
  s_dns_buf = (uint8_t *)heap_caps_malloc(2 * kDnsMax + 16, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!s_dns_buf) return;
  int sk = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (sk < 0) return;
  struct sockaddr_in a = {};
  a.sin_family = AF_INET;
  a.sin_port = htons(53);
  a.sin_addr.s_addr = htonl(INADDR_ANY);
  if (bind(sk, (struct sockaddr *)&a, sizeof a) != 0) {
    close(sk);
    plat_log("[xfer] DNS dell'hotspot non avviato");
    return;
  }
  fcntl(sk, F_SETFL, O_NONBLOCK);
  s_dns = sk;
}

// Risponde alle domande arrivate (dal task del Wi-Fi, ogni 20 ms con l'hotspot)
void dns_poll() {
  if (s_dns < 0) return;
  uint8_t *q = s_dns_buf, *r = s_dns_buf + kDnsMax;
  for (int i = 0; i < 16; i++) {
    struct sockaddr_in from;
    socklen_t fl = sizeof from;
    int n = recvfrom(s_dns, q, kDnsMax, 0, (struct sockaddr *)&from, &fl);
    if (n <= 0) break;
    size_t m = captive_dns_reply(q, n, r, kDnsMax + 16, s_dns_ip);
    if (m) sendto(s_dns, r, m, 0, (struct sockaddr *)&from, fl);
  }
}

esp_err_t redirect_home(httpd_req_t *req) {
  httpd_resp_set_status(req, "302 Found");
  httpd_resp_set_hdr(req, "Location", s_home_url);
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  return httpd_resp_send(req, nullptr, 0);
}

// La richiesta è per il lettore (192.168.4.1, onda.local) e non per un altro sito?
bool own_host(httpd_req_t *req) {
  char h[64];
  if (httpd_req_get_hdr_value_str(req, "Host", h, sizeof h) != ESP_OK) return true;   // senza Host: è per noi
  if (char *c = strchr(h, ':')) *c = 0;
  return !strcmp(h, s_ip_txt) || !strcasecmp(h, APP_NAME) || !strcasecmp(h, APP_NAME ".local");
}

// Indirizzi sconosciuti: con l'hotspot si va alla pagina del lettore (è così che il
// telefono capisce che c'è una pagina da aprire), altrimenti il solito 404
esp_err_t h_404(httpd_req_t *req, httpd_err_code_t err) {
  if (s_captive && strncmp(req->uri, "/api/", 5) != 0) return redirect_home(req);
  return httpd_resp_send_err(req, err, nullptr);
}

// ---------------------------------------------------------- handler ----
esp_err_t h_index(httpd_req_t *req) {
  if (s_captive && !own_host(req)) return redirect_home(req);   // es. http://captive.apple.com/
  httpd_resp_set_type(req, "text/html; charset=utf-8");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  return httpd_resp_send(req, kWebIndex, strlen(kWebIndex));
}

esp_err_t h_info(httpd_req_t *req) {
  XferStatus st = xfer_status();
  std::string s = "{\"name\":\"" APP_NAME "\",\"total\":";
  s += std::to_string(SD_MMC.totalBytes());
  s += ",\"used\":";
  s += std::to_string(SD_MMC.usedBytes());
  s += ",\"mode\":\"";
  s += st.state == XF_AP ? "ap" : "sta";
  s += "\",\"ssid\":";
  json_str(s, st.ssid);
  s += ",\"ip\":";
  json_str(s, st.ip);
  s += ",\"music\":";
  json_str(s, MUSIC_ROOT + strlen(SD_MOUNT));
  s += "}";
  return send_json(req, s);
}

esp_err_t h_ls(httpd_req_t *req) {
  std::string full, rel;
  if (!query_path(req, "p", full, &rel)) return fail(req, "400 Bad Request", "Percorso non valido");
  bool hidden = query_flag(req, "h");
  DIR *d = opendir(full.c_str());
  if (!d) return fail(req, "404 Not Found", "Cartella non trovata");
  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  std::string o = "{\"path\":";
  json_str(o, rel.c_str());
  o += ",\"entries\":[";
  bool first = true;
  while (struct dirent *e = readdir(d)) {
    if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
    if (!hidden && e->d_name[0] == '.') continue;
    std::string p = full + "/" + e->d_name;
    struct stat st;
    bool dir = e->d_type == DT_DIR;
    uint64_t size = 0;
    long mt = 0;
    if (stat(p.c_str(), &st) == 0) {
      dir = S_ISDIR(st.st_mode);
      size = st.st_size;
      mt = st.st_mtime;
    }
    if (!first) o += ',';
    first = false;
    o += "{\"n\":";
    json_str(o, e->d_name);
    o += dir ? ",\"d\":1" : ",\"d\":0";
    o += ",\"s\":" + std::to_string(size) + ",\"t\":" + std::to_string(mt) + "}";
    if (o.size() > 1400) {
      if (httpd_resp_send_chunk(req, o.c_str(), o.size()) != ESP_OK) {
        closedir(d);
        return ESP_FAIL;
      }
      o.clear();
    }
  }
  closedir(d);
  o += "]}";
  httpd_resp_send_chunk(req, o.c_str(), o.size());
  return httpd_resp_send_chunk(req, nullptr, 0);
}

esp_err_t h_upload(httpd_req_t *req) {
  std::string full, rel;
  if (!query_path(req, "p", full, &rel) || rel == "/") return fail(req, "400 Bad Request", "Percorso non valido");
  size_t total = req->content_len;
  mkdirs(parent_of(full));
  std::string part = full + ".part";
  int fd = open(part.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0664);
  if (fd < 0) return fail(req, "500 Internal Server Error", "Impossibile creare il file");

  // buffer in RAM interna: la SD scrive in DMA direttamente da qui
  size_t cap = 16 * 1024;
  char *buf = (char *)heap_caps_malloc(cap, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
  if (!buf) {
    cap = 4096;
    buf = (char *)heap_caps_malloc(cap, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
  }
  if (!buf) {
    close(fd);
    unlink(part.c_str());
    return fail(req, "500 Internal Server Error", "Memoria insufficiente");
  }
  const char *base = strrchr(rel.c_str(), '/') + 1;
  locked([&](XferStatus &s) {
    snprintf(s.cur_name, sizeof s.cur_name, "%s", base);
    s.cur_done = 0;
    s.cur_total = total;
  });
  plat_event("web: carica", base);

  size_t left = total, fill = 0, done = 0;
  int timeouts = 0;
  bool good = true;
  const char *why = "Ricezione interrotta";
  while (left > 0) {
    int r = httpd_req_recv(req, buf + fill, std::min(cap - fill, left));
    if (r == HTTPD_SOCK_ERR_TIMEOUT && ++timeouts < 6) continue;
    if (r <= 0) {
      good = false;
      break;
    }
    timeouts = 0;
    fill += r;
    left -= r;
    if (fill == cap || left == 0) {
      if (write(fd, buf, fill) != (ssize_t)fill) {
        good = false;
        why = errno == ENOSPC ? "Scheda piena" : "Errore di scrittura sulla SD";
        break;
      }
      done += fill;
      fill = 0;
      locked([&](XferStatus &s) { s.cur_done = done; });
    }
  }
  close(fd);
  heap_caps_free(buf);
  locked([](XferStatus &s) { s.cur_name[0] = 0; });
  if (!good) {
    unlink(part.c_str());
    plat_event("web: errore", why);
    return fail(req, "500 Internal Server Error", why);
  }
  unlink(full.c_str());   // sostituisce un file con lo stesso nome
  if (rename(part.c_str(), full.c_str()) != 0) {
    unlink(part.c_str());
    return fail(req, "500 Internal Server Error", "Impossibile salvare il file");
  }
  locked([&](XferStatus &s) {
    s.files_in++;
    s.bytes_in += total;
    s.changes++;
  });
  return ok(req);
}

esp_err_t h_mkdir(httpd_req_t *req) {
  std::string full, rel;
  if (!query_path(req, "p", full, &rel) || rel == "/") return fail(req, "400 Bad Request", "Percorso non valido");
  mkdirs(full);
  struct stat st;
  if (stat(full.c_str(), &st) != 0 || !S_ISDIR(st.st_mode))
    return fail(req, "500 Internal Server Error", "Impossibile creare la cartella");
  changed();
  return ok(req);
}

esp_err_t h_rm(httpd_req_t *req) {
  std::string full, rel;
  if (!query_path(req, "p", full, &rel) || rel == "/") return fail(req, "400 Bad Request", "Percorso non valido");
  plat_event("web: elimina", rel.c_str());
  if (!rm_rf(full)) return fail(req, "500 Internal Server Error", "Impossibile eliminare");
  changed();
  return ok(req);
}

esp_err_t h_mv(httpd_req_t *req) {
  std::string from, to, rf, rt;
  if (!query_path(req, "from", from, &rf) || !query_path(req, "to", to, &rt) || rf == "/" || rt == "/")
    return fail(req, "400 Bad Request", "Percorso non valido");
  if (rt.compare(0, rf.size() + 1, rf + "/") == 0)
    return fail(req, "400 Bad Request", "Non puoi spostare una cartella dentro sé stessa");
  struct stat st;
  if (stat(to.c_str(), &st) == 0) return fail(req, "409 Conflict", "Esiste già un elemento con questo nome");
  mkdirs(parent_of(to));
  if (rename(from.c_str(), to.c_str()) != 0) return fail(req, "500 Internal Server Error", "Impossibile spostare");
  changed();
  return ok(req);
}

esp_err_t h_dl(httpd_req_t *req) {
  std::string full, rel;
  if (!query_path(req, "p", full, &rel)) return fail(req, "400 Bad Request", "Percorso non valido");
  int fd = open(full.c_str(), O_RDONLY);
  if (fd < 0) return fail(req, "404 Not Found", "File non trovato");
  const char *base = strrchr(rel.c_str(), '/') + 1;
  std::string cd = "attachment; filename*=UTF-8''";
  for (const unsigned char *c = (const unsigned char *)base; *c; c++) {
    if (isalnum(*c) || strchr("-._~", *c)) {
      cd += (char)*c;
    } else {
      char h[4];
      snprintf(h, sizeof h, "%%%02X", *c);
      cd += h;
    }
  }
  httpd_resp_set_type(req, "application/octet-stream");
  httpd_resp_set_hdr(req, "Content-Disposition", cd.c_str());
  const size_t cap = 8192;
  char *buf = (char *)heap_caps_malloc(cap, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
  if (!buf) {
    close(fd);
    return fail(req, "500 Internal Server Error", "Memoria insufficiente");
  }
  esp_err_t r = ESP_OK;
  for (;;) {
    ssize_t n = read(fd, buf, cap);
    if (n <= 0) break;
    if ((r = httpd_resp_send_chunk(req, buf, n)) != ESP_OK) break;
  }
  close(fd);
  heap_caps_free(buf);
  if (r == ESP_OK) r = httpd_resp_send_chunk(req, nullptr, 0);
  return r;
}

// ------------------------------------------------- libreria e tag ----
const char *rel_path(const char *full) {
  size_t n = strlen(SD_MOUNT);
  return strncmp(full, SD_MOUNT, n) == 0 ? full + n : full;
}

// JSON a pezzi (risposte lunghe senza tenerle tutte in memoria)
struct Chunked {
  httpd_req_t *req;
  std::string o;
  bool good = true;
  void flush(bool all = false) {
    if (o.size() < 1400 && !all) return;
    if (good && !o.empty() && httpd_resp_send_chunk(req, o.data(), o.size()) != ESP_OK) good = false;
    o.clear();
  }
  esp_err_t end() {
    flush(true);
    if (!good) return ESP_FAIL;
    return httpd_resp_send_chunk(req, nullptr, 0);
  }
};

void kv_num(std::string &o, const char *k, long long v) {
  o += ",\"";
  o += k;
  o += "\":";
  o += std::to_string(v);
}
void kv_str(std::string &o, const char *k, const char *v) {
  o += ",\"";
  o += k;
  o += "\":";
  json_str(o, v ? v : "");
}

// Tutta la libreria: brani (con formato e tag mancanti), album, playlist
esp_err_t h_lib(httpd_req_t *req) {
  if (lib_state() != LIB_READY) return fail(req, "503 Service Unavailable", "Libreria non ancora pronta");
  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  Chunked j{req};
  j.o = "{\"tracks\":[";
  auto &T = lib_tracks();
  for (size_t i = 0; i < T.size() && j.good; i++) {
    const Track &t = T[i];
    j.o += i ? ",{\"p\":" : "{\"p\":";
    json_str(j.o, rel_path(t.path));
    kv_str(j.o, "t", t.title);
    kv_str(j.o, "a", t.artist);
    kv_num(j.o, "al", t.album);
    kv_str(j.o, "g", t.genre_tag);
    kv_num(j.o, "y", t.year);
    kv_num(j.o, "n", t.trackno);
    kv_num(j.o, "d", t.disc);
    kv_num(j.o, "ms", t.dur_ms);
    kv_num(j.o, "sr", t.sample_rate);
    kv_num(j.o, "b", t.bits);
    kv_num(j.o, "s", t.size);
    kv_num(j.o, "pk", t.pic_kind);
    kv_num(j.o, "m", t.miss);
    j.o += '}';
    j.flush();
  }
  j.o += "],\"albums\":[";
  auto &A = lib_albums();
  for (size_t i = 0; i < A.size() && j.good; i++) {
    const Album &a = A[i];
    j.o += i ? ",{\"k\":" : "{\"k\":";
    j.o += std::to_string(a.key);
    kv_str(j.o, "t", a.title);
    kv_str(j.o, "a", a.artist);
    kv_str(j.o, "dir", rel_path(a.dir));
    kv_str(j.o, "af", a.art_file ? rel_path(a.art_file) : "");
    kv_num(j.o, "y", a.year);
    kv_num(j.o, "cv", a.art_state == 1);
    j.o += '}';
    j.flush();
  }
  j.o += "],\"playlists\":[";
  auto &P = lib_playlists();
  for (size_t i = 0; i < P.size() && j.good; i++) {
    const Playlist &p = P[i];
    j.o += i ? ",{\"n\":" : "{\"n\":";
    json_str(j.o, p.name);
    kv_str(j.o, "p", p.path ? rel_path(p.path) : "");
    j.o += ",\"tr\":[";
    for (size_t k = 0; k < p.tracks.size(); k++) {
      if (k) j.o += ',';
      j.o += std::to_string(p.tracks[k]);
      j.flush();
    }
    j.o += "]}";
    j.flush();
  }
  j.o += i18n_lang() == LANG_EN ? "],\"lang\":\"en\"}" : "],\"lang\":\"it\"}";
  return j.end();
}

// Tutto su un file: formato, tag (tutti), immagini, struttura
esp_err_t h_meta(httpd_req_t *req) {
  std::string full, rel;
  if (!query_path(req, "p", full, &rel)) return fail(req, "400 Bad Request", "Percorso non valido");
  FlacInfo fi;
  if (!flac_info(full.c_str(), fi)) return fail(req, "415 Unsupported Media Type", "Non è un file FLAC leggibile");
  std::string o = "{\"p\":";
  json_str(o, rel.c_str());
  kv_num(o, "sr", fi.sample_rate);
  kv_num(o, "ch", fi.channels);
  kv_num(o, "bits", fi.bits);
  kv_num(o, "samples", (long long)fi.total_samples);
  kv_num(o, "size", fi.file_size);
  kv_num(o, "audio", fi.audio_off);
  kv_num(o, "padding", fi.padding);
  kv_num(o, "minb", fi.min_block);
  kv_num(o, "maxb", fi.max_block);
  kv_num(o, "skipped", fi.skipped);
  kv_str(o, "vendor", fi.vendor.c_str());
  char md5[33];
  for (int i = 0; i < 16; i++) snprintf(md5 + 2 * i, 3, "%02x", fi.md5[i]);
  kv_str(o, "md5", md5);
  o += ",\"tags\":[";
  for (size_t i = 0; i < fi.comments.size(); i++) {
    if (i) o += ',';
    json_str(o, fi.comments[i].c_str());
  }
  o += "],\"pics\":[";
  for (size_t i = 0; i < fi.pictures.size(); i++) {
    const auto &p = fi.pictures[i];
    o += i ? ",{\"t\":" : "{\"t\":";
    o += std::to_string(p.type);
    kv_str(o, "mime", p.mime.c_str());
    kv_str(o, "desc", p.desc.c_str());
    kv_num(o, "w", p.w);
    kv_num(o, "h", p.h);
    kv_num(o, "len", p.data_len);
    o += '}';
  }
  o += "],\"blocks\":[";
  for (size_t i = 0; i < fi.blocks.size(); i++) {
    if (i) o += ',';
    o += "[" + std::to_string(fi.blocks[i].type) + "," + std::to_string(fi.blocks[i].len) + "]";
  }
  o += "]}";
  return send_json(req, o);
}

// Riscrive i tag: corpo = un campo per riga, "CHIAVE=valore" codificato come URI
esp_err_t h_tags(httpd_req_t *req) {
  std::string full, rel;
  if (!query_path(req, "p", full, &rel)) return fail(req, "400 Bad Request", "Percorso non valido");
  size_t len = req->content_len;
  if (len > 256 * 1024) return fail(req, "413 Payload Too Large", "Tag troppo grandi");
  std::string body(len, '\0');
  size_t got = 0;
  int timeouts = 0;
  while (got < len) {
    int r = httpd_req_recv(req, &body[got], len - got);
    if (r == HTTPD_SOCK_ERR_TIMEOUT && ++timeouts < 6) continue;
    if (r <= 0) return fail(req, "400 Bad Request", "Ricezione interrotta");
    got += r;
  }
  std::vector<std::string> tags;
  size_t a = 0;
  while (a <= body.size()) {
    size_t e = body.find('\n', a);
    if (e == std::string::npos) e = body.size();
    if (e > a) {
      std::string line = body.substr(a, e - a);
      url_decode(&line[0]);
      line.resize(strlen(line.c_str()));
      if (line.find('=') != std::string::npos) tags.push_back(line);
    }
    a = e + 1;
  }
  plat_event("web: tag", strrchr(rel.c_str(), '/') + 1);
  std::string err;
  bool rewrote = false;
  uint32_t t0 = millis();
  if (!flac_write_comments(full.c_str(), tags, err, &rewrote)) return fail(req, "500 Internal Server Error", err.c_str());
  lib_mark_stale(full.c_str());
  changed();
  std::string o = "{\"ok\":true";
  kv_num(o, "rewrote", rewrote);
  kv_num(o, "ms", millis() - t0);
  o += "}";
  return send_json(req, o);
}

// Quale immagine incorporata: ?i=n, oppure ?c=1 per quella che il lettore usa come copertina
int pic_index(httpd_req_t *req, const FlacInfo &fi) {
  if (query_flag(req, "c")) return flac_cover_index(fi);
  int idx = 0;
  size_t ql = httpd_req_get_url_query_len(req);
  std::string q(ql + 1, '\0');
  char v[8];
  if (httpd_req_get_url_query_str(req, &q[0], ql + 1) == ESP_OK && httpd_query_key_value(q.c_str(), "i", v, sizeof v) == ESP_OK)
    idx = atoi(v);
  return idx;
}

// Immagine incorporata (per mostrarla e convertirla nel browser)
esp_err_t h_pic(httpd_req_t *req) {
  std::string full, rel;
  if (!query_path(req, "p", full, &rel)) return fail(req, "400 Bad Request", "Percorso non valido");
  FlacInfo fi;
  if (!flac_info(full.c_str(), fi)) return fail(req, "404 Not Found", "Nessuna immagine");
  int idx = pic_index(req, fi);
  if (idx < 0 || idx >= (int)fi.pictures.size()) return fail(req, "404 Not Found", "Nessuna immagine");
  const auto &p = fi.pictures[idx];
  int fd = open(full.c_str(), O_RDONLY);
  if (fd < 0) return fail(req, "404 Not Found", "File non trovato");
  // RAM interna e letture da inizio settore: la SD legge più settori per volta in DMA
  // (da un buffer in PSRAM leggerebbe un settore alla volta: un PNG da 1,5 MB = 3000 letture)
  size_t cap = 16384;
  char *buf = (char *)heap_caps_malloc(cap, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
  if (!buf) {
    cap = 4096;
    buf = (char *)heap_caps_malloc(cap, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
  }
  if (!buf) {
    close(fd);
    return fail(req, "500 Internal Server Error", "Memoria insufficiente");
  }
  httpd_resp_set_type(req, p.mime.empty() ? "application/octet-stream" : p.mime.c_str());
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  long off = p.data_off;
  uint32_t left = p.data_len;
  esp_err_t r = lseek(fd, off, SEEK_SET) == off ? ESP_OK : ESP_FAIL;
  while (left && r == ESP_OK) {
    size_t k = std::min<size_t>(left, cap);
    if (off % 512) k = std::min<size_t>(k, 512 - off % 512);
    ssize_t n = read(fd, buf, k);
    if (n <= 0) break;
    left -= n;
    off += n;
    r = httpd_resp_send_chunk(req, buf, n);
  }
  close(fd);
  heap_caps_free(buf);
  return r == ESP_OK ? httpd_resp_send_chunk(req, nullptr, 0) : r;
}

// Sostituisce un'immagine incorporata con un JPEG baseline preparato dal browser
// (copertine PNG o JPEG progressive che il lettore non sa leggere): stesso tipo e
// descrizione, l'audio non si tocca. Sul posto se c'è spazio, altrimenti si ricopia.
esp_err_t h_pic_put(httpd_req_t *req) {
  std::string full, rel;
  if (!query_path(req, "p", full, &rel)) return fail(req, "400 Bad Request", "Percorso non valido");
  size_t len = req->content_len;
  if (len < 100 || len > 4 * 1024 * 1024) return fail(req, "400 Bad Request", "Immagine non valida");
  uint8_t *img = (uint8_t *)plat_alloc_big(len);
  if (!img) return fail(req, "500 Internal Server Error", "Memoria insufficiente");
  size_t got = 0;
  int timeouts = 0;
  while (got < len) {
    int r = httpd_req_recv(req, (char *)img + got, std::min<size_t>(16384, len - got));
    if (r == HTTPD_SOCK_ERR_TIMEOUT && ++timeouts < 6) continue;
    if (r <= 0) break;
    got += r;
  }
  if (got < len) {
    plat_free(img);
    return fail(req, "400 Bad Request", "Ricezione interrotta");
  }
  FlacInfo fi;
  int idx = flac_info(full.c_str(), fi) ? pic_index(req, fi) : -1;
  std::string err;
  bool rewrote = false;
  uint32_t t0 = millis();
  plat_event("web: copertina nel file", strrchr(rel.c_str(), '/') + 1);
  bool good = idx >= 0 && flac_replace_picture(full.c_str(), idx, img, len, err, &rewrote);
  plat_free(img);
  if (!good) return fail(req, "500 Internal Server Error", err.empty() ? "Nessuna immagine nel file" : err.c_str());
  lib_mark_stale(full.c_str());
  for (auto &t : lib_tracks())   // la copertina del suo album va rifatta
    if (!strcmp(t.path, full.c_str())) lib_mark_art_stale(lib_albums()[t.album].key);
  changed();
  std::string o = "{\"ok\":true";
  kv_num(o, "rewrote", rewrote);
  kv_num(o, "ms", millis() - t0);
  o += "}";
  return send_json(req, o);
}

// Miniatura dell'album (64x64) dalla cache del lettore, come BMP a 16 bit
esp_err_t h_thumb(httpd_req_t *req) {
  size_t ql = httpd_req_get_url_query_len(req);
  std::string q(ql + 1, '\0');
  char v[16];
  if (httpd_req_get_url_query_str(req, &q[0], ql + 1) != ESP_OK || httpd_query_key_value(q.c_str(), "k", v, sizeof v) != ESP_OK)
    return fail(req, "400 Bad Request", "Album mancante");
  uint32_t key = strtoul(v, nullptr, 10);
  char path[96];
  art_cache_path(key, ART_SMALL, path, sizeof path);
  FILE *f = fopen(path, "rb");
  if (!f) return fail(req, "404 Not Found", "Nessuna copertina");
  constexpr int S = ART_SMALL;
  std::string out(66 + S * S * 2, '\0');
  uint16_t hdr[4];
  bool okr = fread(hdr, 2, 4, f) == 4 && hdr[0] == 0x414F && fread(&out[66], 2, S * S, f) == (size_t)(S * S);
  fclose(f);
  if (!okr) return fail(req, "404 Not Found", "Nessuna copertina");
  uint8_t *b = (uint8_t *)&out[0];
  auto w32 = [&](int o, uint32_t x) { for (int i = 0; i < 4; i++) b[o + i] = (x >> (8 * i)) & 0xFF; };
  b[0] = 'B';
  b[1] = 'M';
  w32(2, out.size());
  w32(10, 66);
  w32(14, 40);
  w32(18, S);
  w32(22, (uint32_t)-S);   // dall'alto in basso
  b[26] = 1;
  b[28] = 16;
  w32(30, 3);              // BI_BITFIELDS: RGB565
  w32(34, S * S * 2);
  w32(54, 0xF800);
  w32(58, 0x07E0);
  w32(62, 0x001F);
  httpd_resp_set_type(req, "image/bmp");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  return httpd_resp_send(req, out.data(), out.size());
}

// Copertina nuova per un album (JPEG già preparato dal browser): cover.jpg nella
// cartella, oppure "<nome del brano>.jpg" se la cartella ha brani di più album
esp_err_t h_cover(httpd_req_t *req) {
  size_t ql = httpd_req_get_url_query_len(req);
  std::string q(ql + 1, '\0');
  char v[16];
  if (httpd_req_get_url_query_str(req, &q[0], ql + 1) != ESP_OK || httpd_query_key_value(q.c_str(), "k", v, sizeof v) != ESP_OK)
    return fail(req, "400 Bad Request", "Album mancante");
  int ai = lib_album_by_key(strtoul(v, nullptr, 10));
  if (ai < 0) return fail(req, "404 Not Found", "Album non trovato");
  const Album &al = lib_albums()[ai];
  size_t len = req->content_len;
  if (len < 100 || len > 4 * 1024 * 1024) return fail(req, "400 Bad Request", "Immagine non valida");
  std::string dst = lib_album_cover_path(ai), part = dst + ".part";
  FILE *f = fopen(part.c_str(), "wb");
  if (!f) return fail(req, "500 Internal Server Error", "Impossibile scrivere nella cartella dell'album");
  std::string buf(8192, '\0');
  size_t got = 0;
  int timeouts = 0;
  bool good = true;
  while (got < len) {
    int r = httpd_req_recv(req, &buf[0], std::min(buf.size(), len - got));
    if (r == HTTPD_SOCK_ERR_TIMEOUT && ++timeouts < 6) continue;
    if (r <= 0 || fwrite(buf.data(), 1, r, f) != (size_t)r) {
      good = false;
      break;
    }
    if (got == 0 && !((uint8_t)buf[0] == 0xFF && (uint8_t)buf[1] == 0xD8)) {
      good = false;   // non è un JPEG
      break;
    }
    got += r;
  }
  good = (fclose(f) == 0) && good;
  if (!good) {
    unlink(part.c_str());
    return fail(req, "500 Internal Server Error", "Copertina non salvata");
  }
  unlink(dst.c_str());
  if (rename(part.c_str(), dst.c_str()) != 0) {
    unlink(part.c_str());
    return fail(req, "500 Internal Server Error", "Copertina non salvata");
  }
  char cp[96];
  art_cache_path(al.key, ART_SMALL, cp, sizeof cp);
  unlink(cp);
  art_cache_path(al.key, ART_LARGE, cp, sizeof cp);
  unlink(cp);
  lib_mark_art_stale(al.key);
  plat_event("web: copertina", al.title);
  changed();
  return ok(req);
}

// La pagina manda l'ora del telefono/computer appena si apre (ms dal 1970)
esp_err_t h_time(httpd_req_t *req) {
  char b[24] = {0};
  int n = httpd_req_recv(req, b, std::min<size_t>(req->content_len, sizeof b - 1));
  if (n <= 0) return fail(req, "400 Bad Request", "Ora mancante");
  b[n] = 0;
  int64_t ms = strtoll(b, nullptr, 10);
  // da internet è più precisa: il browser la sovrascrive solo se è passata più di un'ora
  if (!(clock_source() == CLOCK_NTP && llabs((long long)(ms / 1000 - time(nullptr))) < 5 &&
        time(nullptr) - clock_synced_at() < 3600))
    clock_set(ms, CLOCK_BROWSER);
  return ok(req);
}

esp_err_t h_done(httpd_req_t *req) {
  locked([](XferStatus &s) { s.done_req = true; });
  return ok(req);
}

void http_start() {
  if (s_http) return;
  httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
  cfg.stack_size = 8192;
  cfg.task_caps = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;   // lo stack in PSRAM: la RAM interna è poca
  cfg.core_id = 0;
  cfg.max_uri_handlers = 24;
  cfg.max_open_sockets = 5;
  cfg.lru_purge_enable = true;
  cfg.recv_wait_timeout = 10;
  cfg.send_wait_timeout = 10;
  if (httpd_start(&s_http, &cfg) != ESP_OK) {
    s_http = nullptr;
    plat_log("[xfer] httpd_start fallito");
    return;
  }
  const struct {
    const char *uri;
    httpd_method_t m;
    esp_err_t (*h)(httpd_req_t *);
  } routes[] = {
    {"/", HTTP_GET, h_index},          {"/api/info", HTTP_GET, h_info}, {"/api/ls", HTTP_GET, h_ls},
    {"/api/up", HTTP_POST, h_upload},  {"/api/mkdir", HTTP_POST, h_mkdir}, {"/api/rm", HTTP_POST, h_rm},
    {"/api/mv", HTTP_POST, h_mv},      {"/api/dl", HTTP_GET, h_dl},    {"/api/done", HTTP_POST, h_done},
    {"/api/time", HTTP_POST, h_time},  {"/api/lib", HTTP_GET, h_lib},   {"/api/meta", HTTP_GET, h_meta},
    {"/api/tags", HTTP_POST, h_tags},  {"/api/pic", HTTP_GET, h_pic},   {"/api/thumb", HTTP_GET, h_thumb},
    {"/api/cover", HTTP_POST, h_cover}, {"/api/pic", HTTP_POST, h_pic_put},
  };
  for (auto &r : routes) {
    httpd_uri_t u = {};
    u.uri = r.uri;
    u.method = r.m;
    u.handler = r.h;
    httpd_register_uri_handler(s_http, &u);
  }
  httpd_register_err_handler(s_http, HTTPD_404_NOT_FOUND, h_404);
}

void http_stop() {
  if (s_http) httpd_stop(s_http);
  s_http = nullptr;
}

// ------------------------------------------------------------- rete ----
void ap_credentials(char *ssid, size_t ns, char *pass, size_t np) {
  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
  snprintf(ssid, ns, APP_NAME "-%02X%02X", mac[4], mac[5]);
  plat_kv_get_str("ap_pass", pass, np);
  if (strlen(pass) < 8) {
    snprintf(pass, np, "%08lu", (unsigned long)(esp_random() % 100000000UL));
    plat_kv_set_str("ap_pass", pass);
  }
}

void mdns_restart() {
  MDNS.end();
  if (MDNS.begin(APP_NAME)) MDNS.addService("http", "tcp", 80);
}

void start_ap(const char *why) {
  char ssid[33], pass[16];
  ap_credentials(ssid, sizeof ssid, pass, sizeof pass);
  dns_stop();
  WiFi.disconnect(true);
  WiFi.mode(WIFI_AP);
  bool okk = WiFi.softAP(ssid, pass);
  IPAddress ip = WiFi.softAPIP();
  locked([&](XferStatus &s) {
    s.state = okk ? XF_AP : XF_ERROR;
    snprintf(s.ssid, sizeof s.ssid, "%s", ssid);
    snprintf(s.ap_pass, sizeof s.ap_pass, "%s", pass);
    snprintf(s.ip, sizeof s.ip, "%s", ip.toString().c_str());
  });
  set_note(why ? why : "");
  if (okk) {
    dns_start(ip);
    mdns_restart();
    http_start();
  }
  plat_log("[xfer] hotspot %s (%s) %s", ssid, okk ? "attivo" : "ERRORE", ip.toString().c_str());
}

// Dopo un tentativo fallito si passa all'hotspot solo se nel frattempo non è stato
// chiesto altro: con "hotspot", "indietro" o un'altra rete in coda ci pensa quel comando.
bool fallback_to_ap() {
  Cmd c;
  return xQueuePeek(s_q, &c, 0) != pdTRUE || c.k == K_SCAN;
}

// Tenta la rete di casa; ritorna true se collegato.
bool start_sta(const char *ssid, const char *pass) {
  locked([&](XferStatus &s) {
    s.state = XF_CONNECTING;
    snprintf(s.ssid, sizeof s.ssid, "%s", ssid);
    s.ip[0] = 0;
  });
  set_note("");
  dns_stop();
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid, pass);
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) {
    // Un altro comando in coda (indietro, hotspot, altra rete, ricerca): si smette di
    // aspettare. Prima xQueuePeek con un comando già in coda tornava subito e questo
    // ciclo girava senza mai cedere la CPU fino al watchdog (riavvio dopo 5 s).
    Cmd c;
    if (xQueuePeek(s_q, &c, 0) == pdTRUE) return false;
    vTaskDelay(pdMS_TO_TICKS(100));
  }
  if (WiFi.status() != WL_CONNECTED) return false;
  IPAddress ip = WiFi.localIP();
  locked([&](XferStatus &s) {
    s.state = XF_STA;
    snprintf(s.ip, sizeof s.ip, "%s", ip.toString().c_str());
  });
  mdns_restart();
  http_start();
  clock_ntp_start();   // con internet l'ora si rimette da sola
  plat_log("[xfer] collegato a %s: http://%s (http://" APP_NAME ".local)", ssid, ip.toString().c_str());
  return true;
}

void start_saved() {
  char ssid[33], pass[65];
  plat_kv_get_str("wifi_ssid", ssid, sizeof ssid);
  plat_kv_get_str("wifi_pass", pass, sizeof pass);
  if (!ssid[0]) {
    start_ap(nullptr);
    return;
  }
  if (!start_sta(ssid, pass) && fallback_to_ap()) {
    char n[72];
    snprintf(n, sizeof n, TR("Rete \"%s\" non raggiungibile: uso l'hotspot"), ssid);
    start_ap(n);
  }
}

void do_scan() {
  s_scan_done = false;
  int n = WiFi.scanNetworks(false, false);   // bloccante (~2-3 s), siamo nel nostro task
  std::vector<XferNet> v;
  for (int i = 0; i < n; i++) {
    XferNet x;
    snprintf(x.ssid, sizeof x.ssid, "%s", WiFi.SSID(i).c_str());
    if (!x.ssid[0]) continue;
    x.rssi = WiFi.RSSI(i);
    x.secure = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
    bool dup = false;
    for (auto &y : v)
      if (!strcmp(y.ssid, x.ssid)) {
        if (x.rssi > y.rssi) y.rssi = x.rssi;
        dup = true;
      }
    if (!dup) v.push_back(x);
  }
  WiFi.scanDelete();
  std::sort(v.begin(), v.end(), [](const XferNet &a, const XferNet &b) { return a.rssi > b.rssi; });
  xSemaphoreTake(s_scan_mtx, portMAX_DELAY);
  s_scan.swap(v);
  xSemaphoreGive(s_scan_mtx);
  s_scan_done = true;
}

void xfer_task(void *) {
  WiFi.persistent(false);
  WiFi.setHostname(APP_NAME);
  for (;;) {
    Cmd c;
    dns_poll();
    if (xQueueReceive(s_q, &c, pdMS_TO_TICKS(s_dns >= 0 ? 20 : 500)) != pdTRUE) continue;
    if (c.k == K_STOP) break;
    switch (c.k) {
      case K_START:
        start_saved();
        break;
      case K_CONNECT: {
        char prev[33];
        plat_kv_get_str("wifi_ssid", prev, sizeof prev);
        if (start_sta(c.ssid, c.pass)) {
          plat_kv_set_str("wifi_ssid", c.ssid);
          plat_kv_set_str("wifi_pass", c.pass);
        } else if (fallback_to_ap()) {
          char n[72];
          snprintf(n, sizeof n, TR("Impossibile collegarsi a \"%s\": password errata?"), c.ssid);
          start_ap(n);
        }
        break;
      }
      case K_HOTSPOT:
        plat_kv_set_str("wifi_ssid", "");
        plat_kv_set_str("wifi_pass", "");
        start_ap(nullptr);
        break;
      case K_SCAN:
        do_scan();
        break;
      default:
        break;
    }
  }
  dns_stop();
  http_stop();
  clock_ntp_stop();
  MDNS.end();
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  locked([](XferStatus &s) {
    s.state = XF_OFF;
    s.ip[0] = 0;
  });
  plat_log("[xfer] Wi-Fi spento");
  s_task = nullptr;
  vTaskDelete(nullptr);
}

void send_cmd(CmdKind k, const char *ssid = nullptr, const char *pass = nullptr) {
  if (!s_q) return;
  Cmd c = {};
  c.k = k;
  if (ssid) snprintf(c.ssid, sizeof c.ssid, "%s", ssid);
  if (pass) snprintf(c.pass, sizeof c.pass, "%s", pass);
  xQueueSend(s_q, &c, pdMS_TO_TICKS(200));
}

}  // namespace

void xfer_start() {
  if (s_task) return;
  if (!s_q) s_q = xQueueCreate(4, sizeof(Cmd));
  if (!s_scan_mtx) s_scan_mtx = xSemaphoreCreateMutex();
  xQueueReset(s_q);
  locked([](XferStatus &s) {
    s = XferStatus();
    s.state = XF_CONNECTING;
  });
  // stack interno: questo task scrive in flash (credenziali) e usa le API Wi-Fi
  xTaskCreatePinnedToCore(xfer_task, "xfer", 6144, nullptr, 2, &s_task, 0);
  send_cmd(K_START);
}

void xfer_stop() {
  if (!s_task) return;
  send_cmd(K_STOP);
  for (int i = 0; i < 60 && s_task; i++) delay(50);
}

XferStatus xfer_status() {
  XferStatus c;
  portENTER_CRITICAL(&s_mux);
  c = s_st;
  portEXIT_CRITICAL(&s_mux);
  return c;
}

void xfer_connect(const char *ssid, const char *pass) { send_cmd(K_CONNECT, ssid, pass); }
void xfer_use_hotspot() { send_cmd(K_HOTSPOT); }
void xfer_saved_network(char *ssid, size_t n) { plat_kv_get_str("wifi_ssid", ssid, n); }

void xfer_scan_start() {
  s_scan_done = false;
  send_cmd(K_SCAN);
}

bool xfer_scan_result(std::vector<XferNet> &out) {
  if (!s_scan_done || !s_scan_mtx) return false;
  xSemaphoreTake(s_scan_mtx, portMAX_DELAY);
  out = s_scan;
  xSemaphoreGive(s_scan_mtx);
  return true;
}

#endif  // ARDUINO
