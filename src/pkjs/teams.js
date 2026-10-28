// Teams in the score ticker. `name` is the short name exactly as the ticker
// shows it; keep it in sync with s_teams in main.c. `zone` is the arena's
// time zone: hours behind UTC in standard time, and the zone letter for the
// abbreviation ("C" -> "CST"/"CDT").
module.exports = [
  { name: 'Ambush', full: 'St. Louis Ambush', zone: [6, 'C'] },
  { name: 'Blast', full: 'Baltimore Blast', zone: [5, 'E'] },
  { name: 'Comets', full: 'Kansas City Comets', zone: [6, 'C'] },
  { name: 'Sockers', full: 'San Diego Sockers', zone: [8, 'P'] },
  { name: 'Spirits', full: 'Lehigh Valley Spirits', zone: [5, 'E'] },
  { name: 'Stars', full: 'Tacoma Stars', zone: [8, 'P'] },
  { name: 'Strykers', full: 'Empire Strykers', zone: [8, 'P'] },
  { name: 'UCFC', full: 'Utica City FC', zone: [5, 'E'] },
  { name: 'Wave', full: 'Milwaukee Wave', zone: [6, 'C'] }
];
