generated_sources = []

boatswain_sources = [
  'bs-actionable.c',
  'bs-action.c',
  'bs-action-factory.c',
  'bs-action-info.c',
  'bs-application.c',
  'bs-button.c',
  'bs-button-editor.c',
  'bs-button-grid-region.c',
  'bs-button-grid-widget.c',
  'bs-button-widget.c',
  'bs-desktop-controller.c',
  'bs-device-editor.c',
  'bs-device-manager.c',
  'bs-device-region.c',
  'bs-dial.c',
  'bs-dial-grid-region.c',
  'bs-dial-widget.c',
  'bs-empty-action.c',
  'bs-events.c',
  'bs-icon.c',
  'bs-log.c',
  'bs-page.c',
  'bs-page-item.c',
  'bs-profile.c',
  'bs-profile-row.c',
  'bs-renderer.c',
  'bs-selection-controller.c',
  'bs-stream-deck.c',
  'bs-touchscreen.c',
  'bs-touchscreen-content.c',
  'bs-touchscreen-region.c',
  'bs-touchscreen-widget.c',
  'bs-touchscreen-slot.c',
  'bs-touchscreen-slot-widget.c',
  'bs-window.c',
]

boatswain_deps = [
  dependency('libadwaita-1', version: '>= 1.6.alpha'),
  dependency('libpeas-2'),
  dependency('libportal-gtk4'),
  dependency('gusb', version: '>= 0.3.0'),
  dependency('gtk4', version: '>= 4.12'),
  dependency('hidapi-libusb'),
  dependency('json-glib-1.0'),
]

subdir('plugins')

gnome = import('gnome')

debug_conf = configuration_data()
debug_conf.set10('ENABLE_TRACING', get_option('tracing'))
debug_conf.set('BUGREPORT_URL', 'https://gitlab.gnome.org/World/boatswain/-/issues/new')

generated_sources += configure_file(
  input: 'bs-debug.h.in',
  output: 'bs-debug.h',
  configuration: debug_conf,
)

generated_sources += gnome.compile_resources(
  'boatswain-resources',
  'boatswain.gresource.xml',
  c_name: 'boatswain'
)

generated_sources += gnome.mkenums(
  'boatswain-enum-types',
  sources: [ 'bs-page-item.h' ],
  h_template: 'bs-enum-types.h.in',
  c_template: 'bs-enum-types.c.in',
)

libboatswain_private = static_library('boatswain-private',
  boatswain_sources + generated_sources,
  install: false,
  dependencies: [boatswain_deps, plugins_dep],
)

libboatswain_private_dep = declare_dependency(
  sources: generated_sources,
  dependencies: [boatswain_deps, plugins_dep],
  link_with: libboatswain_private,
)

executable(
  'boatswain',
  files('main.c'),
  dependencies: [libboatswain_private_dep],
  install: true,
)

gir_headers = [
  'bs-actionable.h',
  'bs-action.h',
  'bs-action-factory.h',
  'bs-action-info.h',
  'bs-button.h',
  'bs-desktop-controller.h',
  'bs-empty-action.h',
  'bs-events.h',
  'bs-icon.h',
  'bs-page.h',
  'bs-page-item.h',
  'bs-profile.h',
  'bs-stream-deck.h',
]

gnome.generate_gir(
  libboatswain_private,
  sources: boatswain_sources + gir_headers,
  nsversion: '0',
  namespace: 'Bs',
  symbol_prefix: 'bs',
  identifier_prefix: 'Bs',
  includes: ['Gtk-4.0', 'Json-1.0', 'Peas-2'],
  install: true,
  export_packages: ['libboatswain_private'],
)
