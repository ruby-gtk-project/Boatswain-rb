generated_sources = []

boatswain_sources = [
  'bs-action-selector.c',
  'bs-application.c',
  'bs-button-editor.c',
  'bs-button-grid-widget.c',
  'bs-button-widget.c',
  'bs-device-editor.c',
  'bs-dial-widget.c',
  'bs-profile-row.c',
  'bs-selection-controller.c',
  'bs-touchscreen-background-row.c',
  'bs-touchscreen-background-dialog.c',
  'bs-touchscreen-widget.c',
  'bs-touchscreen-slot-editor.c',
  'bs-touchscreen-slot-widget.c',
  'bs-window.c',
  'main.c',
]

boatswain_deps = [
  dependency('libadwaita-1', version: '>= 1.6.alpha'),
  libboatswain_dep,
]

subdir('plugins')

generated_sources += gnome.compile_resources(
  'boatswain-resources',
  'boatswain.gresource.xml',
  c_name: 'boatswain'
)

executable('boatswain',
  boatswain_sources + generated_sources,
  dependencies: [boatswain_deps, plugins_dep],
  install: true,
)
