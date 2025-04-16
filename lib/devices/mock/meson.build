libboatswain_devices_ldflags += ['-Wl,--undefined=mock_plugin_register_types']

libboatswain_devices_sources += files(
  'mock.c',
  'mock-device.c',
  'mock-device-provider.c',
)

libboatswain_devices_sources += gnome.compile_resources(
  'mock-resources',
  'mock.gresource.xml',
  c_name: 'mock',
)
