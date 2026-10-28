# MASL Scores — Data Sources Handoff

Oct 27, 2026 · @Josh Wudtke

## Overview

The MASL Scores app gets all its data from two unofficial DigitalShift endpoints that power maslsoccer.com: a score ticker and a standings table. Both return JSON wrapping an HTML fragment, which the phone (PebbleKit JS) scrapes with regexes and sends to the watch as compact pipe-delimited strings.

The flow, as it runs today:

1. Phone JS fetches the ticker on app launch, then every 10 minutes (every 1 minute while a game is live).
2. It parses up to 30 games, converts start times to the chosen time zone, and applies the team filter.
3. It sends one AppMessage string to the watch: one line per game.
4. Standings are fetched only when the watch asks for them (opening the standings screen).

Source of truth for everything below: `src/pkjs/index.js`, `src/pkjs/teams.js`, `src/pkjs/config.js` and `src/c/main.c` in the MASL Scores repo. A watchface can reuse the phone-side parser almost unchanged.

## Data sources

Both feeds are plain `GET` requests to DigitalShift, authenticated with a public site ticket that maslsoccer.com embeds in its own pages.

| Feed | URL | When fetched |
| --- | --- | --- |
| Score ticker | `https://web.api.digitalshift.ca/partials/score-ticker` | Launch, every 10 min idle, every 1 min while any game is live, on manual refresh |
| Standings | `https://web.api.digitalshift.ca/partials/stats/standings/table?division_id=50066&league_toggle=division` | Only when the standings screen opens |

Request headers:

- `Authorization: ticket="<TICKET>"` — copy the current value of `TICKET` from `src/pkjs/index.js`. It rotates; a 401 or 403 means grab a fresh one from DevTools (the `score-ticker` request on maslsoccer.com).
- `Accept: application/json, text/plain, */*`
- `Origin: https://www.maslsoccer.com` and `Referer: https://www.maslsoccer.com/` — set best-effort; the feed returned 200 without them from a desktop, so they are probably optional.
- Timeout: 15 s.

Response: JSON shaped like `{ "content": "<html fragment>" }`. Only `content` is used. Errors surface to the watch as short strings: `Error <status>`, `Could not read data`, `No connection`, `Timed out`.

The standings `division_id` (50066) comes from the maslsoccer.com standings URL `stats#/192/standings?render=division&division_id=50066` and may change next season.

## Parsed fields

### Score ticker → game

The HTML is split on `<li class="item"`; each item is one game (max 30 kept).

| Field | Where it comes from | Example |
| --- | --- | --- |
| `away` | First `<div class="team"` block: text right after its `<img>` | `Ambush` |
| `home` | Second `<div class="team"` block, cut before `<div class="status` | `Wave` |
| `awayScore` / `homeScore` | Any element with a class containing `score` followed by digits, inside that team's block; empty if none | \`\` (preseason) |
| date | Item's `aria-label`, e.g. `November 27, 2026 at 6:05pm` → `Nov 27` | `Nov 27` |
| status | Text of `<div class="status...">` | `6:05pm EST`, `Final` |
| `scheduled` | Status starts with a time (`h:mm am/pm`) | true |
| `isFinal` | Status contains "final" (case-insensitive) | false |
| `live` | Status non-empty and neither scheduled nor final | false |
| `subtitle` | ` LIVE  ` (if live) + date + status, after time-zone conversion | `Nov 27 6:05pm EST` |

Team names are the short names (`Ambush`, not `St. Louis Ambush`). The first team listed is treated as away, inferred from ticket links.

### Standings → row

The partial renders the table twice (screen and print); only the first `<table>` is used. Columns are found by header text, so column order on the site doesn't matter.

| Field | Header | Notes |
| --- | --- | --- |
| `rank` | `Rk` | Row index + 1 if missing |
| `team` | `Team` | Short name from `<span class="p">`, else the full cell text |
| `gp` | `GP` | Games played; shows games in hand |
| `w` / `l` / `ot` | `W` / `L` / `OT` |  |
| `pts` | `Pts` | Required; parse fails without `Team` and `Pts` |
| `rw` | `RW` | Regulation wins, first tiebreaker |
| `diff` | `Diff` | Goal differential |

GF and GA exist on the site but are deliberately dropped.

### Teams

The nine short names in the feed as of Oct 2026, from `src/pkjs/teams.js`, with each arena's time zone:

| Team | Zone |
| --- | --- |
| Ambush | Central |
| Blast | Eastern |
| Comets | Central |
| Sockers | Pacific |
| Spirits | Eastern |
| Stars | Pacific |
| Strykers | Pacific |
| UCFC | Eastern |
| Wave | Central |

Logos for all nine are in `resources/images/logo_<team>.png`: 24×20 px, snapped to Pebble's 64 colors, color platforms only. `tools/convert_logos.py` regenerates them at another size; change `LOGO_W`/`LOGO_H` there for a watchface.

## Phone → watch messages

Everything crosses as strings: one record per line (`\n`), fields separated by `|`. The phone strips `|` and newlines from every field before sending.

| Message key | Direction | Value |
| --- | --- | --- |
| `Games` | phone → watch | `away\|awayScore\|home\|homeScore\|subtitle` per game |
| `Status` | phone → watch | Error or empty-state text: `No games listed`, `No games for your teams`, `Could not read scores`, fetch errors |
| `Refresh` | watch → phone | Any value; triggers a ticker fetch |
| `GetStandings` | watch → phone | Any value; triggers a standings fetch |
| `Standings` | phone → watch | `rank\|team\|gp\|w\|l\|ot\|pts\|rw\|diff` per team |
| `StandingsStatus` | phone → watch | Error or empty-state text for standings |
| `HighlightColor` | phone → watch | Int color from settings (default `0x55AAFF`), saved in watch persist key 1 |
| `TimeZone`, `FilterTeams[9]` | settings only | Stored in phone `localStorage`, never sent to the watch |

Example `Games` line: `Ambush||Wave||Nov 27 6:05pm EST` (scores empty before kickoff).

Watch buffer sizes in `main.c`: team name 20 chars, score 4, standings stat 5, subtitle 40; up to 30 games and 16 standings rows. A watchface has a much smaller AppMessage inbox budget, so trim the payload (see the last section).

## Times, tiebreakers and settings

**Game times.** The site lists every start in Eastern and labels it "EST" year-round. The app treats times as EDT from the second Sunday in March to the first Sunday in November. Three settings modes:

- `eastern` (default): keep the site's text.
- `local`: convert to the phone's zone, e.g. `5:05pm CST`.
- `home`: convert to the home team's arena zone (table above); unknown teams keep Eastern.

Only scheduled games are converted; live and final statuses pass through.

**Tiebreak order** (MASL): Pts > RW > W > head-to-head > GD > GF > GA. Head-to-head isn't in the feed, so the app skips it. Prefer these stats over streaks or home/away records.

**Refresh.** Ticker: on launch, every 10 minutes, every 1 minute while any game is live, and on a `Refresh` message. Standings: only on request.

**Settings page** (Clay, `@rebble/clay` ^1.1.0 — not the old `pebble-clay`, which breaks the `flint` build): highlight color, time-zone mode, and a team filter (empty = all teams).

## Caveats and watchface notes

**Unverified until the season starts (first game Nov 27, 2026):**

- Score parsing and live/final detection are guesses; no live or final game has appeared in the feed yet. Capture a real ticker response after that night and check `parseTicker`.
- Away/home order is inferred from ticket links.
- All data so far is preseason: every game upcoming, every standings stat 0.

**Other caveats:**

- The feed is unofficial. Check MASL's terms before publishing to the Pebble appstore.
- The ticket and `division_id` are hard-coded and can change.
- A new team in the feed shows with no logo until one is added.

**Suggestions for the watchface:**

- Reuse `getPartial`, `parseTicker`, `parseStandings` and the time-zone helpers from `src/pkjs/index.js` as-is; they're exported for Node testing.
- Send far less: a watchface likely needs only the next or current game for one favorite team (e.g. Ambush) and maybe its standings line. One short message keeps inbox memory small.
- Throttle refreshes for battery: hourly when idle, 1–2 min only while that team is live.
- Cache the last payload in watch persistent storage so the face isn't blank on start.
- Set `"watchface": true` in `package.json` and use a new UUID; the message keys can stay the same.
