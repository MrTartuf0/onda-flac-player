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
#include "../config.h"
#include "../platform.h"
#include "web_index.h"
#include "../clock.h"

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

// ---------------------------------------------------------- handler ----
esp_err_t h_index(httpd_req_t *req) {
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
    {"/api/time", HTTP_POST, h_time},
  };
  for (auto &r : routes) {
    httpd_uri_t u = {};
    u.uri = r.uri;
    u.method = r.m;
    u.handler = r.h;
    httpd_register_uri_handler(s_http, &u);
  }
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
    mdns_restart();
    http_start();
  }
  plat_log("[xfer] hotspot %s (%s) %s", ssid, okk ? "attivo" : "ERRORE", ip.toString().c_str());
}

bool stop_pending() {
  Cmd c;
  return xQueuePeek(s_q, &c, 0) == pdTRUE && c.k == K_STOP;
}

// Tenta la rete di casa; ritorna true se collegato.
bool start_sta(const char *ssid, const char *pass) {
  locked([&](XferStatus &s) {
    s.state = XF_CONNECTING;
    snprintf(s.ssid, sizeof s.ssid, "%s", ssid);
    s.ip[0] = 0;
  });
  set_note("");
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid, pass);
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) {
    Cmd c;
    if (xQueuePeek(s_q, &c, pdMS_TO_TICKS(100)) == pdTRUE && c.k == K_STOP) return false;
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
  if (!start_sta(ssid, pass) && !stop_pending()) {
    char n[72];
    snprintf(n, sizeof n, "Rete \"%s\" non raggiungibile: uso l'hotspot", ssid);
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
    if (xQueueReceive(s_q, &c, pdMS_TO_TICKS(500)) != pdTRUE) continue;
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
        } else if (!stop_pending()) {
          char n[72];
          snprintf(n, sizeof n, "Impossibile collegarsi a \"%s\": password errata?", c.ssid);
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
