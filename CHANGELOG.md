# Changelog

Changes to the MASL Watchface, newest first. Anything merged that isn't in
the Pebble appstore yet goes under **Unreleased**. When you publish, rename
that heading to the new version and date, set the same version in
`package.json`, and start a fresh **Unreleased** section above it.

## Picking the version number

Count from the last published version, using the biggest kind of change
listed under Unreleased:

- **Major** (1.x.x → 2.0.0): users lose something or have to set things up
  again, e.g. a setting removed or the saved settings reset.
- **Minor** (1.1.x → 1.2.0): anything new users can see or turn on.
- **Patch** (1.1.0 → 1.1.1): only fixes, with no new features.

## Unreleased

Suggested version: **1.2.0** (it has a new feature).

### Added

- The game bar shows the game clock during live games, as time left in the
  period like the arena clock: "LIVE - 2nd 9:39". It's as of the last score
  update, so it doesn't tick between updates. Where the line is too wide for
  the screen, "LIVE - " is dropped. (#11)
- A font and size for the time, under Look in settings: LECO (42 or 28),
  LECO Bold (38, 36 or 32), Bitham (42 or 34) or Roboto Bold (49), all built
  into the watch. The default stays LECO 42. If the time is too wide for the
  screen in the chosen size, the next size down is used.

### Fixed

- Heart rate never showed, either in its own top row spot or when it was set
  to take over a spot during your team's live games. (#12)

## 1.1.0 (2026-11-19)

- Larger text options, battery percentage, the UV index and a Quick View
  option that keeps the logo. (#9)
