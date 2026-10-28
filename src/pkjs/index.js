// MASL Watchface - phone-side component (PebbleKit JS)
// Fetches the MASL score ticker from DigitalShift, picks the one game the face
// should show (the selected team's next or live game, or the league's next
// game when no team is selected), and sends it to the watch in one message.
// The watch asks for a refresh when the delay sent with each game runs out.

var TEAMS = require('./teams');

var TICKER_URL = 'https://web.api.digitalshift.ca/partials/score-ticker';
// Public site token embedded in maslsoccer.com. If the face starts showing
// "Error 401/403", grab a fresh ticket from DevTools and paste it here.
var TICKET = 'sBo7AT5RH6DLyYeFtGiJvpXa_JsGl9LIj8dSnDZTEwj5gPgukowAIc9Swg-t9T6dgm0IUwtnCacatO0TAb05PNqb';

// Game states, as sent to the watch in State.
var NONE = 0, SCHEDULED = 1, LIVE = 2, FINAL = 3;

// Refresh delays in minutes, sent to the watch in Refresh.
var IDLE_REFRESH = 60;       // between games
var LIVE_NO_SCORES = 5;      // live game, scores hidden: just watch for the end
var FINAL_REFRESH = 30;      // showing a final score
var ERROR_REFRESH = 15;
// A scheduled game this long past its start without going live is treated as
// postponed or stale and skipped.
var STALE_MS = 4 * 3600 * 1000;
// With scores on, a final stays up this long after its start time.
var FINAL_SHOW_MS = 6 * 3600 * 1000;

var MONTHS = ['January', 'February', 'March', 'April', 'May', 'June', 'July',
              'August', 'September', 'October', 'November', 'December'];
var DAYS = ['Sun', 'Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat'];

var SETTINGS_KEY = 'settings';
var DEFAULTS = {
  team: '', view: 'logo', homeOnly: false, mode: 'time', timeZone: 'eastern',
  showScores: false, liveInterval: 1, barColor: 'primary', customColor: 0x00AAAA,
  battery: 'always'
};

function loadSettings() {
  var s = {};
  try {
    s = JSON.parse(localStorage.getItem(SETTINGS_KEY)) || {};
  } catch (e) { /* use defaults */ }
  var out = {};
  for (var k in DEFAULTS) out[k] = k in s ? s[k] : DEFAULTS[k];
  return out;
}

function teamInfo(name) {
  for (var i = 0; i < TEAMS.length; i++) {
    if (TEAMS[i].name === name) return TEAMS[i];
  }
  return null;
}

function decodeEntities(s) {
  return s.replace(/&amp;/g, '&').replace(/&#39;/g, "'").replace(/&quot;/g, '"')
          .replace(/&lt;/g, '<').replace(/&gt;/g, '>').replace(/&nbsp;/g, ' ');
}

function stripTags(s) {
  return decodeEntities(s.replace(/<[^>]*>/g, ' ')).replace(/\s+/g, ' ').trim();
}

function parseTeam(chunk) {
  var nameMatch = chunk.match(/<img[^>]*>([^<]*)/);
  var name = nameMatch ? decodeEntities(nameMatch[1]).trim() : '';
  if (!name) name = stripTags(chunk).split(' ')[0] || '?';
  // Scores aren't in the feed yet (preseason). Look for any element whose
  // class contains "score" followed by a number inside this team's block.
  var scoreMatch = chunk.match(/class="[^"]*score[^"]*"[^>]*>\s*(?:<[^>]*>\s*)*(\d+)/);
  return { name: name, score: scoreMatch ? +scoreMatch[1] : -1 };
}

// Whether US daylight saving time is in effect on the given date: it runs
// from the second Sunday in March to the first Sunday in November. The site
// labels every time "EST", but games after the March change are assumed to
// be EDT.
function isDst(year, month, day) {
  var sunday = function (m, n) {
    var first = new Date(Date.UTC(year, m, 1)).getUTCDay();
    return 1 + (7 - first) % 7 + 7 * (n - 1);
  };
  return (month > 2 || (month === 2 && day >= sunday(2, 2))) &&
         (month < 10 || (month === 10 && day < sunday(10, 1)));
}

// "6:05pm" style, matching the site.
function formatTime(h, m) {
  return ((h + 11) % 12 + 1) + ':' + (m < 10 ? '0' : '') + m + (h < 12 ? 'am' : 'pm');
}

// The phone's short zone name ("CST"), or '' if the JS engine can't say.
function zoneName(d) {
  try {
    var m = d.toLocaleTimeString('en-US', { timeZoneName: 'short' }).match(/ ([A-Z]{2,5})$/);
    return m ? m[1] : '';
  } catch (e) {
    return '';
  }
}

// Turn an aria-label start ("November 27, 2026 at 6:05pm") from Eastern into
// a Date, or null if it doesn't parse.
function startTime(label) {
  var m = label.match(/^(\w+) (\d+), (\d{4}) at (\d{1,2}):(\d{2})\s*([ap]m)/i);
  var month = m ? MONTHS.indexOf(m[1]) : -1;
  if (month === -1) return null;
  var year = +m[3], day = +m[2];
  var hour = +m[4] % 12 + (/pm/i.test(m[6]) ? 12 : 0);
  var offset = isDst(year, month, day) ? 4 : 5;
  return new Date(Date.UTC(year, month, day, hour + offset, +m[5]));
}

// "Sat 11/28": short enough to share the bar's top line with the opponent.
function dayText(weekday, month, date) {
  return DAYS[weekday] + ' ' + (month + 1) + '/' + date;
}

// Start day and time in a fixed US zone `hoursBehind` UTC in standard time,
// e.g. { day: 'Sat 11/28', time: '6:05pm CST' }.
function zonedText(start, hoursBehind, letter) {
  var dst = isDst(start.getUTCFullYear(), start.getUTCMonth(), start.getUTCDate());
  var d = new Date(start.getTime() - (hoursBehind - (dst ? 1 : 0)) * 3600 * 1000);
  return {
    day: dayText(d.getUTCDay(), d.getUTCMonth(), d.getUTCDate()),
    time: formatTime(d.getUTCHours(), d.getUTCMinutes()) + ' ' + letter + (dst ? 'DT' : 'ST')
  };
}

// timeZone: 'eastern' (as listed), 'local' (the phone's zone) or 'home' (the
// home arena's zone; unknown teams fall back to Eastern).
function startText(start, home, timeZone) {
  if (timeZone === 'local') {
    var zone = zoneName(start);
    return {
      day: dayText(start.getDay(), start.getMonth(), start.getDate()),
      time: formatTime(start.getHours(), start.getMinutes()) + (zone ? ' ' + zone : '')
    };
  }
  var info = timeZone === 'home' ? teamInfo(home) : null;
  return info ? zonedText(start, info.zone[0], info.zone[1]) : zonedText(start, 5, 'E');
}

function parseTicker(html) {
  var games = [];
  var items = html.split('<li class="item"');
  for (var i = 1; i < items.length; i++) {
    var c = items[i];
    var label = (c.match(/aria-label="([^"]*)"/) || [])[1] || '';

    // Split on the exact class "team" (not "team-area" / "teams").
    var teamChunks = c.split('<div class="team"').slice(1, 3);
    if (teamChunks.length < 2) continue;
    // Each chunk should stop before the status block.
    teamChunks[1] = teamChunks[1].split('<div class="status')[0];
    var away = parseTeam(teamChunks[0]);
    var home = parseTeam(teamChunks[1]);

    var statusMatch = c.match(/<div class="status[^"]*"[^>]*>([\s\S]*?)<\/div>/);
    var status = statusMatch ? stripTags(statusMatch[1]) : '';
    var scheduled = /^\d{1,2}:\d{2}\s*[ap]m/i.test(status);
    var isFinal = /final/i.test(status);

    games.push({
      away: away.name, awayScore: away.score,
      home: home.name, homeScore: home.score,
      status: status,
      state: scheduled ? SCHEDULED : isFinal ? FINAL : status ? LIVE : NONE,
      start: startTime(label)
    });
  }
  return games;
}

// The game to show: a live one first, then (with scores on) a final from
// earlier today if the next game isn't also today, then the next scheduled.
function pickGame(games, s, now) {
  var mine = games.filter(function (g) {
    if (!s.team) return true;
    return g.home === s.team || (!s.homeOnly && g.away === s.team);
  });
  var live = mine.filter(function (g) { return g.state === LIVE; })[0];
  if (live) return live;

  var next = mine.filter(function (g) {
    return g.state === SCHEDULED && (!g.start || now - g.start.getTime() < STALE_MS);
  }).sort(function (a, b) {
    return (a.start ? a.start.getTime() : Infinity) - (b.start ? b.start.getTime() : Infinity);
  })[0];

  if (s.showScores) {
    var recent = mine.filter(function (g) {
      return g.state === FINAL && g.start && now - g.start.getTime() < FINAL_SHOW_MS;
    }).pop();
    if (recent && (!next || !next.start || next.start.getTime() - now > FINAL_SHOW_MS)) {
      return recent;
    }
  }
  return next || null;
}

// Minutes until the watch should ask again.
function refreshDelay(game, s, now) {
  if (!game) return IDLE_REFRESH;
  if (game.state === LIVE) return s.showScores ? +s.liveInterval || 1 : LIVE_NO_SCORES;
  if (game.state === FINAL) return FINAL_REFRESH;
  if (!game.start) return IDLE_REFRESH;
  // Wake up right at the start time to catch the game going live, then keep
  // checking every few minutes until it does.
  var mins = Math.ceil((game.start.getTime() - now) / 60000);
  return mins <= 0 ? LIVE_NO_SCORES : Math.min(IDLE_REFRESH, mins);
}

// The settings that only change how the watch draws. The watch numbers its
// options in the order they're listed here.
function displayMessage(s) {
  return { Team: s.team, View: s.view === 'split' ? 1 : 0,
           Mode: s.mode === 'countdown' ? 1 : 0,
           BarColor: Math.max(0, ['primary', 'secondary', 'custom'].indexOf(s.barColor)),
           CustomColor: s.customColor,
           Battery: Math.max(0, ['always', 'low', 'never'].indexOf(s.battery)) };
}

// Clay hands back colors as a number, but accept "#00AAAA" or "0x00AAAA" too.
function colorValue(v) {
  return typeof v === 'string' ? parseInt(v.replace(/^#|^0x/i, ''), 16) || 0 : +v || 0;
}

// Everything the watch needs for one game, ready for sendAppMessage. Scores
// are -1 unless the user turned scores on.
function gameMessage(game, s, now) {
  var msg = displayMessage(s);
  msg.Refresh = refreshDelay(game, s, now);
  msg.Status = '';
  if (!game) {
    msg.State = NONE;
    msg.Away = msg.Home = msg.Day = msg.Time = msg.Detail = '';
    msg.Start = 0;
    msg.AwayScore = msg.HomeScore = -1;
    msg.Status = s.team && s.homeOnly ? 'No home games listed' : 'No games listed';
    return msg;
  }
  var when = game.start ? startText(game.start, game.home, s.timeZone)
                        : { day: '', time: game.state === SCHEDULED ? game.status : '' };
  // Keep the site's own text when it's Eastern and parsed fine.
  if (game.state === SCHEDULED && s.timeZone === 'eastern') when.time = game.status;
  msg.State = game.state;
  msg.Away = game.away;
  msg.Home = game.home;
  msg.Start = game.start ? Math.floor(game.start.getTime() / 1000) : 0;
  msg.Day = when.day;
  msg.Time = when.time;
  msg.Detail = game.state === LIVE ? game.status : '';
  msg.AwayScore = s.showScores ? game.awayScore : -1;
  msg.HomeScore = s.showScores ? game.homeScore : -1;
  return msg;
}

// GET the ticker and pass its HTML content to onContent, or a short error
// message for the watch to onError.
function getTicker(onContent, onError) {
  var xhr = new XMLHttpRequest();
  xhr.open('GET', TICKER_URL, true);
  xhr.setRequestHeader('Accept', 'application/json, text/plain, */*');
  xhr.setRequestHeader('Authorization', 'ticket="' + TICKET + '"');
  // Some environments ignore these; harmless if so.
  try {
    xhr.setRequestHeader('Origin', 'https://www.maslsoccer.com');
    xhr.setRequestHeader('Referer', 'https://www.maslsoccer.com/');
  } catch (e) { /* ignored */ }
  xhr.timeout = 15000;

  xhr.onload = function () {
    if (xhr.status !== 200) {
      onError('Error ' + xhr.status);
      return;
    }
    var content;
    try {
      content = JSON.parse(xhr.responseText).content || '';
    } catch (e) {
      console.log('Bad ticker response: ' + e);
      onError('Could not read data');
      return;
    }
    onContent(content);
  };
  xhr.onerror = function () { onError('No connection'); };
  xhr.ontimeout = function () { onError('Timed out'); };
  xhr.send();
}

function send(msg) {
  Pebble.sendAppMessage(msg, null, function () {
    console.log('Failed to send ' + Object.keys(msg).join(','));
  });
}

function fetchGame() {
  getTicker(function (html) {
    var s = loadSettings(), now = Date.now(), game;
    try {
      game = pickGame(parseTicker(html), s, now);
    } catch (e) {
      console.log('Parse error: ' + e);
      send({ Status: 'Could not read scores', Refresh: ERROR_REFRESH });
      return;
    }
    send(gameMessage(game, s, now));
  }, function (message) {
    // Keep whatever game the watch already has; the status only shows when
    // it has none.
    send({ Status: message, Refresh: ERROR_REFRESH });
  });
}

if (typeof Pebble !== 'undefined') {
  var Clay = require('@rebble/clay');
  // Events are handled here so settings that only shape the request stay on
  // the phone.
  var clay = new Clay(require('./config'), null, { autoHandleEvents: false });

  Pebble.addEventListener('ready', fetchGame);
  Pebble.addEventListener('appmessage', function (e) {
    if (e.payload && 'Refresh' in e.payload) fetchGame();
  });

  Pebble.addEventListener('showConfiguration', function () {
    Pebble.openURL(clay.generateUrl());
  });

  Pebble.addEventListener('webviewclosed', function (e) {
    if (!e || !e.response) return;
    var c = clay.getSettings(e.response, false);
    var v = function (key, fallback) { return c[key] ? c[key].value : fallback; };
    var s = {
      team: v('Team', DEFAULTS.team),
      view: v('View', DEFAULTS.view),
      homeOnly: !!v('HomeOnly', DEFAULTS.homeOnly),
      mode: v('Mode', DEFAULTS.mode),
      timeZone: v('TimeZone', DEFAULTS.timeZone),
      showScores: !!v('ShowScores', DEFAULTS.showScores),
      liveInterval: +v('LiveInterval', DEFAULTS.liveInterval),
      barColor: v('BarColor', DEFAULTS.barColor),
      customColor: colorValue(v('CustomColor', DEFAULTS.customColor)),
      battery: v('Battery', DEFAULTS.battery)
    };
    localStorage.setItem(SETTINGS_KEY, JSON.stringify(s));
    // Switch the theme right away; the game follows once the fetch is back.
    send(displayMessage(s));
    fetchGame();
  });
}

// Exported only so the parser can be tested outside the watch environment.
if (typeof module !== 'undefined' && typeof Pebble === 'undefined') {
  module.exports = { parseTicker: parseTicker, pickGame: pickGame,
                     gameMessage: gameMessage, refreshDelay: refreshDelay,
                     DEFAULTS: DEFAULTS };
}
