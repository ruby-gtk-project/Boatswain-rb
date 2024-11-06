plugins_ldflags += ['-Wl,--undefined=default_plugin_register_types']

plugin_file = i18n.merge_file(
  input: 'default.plugin.desktop.in',
  output: 'default.plugin',
  type: 'desktop',
  po_dir: meson.project_source_root() / 'po',
)

plugins_sources += files(
  'default-action-factory.c',
  'default-brightness-action.c',
  'default-multi-action.c',
  'default-multi-action-editor.c',
  'default-multi-action-row.c',
  'default-page-up-action.c',
  'default-plugin.c',
  'default-switch-page-action.c',
  'default-switch-profile-action.c',
)

plugins_sources += gnome.compile_resources(
  'default-resources',
  'default.gresource.xml',
  c_name: 'default_plugin',
  dependencies: [ plugin_file ],
)
