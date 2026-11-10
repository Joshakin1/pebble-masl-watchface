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
// start. Weather, when on, comes in its own message.
//
// The top row has three zones, left, middle and right, each holding one item
// chosen in settings: the date, the weather, the battery, Bluetooth, or the
// battery and Bluetooth together.

#include <pebble.h>

#define PERSIST_KEY_VERSION 1
#define PERSIST_KEY_GAME 2
// Bump when Game changes shape so an old saved copy is ignored.
#define PERSIST_VERSION 11

// Minutes before asking the phone again when it hasn't said.
#define DEFAULT_REFRESH 30

enum { STATE_NONE, STATE_SCHEDULED, STATE_LIVE, STATE_FINAL };
enum { MODE_START_TIME, MODE_COUNTDOWN };
enum { BAR_PRIMARY, BAR_SECONDARY, BAR_CUSTOM };
enum { BATTERY_ALWAYS, BATTERY_LOW, BATTERY_NEVER };
// How finely the countdown counts: "in 3d 4h", "in 3d 4h 12m" or
// "3d 04:12:30".
enum { COUNTDOWN_HOURS, COUNTDOWN_MINUTES, COUNTDOWN_SECONDS };
enum { BG_TEAM, BG_CUSTOM };
enum { WEATHER_OFF, WEATHER_TEMP, WEATHER_ICON, WEATHER_ICON_ONLY };
// What a top row zone holds. ITEM_STATUS is the battery and Bluetooth side by
// side. ITEM_QUIET is a bell, crossed out while Quiet Time is on. Keep in sync with
// ZONE_ITEMS in src/pkjs/index.js.
enum { ITEM_NONE, ITEM_DATE, ITEM_WEATHER, ITEM_BATTERY, ITEM_BLUETOOTH, ITEM_STATUS,
       ITEM_STEPS, ITEM_HEART_RATE, ITEM_QUIET };
enum { ZONE_LEFT, ZONE_MIDDLE, ZONE_RIGHT, NUM_ZONES };
// Where the middle zone goes on rectangular screens: with equal space to the
// left and right zones (or the screen edges where they're empty), or
// centered on the screen and moved only to stay clear of them.
enum { MIDDLE_BALANCED, MIDDLE_CENTER };
enum { BT_DISCONNECTED, BT_ALWAYS, BT_NEVER };
// Date formats, e.g. for Wednesday, November 5, 2026:
//   WED NOV 5, WED 5, NOV 5, 5 NOV, WED 11/5, WED 5/11, 11/5, 5/11, 2026-11-05
enum { DF_DAY_MONTH_DATE, DF_DAY_DATE, DF_MONTH_DATE, DF_DATE_MONTH, DF_DAY_MD, DF_DAY_DM,
       DF_MD, DF_DM, DF_ISO };
// What a date with both a weekday and a month drops when the row is full.
enum { DROP_MONTH, DROP_WEEKDAY };
// How game start times are shown: like the watch's clock, or always 12 or 24
// hour.
enum { CLOCK_WATCH, CLOCK_12H, CLOCK_24H };
// Weather icons. Keep in sync with WX_* in src/pkjs/index.js.
enum { WX_NONE, WX_SUN, WX_MOON, WX_PARTLY_SUN, WX_PARTLY_MOON, WX_CLOUD, WX_FOG,
       WX_RAIN, WX_SNOW, WX_STORM };

// Seconds sit after the time in a smaller font, bottoms lined up. Offsets
// are from the LECO fonts' line boxes; adjust here if they look off.
#define SECONDS_GAP 2
#define SECONDS_DY 21

// Battery levels, at or below: low (shown in yellow, and shown at all with the
// "when low" setting) and critical (shown in red).
#define BATTERY_LOW_PERCENT 40
#define BATTERY_CRITICAL_PERCENT 20

// Space between the top row's zones, between the battery and Bluetooth when
// they share one, and between the weather icon and the temperature.
#define ZONE_GAP 4
#define STATUS_GAP 3
#define ICON_GAP 2
// Weather older than this many seconds is hidden: the phone sends it every
// 30 minutes, so this means it has lost touch.
#define WEATHER_STALE (2 * 3600)

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
  uint8_t bg_style;    // BG_*: the selected team's background
  int32_t bg_color;    // hex color for BG_CUSTOM
  uint8_t weather;     // WEATHER_*: what weather to show
  uint8_t wx_icon;     // WX_*
  int16_t temp;        // in the units chosen in settings
  int32_t weather_time;  // when the weather arrived, 0 if never
  uint8_t zones[NUM_ZONES];  // ITEM_* in each top row zone
  uint8_t middle_align;  // MIDDLE_*
  uint8_t bluetooth;     // BT_*: when to show Bluetooth
  uint8_t date_format;   // DF_*
  uint8_t date_drop;     // DROP_*
  uint8_t game_clock;    // CLOCK_*: start times in the game bar
  uint8_t live_hr;       // 1 to read heart rate every minute during the team's live games
  uint8_t live_hr_zone;  // 0 for off, or 1 + the ZONE_* that shows heart rate during them
  uint8_t battery_zone;  // 0 for off, or 1 + the ZONE_* that shows the battery when low
  uint8_t bt_zone;       // 0 for off, or 1 + the ZONE_* that shows Bluetooth when disconnected
  uint8_t quiet_zone;    // 0 for off, or 1 + the ZONE_* that shows Quiet Time while it's on
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
//
// On rectangular screens the top row's zones sit against the left edge, in
// the middle and against the right edge. Round ones have no corners, so
// there the zones are packed together, in order, and centered.
typedef struct {
  GRect date, logo, time, line1, line2;
  GSize battery;
  int16_t bar_y;
  int16_t half_dx;  // distance from the center to each half logo's center
  bool round;
  int16_t edge;     // rectangular: space between the top row and the sides
  int16_t row_w;    // round: how wide the top row can be without clipping
  int16_t row_cy;   // middle of the icons in the top row
  int16_t wx_size;  // weather icon's width and height
  int16_t temp_y;   // top of the temperature's text box
  GFont date_font, time_font, line1_font, line2_font, temp_font;
} Layout;

static Window *s_window;
static Layer *s_canvas;
static Layout s_base;    // the layout with the whole screen in view
static Layout s_layout;  // s_base moved for quick view, used for drawing
static Game s_game;
static GBitmap *s_logo, *s_away_logo, *s_home_logo;
static int s_refresh_in = DEFAULT_REFRESH;
static bool s_hr_fast;  // whether this face has asked for one-minute heart rate readings
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
// A team's background color, or black for a team not in s_teams. The
// selected team uses the custom color from settings when chosen.
static GColor team_bg(const Team *t) {
  if (!t) return GColorBlack;
  if (s_game.bg_style == BG_CUSTOM && strcmp(t->name, s_game.team) == 0) {
    return GColorFromHEX(s_game.bg_color);
  }
  return GColorFromHEX(t->bg);
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
  Layout *l = &s_base;
  if (w >= 260) {         // gabbro, 260x260 round
    *l = (Layout){ .date = GRect(0, 20, w, 30), .logo = GRect(0, 50, w, 110),
                   .time = GRect(0, 152, w, 52), .bar_y = 206,
                   .line1 = GRect(40, 203, w - 80, 22), .line2 = GRect(40, 220, w - 80, 30),
                   .half_dx = 56, .battery = GSize(20, 10),
                   .round = true, .row_w = 168, .row_cy = 36, .wx_size = 16, .temp_y = 20 };
  } else if (w >= 200) {  // emery, 200x228
    *l = (Layout){ .date = GRect(0, -4, w, 30), .logo = GRect(0, 26, w, 106),
                   .time = GRect(0, 124, w, 52), .bar_y = 182,
                   .line1 = GRect(4, 179, w - 8, 22), .line2 = GRect(4, 196, w - 8, 30),
                   .half_dx = 50, .battery = GSize(20, 10),
                   .edge = 6, .row_cy = 12, .wx_size = 16, .temp_y = 0 };
  } else if (w >= 180) {  // chalk, 180x180 round
    *l = (Layout){ .date = GRect(0, 10, w, 22), .logo = GRect(0, 31, w, 68),
                   .time = GRect(0, 88, w, 50), .bar_y = 140,
                   .line1 = GRect(25, 137, w - 50, 16), .line2 = GRect(25, 150, w - 50, 22),
                   .half_dx = 38, .battery = GSize(16, 8),
                   .round = true, .row_w = 110, .row_cy = 22, .wx_size = 12, .temp_y = 10 };
  } else {                // basalt, diorite, flint, 144x168
    *l = (Layout){ .date = GRect(0, -3, w, 22), .logo = GRect(0, 20, w, 74),
                   .time = GRect(0, 84, w, 50), .bar_y = 136,
                   .line1 = GRect(2, 133, w - 4, 16), .line2 = GRect(2, 146, w - 4, 22),
                   .half_dx = 36, .battery = GSize(16, 8),
                   .edge = 3, .row_cy = 8, .wx_size = 12, .temp_y = -2 };
  }
  bool large = w >= 200;
  l->date_font = fonts_get_system_font(large ? FONT_KEY_GOTHIC_24_BOLD : FONT_KEY_GOTHIC_18_BOLD);
  l->time_font = fonts_get_system_font(FONT_KEY_LECO_42_NUMBERS);
  l->line1_font = fonts_get_system_font(large ? FONT_KEY_GOTHIC_18_BOLD : FONT_KEY_GOTHIC_14_BOLD);
  l->line2_font = fonts_get_system_font(large ? FONT_KEY_GOTHIC_24_BOLD : FONT_KEY_GOTHIC_18_BOLD);
  // In the date line on round screens, so it matches the date there.
  l->temp_font = l->round ? l->date_font
                          : fonts_get_system_font(large ? FONT_KEY_GOTHIC_18_BOLD
                                                        : FONT_KEY_GOTHIC_14_BOLD);
  s_layout = *l;
}

// Quick view (the timeline peek) covers the bottom of the screen. While it's
// up, the time and game bar move up by the covered height so both stay in
// view, and the logo, which no longer fits between the date and the time, is
// hidden. The same on every watch. Returns whether the logo is shown.
static bool fit_layout(GRect full, GRect visible) {
  s_layout = s_base;
  int16_t covered = full.size.h - (visible.origin.y + visible.size.h);
  if (covered <= 0) return true;
  s_layout.time.origin.y -= covered;
  s_layout.bar_y -= covered;
  s_layout.line1.origin.y -= covered;
  s_layout.line2.origin.y -= covered;
  return false;
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

// The start time, in 12 or 24 hour per settings. The phone always sends 12
// hour ("7:05pm EST"); in 24 hour that becomes "19:05 EST". Anything that
// doesn't look like a time is left alone.
static void format_start(char *buf, size_t size, const char *time) {
  bool h24 = s_game.game_clock == CLOCK_24H ||
             (s_game.game_clock == CLOCK_WATCH && clock_is_24h_style());
  const char *p = time;
  int h = 0, digits = 0;
  while (*p >= '0' && *p <= '9' && digits < 2) {
    h = h * 10 + (*p++ - '0');
    digits++;
  }
  bool ok = h24 && digits && p[0] == ':' && p[1] >= '0' && p[1] <= '9' &&
            p[2] >= '0' && p[2] <= '9';
  if (!ok) {
    snprintf(buf, size, "%s", time);
    return;
  }
  int m = (p[1] - '0') * 10 + (p[2] - '0');
  p += 3;
  while (*p == ' ') p++;
  char ap = *p | 0x20;  // lowercase
  if ((ap != 'a' && ap != 'p') || (p[1] | 0x20) != 'm') {
    snprintf(buf, size, "%s", time);
    return;
  }
  h = h % 12 + (ap == 'p' ? 12 : 0);
  snprintf(buf, size, "%02d:%02d%s", h, m, p + 2);
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
        format_start(l2, n2, g->time);
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

// The battery's fill color: green while charging, then red when critical and
// yellow when low (color watches).
static GColor battery_color(BatteryChargeState b, GColor fg) {
#ifdef PBL_COLOR
  if (b.is_charging) return GColorGreen;
  if (b.charge_percent <= BATTERY_CRITICAL_PERCENT) return GColorRed;
  if (b.charge_percent <= BATTERY_LOW_PERCENT) return GColorYellow;
#endif
  return fg;
}

static bool battery_shown(BatteryChargeState b) {
  bool low = b.charge_percent <= BATTERY_LOW_PERCENT;
  return s_game.battery == BATTERY_ALWAYS ||
         (s_game.battery == BATTERY_LOW && (low || b.is_charging));
}

static bool bluetooth_shown(bool connected) {
  return s_game.bluetooth == BT_ALWAYS || (s_game.bluetooth == BT_DISCONNECTED && !connected);
}

// The Bluetooth rune, about half as wide as it is tall. While the phone isn't
// connected it has a slash through it, and is red on color watches.
static void draw_bluetooth(GContext *ctx, GRect r, bool connected, GColor fg) {
  int16_t x0 = r.origin.x, x1 = r.origin.x + r.size.w - 1, xm = (x0 + x1) / 2;
  int16_t y0 = r.origin.y, y1 = r.origin.y + r.size.h - 1, q = r.size.h / 4;
  graphics_context_set_stroke_color(ctx, connected ? fg : PBL_IF_COLOR_ELSE(GColorRed, fg));
  graphics_draw_line(ctx, GPoint(xm, y0), GPoint(xm, y1));
  graphics_draw_line(ctx, GPoint(xm, y0), GPoint(x1, y0 + q));
  graphics_draw_line(ctx, GPoint(x1, y0 + q), GPoint(x0, y1 - q));
  graphics_draw_line(ctx, GPoint(x0, y0 + q), GPoint(x1, y1 - q));
  graphics_draw_line(ctx, GPoint(x1, y1 - q), GPoint(xm, y1));
  if (!connected) {
    graphics_draw_line(ctx, GPoint(x0 - 1, y0), GPoint(x1 + 1, y1));
  }
}

// A bell for Quiet Time: a round top, straight sides, a wider rim and a
// clapper. While Quiet Time is on it has a slash through it, red on color
// watches, with a gap in the background color so it reads at this size.
static void draw_quiet(GContext *ctx, GRect r, bool on, GColor fg, GColor bg) {
  int16_t x0 = r.origin.x, x1 = r.origin.x + r.size.w - 1, xm = (x0 + x1) / 2;
  int16_t y0 = r.origin.y, y1 = r.origin.y + r.size.h - 1;
  int16_t rad = (r.size.w - 4) / 2;
  int16_t cy = y0 + 1 + rad;
  graphics_context_set_fill_color(ctx, fg);
  graphics_fill_rect(ctx, GRect(xm, y0, 1, 1), 0, GCornerNone);  // the knob on top
  graphics_fill_circle(ctx, GPoint(xm, cy), rad);
  graphics_fill_rect(ctx, GRect(xm - rad, cy, 2 * rad + 1, y1 - 2 - cy), 0, GCornerNone);
  graphics_fill_rect(ctx, GRect(x0, y1 - 3, x1 - x0 + 1, 2), 0, GCornerNone);
  graphics_fill_rect(ctx, GRect(xm - 1, y1, 3, 1), 0, GCornerNone);
  if (!on) return;
  graphics_context_set_stroke_width(ctx, 3);
  graphics_context_set_stroke_color(ctx, bg);
  graphics_draw_line(ctx, GPoint(x0, y0 + 1), GPoint(x1, y1 - 1));
  graphics_context_set_stroke_width(ctx, 1);
  graphics_context_set_stroke_color(ctx, PBL_IF_COLOR_ELSE(GColorRed, fg));
  graphics_draw_line(ctx, GPoint(x0, y0 + 1), GPoint(x1, y1 - 1));
}

// A battery outline with a nub on the right, filled to the charge level.
static void draw_battery(GContext *ctx, GRect r, BatteryChargeState b, GColor fg) {
  GRect body = GRect(r.origin.x, r.origin.y, r.size.w - 2, r.size.h);
  graphics_context_set_stroke_color(ctx, fg);
  graphics_draw_rect(ctx, body);
  graphics_context_set_fill_color(ctx, fg);
  graphics_fill_rect(ctx, GRect(body.origin.x + body.size.w, r.origin.y + r.size.h / 4, 2,
                                r.size.h - r.size.h / 4 * 2), 0, GCornerNone);
  int fill = (body.size.w - 4) * b.charge_percent / 100;
  graphics_context_set_fill_color(ctx, battery_color(b, fg));
  graphics_fill_rect(ctx, GRect(body.origin.x + 2, body.origin.y + 2, fill > 0 ? fill : 1,
                                body.size.h - 4), 0, GCornerNone);
}

// ---------------------------------------------------------------------------
// Weather. The icons are drawn rather than stored as images so they work in
// any color on every screen. Each fits in a square box `s` pixels wide.

static bool icon_shown(void) {
  return (s_game.weather == WEATHER_ICON || s_game.weather == WEATHER_ICON_ONLY) &&
         s_game.wx_icon != WX_NONE;
}

static bool temp_shown(void) {
  return s_game.weather == WEATHER_TEMP || s_game.weather == WEATHER_ICON;
}

static bool weather_shown(void) {
  return (temp_shown() || icon_shown()) && s_game.weather_time &&
         time(NULL) - s_game.weather_time < WEATHER_STALE;
}

// A sun: a disc of radius r with eight rays reaching out to `reach`.
static void draw_sun(GContext *ctx, GPoint c, int16_t r, int16_t reach) {
  // Ray directions, in tenths.
  static const int8_t dirs[8][2] = {
    { 10, 0 }, { 7, 7 }, { 0, 10 }, { -7, 7 }, { -10, 0 }, { -7, -7 }, { 0, -10 }, { 7, -7 },
  };
  graphics_fill_circle(ctx, c, r);
  int16_t in = r + 2;
  for (int i = 0; i < 8; i++) {
    graphics_draw_line(ctx, GPoint(c.x + dirs[i][0] * in / 10, c.y + dirs[i][1] * in / 10),
                       GPoint(c.x + dirs[i][0] * reach / 10, c.y + dirs[i][1] * reach / 10));
  }
}

// A crescent: a disc with a background-colored disc taken out of its upper
// right.
static void draw_moon(GContext *ctx, GPoint c, int16_t r, GColor fg, GColor bg) {
  graphics_context_set_fill_color(ctx, fg);
  graphics_fill_circle(ctx, c, r);
  graphics_context_set_fill_color(ctx, bg);
  graphics_fill_circle(ctx, GPoint(c.x + r / 2 + 1, c.y - r / 3), r - 1);
  graphics_context_set_fill_color(ctx, fg);
}

// A cloud filling r: a flat-bottomed base with a big lump and a small one.
static void fill_cloud(GContext *ctx, GRect r) {
  int16_t x = r.origin.x, y = r.origin.y, w = r.size.w, h = r.size.h;
  int16_t base_h = h - h / 2;
  graphics_fill_rect(ctx, GRect(x, y + h / 2, w, base_h), base_h / 2, GCornersAll);
  graphics_fill_circle(ctx, GPoint(x + w * 3 / 5, y + h / 2), (h - 1) / 2);
  graphics_fill_circle(ctx, GPoint(x + w * 3 / 10, y + h * 3 / 5), h * 3 / 10);
}

// A small sun or moon peeking out from behind a cloud. The cloud gets a
// background-colored edge so the two don't run together.
static void draw_partly(GContext *ctx, int16_t x, int16_t y, int16_t s, bool night,
                        GColor fg, GColor bg) {
  GPoint c = GPoint(x + s * 2 / 3, y + s / 3);
  if (night) {
    draw_moon(ctx, c, s / 3, fg, bg);
  } else {
    draw_sun(ctx, c, s / 5, s / 3 + 1);
  }
  GRect cloud = GRect(x, y + s * 5 / 12, s * 5 / 6, s - s * 5 / 12);
  graphics_context_set_fill_color(ctx, bg);
  fill_cloud(ctx, grect_inset(cloud, GEdgeInsets(-1)));
  graphics_context_set_fill_color(ctx, fg);
  fill_cloud(ctx, cloud);
}

static void draw_weather_icon(GContext *ctx, int16_t x, int16_t y, int16_t s, GColor fg,
                              GColor bg) {
  graphics_context_set_fill_color(ctx, fg);
  graphics_context_set_stroke_color(ctx, fg);
  // Rain and snow hang below a cloud in the top part of the box.
  int16_t cloud_h = s * 7 / 12;
  int16_t top = y + cloud_h + 1, bottom = y + s - 1;
  switch (s_game.wx_icon) {
    case WX_SUN:
      draw_sun(ctx, GPoint(x + s / 2, y + s / 2), s / 5, s / 2 - 1);
      break;
    case WX_MOON:
      draw_moon(ctx, GPoint(x + s / 2, y + s / 2), s / 2 - 1, fg, bg);
      break;
    case WX_PARTLY_SUN:
    case WX_PARTLY_MOON:
      draw_partly(ctx, x, y, s, s_game.wx_icon == WX_PARTLY_MOON, fg, bg);
      break;
    case WX_CLOUD:
      fill_cloud(ctx, GRect(x, y + s / 6, s, s * 2 / 3));
      break;
    case WX_FOG: {
      int16_t thick = s / 8 > 1 ? s / 8 : 1;
      for (int i = 1; i <= 3; i++) {
        graphics_fill_rect(ctx, GRect(x + (i == 2 ? 2 : 0), y + s * i / 4 - 1, s - 2, thick),
                           0, GCornerNone);
      }
      break;
    }
    case WX_RAIN:
    case WX_SNOW:
      fill_cloud(ctx, GRect(x, y, s, cloud_h));
      for (int i = 1; i <= 3; i++) {
        int16_t dx = x + s * i / 4;
        if (s_game.wx_icon == WX_RAIN) {
          graphics_draw_line(ctx, GPoint(dx, top), GPoint(dx - s / 8, bottom));
        } else {
          graphics_fill_rect(ctx, GRect(dx - 1, i == 2 ? bottom - 1 : top, 2, 2), 0, GCornerNone);
        }
      }
      break;
    case WX_STORM: {
      // A shorter cloud, to leave room for a two pixel wide bolt.
      fill_cloud(ctx, GRect(x, y, s, s / 2));
      top = y + s / 2 + 1;
      int16_t mid = (top + bottom) / 2;
      for (int16_t cx = x + s / 2; cx <= x + s / 2 + 1; cx++) {
        graphics_draw_line(ctx, GPoint(cx + 1, top), GPoint(cx - 1, mid));
        graphics_draw_line(ctx, GPoint(cx - 1, mid), GPoint(cx + 1, mid));
        graphics_draw_line(ctx, GPoint(cx + 1, mid), GPoint(cx - 1, bottom));
      }
      break;
    }
  }
}

static int16_t text_width(const char *text, GFont font) {
  return graphics_text_layout_get_content_size(text, font, GRect(0, 0, 300, 60),
                                               GTextOverflowModeFill, GTextAlignmentLeft).w;
}

// Width of the weather: the icon and the temperature, whichever are on.
static int16_t weather_width(const char *temp) {
  const Layout *l = &s_layout;
  int16_t w = icon_shown() ? l->wx_size : 0;
  if (temp_shown()) w += (w ? ICON_GAP : 0) + text_width(temp, l->temp_font);
  return w;
}

// The weather with its left edge at x, w wide (from weather_width). bg is the
// background behind it, for the icons that cut shapes out.
static void draw_weather(GContext *ctx, int16_t x, int16_t w, const char *temp, GColor fg,
                         GColor bg) {
  const Layout *l = &s_layout;
  if (icon_shown()) {
    draw_weather_icon(ctx, x, l->row_cy - l->wx_size / 2, l->wx_size, fg, bg);
    x += l->wx_size + ICON_GAP;
    w -= l->wx_size + ICON_GAP;
  }
  if (!temp_shown()) return;
  graphics_context_set_text_color(ctx, fg);
  graphics_draw_text(ctx, temp, l->temp_font,
                     GRect(x, l->temp_y, w + 4, l->date.size.h),
                     GTextOverflowModeFill, GTextAlignmentLeft, NULL);
}

// ---------------------------------------------------------------------------
// Top row

// What the top row shows this time round. Items that are off or hidden take
// no space.
typedef struct {
  char date[16], temp[8];
  const char *steps, *heart;  // "" when not available or not in the row
  bool battery, bluetooth, weather;
  bool quiet;  // Quiet Time is on
  uint8_t zones[NUM_ZONES];  // ITEM_* in each zone this time round
  // Read once per redraw and shared by everything in the row.
  BatteryChargeState charge;
  bool connected;
} Row;

// ---------------------------------------------------------------------------
// Health. Steps today and the latest heart rate, from the watch's health
// service. Hidden where there's no data, e.g. heart rate on a watch with no
// sensor. Each is read at most once a minute and kept until the next minute
// tick, so redraws every second (with seconds on) don't keep asking.

static char s_steps[8], s_heart[8];
static bool s_steps_read, s_heart_read;

// Called on each minute tick so the next redraw reads fresh numbers.
static void expire_health(void) {
  s_steps_read = s_heart_read = false;
}

#if defined(PBL_HEALTH)
static bool health_available(HealthMetric metric) {
  return health_service_metric_accessible(metric, time_start_of_today(), time(NULL)) &
         HealthServiceAccessibilityMaskAvailable;
}
#endif

// "8432", or "12.3k" and "123k" once it gets long.
static void format_steps(char *buf, size_t size) {
  buf[0] = '\0';
#if defined(PBL_HEALTH)
  if (!health_available(HealthMetricStepCount)) return;
  int n = (int)health_service_sum_today(HealthMetricStepCount);
  if (n < 10000) {
    snprintf(buf, size, "%d", n);
  } else if (n < 100000) {
    snprintf(buf, size, "%d.%dk", n / 1000, n / 100 % 10);
  } else {
    snprintf(buf, size, "%dk", n / 1000);
  }
#endif
}

static void format_heart(char *buf, size_t size) {
  buf[0] = '\0';
#if defined(PBL_HEALTH)
  if (!health_available(HealthMetricHeartRateBPM)) return;
  int bpm = (int)health_service_peek_current_value(HealthMetricHeartRateBPM);
  if (bpm > 0) snprintf(buf, size, "%d", bpm);
#endif
}

static const char *steps_text(void) {
  if (!s_steps_read) {
    format_steps(s_steps, sizeof(s_steps));
    s_steps_read = true;
  }
  return s_steps;
}

static const char *heart_text(void) {
  if (!s_heart_read) {
    format_heart(s_heart, sizeof(s_heart));
    s_heart_read = true;
  }
  return s_heart;
}

// Seconds between heart rate readings while the selected team's game is live
// and that setting is on.
#define LIVE_HR_PERIOD 60

// Ask for a heart rate reading every minute while the selected team's game is
// live (if chosen in settings), and hand back to the watch's own setting
// otherwise. The watch treats the request as a suggestion. On a watch with no
// sensor the request fails and nothing changes.
static void update_heart_rate_sampling(void) {
#if defined(PBL_HEALTH)
  bool want = s_game.live_hr && s_game.team[0] && s_game.state == STATE_LIVE;
  if (want == s_hr_fast) return;
  if (health_service_set_heart_rate_sample_period(want ? LIVE_HR_PERIOD : 0)) {
    s_hr_fast = want;
  }
#endif
}

// Cancel the request when the face closes, so it doesn't keep the sensor
// busy afterwards.
static void stop_heart_rate_sampling(void) {
#if defined(PBL_HEALTH)
  if (s_hr_fast) health_service_set_heart_rate_sample_period(0);
  s_hr_fast = false;
#endif
}

// Steps and heart rate icons fit in a square box this many pixels wide.
static int16_t health_icon_size(void) { return s_layout.wx_size - 2; }

// Two footprints, the left one a little higher.
static void draw_steps_icon(GContext *ctx, int16_t x, int16_t y, int16_t s, GColor fg) {
  int16_t fw = s * 2 / 5, sole = s / 2, heel = s / 5 > 1 ? s / 5 : 1;
  int16_t foot = sole + 1 + heel;
  graphics_context_set_fill_color(ctx, fg);
  for (int i = 0; i < 2; i++) {
    int16_t fx = i ? x + s - fw : x, fy = i ? y + s - foot : y;
    graphics_fill_rect(ctx, GRect(fx, fy, fw, sole), fw / 2, GCornersAll);
    graphics_fill_rect(ctx, GRect(fx, fy + sole + 1, fw, heel), 0, GCornerNone);
  }
}

// A heart: two circles over a triangle narrowing to a point at the bottom.
static void draw_heart_icon(GContext *ctx, int16_t x, int16_t y, int16_t s, GColor fg) {
  int16_t r = (s + 2) / 4, cx = x + (s - 1) / 2;
  graphics_context_set_fill_color(ctx, fg);
  graphics_context_set_stroke_color(ctx, fg);
  graphics_fill_circle(ctx, GPoint(x + r, y + r), r);
  graphics_fill_circle(ctx, GPoint(x + s - 1 - r, y + r), r);
  int16_t top = y + r, bottom = y + s - 1, rows = bottom - top;
  for (int16_t yy = top; yy <= bottom; yy++) {
    int16_t half = rows ? (s / 2) * (bottom - yy) / rows : 0;
    graphics_draw_line(ctx, GPoint(cx - half, yy), GPoint(x + s - 1 - (cx - x) + half, yy));
  }
}

// Width of an icon followed by its number, or 0 when there's no number.
static int16_t health_width(const char *text) {
  if (!text[0]) return 0;
  return health_icon_size() + ICON_GAP + text_width(text, s_layout.temp_font);
}

// An icon and its number with the left edge at x, w wide (from health_width).
static void draw_health(GContext *ctx, uint8_t item, int16_t x, int16_t w, const char *text,
                        GColor fg) {
  const Layout *l = &s_layout;
  int16_t s = health_icon_size(), y = l->row_cy - s / 2;
  if (item == ITEM_STEPS) {
    draw_steps_icon(ctx, x, y, s, fg);
  } else {
    draw_heart_icon(ctx, x, y, s, fg);
  }
  graphics_context_set_text_color(ctx, fg);
  graphics_draw_text(ctx, text, l->temp_font,
                     GRect(x + s + ICON_GAP, l->temp_y, w - s - ICON_GAP + 4, l->date.size.h),
                     GTextOverflowModeFill, GTextAlignmentLeft, NULL);
}

// The date in the chosen format. `shorter` drops the weekday or the month
// (per settings) from formats that have both, and the year from ISO.
static void format_date(char *buf, size_t size, const struct tm *t, bool shorter) {
  char wd[8], mon[8];
  strftime(wd, sizeof(wd), "%a", t);
  strftime(mon, sizeof(mon), "%b", t);
  int d = t->tm_mday, m = t->tm_mon + 1;
  bool no_wd = shorter && s_game.date_drop == DROP_WEEKDAY;
  bool no_mon = shorter && !no_wd;
  switch (s_game.date_format) {
    case DF_DAY_DATE:   snprintf(buf, size, "%s %d", wd, d); break;
    case DF_MONTH_DATE: snprintf(buf, size, "%s %d", mon, d); break;
    case DF_DATE_MONTH: snprintf(buf, size, "%d %s", d, mon); break;
    case DF_MD:         snprintf(buf, size, "%d/%d", m, d); break;
    case DF_DM:         snprintf(buf, size, "%d/%d", d, m); break;
    case DF_DAY_MD:
    case DF_DAY_DM: {
      bool md = s_game.date_format == DF_DAY_MD;
      if (no_wd) {
        snprintf(buf, size, "%d/%d", md ? m : d, md ? d : m);
      } else if (no_mon) {
        snprintf(buf, size, "%s %d", wd, d);
      } else {
        snprintf(buf, size, "%s %d/%d", wd, md ? m : d, md ? d : m);
      }
      break;
    }
    case DF_ISO:
      if (shorter) {
        snprintf(buf, size, "%02d-%02d", m, d);
      } else {
        snprintf(buf, size, "%d-%02d-%02d", t->tm_year + 1900, m, d);
      }
      break;
    default:  // DF_DAY_MONTH_DATE
      if (no_wd) {
        snprintf(buf, size, "%s %d", mon, d);
      } else if (no_mon) {
        snprintf(buf, size, "%s %d", wd, d);
      } else {
        snprintf(buf, size, "%s %s %d", wd, mon, d);
      }
      break;
  }
  upcase(buf);
}

static int16_t bluetooth_h(void) { return s_layout.battery.h + 2; }
static int16_t bluetooth_w(void) { return bluetooth_h() / 2 + 1; }
static int16_t quiet_size(void) { return bluetooth_h(); }

static int16_t item_width(const Row *r, uint8_t item) {
  const Layout *l = &s_layout;
  switch (item) {
    case ITEM_DATE: return text_width(r->date, l->date_font);
    case ITEM_WEATHER: return r->weather ? weather_width(r->temp) : 0;
    case ITEM_BATTERY: return r->battery ? l->battery.w : 0;
    case ITEM_BLUETOOTH: return r->bluetooth ? bluetooth_w() : 0;
    case ITEM_STATUS: {
      int16_t bat = item_width(r, ITEM_BATTERY), bt = item_width(r, ITEM_BLUETOOTH);
      return bat && bt ? bat + STATUS_GAP + bt : bat + bt;
    }
    case ITEM_STEPS: return health_width(r->steps);
    case ITEM_HEART_RATE: return health_width(r->heart);
    case ITEM_QUIET: return quiet_size();
  }
  return 0;
}

// Draw an item with its left edge at x. w is its width from item_width, so
// text isn't measured twice. bg is the background behind it.
static void draw_item(GContext *ctx, const Row *r, uint8_t item, int16_t x, int16_t w,
                      GColor fg, GColor bg) {
  const Layout *l = &s_layout;
  switch (item) {
    case ITEM_DATE:
      graphics_context_set_text_color(ctx, fg);
      graphics_draw_text(ctx, r->date, l->date_font,
                         GRect(x, l->date.origin.y, w + 4, l->date.size.h),
                         GTextOverflowModeFill, GTextAlignmentLeft, NULL);
      break;
    case ITEM_WEATHER:
      if (r->weather) draw_weather(ctx, x, w, r->temp, fg, bg);
      break;
    case ITEM_BATTERY:
      if (r->battery) {
        draw_battery(ctx, GRect(x, l->row_cy - l->battery.h / 2, l->battery.w, l->battery.h),
                     r->charge, fg);
      }
      break;
    case ITEM_BLUETOOTH:
      if (r->bluetooth) {
        draw_bluetooth(ctx, GRect(x, l->row_cy - bluetooth_h() / 2, bluetooth_w(), bluetooth_h()),
                       r->connected, fg);
      }
      break;
    case ITEM_STATUS:  // battery and Bluetooth have fixed sizes, so no width needed
      draw_item(ctx, r, ITEM_BATTERY, x, 0, fg, bg);
      if (r->battery) x += l->battery.w + STATUS_GAP;
      draw_item(ctx, r, ITEM_BLUETOOTH, x, 0, fg, bg);
      break;
    case ITEM_STEPS:
      if (r->steps[0]) draw_health(ctx, item, x, w, r->steps, fg);
      break;
    case ITEM_HEART_RATE:
      if (r->heart[0]) draw_health(ctx, item, x, w, r->heart, fg);
      break;
    case ITEM_QUIET:
      draw_quiet(ctx, GRect(x, l->row_cy - quiet_size() / 2, quiet_size(), quiet_size()),
                 r->quiet, fg, bg);
      break;
  }
}

// Work out each zone's width (0 when empty) and left edge on a screen w
// wide. Returns false when they don't all fit.
static bool place_row(const Row *r, int16_t w, int16_t xs[], int16_t ws[]) {
  const Layout *l = &s_layout;
  int16_t total = 0;
  int shown = 0;
  for (int i = 0; i < NUM_ZONES; i++) {
    ws[i] = item_width(r, r->zones[i]);
    if (ws[i]) {
      total += ws[i];
      shown++;
    }
  }
  if (shown > 1) total += (shown - 1) * ZONE_GAP;

  if (l->round) {
    int16_t x = (w - total) / 2;
    for (int i = 0; i < NUM_ZONES; i++) {
      xs[i] = x;
      if (ws[i]) x += ws[i] + ZONE_GAP;
    }
    return total <= l->row_w;
  }

  xs[ZONE_LEFT] = l->edge;
  xs[ZONE_RIGHT] = w - l->edge - ws[ZONE_RIGHT];
  // The space the middle zone has, from the left zone to the right one, or
  // to the screen edge (less the same margin) where one is empty.
  int16_t left = l->edge + ws[ZONE_LEFT], right = w - l->edge - ws[ZONE_RIGHT];
  int16_t mw = ws[ZONE_MIDDLE];
  int16_t x = s_game.middle_align == MIDDLE_BALANCED ? (left + right - mw) / 2 : (w - mw) / 2;
  int16_t lo = ws[ZONE_LEFT] ? left + ZONE_GAP : left;
  int16_t hi = (ws[ZONE_RIGHT] ? right - ZONE_GAP : right) - mw;
  if (x > hi) x = hi;
  if (x < lo) x = lo;
  xs[ZONE_MIDDLE] = x;
  return total <= w - 2 * l->edge;
}

static bool item_in_row(const Row *r, uint8_t item) {
  for (int i = 0; i < NUM_ZONES; i++) {
    if (r->zones[i] == item) return true;
  }
  return false;
}

// Put item in zone (a 1-based setting, 0 for off) unless that spot was
// already taken this time round. Bluetooth and the battery share a spot when
// both want the same one.
static void take_zone(Row *r, uint8_t item, uint8_t zone, bool taken[]) {
  if (!zone || zone > NUM_ZONES) return;
  int i = zone - 1;
  if (!taken[i]) {
    r->zones[i] = item;
    taken[i] = true;
  } else if (item == ITEM_BATTERY && r->zones[i] == ITEM_BLUETOOTH) {
    r->zones[i] = ITEM_STATUS;
  }
}

// Spots that change with what's going on, as chosen in settings, in order of
// importance: Bluetooth while the phone is disconnected, the battery while
// it's low, heart rate while the selected team's game is live, and Quiet Time
// while it's on. Each only takes its spot when it isn't already showing in
// another one.
static void swap_zones(Row *r) {
  bool taken[NUM_ZONES] = { false };
  if (s_game.bt_zone && !r->connected) {
    r->bluetooth = true;
    if (!item_in_row(r, ITEM_BLUETOOTH) && !item_in_row(r, ITEM_STATUS)) {
      take_zone(r, ITEM_BLUETOOTH, s_game.bt_zone, taken);
    }
  }
  if (s_game.battery_zone && r->charge.charge_percent <= BATTERY_LOW_PERCENT) {
    r->battery = true;
    if (!item_in_row(r, ITEM_BATTERY) && !item_in_row(r, ITEM_STATUS)) {
      take_zone(r, ITEM_BATTERY, s_game.battery_zone, taken);
    }
  }
  if (s_game.live_hr_zone && s_game.team[0] && s_game.state == STATE_LIVE &&
      !item_in_row(r, ITEM_HEART_RATE)) {
    r->heart = heart_text();
    if (r->heart[0]) take_zone(r, ITEM_HEART_RATE, s_game.live_hr_zone, taken);
  }
  if (s_game.quiet_zone && r->quiet && !item_in_row(r, ITEM_QUIET)) {
    take_zone(r, ITEM_QUIET, s_game.quiet_zone, taken);
  }
}

// The top row, shortening the date when it doesn't fit. left_bg and right_bg
// are the background on each half of the screen.
static void draw_row(GContext *ctx, const struct tm *t, int16_t w, GColor fg, GColor left_bg,
                     GColor right_bg) {
  Row row = { .steps = "", .heart = "", .weather = weather_shown(),
              .charge = battery_state_service_peek(),
              .connected = connection_service_peek_pebble_app_connection() };
  row.battery = battery_shown(row.charge);
  row.bluetooth = bluetooth_shown(row.connected);
  // There's no event for Quiet Time turning on or off, so it's checked on
  // each redraw, at least once a minute.
  row.quiet = quiet_time_is_active();
  if (row.weather) {
    snprintf(row.temp, sizeof(row.temp), "%d\xc2\xb0", s_game.temp);  // degree sign in UTF-8
  }
  memcpy(row.zones, s_game.zones, sizeof(row.zones));
  swap_zones(&row);
  // Health is only read when it's in the row.
  for (int i = 0; i < NUM_ZONES; i++) {
    if (row.zones[i] == ITEM_STEPS) row.steps = steps_text();
    if (row.zones[i] == ITEM_HEART_RATE) row.heart = heart_text();
  }
  format_date(row.date, sizeof(row.date), t, false);
  int16_t xs[NUM_ZONES], ws[NUM_ZONES];
  if (!place_row(&row, w, xs, ws) && item_in_row(&row, ITEM_DATE)) {
    format_date(row.date, sizeof(row.date), t, true);
    place_row(&row, w, xs, ws);
  }
  for (int i = 0; i < NUM_ZONES; i++) {
    if (!ws[i]) continue;
    GColor bg = xs[i] + ws[i] / 2 < w / 2 ? left_bg : right_bg;
    draw_item(ctx, &row, row.zones[i], xs[i], ws[i], fg, bg);
  }
}

static void canvas_update(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  bool logo_shown = fit_layout(b, layer_get_unobstructed_bounds(layer));
  const Layout *l = &s_layout;
  bool matchup = split_view();
  int16_t cx = b.size.w / 2;

  // Background: the team's color, or split down the middle for a matchup.
  // The bar color comes from bar_color().
#ifdef PBL_COLOR
  const Team *team = find_team(s_game.team);
  GColor left_bg = team_bg(matchup ? find_team(s_game.away) : team);
  GColor right_bg = matchup ? team_bg(find_team(s_game.home)) : left_bg;
  // Text on the background is white unless both sides are light enough for
  // black (only possible with a custom background).
  GColor fg = text_on(left_bg);
  if (!gcolor_equal(fg, text_on(right_bg))) fg = GColorWhite;
  GColor bar = bar_color(team);
#else
  GColor left_bg = GColorWhite, right_bg = GColorWhite, fg = GColorBlack;
  GColor bar = bar_color(NULL);
#endif
  graphics_context_set_fill_color(ctx, left_bg);
  graphics_fill_rect(ctx, GRect(0, 0, cx, b.size.h), 0, GCornerNone);
  graphics_context_set_fill_color(ctx, right_bg);
  graphics_fill_rect(ctx, GRect(cx, 0, b.size.w - cx, b.size.h), 0, GCornerNone);
  graphics_context_set_fill_color(ctx, bar);
  graphics_fill_rect(ctx, GRect(0, l->bar_y, b.size.w, b.size.h - l->bar_y), 0, GCornerNone);

  // Top row: date, weather, battery and Bluetooth, wherever settings put them.
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  draw_row(ctx, t, b.size.w, fg, left_bg, right_bg);

  // Time
  char clock[8];
  strftime(clock, sizeof(clock), clock_is_24h_style() ? "%H:%M" : "%I:%M", t);
  const char *clock_text = clock[0] == '0' && !clock_is_24h_style() ? clock + 1 : clock;
  draw_clock(ctx, clock_text, t->tm_sec, fg);

  // Logos
  if (!logo_shown) {
    // hidden under quick view
  } else if (matchup && (s_game.away[0] || s_game.home[0])) {
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
  // Same as the settings page's defaults, until the phone says otherwise.
  s_game = (Game){ .away_score = -1, .home_score = -1,
                   .zones = { ITEM_WEATHER, ITEM_DATE, ITEM_STATUS } };
  if (persist_read_int(PERSIST_KEY_VERSION) == PERSIST_VERSION) {
    persist_read_data(PERSIST_KEY_GAME, &s_game, sizeof(s_game));
  }
}

static void inbox_received(DictionaryIterator *it, void *context) {
  // A byte-for-byte copy (padding included) so memcmp below can tell
  // whether the message changed anything.
  Game old;
  memcpy(&old, &s_game, sizeof(old));
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
  if (read_int(it, MESSAGE_KEY_BgColor, &n)) s_game.bg_style = n;
  if (read_int(it, MESSAGE_KEY_CustomBg, &n)) s_game.bg_color = n;
  if (read_int(it, MESSAGE_KEY_Weather, &n)) s_game.weather = n;
  if (read_int(it, MESSAGE_KEY_WeatherIcon, &n)) s_game.wx_icon = n;
  if (read_int(it, MESSAGE_KEY_ZoneLeft, &n)) s_game.zones[ZONE_LEFT] = n;
  if (read_int(it, MESSAGE_KEY_ZoneMiddle, &n)) s_game.zones[ZONE_MIDDLE] = n;
  if (read_int(it, MESSAGE_KEY_ZoneRight, &n)) s_game.zones[ZONE_RIGHT] = n;
  if (read_int(it, MESSAGE_KEY_MiddleAlign, &n)) s_game.middle_align = n;
  if (read_int(it, MESSAGE_KEY_Bluetooth, &n)) s_game.bluetooth = n;
  if (read_int(it, MESSAGE_KEY_DateFormat, &n)) s_game.date_format = n;
  if (read_int(it, MESSAGE_KEY_DateDrop, &n)) s_game.date_drop = n;
  if (read_int(it, MESSAGE_KEY_GameClock, &n)) s_game.game_clock = n;
  if (read_int(it, MESSAGE_KEY_LiveHeartRate, &n)) s_game.live_hr = n;
  if (read_int(it, MESSAGE_KEY_LiveHeartRateZone, &n)) s_game.live_hr_zone = n;
  if (read_int(it, MESSAGE_KEY_LowBatteryZone, &n)) s_game.battery_zone = n;
  if (read_int(it, MESSAGE_KEY_DisconnectedZone, &n)) s_game.bt_zone = n;
  if (read_int(it, MESSAGE_KEY_QuietTimeZone, &n)) s_game.quiet_zone = n;
  if (read_int(it, MESSAGE_KEY_Temp, &n)) {
    s_game.temp = n;
    s_game.weather_time = time(NULL);
  }
  if (read_int(it, MESSAGE_KEY_State, &n)) s_game.state = n;
  if (read_int(it, MESSAGE_KEY_Start, &n)) s_game.start = n;
  if (read_int(it, MESSAGE_KEY_AwayScore, &n)) s_game.away_score = n;
  if (read_int(it, MESSAGE_KEY_HomeScore, &n)) s_game.home_score = n;
  if (read_int(it, MESSAGE_KEY_Refresh, &n) && n > 0) s_refresh_in = n;

  // Most live-game refreshes repeat what the watch already has. Skip the
  // flash write and the redraw then.
  if (memcmp(&old, &s_game, sizeof(s_game)) == 0) return;

  if (strcmp(old.team, s_game.team) != 0 || strcmp(old.away, s_game.away) != 0 ||
      strcmp(old.home, s_game.home) != 0 || old.split != s_game.split) {
    load_logos();
  }
  save_game();
  update_tick_unit();
  update_heart_rate_sampling();
  layer_mark_dirty(s_canvas);
}

static void inbox_dropped(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_WARNING, "Message dropped: %d", (int)reason);
}

// ---------------------------------------------------------------------------
// Events

static void tick_handler(struct tm *tick_time, TimeUnits changed) {
  if (changed & MINUTE_UNIT) {
    expire_health();
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

// Redraw through quick view's slide in and out so the layout follows it.
static void unobstructed_change(AnimationProgress progress, void *context) {
  layer_mark_dirty(s_canvas);
}

static void app_connection_handler(bool connected) {
  if (connected) request_refresh();
  layer_mark_dirty(s_canvas);  // for the Bluetooth icon
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
  update_heart_rate_sampling();  // in case the face starts mid-game
  battery_state_service_subscribe(battery_handler);
  unobstructed_area_service_subscribe((UnobstructedAreaHandlers){
    .change = unobstructed_change,
  }, NULL);
  connection_service_subscribe((ConnectionHandlers){
    .pebble_app_connection_handler = app_connection_handler,
  });
}

static void deinit(void) {
  stop_heart_rate_sampling();
  tick_timer_service_unsubscribe();
  battery_state_service_unsubscribe();
  unobstructed_area_service_unsubscribe();
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
