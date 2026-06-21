libboatswain_devices_ldflags += ['-Wl,--undefined=loupedeck_plugin_register_types']

libboatswain_devices_sources += files(
  'loupedeck.c',
  'loupedeck-device-provider.c',
)

libboatswain_devices_sources += gnome.compile_resources(
  'loupedeck-resources',
  'loupedeck.gresource.xml',
  c_name: 'loupedeck',
)
