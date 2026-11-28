// The time's fonts, as the settings page lists them. Keep the styles in the
// order of TF_* and the sizes as in s_time_fonts, both in src/c/main.c. A
// style with more than one size gets its own size picker, saved under
// sizeKey (also named in config-custom.js).
module.exports = [
  { value: 'leco', label: 'LECO (default)', sizes: [42, 28], sizeKey: 'TimeSizeLeco' },
  { value: 'lecoBold', label: 'LECO Bold', sizes: [38, 36, 32], sizeKey: 'TimeSizeLecoBold' },
  { value: 'bitham', label: 'Bitham', sizes: [42, 34], sizeKey: 'TimeSizeBitham' },
  { value: 'roboto', label: 'Roboto Bold (one size, a little larger)', sizes: [49] }
];
