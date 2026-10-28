// MASL Watchface - watch side
// Shows the time and date over a big logo for the selected team, in its
// colors, with its next (or live) game in a bar at the bottom. In split view
// the game is shown instead as both logos on a background split down the
// middle in each team's color. With no team selected it always shows the
// league's next game in split view.
//
// The phone picks the game and sends it in one AppMessage, with the number of
// minutes until the face should ask again (sooner while a game is live).
// The last message is kept in persistent storage so the face isn't blank on
// start.

#include <pebble.h>

#define PERSIST_KEY_VERSION 1
#define PERSIST_KEY_GAME 2
// Bump when Game changes shape so an old saved copy is ignored.
#define PERSIST_VERSION 4

// Minutes before asking the phone again when it hasn't said.
#define DEFAULT_REFRESH 30

enum { STATE_NONE, STATE_SCHEDULED, STATE_LIVE, STATE_FINAL };
enum { MODE_START_TIME, MODE_COUNTDOWN };
enum { BAR_PRIMARY, BAR_SECONDARY, BAR_CUSTOM };
enum { BATTERY_ALWAYS, BATTERY_LOW, BATTERY_NEVER };
// How finely the countdown counts: "in 3d 4h", "in 3d 4h 12m" or
// "3d 04:12:30".
enum { COUNTDOWN_HOURS, COUNTDOWN_MINUTES, COUNTDOWN_SECONDS };

// Seconds sit after the time in a smaller font, bottoms lined up. Offsets
// are from the LECO fonts' line boxes; adjust here if they look off.
#define SECONDS_GAP 2
#define SECONDS_DY 21

// Battery level at or below which it counts as low (shown in red, and shown
// at all with the "when low" setting).
#define BATTERY_LOW_PERCENT 20

typedef struct {
  char team[12];    // selected team's short name, "" for the league's next game
  char away[12];
  char home[12];
  char day[12];     // "Sat 11/28", in the zone chosen in settings
  char time[14];    // "10:05pm CST"
  char detail[20];  // period and clock while live ("2nd 4:12")
  char status[24];  // shown when there's no game ("No games listed")
  int32_t start;    // kickoff, Unix time, 0 if unknown
  int16_t away_score;  // -1 when hidden (no-spoilers mode) or unknown
  int16_t home_score;
  uint8_t state;
  uint8_t mode;
  uint8_t split;    // 1 to show the selected team's game in split view
  uint8_t bar_style;   // BAR_*: which color the game bar uses
  uint8_t battery;     // BATTERY_*: when to show the battery
  int32_t bar_color;   // hex color for BAR_CUSTOM
  uint8_t seconds;     // 1 to show seconds on the clock
  uint8_t countdown;   // COUNTDOWN_*
} Game;

typedef struct {
  const char *name;   // short name exactly as the ticker shows it
  const char *code;   // abbreviation for scores ("STL 3 - 2 MKE")
  uint32_t logo;      // big logo, for the selected team
  uint32_t half;      // smaller logo, for two teams side by side
  uint32_t bg;        // background color
  uint32_t accent;    // game bar color, primary
  uint32_t accent2;   // game bar color, secondary
} Team;

// Keep in sync with src/pkjs/teams.js. Backgrounds are all dark enough for
// white text, so the split matchup screen works for any pair.
static const Team s_teams[] = {
  { "Ambush", "STL", RESOURCE_ID_LOGO_AMBUSH, RESOURCE_ID_HALF_AMBUSH, 0x000000, 0xFF55AA, 0x00AAAA },
  { "Blast", "BAL", RESOURCE_ID_LOGO_BLAST, RESOURCE_ID_HALF_BLAST, 0xAA0000, 0xFFFF00, 0x000000 },
  { "Comets", "KC", RESOURCE_ID_LOGO_COMETS, RESOURCE_ID_HALF_COMETS, 0x0055AA, 0xFF5500, 0xFFFFFF },
  { "Sockers", "SD", RESOURCE_ID_LOGO_SOCKERS, RESOURCE_ID_HALF_SOCKERS, 0x000055, 0xFFAA00, 0x0055AA },
  { "Spirits", "LV", RESOURCE_ID_LOGO_SPIRITS, RESOURCE_ID_HALF_SPIRITS, 0x000000, 0xFF5500, 0xFFFFFF },
  { "Stars", "TAC", RESOURCE_ID_LOGO_STARS, RESOURCE_ID_HALF_STARS, 0x000000, 0xFFAA00, 0x000055 },
  { "Strykers", "EMP", RESOURCE_ID_LOGO_STRYKERS, RESOURCE_ID_HALF_STRYKERS, 0x550055, 0xAA00AA, 0xAAAAAA },
  { "UCFC", "UTI", RESOURCE_ID_LOGO_UCFC, RESOURCE_ID_HALF_UCFC, 0x000055, 0x00AAFF, 0x0055AA },
  { "Wave", "MKE", RESOURCE_ID_LOGO_WAVE, RESOURCE_ID_HALF_WAVE, 0x000000, 0xFFAA00, 0xFFFFFF },
};
#define NUM_TEAMS ((int)ARRAY_LENGTH(s_teams))

// Where everything goes on this screen. Logo boxes match the image sizes in
// tools/convert_logos.py.
typedef struct {
  GRect date, logo, time, line1, line2, battery;
  int16_t bar_y;
  int16_t half_dx;  // distance from the center to each half logo's center
  GFont date_font, time_font, line1_font, line2_font;
} Layout;

static Window *s_window;
static Layer *s_canvas;
static Layout s_layout;
static Game s_game;
static GBitmap *s_logo, *s_away_logo, *s_home_logo;
static int s_refresh_in = DEFAULT_REFRESH;
static TimeUnits s_tick_unit;  // what tick_handler is subscribed to, 0 before init

static void update_tick_unit(void);

static const Team *find_team(const char *name) {
  for (int i = 0; i < NUM_TEAMS; i++) {
    if (strcmp(s_teams[i].name, name) == 0) return &s_teams[i];
  }
  return NULL;
}

static void upcase(char *s) {
  for (; *s; s++) {
    if (*s >= 'a' && *s <= 'z') *s -= 'a' - 'A';
  }
}

// The team's abbreviation, or the first three letters of a team that isn't
// in s_teams yet.
static void team_code(const char *name, char *out, size_t size) {
  const Team *t = find_team(name);
  if (t) {
    snprintf(out, size, "%s", t->code);
  } else {
    snprintf(out, size, "%.3s", name);
    upcase(out);
  }
}

#ifdef PBL_COLOR
// A team's background color, or black for a team not in s_teams.
static GColor team_bg(const Team *t) {
  return t ? GColorFromHEX(t->bg) : GColorBlack;
}
#endif

// The game bar's color: the selected team's primary or secondary accent, or
// the custom color from settings. Black in league mode unless custom.
static GColor bar_color(const Team *team) {
#ifdef PBL_COLOR
  if (s_game.bar_style == BAR_CUSTOM) return GColorFromHEX(s_game.bar_color);
  if (!team) return GColorBlack;
  return GColorFromHEX(s_game.bar_style == BAR_SECONDARY ? team->accent2 : team->accent);
#else
  return GColorBlack;
#endif
}

// Black or white, whichever reads better on bg.
static GColor text_on(GColor bg) {
#ifdef PBL_COLOR
  int lum = bg.r * 299 + bg.g * 587 + bg.b * 114;  // channels are 0-3
  return lum > 1500 ? GColorBlack : GColorWhite;
#else
  return gcolor_equal(bg, GColorBlack) ? GColorWhite : GColorBlack;
#endif
}

static void set_layout(GRect b) {
  int w = b.size.w;
  Layout *l = &s_layout;
  if (w >= 260) {         // gabbro, 260x260 round
    *l = (Layout){ .date = GRect(0, 20, w, 30), .logo = GRect(0, 50, w, 110),
                   .time = GRect(0, 152, w, 52), .bar_y = 206,
                   .line1 = GRect(40, 203, w - 80, 22), .line2 = GRect(40, 220, w - 80, 30),
                   .half_dx = 56, .battery = GRect(w / 2 - 10, 6, 20, 10) };
  } else if (w >= 200) {  // emery, 200x228
    *l = (Layout){ .date = GRect(0, -4, w, 30), .logo = GRect(0, 26, w, 106),
                   .time = GRect(0, 124, w, 52), .bar_y = 182,
                   .line1 = GRect(4, 179, w - 8, 22), .line2 = GRect(4, 196, w - 8, 30),
                   .half_dx = 50, .battery = GRect(w - 26, 7, 20, 10) };
  } else if (w >= 180) {  // chalk, 180x180 round
    *l = (Layout){ .date = GRect(0, 10, w, 22), .logo = GRect(0, 31, w, 68),
                   .time = GRect(0, 88, w, 50), .bar_y = 140,
                   .line1 = GRect(25, 137, w - 50, 16), .line2 = GRect(25, 150, w - 50, 22),
                   .half_dx = 38, .battery = GRect(w / 2 - 8, 2, 16, 8) };
  } else {                // basalt, diorite, flint, 144x168
    *l = (Layout){ .date = GRect(0, -3, w, 22), .logo = GRect(0, 20, w, 74),
                   .time = GRect(0, 84, w, 50), .bar_y = 136,
                   .line1 = GRect(2, 133, w - 4, 16), .line2 = GRect(2, 146, w - 4, 22),
                   .half_dx = 36, .battery = GRect(w - 19, 4, 16, 8) };
  }
  bool large = w >= 200;
  l->date_font = fonts_get_system_font(large ? FONT_KEY_GOTHIC_24_BOLD : FONT_KEY_GOTHIC_18_BOLD);
  l->time_font = fonts_get_system_font(FONT_KEY_LECO_42_NUMBERS);
  l->line1_font = fonts_get_system_font(large ? FONT_KEY_GOTHIC_18_BOLD : FONT_KEY_GOTHIC_14_BOLD);
  l->line2_font = fonts_get_system_font(large ? FONT_KEY_GOTHIC_24_BOLD : FONT_KEY_GOTHIC_18_BOLD);
}

// ---------------------------------------------------------------------------
// Logos

static void destroy_logo(GBitmap **bmp) {
  if (*bmp) {
    gbitmap_destroy(*bmp);
    *bmp = NULL;
  }
}

static GBitmap *create_logo(const char *name, bool half) {
  const Team *t = find_team(name);
  return t ? gbitmap_create_with_resource(half ? t->half : t->logo) : NULL;
}

// Split view: always for the league's next game, and for the selected
// team's game when chosen in settings and there is a game to show.
static bool split_view(void) {
  return !s_game.team[0] || (s_game.split && (s_game.away[0] || s_game.home[0]));
}

// Only the logos on screen are loaded: the team's big one, or both halves of
// the game in split view.
static void load_logos(void) {
  destroy_logo(&s_logo);
  destroy_logo(&s_away_logo);
  destroy_logo(&s_home_logo);
  if (!split_view()) {
    s_logo = create_logo(s_game.team, false);
  } else {
    s_away_logo = create_logo(s_game.away, true);
    s_home_logo = create_logo(s_game.home, true);
  }
}

// Draw bmp centered on (cx, center of box), or the team's name if it has no
// logo yet.
static void draw_logo(GContext *ctx, GBitmap *bmp, const char *name, int16_t cx, GRect box,
                      GColor fg) {
  if (bmp) {
    GSize sz = gbitmap_get_bounds(bmp).size;
    GRect r = GRect(cx - sz.w / 2, box.origin.y + (box.size.h - sz.h) / 2, sz.w, sz.h);
    graphics_context_set_compositing_mode(ctx, PBL_IF_COLOR_ELSE(GCompOpSet, GCompOpAssign));
    graphics_draw_bitmap_in_rect(ctx, bmp, r);
    return;
  }
  char text[12];
  snprintf(text, sizeof(text), "%s", name[0] ? name : "MASL");
  upcase(text);
  GFont font = fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);
  GRect r = GRect(cx - box.size.w / 2, box.origin.y + box.size.h / 2 - 20, box.size.w, 34);
  graphics_context_set_text_color(ctx, fg);
  graphics_draw_text(ctx, text, font, r, GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentCenter, NULL);
}

// ---------------------------------------------------------------------------
// Text

static bool is_today(time_t when) {
  time_t now = time(NULL);
  struct tm today = *localtime(&now);
  struct tm then = *localtime(&when);
  return today.tm_year == then.tm_year && today.tm_yday == then.tm_yday;
}

static bool countdown_running(void) {
  return s_game.state == STATE_SCHEDULED && s_game.mode == MODE_COUNTDOWN &&
         s_game.start > (int32_t)time(NULL);
}

static void format_countdown(char *buf, size_t size, int32_t start, uint8_t detail) {
  int32_t secs = start - (int32_t)time(NULL);
  if (secs <= 0) {
    snprintf(buf, size, "Starting");
    return;
  }
  if (detail == COUNTDOWN_SECONDS) {
    int days = secs / 86400, hours = secs / 3600 % 24, mins = secs / 60 % 60;
    if (days > 0) {
      // No "in": this one is the widest and gets clipped on round screens.
      snprintf(buf, size, "%dd %02d:%02d:%02d", days, hours, mins, (int)(secs % 60));
    } else if (hours > 0) {
      snprintf(buf, size, "in %d:%02d:%02d", hours, mins, (int)(secs % 60));
    } else {
      snprintf(buf, size, "in %d:%02d", mins, (int)(secs % 60));
    }
    return;
  }
  int mins = (secs + 59) / 60;
  int days = mins / (24 * 60), hours = mins / 60 % 24;
  if (days > 0 && detail == COUNTDOWN_MINUTES) {
    snprintf(buf, size, "in %dd %dh %02dm", days, hours, mins % 60);
  } else if (days > 0) {
    snprintf(buf, size, "in %dd %dh", days, hours);
  } else if (hours > 0) {
    snprintf(buf, size, "in %dh %02dm", hours, mins % 60);
  } else {
    snprintf(buf, size, "in %dm", mins);
  }
}

// The two lines in the game bar.
//   Scheduled:  "@ Wave - Sat 11/28" / "7:05pm EST" or "in 3d 4h"
//   Live:       "LIVE - 2nd 4:12"    / "STL 3 - 2 MKE", or "@ Wave" with no spoilers
//   Final:      "FINAL"              / "STL 3 - 2 MKE" (only sent with scores on)
// With no team selected, "@ Wave" becomes "KC @ STL".
static void format_lines(char *l1, size_t n1, char *l2, size_t n2) {
  const Game *g = &s_game;
  char away[8], home[8], who[24];
  team_code(g->away, away, sizeof(away));
  team_code(g->home, home, sizeof(home));
  if (!g->team[0]) {
    snprintf(who, sizeof(who), "%s @ %s", away, home);
  } else {
    bool at_home = strcmp(g->home, g->team) == 0;
    snprintf(who, sizeof(who), "%s %s", at_home ? "vs" : "@", at_home ? g->away : g->home);
  }
  bool scores = g->away_score >= 0 && g->home_score >= 0;

  switch (g->state) {
    case STATE_SCHEDULED: {
      const char *day = g->start && is_today(g->start) ? "Today" : g->day;
      if (day[0]) {
        snprintf(l1, n1, "%s - %s", who, day);
      } else {
        snprintf(l1, n1, "%s", who);
      }
      if (g->mode == MODE_COUNTDOWN && g->start) {
        format_countdown(l2, n2, g->start, g->countdown);
      } else {
        snprintf(l2, n2, "%s", g->time);
      }
      break;
    }
    case STATE_LIVE:
    case STATE_FINAL:
      if (g->state == STATE_FINAL) {
        snprintf(l1, n1, "FINAL");
      } else if (g->detail[0]) {
        snprintf(l1, n1, "LIVE - %s", g->detail);
      } else {
        snprintf(l1, n1, "LIVE");
      }
      if (scores) {
        snprintf(l2, n2, "%s %d - %d %s", away, g->away_score, g->home_score, home);
      } else {
        snprintf(l2, n2, "%s", who);
      }
      break;
    default:
      snprintf(l1, n1, "%s", g->team[0] ? "NEXT GAME" : "MASL");
      snprintf(l2, n2, "%s", g->status[0] ? g->status : "Loading...");
      break;
  }
}

static void draw_text(GContext *ctx, const char *text, GFont font, GRect r, GColor color) {
  graphics_context_set_text_color(ctx, color);
  graphics_draw_text(ctx, text, font, r, GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentCenter, NULL);
}

// ---------------------------------------------------------------------------
// Drawing

// The time, followed by seconds in a smaller font when they're on. The two
// are centered together.
static void draw_clock(GContext *ctx, const char *clock, int sec, GColor fg) {
  const Layout *l = &s_layout;
  if (!s_game.seconds) {
    draw_text(ctx, clock, l->time_font, l->time, fg);
    return;
  }
  char secs[4];
  snprintf(secs, sizeof(secs), "%02d", sec);
  GFont small = fonts_get_system_font(FONT_KEY_LECO_20_BOLD_NUMBERS);
  GSize big_size = graphics_text_layout_get_content_size(
      clock, l->time_font, l->time, GTextOverflowModeFill, GTextAlignmentLeft);
  GSize small_size = graphics_text_layout_get_content_size(
      secs, small, l->time, GTextOverflowModeFill, GTextAlignmentLeft);
  int16_t x = l->time.origin.x +
              (l->time.size.w - big_size.w - SECONDS_GAP - small_size.w) / 2;
  graphics_context_set_text_color(ctx, fg);
  graphics_draw_text(ctx, clock, l->time_font,
                     GRect(x, l->time.origin.y, big_size.w + 4, l->time.size.h),
                     GTextOverflowModeFill, GTextAlignmentLeft, NULL);
  graphics_draw_text(ctx, secs, small,
                     GRect(x + big_size.w + SECONDS_GAP, l->time.origin.y + SECONDS_DY,
                           small_size.w + 4, 26),
                     GTextOverflowModeFill, GTextAlignmentLeft, NULL);
}

// A battery outline with a nub on the right, filled to the charge level: red
// when low, green while charging (color watches).
static void draw_battery(GContext *ctx, GRect r, GColor fg) {
  BatteryChargeState b = battery_state_service_peek();
  bool low = b.charge_percent <= BATTERY_LOW_PERCENT;
  if (s_game.battery == BATTERY_NEVER ||
      (s_game.battery == BATTERY_LOW && !low && !b.is_charging)) {
    return;
  }
  GRect body = GRect(r.origin.x, r.origin.y, r.size.w - 2, r.size.h);
  graphics_context_set_stroke_color(ctx, fg);
  graphics_draw_rect(ctx, body);
  graphics_context_set_fill_color(ctx, fg);
  graphics_fill_rect(ctx, GRect(body.origin.x + body.size.w, r.origin.y + r.size.h / 4, 2,
                                r.size.h - r.size.h / 4 * 2), 0, GCornerNone);
  int fill = (body.size.w - 4) * b.charge_percent / 100;
  graphics_context_set_fill_color(
      ctx, PBL_IF_COLOR_ELSE(b.is_charging ? GColorGreen : low ? GColorRed : fg, fg));
  graphics_fill_rect(ctx, GRect(body.origin.x + 2, body.origin.y + 2, fill > 0 ? fill : 1,
                                body.size.h - 4), 0, GCornerNone);
}

static void canvas_update(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  const Layout *l = &s_layout;
  bool matchup = split_view();
  int16_t cx = b.size.w / 2;

  // Background: the team's color, or split down the middle for a matchup.
  // The bar color comes from bar_color().
#ifdef PBL_COLOR
  const Team *team = find_team(s_game.team);
  if (matchup) {
    const Team *away = find_team(s_game.away), *home = find_team(s_game.home);
    graphics_context_set_fill_color(ctx, team_bg(away));
    graphics_fill_rect(ctx, GRect(0, 0, cx, b.size.h), 0, GCornerNone);
    graphics_context_set_fill_color(ctx, team_bg(home));
    graphics_fill_rect(ctx, GRect(cx, 0, b.size.w - cx, b.size.h), 0, GCornerNone);
  } else {
    graphics_context_set_fill_color(ctx, team_bg(team));
    graphics_fill_rect(ctx, b, 0, GCornerNone);
  }
  GColor fg = GColorWhite;
  GColor bar = bar_color(team);
#else
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, b, 0, GCornerNone);
  GColor fg = GColorBlack;
  GColor bar = bar_color(NULL);
#endif
  graphics_context_set_fill_color(ctx, bar);
  graphics_fill_rect(ctx, GRect(0, l->bar_y, b.size.w, b.size.h - l->bar_y), 0, GCornerNone);

  // Date and time
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  char date[16], clock[8];
  strftime(date, sizeof(date), "%a %b", t);
  upcase(date);
  snprintf(date + strlen(date), sizeof(date) - strlen(date), " %d", t->tm_mday);
  strftime(clock, sizeof(clock), clock_is_24h_style() ? "%H:%M" : "%I:%M", t);
  const char *clock_text = clock[0] == '0' && !clock_is_24h_style() ? clock + 1 : clock;
  draw_text(ctx, date, l->date_font, l->date, fg);
  draw_clock(ctx, clock_text, t->tm_sec, fg);
  draw_battery(ctx, l->battery, fg);

  // Logos
  if (matchup && (s_game.away[0] || s_game.home[0])) {
    draw_logo(ctx, s_away_logo, s_game.away, cx - l->half_dx,
              GRect(0, l->logo.origin.y, l->half_dx * 2, l->logo.size.h), fg);
    draw_logo(ctx, s_home_logo, s_game.home, cx + l->half_dx,
              GRect(0, l->logo.origin.y, l->half_dx * 2, l->logo.size.h), fg);
  } else {
    draw_logo(ctx, s_logo, s_game.team, cx, l->logo, fg);
  }

  // Game bar
  char line1[32], line2[32];
  format_lines(line1, sizeof(line1), line2, sizeof(line2));
  GColor bar_text = text_on(bar);
  draw_text(ctx, line1, l->line1_font, l->line1, bar_text);
  draw_text(ctx, line2, l->line2_font, l->line2, bar_text);
}

// ---------------------------------------------------------------------------
// Phone messages

static void request_refresh(void) {
  DictionaryIterator *it;
  if (app_message_outbox_begin(&it) != APP_MSG_OK) return;
  dict_write_uint8(it, MESSAGE_KEY_Refresh, 1);
  app_message_outbox_send();
}

// PebbleKit JS may send small numbers as 1, 2 or 4 byte ints.
static int32_t tuple_int(const Tuple *t) {
  bool is_signed = t->type == TUPLE_INT;
  switch (t->length) {
    case 1: return is_signed ? t->value->int8 : t->value->uint8;
    case 2: return is_signed ? t->value->int16 : t->value->uint16;
    default: return t->value->int32;
  }
}

static void read_string(DictionaryIterator *it, uint32_t key, char *dst, size_t size) {
  Tuple *t = dict_find(it, key);
  if (t && t->type == TUPLE_CSTRING) snprintf(dst, size, "%s", t->value->cstring);
}

static bool read_int(DictionaryIterator *it, uint32_t key, int32_t *out) {
  Tuple *t = dict_find(it, key);
  if (!t || t->type == TUPLE_CSTRING || t->type == TUPLE_BYTE_ARRAY) return false;
  *out = tuple_int(t);
  return true;
}

static void save_game(void) {
  persist_write_int(PERSIST_KEY_VERSION, PERSIST_VERSION);
  persist_write_data(PERSIST_KEY_GAME, &s_game, sizeof(s_game));
}

static void load_game(void) {
  s_game = (Game){ .away_score = -1, .home_score = -1 };
  if (persist_read_int(PERSIST_KEY_VERSION) == PERSIST_VERSION) {
    persist_read_data(PERSIST_KEY_GAME, &s_game, sizeof(s_game));
  }
}

static void inbox_received(DictionaryIterator *it, void *context) {
  Game old = s_game;
  int32_t n;

  read_string(it, MESSAGE_KEY_Team, s_game.team, sizeof(s_game.team));
  read_string(it, MESSAGE_KEY_Away, s_game.away, sizeof(s_game.away));
  read_string(it, MESSAGE_KEY_Home, s_game.home, sizeof(s_game.home));
  read_string(it, MESSAGE_KEY_Day, s_game.day, sizeof(s_game.day));
  read_string(it, MESSAGE_KEY_Time, s_game.time, sizeof(s_game.time));
  read_string(it, MESSAGE_KEY_Detail, s_game.detail, sizeof(s_game.detail));
  read_string(it, MESSAGE_KEY_Status, s_game.status, sizeof(s_game.status));
  if (read_int(it, MESSAGE_KEY_Mode, &n)) s_game.mode = n;
  if (read_int(it, MESSAGE_KEY_View, &n)) s_game.split = n;
  if (read_int(it, MESSAGE_KEY_BarColor, &n)) s_game.bar_style = n;
  if (read_int(it, MESSAGE_KEY_CustomColor, &n)) s_game.bar_color = n;
  if (read_int(it, MESSAGE_KEY_Battery, &n)) s_game.battery = n;
  if (read_int(it, MESSAGE_KEY_Seconds, &n)) s_game.seconds = n;
  if (read_int(it, MESSAGE_KEY_CountdownDetail, &n)) s_game.countdown = n;
  if (read_int(it, MESSAGE_KEY_State, &n)) s_game.state = n;
  if (read_int(it, MESSAGE_KEY_Start, &n)) s_game.start = n;
  if (read_int(it, MESSAGE_KEY_AwayScore, &n)) s_game.away_score = n;
  if (read_int(it, MESSAGE_KEY_HomeScore, &n)) s_game.home_score = n;
  if (read_int(it, MESSAGE_KEY_Refresh, &n) && n > 0) s_refresh_in = n;

  if (strcmp(old.team, s_game.team) != 0 || strcmp(old.away, s_game.away) != 0 ||
      strcmp(old.home, s_game.home) != 0 || old.split != s_game.split) {
    load_logos();
  }
  save_game();
  update_tick_unit();
  layer_mark_dirty(s_canvas);
}

static void inbox_dropped(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_WARNING, "Message dropped: %d", (int)reason);
}

// ---------------------------------------------------------------------------
// Events

static void tick_handler(struct tm *tick_time, TimeUnits changed) {
  if (changed & MINUTE_UNIT) {
    if (--s_refresh_in <= 0) {
      s_refresh_in = DEFAULT_REFRESH;  // until the phone answers with its own delay
      request_refresh();
    }
    update_tick_unit();  // drop back to minutes once a seconds countdown ends
  }
  layer_mark_dirty(s_canvas);
}

// Tick every second only while something on screen shows seconds: the clock
// with seconds on, or a running countdown set to seconds. Otherwise every
// minute, which is much easier on the battery.
static void update_tick_unit(void) {
  bool seconds = s_game.seconds ||
                 (s_game.countdown == COUNTDOWN_SECONDS && countdown_running());
  TimeUnits unit = seconds ? SECOND_UNIT : MINUTE_UNIT;
  if (unit == s_tick_unit) return;
  s_tick_unit = unit;
  tick_timer_service_unsubscribe();
  tick_timer_service_subscribe(unit, tick_handler);
}

static void battery_handler(BatteryChargeState state) {
  layer_mark_dirty(s_canvas);
}

static void app_connection_handler(bool connected) {
  if (connected) request_refresh();
}

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);
  set_layout(bounds);
  s_canvas = layer_create(bounds);
  layer_set_update_proc(s_canvas, canvas_update);
  layer_add_child(root, s_canvas);
}

static void window_unload(Window *window) {
  layer_destroy(s_canvas);
}

static void init(void) {
  load_game();
  load_logos();

  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers){
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);

  app_message_register_inbox_received(inbox_received);
  app_message_register_inbox_dropped(inbox_dropped);
  app_message_open(512, 64);

  update_tick_unit();
  battery_state_service_subscribe(battery_handler);
  connection_service_subscribe((ConnectionHandlers){
    .pebble_app_connection_handler = app_connection_handler,
  });
}

static void deinit(void) {
  tick_timer_service_unsubscribe();
  battery_state_service_unsubscribe();
  connection_service_unsubscribe();
  window_destroy(s_window);
  destroy_logo(&s_logo);
  destroy_logo(&s_away_logo);
  destroy_logo(&s_home_logo);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
