// Settings page, built by Clay (https://github.com/pebble-dev/clay).

var TEAMS = require('./teams');

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
      { type: 'heading', defaultValue: 'Battery' },
      {
        type: 'select',
        messageKey: 'Battery',
        label: 'Show battery',
        defaultValue: 'always',
        options: [
          { label: 'Always', value: 'always' },
          { label: 'When low (20% or less) or charging', value: 'low' },
          { label: 'Never', value: 'never' }
        ]
      }
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
      }
    ]
  },
  { type: 'submit', defaultValue: 'Save' }
];
