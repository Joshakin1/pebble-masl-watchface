// Runs inside the settings page (Clay turns it into a string, so it can't use
// anything from outside the function). Shows "When the row is full, drop
// the" only for date formats that have both a weekday and a month.

module.exports = function () {
  var clayConfig = this;
  var BOTH = ['dayMonthDate', 'dayMD', 'dayDM'];

  clayConfig.on(clayConfig.EVENTS.AFTER_BUILD, function () {
    var format = clayConfig.getItemByMessageKey('DateFormat');
    var drop = clayConfig.getItemByMessageKey('DateDrop');
    if (!format || !drop) return;
    var update = function () {
      if (BOTH.indexOf(format.get()) !== -1) {
        drop.show();
      } else {
        drop.hide();
      }
    };
    update();
    format.on('change', update);
  });
};
