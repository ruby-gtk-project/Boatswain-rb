plugins_ldflags += ['-Wl,--undefined=obs_studio_plugin_register_types']

plugin_file = i18n.merge_file(
  input: 'obs-studio.plugin.desktop.in',
  output: 'obs-studio.plugin',
  type: 'desktop',
  po_dir: meson.project_source_root() / 'po',
)

plugins_sources += files(
  'obs-action.c',
  'obs-action-factory.c',
  'obs-connection.c',
  'obs-connection-manager.c',
  'obs-connection-settings.c',
  'obs-record-action.c',
  'obs-scene.c',
  'obs-source.c',
  'obs-stream-action.c',
  'obs-studio-plugin.c',
  'obs-switch-scene-action.c',
  'obs-toggle-source-action.c',
  'obs-utils.c',
  'obs-virtualcam-action.c',
)

plugins_sources += gnome.mkenums_simple('obs-enum-types', sources: ['obs-types.h'])

plugins_incs += [
  include_directories('.'),
]

plugins_deps += [
  dependency('libsecret-1'),
  dependency('libsoup-3.0'),
]

plugins_sources += gnome.compile_resources(
  'obs-studio-resources',
  'obs-studio.gresource.xml',
  c_name: 'obs_studio_plugin',
  dependencies: [ plugin_file ],
)
