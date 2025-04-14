libboatswain_devices_ldflags += ['-Wl,--undefined=elgato_plugin_register_types']

libboatswain_devices_sources += files(
  'elgato.c',
  'elgato-device-provider.c',
  'elgato-stream-deck.c',
)

libboatswain_devices_sources += gnome.compile_resources(
  'elgato-resources',
  'elgato.gresource.xml',
  c_name: 'elgato',
)
