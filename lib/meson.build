generated_sources = []

libboatswain_cflags = [
  '-DBOATSWAIN_COMPILATION',
]

libboatswain_headers = [
  'boatswain.h',
  'bs-actionable.h',
  'bs-action-factory.h',
  'bs-action-info.h',
  'bs-action.h',
  'bs-button.h',
  'bs-button-grid.h',
  'bs-context.h',
  'bs-desktop-controller.h',
  'bs-device.h',
  'bs-device-manager.h',
  'bs-device-region.h',
  'bs-dial.h',
  'bs-dial-grid.h',
  'bs-empty-action.h',
  'bs-events.h',
  'bs-icon.h',
  'bs-init.h',
  'bs-page.h',
  'bs-page-item.h',
  'bs-profile.h',
  'bs-renderer.h',
  'bs-touchscreen.h',
  'bs-touchscreen-content.h',
  'bs-touchscreen-slot.h',
  'bs-types.h',
]

libboatswain_sources = [
  'bs-actionable.c',
  'bs-action.c',
  'bs-action-factory.c',
  'bs-action-info.c',
  'bs-button.c',
  'bs-button-grid.c',
  'bs-context.c',
  'bs-desktop-controller.c',
  'bs-device.c',
  'bs-device-layout-builder.c',
  'bs-device-manager.c',
  'bs-device-provider.c',
  'bs-device-region.c',
  'bs-device-update.c',
  'bs-dial.c',
  'bs-dial-grid.c',
  'bs-empty-action.c',
  'bs-events.c',
  'bs-icon.c',
  'bs-init.c',
  'bs-log.c',
  'bs-page.c',
  'bs-page-item.c',
  'bs-profile.c',
  'bs-renderer.c',
  'bs-touchscreen.c',
  'bs-touchscreen-content.c',
  'bs-touchscreen-slot.c',
]

libboatswain_deps = [
  dependency('libpeas-2'),
  dependency('libportal-gtk4'),
  dependency('graphene-gobject-1.0'),
  dependency('gtk4'),
  dependency('gusb', version: '>= 0.3.0'),
  dependency('hidapi-libusb'),
  dependency('json-glib-1.0'),
  meson.get_compiler('c').find_library('m'),
]

debug_conf = configuration_data()
debug_conf.set10('ENABLE_TRACING', get_option('tracing'))
debug_conf.set('BUGREPORT_URL', 'https://gitlab.gnome.org/World/boatswain/-/issues/new')

generated_sources += configure_file(
  input: 'bs-debug.h.in',
  output: 'bs-debug.h',
  configuration: debug_conf,
)

generated_sources += gnome.mkenums_simple('bs-enum-types',
  body_prefix: '#include "bs-config.h"',
  sources: ['bs-page-item.h'],
  install_header: true,
)

libboatswain_include_directories = [
  include_directories('.'),
]

libboatswain_static = static_library('boatswain-static',
  libboatswain_sources + generated_sources,
  c_args: libboatswain_cflags,
  install: false,
  dependencies: libboatswain_deps,
  include_directories: libboatswain_include_directories,
)

libboatswain_static_dep = declare_dependency(
  dependencies: libboatswain_deps,
  include_directories: libboatswain_include_directories,
  link_whole: libboatswain_static,
)

libboatswain = shared_library('boatswain',
  dependencies: [libboatswain_static_dep],
  include_directories: libboatswain_include_directories,
  install: true,
)

libboatswain_dep = declare_dependency(
  dependencies: libboatswain_deps,
  include_directories: libboatswain_include_directories,
  link_with: libboatswain,
)

install_headers(libboatswain_headers, subdir: 'boatswain')

gir_headers = [
  'bs-actionable.h',
  'bs-action.h',
  'bs-action-factory.h',
  'bs-action-info.h',
  'bs-button.h',
  'bs-desktop-controller.h',
  'bs-device.h',
  'bs-empty-action.h',
  'bs-events.h',
  'bs-icon.h',
  'bs-page.h',
  'bs-page-item.h',
  'bs-profile.h',
  'bs-touchscreen.h',
  'bs-touchscreen-slot.h',
]

gnome.generate_gir(
  libboatswain,
  sources: libboatswain_sources + gir_headers,
  nsversion: '0',
  namespace: 'Bs',
  symbol_prefix: 'bs',
  identifier_prefix: 'Bs',
  includes: ['Gtk-4.0', 'Json-1.0', 'Peas-2'],
  install: true,
  export_packages: ['libboatswain'],
  extra_args: libboatswain_cflags,
)
