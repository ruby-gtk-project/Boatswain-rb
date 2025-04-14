libboatswain_devices_incs = [
  include_directories('..'),
]

libboatswain_devices_sources = []
libboatswain_devices_cflags = [ libboatswain_cflags ]
libboatswain_devices_deps = [ libboatswain_deps ]
libboatswain_devices_ldflags = []
libboatswain_devices_libs = []
libboatswain_devices_confs = []

subdir('elgato')

libboatswain_devices_lib = static_library(
  'boatswain-devices',
  libboatswain_devices_sources,
  c_args: libboatswain_devices_cflags,
  dependencies: libboatswain_devices_deps,
  include_directories: libboatswain_devices_incs,
  link_with: libboatswain_devices_libs,
  link_args: libboatswain_devices_ldflags,
)

libboatswain_devices_dep = declare_dependency(
  link_whole: libboatswain_devices_lib,
)
