libboatswain_devices_ldflags += ['-Wl,--undefined=elgato_plugin_register_types']

libboatswain_devices_deps += [
  dependency('hidapi-libusb'),
  dependency('gudev-1.0'),
]

libboatswain_devices_sources += files(
  'elgato.c',
  'elgato-device-provider.c',
  'elgato-stream-deck.c',
  'elgato-stream-deck-gen1.c',
  'elgato-stream-deck-gen2.c',
  'elgato-stream-deck-mini.c',
  'elgato-stream-deck-mk2.c',
  'elgato-stream-deck-neo.c',
  'elgato-stream-deck-original.c',
  'elgato-stream-deck-original-v2.c',
  'elgato-stream-deck-pedal.c',
  'elgato-stream-deck-plus.c',
  'elgato-stream-deck-xl.c',
)

libboatswain_devices_sources += gnome.compile_resources(
  'elgato-resources',
  'elgato.gresource.xml',
  c_name: 'elgato',
)
