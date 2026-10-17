#include "server.h"

#include <Arduino.h>
#include <WiFi.h>
#include <text.h>

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../history.h"
#include "app_bundle.h"
#include "wifi_link.h"

using namespace petcore;

namespace server {

namespace {

constexpr uint32_t kReadTimeoutMs = 400;
constexpr size_t kMaxHeader = 1024;
constexpr size_t kMaxBody = 512;

Game* g = nullptr;
WiFiServer srv(80);
bool listening = false;

char req[kMaxHeader + kMaxBody + 1];
char out[6144];

// ---- JSON output -----------------------------------------------------------

struct Json {
  char* buf;
  size_t cap;
  size_t len = 0;
  void raw(const char* s) { fmt("%s", s); }
  void fmt(const char* f, ...) {
    if (len >= cap) return;
    va_list ap;
    va_start(ap, f);
    int n = vsnprintf(buf + len, cap - len, f, ap);
    va_end(ap);
    if (n > 0) len = (len + n < cap) ? len + n : cap - 1;
  }
  void str(const char* s) {
    raw("\"");
    for (; s && *s; s++) {
      unsigned char c = static_cast<unsigned char>(*s);
      if (c == '"' || c == '\\') fmt("\\%c", c);
      else if (c < 0x20) fmt("\\u%04x", c);
      else fmt("%c", c);
    }
    raw("\"");
  }
  void key(const char* k) {
    str(k);
    raw(":");
  }
  void boolean(bool b) { raw(b ? "true" : "false"); }
  void slot_or_null(uint8_t slot) {
    if (slot < kMaxSlots) fmt("%u", slot);
    else raw("null");
  }
};

const char* state_name(State s) {
  switch (s) {
    case State::Setup: return "Setup";
    case State::Idle: return "Idle";
    case State::Arming: return "Arming";
    case State::Running: return "Running";
    case State::Result: return "Result";
  }
  return "Idle";
}
const char* mood_name(Mood m) {
  static const char* kNames[] = {"Happy", "Content", "Sad", "Crying", "Dead", "Sleeping"};
  return kNames[static_cast<int>(m)];
}
const char* result_name(ResultKind r) {
  static const char* kNames[] = {"None", "Success", "Aborted", "Broken", "Died"};
  return kNames[static_cast<int>(r)];
}
const char* reason_name(ShameReason r) {
  static const char* kNames[] = {nullptr, "Lift", "MissedDaily", "PowerCut"};
  return kNames[static_cast<int>(r)];
}
const char* event_name(EventType t) {
  static const char* kNames[] = {"ArmingStarted", "ArmingCancelled", "SessionStarted", "PlayerJoined",
                                 "Evolved",       "TargetReached",   "EmergencyArmed", "EmergencyUsed",
                                 "EmergencyExpired", "SessionEnded", "Hatched",        "DailyMissed",
                                 "DayClosed",     "BonusLife",       "SetupDone",      "NewSpecies",
                                 "NightStarted",  "Morning"};
  return kNames[static_cast<int>(t)];
}

// Days since 1970-01-01 -> "YYYY-MM-DD" (Howard Hinnant's civil_from_days).
void format_date(uint32_t days, char* buf, size_t len) {
  int32_t z = static_cast<int32_t>(days) + 719468;
  int32_t era = z / 146097;
  uint32_t doe = static_cast<uint32_t>(z - era * 146097);
  uint32_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  int32_t y = static_cast<int32_t>(yoe) + era * 400;
  uint32_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  uint32_t mp = (5 * doy + 2) / 153;
  uint32_t d = doy - (153 * mp + 2) / 5 + 1;
  uint32_t m = mp < 10 ? mp + 3 : mp - 9;
  snprintf(buf, len, "%04ld-%02lu-%02lu", static_cast<long>(y + (m <= 2)), static_cast<unsigned long>(m),
           static_cast<unsigned long>(d));
}

void settings_json(Json& j, const Settings& st) {
  j.raw("{");
  j.key("players"); j.fmt("%u,", st.num_players);
  j.key("daily_minimum_min"); j.fmt("%u,", st.daily_minimum_min);
  j.key("day_reset_hour"); j.fmt("%u,", st.day_reset_hour);
  j.key("tz_offset_min"); j.fmt("%d", st.tz_offset_min);
  j.raw("}");
}

// ---- tiny JSON input -------------------------------------------------------

// Finds "key": in a flat JSON object and returns a pointer to the value.
const char* find_value(const char* body, const char* key) {
  char pattern[40];
  snprintf(pattern, sizeof pattern, "\"%s\"", key);
  const char* p = strstr(body, pattern);
  if (!p) return nullptr;
  p += strlen(pattern);
  while (*p == ' ' || *p == ':' || *p == '\t') p++;
  return p;
}

bool get_string(const char* body, const char* key, char* outbuf, size_t len) {
  const char* p = find_value(body, key);
  if (!p || *p != '"') return false;
  p++;
  size_t n = 0;
  while (*p && *p != '"' && n + 1 < len) {
    if (*p == '\\' && p[1]) p++;  // keep escaped char literally
    outbuf[n++] = *p++;
  }
  outbuf[n] = '\0';
  return true;
}

bool get_int(const char* body, const char* key, long* value) {
  const char* p = find_value(body, key);
  if (!p) return false;
  if (*p == '"') p++;  // tolerate "1234"
  char* end = nullptr;
  long v = strtol(p, &end, 10);
  if (end == p) return false;
  *value = v;
  return true;
}

bool pin_ok(const char* body) {
  long pin = -1;
  return g->data().settings.pin != 0 && get_int(body, "pin", &pin) && pin == g->data().settings.pin;
}

// ---- responses ---------------------------------------------------------------

void send_all(WiFiClient& c, const uint8_t* data, size_t len) {
  uint32_t start = millis();
  while (len > 0 && millis() - start < 5000) {
    size_t chunk = len > 1024 ? 1024 : len;
    size_t sent = c.write(data, chunk);
    if (sent == 0) {
      delay(2);
      continue;
    }
    data += sent;
    len -= sent;
  }
}

void send(WiFiClient& c, int status, const char* type, const uint8_t* body, size_t len, bool gzip = false) {
  const char* text = status == 200 ? "OK" : status == 204 ? "No Content" : status == 400 ? "Bad Request"
                   : status == 403 ? "Forbidden" : status == 404 ? "Not Found" : status == 409 ? "Conflict"
                   : "Error";
  char head[320];
  int n = snprintf(head, sizeof head,
                   "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %u\r\n%s"
                   "Access-Control-Allow-Origin: *\r\nAccess-Control-Allow-Headers: Content-Type\r\n"
                   "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\nCache-Control: no-store\r\n"
                   "Connection: close\r\n\r\n",
                   status, text, type, static_cast<unsigned>(len), gzip ? "Content-Encoding: gzip\r\n" : "");
  send_all(c, reinterpret_cast<const uint8_t*>(head), n);
  if (len) send_all(c, body, len);
}

void send_json(WiFiClient& c, int status, const Json& j) {
  send(c, status, "application/json; charset=utf-8", reinterpret_cast<const uint8_t*>(j.buf), j.len);
}

void send_error(WiFiClient& c, int status, const char* message) {
  Json j{out, sizeof out};
  j.raw("{");
  j.key("error");
  j.str(message);
  j.raw("}");
  send_json(c, status, j);
}

// ---- API handlers --------------------------------------------------------------

void handle_state(WiFiClient& c, uint32_t now_ms) {
  const SaveData& d = g->data();
  const Pet& pet = d.pet;
  const Lifetime& life = d.life;
  uint32_t unix = wifi_link::unix_now();
  bool clock = unix != 0;
  bool running = g->state() == State::Running;
  Json j{out, sizeof out};

  j.raw("{\"api\":1,");
  j.key("now"); clock ? j.fmt("%lu,", (unsigned long)unix) : j.raw("null,");
  j.key("clock"); j.boolean(clock); j.raw(",");
  j.key("night"); j.boolean(g->is_night()); j.raw(",");
  j.key("night_hours"); j.fmt("[%u,%u],", kNightStartHour, kNightEndHour);
  j.key("day_ends_in_s");
  clock && g->clock_known() ? j.fmt("%lu,", (unsigned long)g->seconds_until_day_end()) : j.raw("null,");
  j.key("settings"); settings_json(j, d.settings); j.raw(",");

  uint32_t lo = kStageXp[static_cast<int>(pet.stage)];
  j.key("pet"); j.raw("{");
  j.key("stage"); j.str(stage_name(pet.stage)); j.raw(",");
  j.key("stage_index"); j.fmt("%d,", static_cast<int>(pet.stage));
  j.key("species");
  pet.species < kSpeciesCount ? j.str(species_name(pet.species)) : j.raw("null");
  j.raw(",");
  j.key("species_index");
  pet.species < kSpeciesCount ? j.fmt("%u,", pet.species) : j.raw("null,");
  j.key("rarity");
  pet.species < kSpeciesCount ? j.str(rarity_name(species_info(pet.species).rarity)) : j.raw("null");
  j.raw(",");
  j.key("egg_minutes"); j.fmt("%u,", pet.egg_minutes);
  j.key("egg_locked"); j.boolean(g->egg_locked()); j.raw(",");
  j.key("rare_chance"); j.fmt("%d,", egg_rare_percent(pet.egg_minutes));
  j.key("generation"); j.fmt("%u,", pet.generation);
  j.key("hearts"); j.fmt("%u,", pet.hearts);
  j.key("max_hearts"); j.fmt("%u,", kMaxHearts);
  j.key("bonus_cap"); j.fmt("%u,", kMaxBonusHearts);
  j.key("happiness"); j.fmt("%u,", pet.happiness);
  j.key("mood"); j.str(mood_name(g->mood())); j.raw(",");
  j.key("xp"); j.fmt("%lu,", (unsigned long)pet.xp);
  j.key("stage_xp"); j.fmt("%lu,", (unsigned long)lo);
  j.key("next_stage_xp"); j.fmt("%lu", (unsigned long)next_stage_xp(pet.stage));
  j.raw("},");

  j.key("session"); j.raw("{");
  j.key("state"); j.str(state_name(g->state())); j.raw(",");
  j.key("elapsed_s"); j.fmt("%lu,", running ? (unsigned long)(g->session_elapsed_ms(now_ms) / 1000) : 0UL);
  j.key("minutes"); j.fmt("%lu,", running ? (unsigned long)g->session_minutes() : 0UL);
  j.key("xp"); j.fmt("%lu,", running ? (unsigned long)g->session_xp() : 0UL);
  j.key("target_min"); j.fmt("%u,", g->target_minutes());
  j.key("target_reached"); j.boolean(running && g->target_reached()); j.raw(",");
  j.key("bonus_goal_min"); j.fmt("%lu,", (unsigned long)g->bonus_goal_minutes());
  j.key("bonus_earned"); j.boolean(running && g->bonus_earned()); j.raw(",");
  j.key("bonus_available"); j.boolean(g->bonus_available_today()); j.raw(",");
  j.key("emergency_available"); j.boolean(g->emergency_available());
  j.raw("},");

  j.key("result");
  if (g->state() == State::Result) {
    j.raw("{");
    j.key("kind"); j.str(result_name(g->result())); j.raw(",");
    j.key("culprit"); j.slot_or_null(g->culprit()); j.raw(",");
    j.key("cause");
    if (reason_name(g->result_cause())) j.str(reason_name(g->result_cause()));
    else j.raw("null");
    j.raw(",");
    char text[128] = "";
    Event e{EventType::SessionEnded, g->culprit(), static_cast<uint32_t>(g->result())};
    describe_event(e, *g, text, sizeof text);
    j.key("text"); j.str(text);
    j.raw("},");
  } else {
    j.raw("null,");
  }

  uint16_t minimum = d.settings.daily_minimum_min;
  j.key("players"); j.raw("[");
  for (int p = 0; p < g->num_players(); p++) {
    uint8_t bit = 1u << p;
    if (p) j.raw(",");
    j.raw("{");
    j.key("slot"); j.fmt("%d,", p);
    j.key("name"); j.str(player_name(d.settings, p)); j.raw(",");
    j.key("docked"); j.boolean(g->docked_mask() & bit); j.raw(",");
    j.key("in_session"); j.boolean(g->armed_mask() & bit); j.raw(",");
    j.key("lifting"); j.boolean(g->lifting_mask() & bit); j.raw(",");
    j.key("today_min"); j.fmt("%u,", d.daily.minutes[p]);
    j.key("met_today"); j.boolean(minimum == 0 || d.daily.minutes[p] >= minimum); j.raw(",");
    j.key("daily_streak"); j.fmt("%u,", life.daily_streak[p]);
    j.key("best_daily_streak"); j.fmt("%u,", life.best_daily_streak[p]);
    j.key("lifted"); j.fmt("%u,", life.breaks[p]);
    j.key("missed_days"); j.fmt("%u", life.missed_days[p]);
    j.raw("}");
  }
  j.raw("],");

  j.key("shame");
  char shame[96];
  if (describe_shame(*g, shame, sizeof shame)) {
    j.raw("{");
    j.key("slot"); j.slot_or_null(life.shame.slot); j.raw(",");
    j.key("name");
    life.shame.slot < kMaxSlots ? j.str(player_name(d.settings, life.shame.slot)) : j.raw("null");
    j.raw(",");
    j.key("reason"); j.str(reason_name(life.shame.reason)); j.raw(",");
    j.key("at"); life.shame.unix ? j.fmt("%lu,", (unsigned long)life.shame.unix) : j.raw("null,");
    j.key("text"); j.str(shame);
    j.raw("},");
  } else {
    j.raw("null,");
  }

  j.key("life"); j.raw("{");
  j.key("total_min"); j.fmt("%lu,", (unsigned long)life.total_minutes);
  j.key("longest_min"); j.fmt("%lu,", (unsigned long)life.longest_session_min);
  j.key("wins"); j.fmt("%u,", life.sessions_success);
  j.key("broken"); j.fmt("%u,", life.sessions_broken);
  j.key("streak"); j.fmt("%u,", life.streak);
  j.key("best_streak"); j.fmt("%u,", life.best_streak);
  j.key("perfect_days"); j.fmt("%u,", life.perfect_days);
  j.key("bonus_lives"); j.fmt("%u,", life.bonus_lives);
  j.key("graveyard"); j.fmt("%u", life.graveyard);
  j.raw("}}");
  send_json(c, 200, j);
}

void day_json(Json& j, uint32_t day, const uint16_t* minutes, bool today) {
  const Settings& st = g->data().settings;
  char date[32];
  format_date(day, date, sizeof date);
  j.raw("{");
  j.key("date"); j.str(date); j.raw(",");
  j.key("minutes"); j.raw("[");
  for (int p = 0; p < st.num_players; p++) j.fmt(p ? ",%u" : "%u", minutes[p]);
  j.raw("],");
  j.key("met"); j.raw("[");
  for (int p = 0; p < st.num_players; p++) {
    if (p) j.raw(",");
    j.boolean(st.daily_minimum_min == 0 || minutes[p] >= st.daily_minimum_min);
  }
  j.raw("],");
  j.key("today"); j.boolean(today);
  j.raw("}");
}

void handle_history(WiFiClient& c, const char* query) {
  long days = 30;
  const char* p = strstr(query, "days=");
  if (p) days = strtol(p + 5, nullptr, 10);
  if (days < 1) days = 1;
  if (days > history::kDays) days = history::kDays;

  const SaveData& d = g->data();
  Json j{out, sizeof out};
  j.raw("{");
  j.key("minimum"); j.fmt("%u,", d.settings.daily_minimum_min);
  j.key("players"); j.fmt("%u,", d.settings.num_players);
  j.key("days"); j.raw("[");
  int written = 0;
  if (d.daily.day != 0) {
    day_json(j, d.daily.day, d.daily.minutes, true);
    written++;
  }
  static history::DayRecord recs[history::kDays];
  int n = history::recent_days(recs, static_cast<int>(days) - written);
  for (int i = 0; i < n; i++) {
    if (written++) j.raw(",");
    day_json(j, recs[i].day, recs[i].minutes, false);
  }
  j.raw("]}");
  send_json(c, 200, j);
}

void handle_collection(WiFiClient& c) {
  const Collection& col = g->data().collection;
  Json j{out, sizeof out};
  j.raw("{");
  j.key("species"); j.raw("[");
  for (uint8_t s = 0; s < kSpeciesCount; s++) {
    if (s) j.raw(",");
    j.raw("{");
    j.key("index"); j.fmt("%u,", s);
    j.key("name"); j.str(species_name(s)); j.raw(",");
    j.key("rarity"); j.str(rarity_name(species_info(s).rarity)); j.raw(",");
    j.key("count"); j.fmt("%u,", col.count[s]);
    j.key("best_stage");
    col.count[s] ? j.str(stage_name(static_cast<Stage>(col.best_stage[s]))) : j.raw("null");
    j.raw("}");
  }
  j.raw("]}");
  send_json(c, 200, j);
}

void handle_events(WiFiClient& c, const char* query) {
  unsigned long since = 0;
  const char* p = strstr(query, "since=");
  if (p) since = strtoul(p + 6, nullptr, 10);
  const history::LoggedEvent* list[history::kEvents];
  int n = history::events_since(since, list, history::kEvents);
  Json j{out, sizeof out};
  j.raw("{");
  j.key("next"); j.fmt("%lu,", (unsigned long)(history::next_event_id() - 1));
  j.key("events"); j.raw("[");
  // Newest events matter most if the buffer is too small for all of them.
  int first = 0;
  while (first < n && (n - first) * 200 > static_cast<int>(sizeof out) - 64) first++;
  for (int i = first; i < n; i++) {
    const history::LoggedEvent& e = *list[i];
    if (i > first) j.raw(",");
    j.raw("{");
    j.key("id"); j.fmt("%lu,", (unsigned long)e.id);
    j.key("at"); e.at ? j.fmt("%lu,", (unsigned long)e.at) : j.raw("null,");
    j.key("type"); j.str(event_name(e.event.type)); j.raw(",");
    j.key("slot"); j.slot_or_null(e.event.slot); j.raw(",");
    j.key("value");
    if (e.event.type == EventType::SessionEnded) j.str(result_name(static_cast<ResultKind>(e.event.value)));
    else j.fmt("%lu", (unsigned long)e.event.value);
    j.raw(",");
    j.key("text"); j.str(e.text);
    j.raw("}");
  }
  j.raw("]}");
  send_json(c, 200, j);
}

void handle_name(WiFiClient& c, int slot, const char* body) {
  if (!pin_ok(body)) return send_error(c, 403, "Wrong PIN");
  if (slot < 0 || slot >= g->num_players()) return send_error(c, 400, "No such player");
  char name[64];
  if (!get_string(body, "name", name, sizeof name) || name[0] == '\0') return send_error(c, 400, "Name is empty");
  // Cut to 12 bytes without splitting a UTF-8 character.
  size_t n = strlen(name);
  if (n > kNameLen) {
    n = kNameLen;
    while (n > 0 && (static_cast<unsigned char>(name[n]) & 0xC0) == 0x80) n--;
    name[n] = '\0';
  }
  g->set_player_name(static_cast<uint8_t>(slot), name);
  Json j{out, sizeof out};
  j.raw("{\"ok\":true,");
  j.key("name"); j.str(player_name(g->data().settings, slot));
  j.raw("}");
  send_json(c, 200, j);
}

// The egg's time setting, until its first session.
void handle_egg(WiFiClient& c, const char* body) {
  if (!pin_ok(body)) return send_error(c, 403, "Wrong PIN");
  long v;
  if (!get_int(body, "minutes", &v) || !is_egg_minutes(static_cast<uint16_t>(v)))
    return send_error(c, 400, "minutes must be 15, 30, 60 or 90");
  if (!g->choose_egg(static_cast<uint16_t>(v)))
    return send_error(c, 409, "This egg's time is set until the pet is gone");
  Json j{out, sizeof out};
  j.fmt("{\"ok\":true,\"egg_minutes\":%u,\"rare_chance\":%d}", g->target_minutes(), egg_rare_percent(g->target_minutes()));
  send_json(c, 200, j);
}

// Retire the pet and start a fresh egg.
void handle_new_pet(WiFiClient& c, const char* body) {
  if (!pin_ok(body)) return send_error(c, 403, "Wrong PIN");
  if (!g->new_pet()) return send_error(c, 409, "A new pet can only start between sessions");
  Json j{out, sizeof out};
  j.fmt("{\"ok\":true,\"generation\":%u}", g->data().pet.generation);
  send_json(c, 200, j);
}

void handle_settings(WiFiClient& c, const char* body) {
  if (!pin_ok(body)) return send_error(c, 403, "Wrong PIN");
  Settings st = g->data().settings;
  long v;
  if (get_int(body, "tz_offset_min", &v)) {
    if (v < -720 || v > 840) return send_error(c, 400, "tz_offset_min out of range");
    st.tz_offset_min = static_cast<int16_t>(v);
  }
  if (get_int(body, "daily_minimum_min", &v)) {
    if (v < 0 || v > 600) return send_error(c, 400, "daily_minimum_min out of range");
    st.daily_minimum_min = static_cast<uint16_t>(v);
  }
  if (get_int(body, "day_reset_hour", &v)) {
    if (v < 0 || v > 23) return send_error(c, 400, "day_reset_hour out of range");
    st.day_reset_hour = static_cast<uint8_t>(v);
  }
  if (!g->update_settings(st)) return send_error(c, 409, "Settings can only change between sessions");
  Json j{out, sizeof out};
  j.raw("{");
  j.key("settings"); settings_json(j, g->data().settings);
  j.raw("}");
  send_json(c, 200, j);
}

void handle_time(WiFiClient& c, const char* body) {
  if (wifi_link::unix_now() != 0) return send_error(c, 409, "The box already knows the time");
  long unix = 0;
  if (!get_int(body, "unix", &unix) || unix < 1700000000L) return send_error(c, 400, "Send {\"unix\": <seconds>}");
  wifi_link::set_unix(static_cast<uint32_t>(unix));
  Json j{out, sizeof out};
  j.raw("{\"ok\":true,\"clock\":true}");
  send_json(c, 200, j);
}

bool serve_static(WiFiClient& c, const char* path) {
  for (const app_bundle::File& f : app_bundle::kFiles) {
    if (strcmp(path, f.path) == 0) {
      send(c, 200, f.type, f.data, f.len, f.gzip);
      return true;
    }
  }
  return false;
}

// ---- request handling ------------------------------------------------------------

const char* find_header(const char* headers, const char* name) {
  size_t n = strlen(name);
  for (const char* p = headers; *p; p++)
    if (strncasecmp(p, name, n) == 0) return p + n;
  return nullptr;
}

// Reads headers and body into `req`. Returns body pointer, or nullptr on timeout.
const char* read_request(WiFiClient& c, size_t* header_len) {
  size_t len = 0;
  uint32_t start = millis();
  const char* body = nullptr;
  size_t content_length = 0;
  while (millis() - start < kReadTimeoutMs) {
    while (c.available() && len < sizeof req - 1) {
      req[len++] = static_cast<char>(c.read());
      req[len] = '\0';
      if (!body) {
        char* end = strstr(req, "\r\n\r\n");
        if (end) {
          body = end + 4;
          *header_len = body - req;
          const char* cl = find_header(req, "Content-Length:");
          content_length = cl ? strtoul(cl, nullptr, 10) : 0;
          if (content_length > kMaxBody) content_length = kMaxBody;
        }
      }
      if (body && len - *header_len >= content_length) return body;
    }
    if (len >= sizeof req - 1) break;
    delay(1);
  }
  return body;
}

void handle(WiFiClient& c, uint32_t now_ms) {
  size_t header_len = 0;
  const char* body = read_request(c, &header_len);
  if (!body) return;

  char method[8] = "", target[160] = "";
  sscanf(req, "%7s %159s", method, target);
  char* query = strchr(target, '?');
  if (query) *query++ = '\0';
  else query = const_cast<char*>("");

  bool get = strcmp(method, "GET") == 0, post = strcmp(method, "POST") == 0;
  if (strcmp(method, "OPTIONS") == 0) return send(c, 204, "text/plain", nullptr, 0);

  if (get && strcmp(target, "/api/state") == 0) return handle_state(c, now_ms);
  if (get && strcmp(target, "/api/history") == 0) return handle_history(c, query);
  if (get && strcmp(target, "/api/events") == 0) return handle_events(c, query);
  if (get && strcmp(target, "/api/collection") == 0) return handle_collection(c);
  if (post && strcmp(target, "/api/settings") == 0) return handle_settings(c, body);
  if (post && strcmp(target, "/api/time") == 0) return handle_time(c, body);
  if (post && strcmp(target, "/api/egg") == 0) return handle_egg(c, body);
  if (post && strcmp(target, "/api/pet/new") == 0) return handle_new_pet(c, body);
  int slot = -1;
  char tail[8] = "";
  if (post && sscanf(target, "/api/players/%d/%7s", &slot, tail) == 2 && strcmp(tail, "name") == 0)
    return handle_name(c, slot, body);
  if (get && serve_static(c, strcmp(target, "/index.html") == 0 ? "/" : target)) return;
  send_error(c, 404, "Not found");
}

}  // namespace

void begin(Game& game) { g = &game; }

void poll(uint32_t now_ms) {
  if (!wifi_link::connected()) {
    listening = false;
    return;
  }
  if (!listening) {
    srv.begin();
    listening = true;
    Serial.println("server: listening on port 80");
  }
  WiFiClient c = srv.accept();
  if (!c) return;
  handle(c, now_ms);
  c.stop();
}

}  // namespace server
