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

metainfo_conf = configuration_data()
metainfo_conf.set('appid', application_id)
appstream_file = i18n.merge_file(
  input: configure_file(
    input: files('com.feaneron.Boatswain.metainfo.xml.in.in'),
    output: 'com.feaneron.Boatswain.metainfo.xml.in',
    configuration: metainfo_conf
  ),
  output: '@0@.metainfo.xml'.format(application_id),
  po_dir: '../po',
  install: true,
  install_dir: get_option('datadir') / 'metainfo',
)

# Validate AppStream file
appstreamcli = find_program('appstreamcli', required: false)
if (appstreamcli.found())
  test('Validate metainfo file',
    appstreamcli,
    args: ['validate', '--no-net', '--explain', appstream_file],
    workdir: meson.current_build_dir()
  )
endif

install_data('com.feaneron.Boatswain.gschema.xml',
  rename: '@0@.gschema.xml'.format(application_id),
  install_dir: get_option('datadir') / 'glib-2.0' / 'schemas',
)

# Compile the schema in the current directory
gnome.compile_schemas(depend_files: ['com.feaneron.Boatswain.gschema.xml'])

compile_schemas = find_program('glib-compile-schemas', required: false)
if compile_schemas.found()
  test('Validate schema file', compile_schemas,
    args: ['--strict', '--dry-run', meson.current_source_dir()]
  )
endif

subdir('icons')
