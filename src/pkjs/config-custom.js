// Runs inside the settings page (Clay turns it into a string, so it can't use
// anything from outside the function). Hides the settings that don't matter
// with the current choices, so the page only shows what applies.

module.exports = function () {
  var clayConfig = this;

  clayConfig.on(clayConfig.EVENTS.AFTER_BUILD, function () {
    // Items left out for this watch (like colors on black and white) are
    // missing, so everything here has to cope with that.
    var item = function (key) { return clayConfig.getItemByMessageKey(key); };
    var get = function (key) { var i = item(key); return i ? i.get() : undefined; };
    var showIf = function (i, on) { if (i) { if (on) i.show(); else i.hide(); } };
    var show = function (key, on) { showIf(item(key), on); };

    var SWAPS = ['LowBatteryZone', 'DisconnectedZone', 'QuietTimeZone', 'LiveHeartRateZone'];
    var BOTH = ['dayMonthDate', 'dayMD', 'dayDM'];  // date formats with a weekday and a month
    var swaps = clayConfig.getItemById('Swaps');

    var update = function () {
      var team = !!get('Team');
      var zones = [get('ZoneLeft'), get('ZoneMiddle'), get('ZoneRight')];
      var has = function (z) { return zones.indexOf(z) !== -1; };
      var battery = has('battery') || has('status');
      var swapsOn = !!(swaps && swaps.get());

      // Team and game
      show('View', team);
      show('HomeOnly', team);
      show('NoTeamGame', team);
      show('CountdownDetail', get('Mode') === 'countdown');
      show('GameClock', get('Mode') !== 'countdown');

      // Look: the custom background only replaces the picked team's color.
      show('CustomColor', get('BarColor') === 'custom');
      show('BgColor', team);
      show('CustomBg', team && get('BgColor') === 'custom');

      // Top row: each item's options only while it has a spot.
      show('MiddleAlign', get('ZoneMiddle') !== 'none');
      show('DateFormat', has('date'));
      show('DateDrop', has('date') && BOTH.indexOf(get('DateFormat')) !== -1);
      show('Weather', has('weather'));
      show('TempUnits', has('weather') && get('Weather') !== 'off');
      show('Battery', battery);
      var levels = (battery && get('Battery') !== 'never') ||
                   (swapsOn && get('LowBatteryZone') !== 'off');
      show('BatteryLow', levels);
      show('BatteryCritical', levels);
      show('Bluetooth', has('bluetooth') || has('status'));

      // Switch spots automatically
      showIf(clayConfig.getItemById('SwapsNote'), swapsOn);
      SWAPS.forEach(function (key) {
        show(key, swapsOn && (key !== 'LiveHeartRateZone' || team));
      });

      // Live games
      show('LiveInterval', !!get('ShowScores'));
      show('LiveHeartRate', team);
    };

    if (swaps) {
      swaps.set(SWAPS.some(function (key) {
        var v = get(key);
        return v && v !== 'off';
      }));
      // Turning it off means nothing switches, not just that the choices hide.
      swaps.on('change', function () {
        if (!swaps.get()) {
          SWAPS.forEach(function (key) { var i = item(key); if (i) i.set('off'); });
        }
        update();
      });
    }

    ['Team', 'Mode', 'BarColor', 'BgColor', 'ZoneLeft', 'ZoneMiddle', 'ZoneRight',
     'DateFormat', 'Weather', 'Battery', 'LowBatteryZone', 'ShowScores']
      .forEach(function (key) { var i = item(key); if (i) i.on('change', update); });
    update();
  });
};
