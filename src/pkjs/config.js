// Settings page, built by Clay (https://github.com/pebble-dev/clay).
//
// Items that only matter after another choice are shown and hidden by
// config-custom.js, so keep the two in step when adding or renaming one.

var TEAMS = require('./teams');
var TIME_FONTS = require('./time-fonts');

// A top row spot that something switches to while it matters, or off.
function swapSelect(key, label) {
  return {
    type: 'select',
    messageKey: key,
    label: label,
    defaultValue: 'off',
    options: [
      { label: "Don't switch", value: 'off' },
      { label: 'Left spot', value: 'left' },
      { label: 'Middle spot', value: 'middle' },
      { label: 'Right spot', value: 'right' }
    ]
  };
}

// A battery level, in the 10% steps the watch reports.
function percentSelect(key, label, description, value, min, max) {
  var options = [];
  for (var p = min; p <= max; p += 10) options.push({ label: p + '% or less', value: '' + p });
  return {
    type: 'select',
    messageKey: key,
    label: label,
    description: description,
    defaultValue: '' + value,
    options: options
  };
}

// One of the top row's three spots.
function zoneSelect(key, label, value) {
  return {
    type: 'select',
    messageKey: key,
    label: label,
    defaultValue: value,
    options: [
      { label: 'Empty', value: 'none' },
      { label: 'Date', value: 'date' },
      { label: 'Weather', value: 'weather' },
      { label: 'Battery', value: 'battery' },
      { label: 'Bluetooth', value: 'bluetooth' },
      { label: 'Battery and Bluetooth', value: 'status' },
      { label: 'Steps today', value: 'steps' },
      { label: 'Heart rate', value: 'heartRate' },
      { label: 'Quiet Time bell', value: 'quiet' },
      { label: 'UV index', value: 'uv' }
    ]
  };
}

// The time's font, then a size picker for each style that has more than one
// size. Only the chosen style's size picker shows.
function timeFontItems() {
  var items = [{
    type: 'select',
    messageKey: 'TimeFont',
    label: 'Time font',
    defaultValue: TIME_FONTS[0].value,
    options: TIME_FONTS.map(function (f) { return { label: f.label, value: f.value }; })
  }];
  TIME_FONTS.forEach(function (f) {
    if (f.sizes.length < 2) return;
    var names = f.sizes.length === 2 ? ['Large', 'Small'] : ['Large', 'Medium', 'Small'];
    items.push({
      type: 'select',
      messageKey: f.sizeKey,
      label: 'Time size',
      description: 'If the time is too wide for this screen, the next size down is used.',
      defaultValue: '' + f.sizes[0],
      options: f.sizes.map(function (size, i) {
        return { label: names[i] + ' (' + size + ')', value: '' + size };
      })
    });
  });
  return items;
}

module.exports = [
  { type: 'heading', defaultValue: 'MASL Watchface' },
  {
    type: 'section',
    items: [
      { type: 'heading', defaultValue: 'Team and game' },
      {
        type: 'select',
        messageKey: 'Team',
        label: 'My team',
        defaultValue: '',
        options: [{ label: 'None (next game in the league)', value: '' }].concat(
          TEAMS.map(function (t) { return { label: t.full, value: t.name }; }))
      },
      {
        type: 'radiogroup',
        messageKey: 'View',
        label: 'Layout',
        defaultValue: 'logo',
        options: [
          { label: 'Big team logo', value: 'logo' },
          { label: 'Split view: both logos and colors', value: 'split' }
        ]
      },
      {
        type: 'toggle',
        messageKey: 'HomeOnly',
        label: 'Home games only',
        defaultValue: false
      },
      {
        type: 'radiogroup',
        messageKey: 'NoTeamGame',
        label: 'If my team has no game listed',
        defaultValue: 'status',
        options: [
          { label: 'Say "No games listed"', value: 'status' },
          { label: "Show the league's next game in my team's look", value: 'leagueLogo' },
          { label: "Show the league's next game", value: 'league' }
        ]
      },
      {
        type: 'radiogroup',
        messageKey: 'Mode',
        label: 'Before the game, show',
        defaultValue: 'time',
        options: [
          { label: 'Start time', value: 'time' },
          { label: 'Countdown', value: 'countdown' }
        ]
      },
      {
        type: 'select',
        messageKey: 'CountdownDetail',
        label: 'Countdown shows',
        description: 'Seconds use more battery.',
        defaultValue: 'hours',
        options: [
          { label: 'Days and hours (in 3d 4h)', value: 'hours' },
          { label: 'Down to minutes (in 3d 4h 12m)', value: 'minutes' },
          { label: 'Down to seconds (3d 04:12:30)', value: 'seconds' }
        ]
      },
      {
        type: 'select',
        messageKey: 'GameClock',
        label: 'Start time clock',
        defaultValue: 'watch',
        options: [
          { label: "Same as the watch's clock", value: 'watch' },
          { label: '12 hour (7:05pm)', value: '12' },
          { label: '24 hour (19:05)', value: '24' }
        ]
      },
      {
        type: 'select',
        messageKey: 'TimeZone',
        label: 'Game days and times in',
        description: 'MASL lists every game in Eastern time.',
        defaultValue: 'eastern',
        options: [
          { label: 'Eastern (as listed)', value: 'eastern' },
          { label: 'My time zone', value: 'local' },
          { label: "Home team's time zone", value: 'home' }
        ]
      }
    ]
  },
  {
    type: 'section',
    items: [
      { type: 'heading', defaultValue: 'Look' },
      {
        type: 'select',
        messageKey: 'BarColor',
        label: 'Game bar color',
        capabilities: ['COLOR'],
        defaultValue: 'primary',
        options: [
          { label: 'Team primary', value: 'primary' },
          { label: 'Team secondary', value: 'secondary' },
          { label: 'Custom', value: 'custom' }
        ]
      },
      {
        type: 'color',
        messageKey: 'CustomColor',
        label: 'Custom bar color',
        capabilities: ['COLOR'],
        defaultValue: '00AAAA',
        sunlight: false
      },
      {
        type: 'select',
        messageKey: 'BgColor',
        label: 'Background color',
        capabilities: ['COLOR'],
        defaultValue: 'team',
        options: [
          { label: 'Team color', value: 'team' },
          { label: 'Custom', value: 'custom' }
        ]
      },
      {
        type: 'color',
        messageKey: 'CustomBg',
        label: 'Custom background color',
        description: 'With a light color, the time and date turn black.',
        capabilities: ['COLOR'],
        defaultValue: '000000',
        sunlight: false
      },
    ].concat(timeFontItems(), [
      {
        type: 'toggle',
        messageKey: 'Seconds',
        label: 'Show seconds',
        description: 'Uses noticeably more battery.',
        defaultValue: false
      },
      {
        type: 'toggle',
        messageKey: 'SecondsBacklight',
        label: 'Only while the backlight is on',
        description: 'Shows seconds for a few seconds after a wrist flick, then hides them. ' +
                     'Saves most of the battery cost. (A watchface cannot see button presses.)',
        defaultValue: false
      },
      {
        type: 'toggle',
        messageKey: 'LargeRow',
        label: 'Larger top row text',
        description: 'The row fills up sooner, so the date drops a word more often.',
        defaultValue: false
      },
      {
        type: 'toggle',
        messageKey: 'LargeBar',
        label: 'Larger game bar text',
        description: 'The bar grows a little taller and the time moves up. ' +
                     'A tall logo may switch to its smaller version to make room.',
        defaultValue: false
      },
      {
        type: 'radiogroup',
        messageKey: 'QuickView',
        label: 'When Quick View covers the bottom',
        defaultValue: 'slide',
        options: [
          { label: 'Move the time and game bar up (hides the logo)', value: 'slide' },
          { label: 'Keep the logo and move the time up (hides the top row and game bar)',
            value: 'logo' }
        ]
      }
    ])
  },
  {
    type: 'section',
    items: [
      { type: 'heading', defaultValue: 'Top row' },
      zoneSelect('ZoneLeft', 'Left', 'weather'),
      zoneSelect('ZoneMiddle', 'Middle', 'date'),
      zoneSelect('ZoneRight', 'Right', 'status'),
      {
        type: 'radiogroup',
        messageKey: 'MiddleAlign',
        label: 'Middle item',
        capabilities: ['RECT'],
        defaultValue: 'balanced',
        options: [
          { label: 'Balanced between its neighbors', value: 'balanced' },
          { label: 'Centered on the screen', value: 'center' }
        ]
      },
      {
        type: 'select',
        messageKey: 'DateFormat',
        label: 'Date',
        defaultValue: 'dayMonthDate',
        options: [
          { label: 'WED NOV 5', value: 'dayMonthDate' },
          { label: 'WED 5', value: 'dayDate' },
          { label: 'NOV 5', value: 'monthDate' },
          { label: '5 NOV', value: 'dateMonth' },
          { label: 'WED 11/5', value: 'dayMD' },
          { label: 'WED 5/11', value: 'dayDM' },
          { label: '11/5', value: 'md' },
          { label: '5/11', value: 'dm' },
          { label: '2026-11-05', value: 'iso' }
        ]
      },
      {
        type: 'radiogroup',
        messageKey: 'DateDrop',
        label: 'If the row is too full, drop the',
        defaultValue: 'month',
        options: [
          { label: 'Month (WED 5)', value: 'month' },
          { label: 'Weekday (NOV 5 or 11/5)', value: 'weekday' }
        ]
      },
      {
        type: 'select',
        messageKey: 'Weather',
        label: 'Weather',
        description: "From your phone's location, every 30 minutes.",
        defaultValue: 'off',
        options: [
          { label: 'Off', value: 'off' },
          { label: 'Temperature', value: 'temp' },
          { label: 'Icon', value: 'iconOnly' },
          { label: 'Temperature and icon', value: 'icon' }
        ]
      },
      {
        type: 'radiogroup',
        messageKey: 'TempUnits',
        label: 'Temperature in',
        defaultValue: 'f',
        options: [
          { label: 'Fahrenheit', value: 'f' },
          { label: 'Celsius', value: 'c' }
        ]
      },
      {
        type: 'radiogroup',
        messageKey: 'UVShow',
        label: 'Show UV index',
        description: "From the weather where your phone is, every 30 minutes.",
        defaultValue: 'high',
        options: [
          { label: 'Only when high', value: 'high' },
          { label: 'Always', value: 'always' }
        ]
      },
      {
        type: 'select',
        messageKey: 'UVHigh',
        label: 'UV index is high at',
        defaultValue: '6',
        options: [
          { label: '3 or more (moderate)', value: '3' },
          { label: '6 or more (high)', value: '6' },
          { label: '8 or more (very high)', value: '8' },
          { label: '11 or more (extreme)', value: '11' }
        ]
      },
      {
        type: 'select',
        messageKey: 'Battery',
        label: 'Show battery',
        defaultValue: 'always',
        options: [
          { label: 'Always', value: 'always' },
          { label: 'When low or charging', value: 'low' },
          { label: 'Never', value: 'never' }
        ]
      },
      {
        type: 'select',
        messageKey: 'BatteryText',
        label: 'Battery shows',
        defaultValue: 'icon',
        options: [
          { label: 'Icon', value: 'icon' },
          { label: 'Percentage (80%)', value: 'percent' },
          { label: 'Icon and percentage', value: 'both' }
        ]
      },
      percentSelect('BatteryLow', 'Battery is low at', 'Yellow on color watches.',
                    40, 10, 60),
      percentSelect('BatteryCritical', 'Battery is critical at',
                    'Red on color watches. Always kept below the low level.',
                    20, 10, 40),
      {
        type: 'select',
        messageKey: 'Bluetooth',
        label: 'Show Bluetooth',
        description: 'Slashed while the phone is disconnected.',
        defaultValue: 'disconnected',
        options: [
          { label: 'Only when disconnected', value: 'disconnected' },
          { label: 'Always', value: 'always' },
          { label: 'Never', value: 'never' }
        ]
      }
    ]
  },
  {
    type: 'section',
    items: [
      { type: 'heading', defaultValue: 'Switch spots automatically' },
      {
        // Not saved: config-custom.js turns it on when any swap is set, and
        // turning it off sets them all back to "Don't switch".
        type: 'toggle',
        id: 'Swaps',
        label: 'Switch a top row spot when something needs attention',
        defaultValue: false
      },
      {
        type: 'text',
        id: 'SwapsNote',
        defaultValue: 'The spot shows it until things are back to normal. ' +
                      'Nothing switches if it already has a spot.'
      },
      swapSelect('LowBatteryZone', 'Battery gets low'),
      swapSelect('DisconnectedZone', 'Phone disconnects'),
      swapSelect('QuietTimeZone', 'Quiet Time turns on'),
      swapSelect('UVZone', 'UV index gets high'),
      swapSelect('LiveHeartRateZone', "My team's game is live (heart rate)")
    ]
  },
  {
    type: 'section',
    items: [
      { type: 'heading', defaultValue: 'Live games' },
      {
        type: 'toggle',
        messageKey: 'ShowScores',
        label: 'Show scores',
        description: 'Off hides scores and skips finished games.',
        defaultValue: false
      },
      {
        type: 'select',
        messageKey: 'LiveInterval',
        label: 'Update scores every',
        defaultValue: '1',
        options: [
          { label: '1 minute', value: '1' },
          { label: '2 minutes', value: '2' },
          { label: '5 minutes', value: '5' }
        ]
      },
      {
        type: 'toggle',
        messageKey: 'LiveHeartRate',
        label: "Read heart rate every minute during my team's games",
        description: 'Uses more battery. Needs a heart rate sensor.',
        defaultValue: false
      }
    ]
  },
  { type: 'submit', defaultValue: 'Save' }
];
