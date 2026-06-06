project('boatswain', 'c',
          version: '6.1',
    meson_version: '>= 0.59',
  default_options: [ 'warning_level=1', 'c_std=gnu11' ],
)

i18n = import('i18n')
gnome = import('gnome')

###########
# Profile #
###########

profile = get_option('profile')
if profile == 'development'
  find_program('git')
  rev_txt = run_command('git','rev-parse','--short','HEAD').stdout().strip()
  rev = '-@0@'.format(rev_txt)
  application_id = 'com.feaneron.Boatswain.Devel'
else
  rev = ''
  application_id = 'com.feaneron.Boatswain'
endif

##########
# devenv #
##########
devenv = environment()

devenv.set('XDG_CONFIG_HOME', meson.global_build_root() / 'config')
devenv.set('XDG_CACHE_HOME', meson.global_build_root() / 'cache')
devenv.set('XDG_DATA_HOME', meson.global_build_root() / 'share')
devenv.set('XDG_STATE_HOME', meson.global_build_root() / 'state')

meson.add_devenv(devenv)

############
# config.h #
############

config_h = configuration_data()
config_h.set_quoted('APPLICATION_ID', application_id)
config_h.set_quoted('GETTEXT_PACKAGE', 'boatswain')
config_h.set_quoted('LOCALEDIR', get_option('prefix') / get_option('localedir'))
config_h.set_quoted('PACKAGE_VERSION', meson.project_version())
config_h.set_quoted('PROFILE', profile)
configure_file(
  output: 'bs-config.h',
  configuration: config_h,
)
add_project_arguments([
  '-I' + meson.current_build_dir(),
], language: 'c')

subdir('data')
subdir('lib')
subdir('src')
subdir('po')

summary({
  'Tracing': get_option('tracing'),
  'Profile': get_option('profile'),
}, section: 'Development')

gnome.post_install(
  glib_compile_schemas: true,
  gtk_update_icon_cache: true,
  update_desktop_database: true,
)
