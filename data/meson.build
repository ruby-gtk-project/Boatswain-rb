###########
# Desktop #
###########

desktop_conf = configuration_data()
desktop_conf.set('icon', application_id)
desktop_file = i18n.merge_file(
  input: configure_file(
    input: files('com.feaneron.Boatswain.desktop.in.in'),
    output: 'com.feaneron.Boatswain.desktop.in',
    configuration: desktop_conf,
  ),
  output: '@0@.desktop'.format(application_id),
  type: 'desktop',
  po_dir: '../po',
  install: true,
  install_dir: get_option('datadir') / 'applications',
)

desktop_utils = find_program('desktop-file-validate', required: false)
if desktop_utils.found()
  test('Validate desktop file', desktop_utils, args: [desktop_file])
endif


#############
# AppStream #
#############

appdata_conf = configuration_data()
appdata_conf.set('appid', application_id)
appstream_file = i18n.merge_file(
  input: configure_file(
    input: files('com.feaneron.Boatswain.appdata.xml.in.in'),
    output: 'com.feaneron.Boatswain.appdata.xml.in',
    configuration: appdata_conf
  ),
  output: '@0@.appdata.xml'.format(application_id),
  po_dir: '../po',
  install: true,
  install_dir: get_option('datadir') / 'appdata',
)

install_data('com.feaneron.Boatswain.gschema.xml',
  install_dir: get_option('datadir') / 'glib-2.0/schemas',
)

install_data(
  'devices.json',
  rename: '@0@.json'.format(application_id),
  install_dir: get_option('datadir') / 'xdg-desktop-portal' / 'devices',
)

compile_schemas = find_program('glib-compile-schemas', required: false)
if compile_schemas.found()
  test('Validate schema file', compile_schemas,
    args: ['--strict', '--dry-run', meson.current_source_dir()]
  )
endif

subdir('icons')
