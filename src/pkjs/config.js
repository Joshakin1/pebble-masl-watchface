// Settings page, built by Clay (https://github.com/pebble-dev/clay).

var TEAMS = require('./teams');

// A top row spot that something switches to while it matters, or off.
function swapSelect(key, label, description) {
  return {
    type: 'select',
    messageKey: key,
    label: label,
    description: description,
    defaultValue: 'off',
    options: [
      { label: 'Off', value: 'off' },
      { label: 'Left spot', value: 'left' },
      { label: 'Middle spot', value: 'middle' },
      { label: 'Right spot', value: 'right' }
    ]
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
      { label: 'Heart rate', value: 'heartRate' }
    ]
  };
}

module.exports = [
  { type: 'heading', defaultValue: 'MASL Watchface' },
  {
    type: 'section',
    items: [
      { type: 'heading', defaultValue: 'Team' },
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
        description: "With no team picked, the league's next game always uses the split view.",
        defaultValue: 'logo',
        options: [
          { label: 'Big team logo', value: 'logo' },
          { label: "Split view: both logos, each team's color", value: 'split' }
        ]
      },
      {
        type: 'toggle',
        messageKey: 'HomeOnly',
        label: 'Home games only',
        description: 'Only applies when a team is picked.',
        defaultValue: false
      }
    ]
  },
  {
    type: 'section',
    items: [
      { type: 'heading', defaultValue: 'Next game' },
      {
        type: 'radiogroup',
        messageKey: 'Mode',
        label: 'Show',
        defaultValue: 'time',
        options: [
          { label: 'Start time', value: 'time' },
          { label: 'Countdown to start', value: 'countdown' }
        ]
      },
      {
        type: 'select',
        messageKey: 'CountdownDetail',
        label: 'Countdown shows',
        description: 'Seconds update every second while the countdown runs, which ' +
                     'uses more battery.',
        defaultValue: 'hours',
        options: [
          { label: 'Days and hours (in 3d 4h)', value: 'hours' },
          { label: 'Down to minutes (in 3d 4h 12m)', value: 'minutes' },
          { label: 'Down to seconds (3d 04:12:30)', value: 'seconds' }
        ]
      },
      {
        type: 'select',
        messageKey: 'TimeZone',
        label: 'Start times in',
        description: 'MASL lists every game in Eastern time.',
        defaultValue: 'eastern',
        options: [
          { label: 'Eastern (as listed)', value: 'eastern' },
          { label: 'My time zone', value: 'local' },
          { label: "Home team's time zone", value: 'home' }
        ]
      },
      {
        type: 'radiogroup',
        messageKey: 'GameClock',
        label: 'Start time clock',
        defaultValue: 'watch',
        options: [
          { label: "Same as the watch's clock", value: 'watch' },
          { label: '12 hour (7:05pm)', value: '12' },
          { label: '24 hour (19:05)', value: '24' }
        ]
      }
    ]
  },
  {
    type: 'section',
    capabilities: ['COLOR'],
    items: [
      { type: 'heading', defaultValue: 'Colors' },
      {
        type: 'select',
        messageKey: 'BarColor',
        label: 'Game bar color',
        description: "Primary and secondary are your team's colors. With no team " +
                     'picked the bar is black unless you choose Custom.',
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
        description: 'Used when the game bar color is Custom.',
        defaultValue: '00AAAA',
        sunlight: false
      },
      {
        type: 'select',
        messageKey: 'BgColor',
        label: 'Background color',
        description: "Custom replaces your team's color, in split view too. " +
                     'With a light color the time and date turn black.',
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
        description: 'Used when the background color is Custom and a team is picked.',
        defaultValue: '000000',
        sunlight: false
      }
    ]
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
        label: 'Middle position',
        description: 'Rectangular watches only. Balanced leaves the same space on ' +
                     'each side, up to the left and right items (or the screen edge ' +
                     'where one is empty), so it may sit a little off center. ' +
                     'Round watches center the whole row, in this order.',
        defaultValue: 'balanced',
        options: [
          { label: 'Balanced between left and right', value: 'balanced' },
          { label: 'Centered on the screen', value: 'center' }
        ]
      },
      {
        type: 'select',
        messageKey: 'DateFormat',
        label: 'Date format',
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
        label: 'When the row is full, drop the',
        description: 'Only when everything in the top row would not fit.',
        defaultValue: 'month',
        options: [
          { label: 'Month (WED 5)', value: 'month' },
          { label: 'Weekday (NOV 5 or 11/5)', value: 'weekday' }
        ]
      }
    ]
  },
  {
    type: 'section',
    items: [
      { type: 'heading', defaultValue: 'Clock' },
      {
        type: 'toggle',
        messageKey: 'Seconds',
        label: 'Show seconds',
        description: 'Redraws the face every second, which uses noticeably more battery.',
        defaultValue: false
      }
    ]
  },
  {
    type: 'section',
    items: [
      { type: 'heading', defaultValue: 'Weather' },
      {
        type: 'select',
        messageKey: 'Weather',
        label: 'Show weather',
        description: "From your phone's location via Open-Meteo. Updates every 30 " +
                     'minutes. Shows in whichever top row spot is set to Weather.',
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
        label: 'Units',
        defaultValue: 'f',
        options: [
          { label: 'Fahrenheit', value: 'f' },
          { label: 'Celsius', value: 'c' }
        ]
      },
    ]
  },
  {
    type: 'section',
    items: [
      { type: 'heading', defaultValue: 'Battery and Bluetooth' },
      {
        type: 'select',
        messageKey: 'Battery',
        label: 'Show battery',
        defaultValue: 'always',
        options: [
          { label: 'Always', value: 'always' },
          { label: 'When low (40% or less) or charging', value: 'low' },
          { label: 'Never', value: 'never' }
        ]
      },
      {
        type: 'select',
        messageKey: 'Bluetooth',
        label: 'Show Bluetooth',
        description: 'The icon has a slash through it while the phone is disconnected. ' +
                     'Scores and weather only update while connected.',
        defaultValue: 'disconnected',
        options: [
          { label: 'Only when disconnected', value: 'disconnected' },
          { label: 'Always', value: 'always' },
          { label: 'Never', value: 'never' }
        ]
      },
      swapSelect('LowBatteryZone', 'When the battery is low, show it in',
                 'At 40% or less that top row spot switches to the battery, ' +
                 'even if Show battery is Never, and back once it charges. ' +
                 'Does nothing if the battery is already in another spot.'),
      swapSelect('DisconnectedZone', 'When the phone disconnects, show Bluetooth in',
                 'That top row spot switches to the Bluetooth icon until the ' +
                 'phone reconnects. Does nothing if Bluetooth is already in ' +
                 'another spot. If both pick the same spot, it shows both.')
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
        description: 'Off is no-spoilers mode: live games show the period and clock, ' +
                     'never the score, and finished games are skipped.',
        defaultValue: false
      },
      {
        type: 'select',
        messageKey: 'LiveInterval',
        label: 'Update scores every',
        description: 'Only while a game is live and scores are on. ' +
                     'Otherwise the face checks every 5 minutes during a game ' +
                     'and hourly between games.',
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
        label: "Heart rate every minute during my team's games",
        description: "While your team's game is live, the watch reads your heart rate " +
                     'every minute instead of on its usual schedule, then goes back ' +
                     'when the game ends. Uses noticeably more battery during games. ' +
                     'Needs a team picked and a watch with a heart rate sensor.',
        defaultValue: false
      },
      {
        type: 'select',
        messageKey: 'LiveHeartRateZone',
        label: "During my team's games, show heart rate in",
        description: 'That top row spot switches to heart rate while the game is ' +
                     'live, and back when it ends. It keeps its usual item if heart ' +
                     'rate is already in another spot or there is no reading yet.',
        defaultValue: 'off',
        options: [
          { label: 'Off', value: 'off' },
          { label: 'Left spot', value: 'left' },
          { label: 'Middle spot', value: 'middle' },
          { label: 'Right spot', value: 'right' }
        ]
      }
    ]
  },
  { type: 'submit', defaultValue: 'Save' }
];
