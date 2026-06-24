libboatswain_devices_ldflags += ['-Wl,--undefined=loupedeck_plugin_register_types']

libboatswain_devices_deps += dependency('libusb-1.0')

libboatswain_devices_sources += files(
  'loupedeck.c',
  'loupedeck-device.c',
  'loupedeck-device-provider.c',
  'razer-stream-controller-x.c',
)

libboatswain_devices_sources += gnome.compile_resources(
  'loupedeck-resources',
  'loupedeck.gresource.xml',
  c_name: 'loupedeck',
)
