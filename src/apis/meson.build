
# This section needs to be in a separate meson.build file to correctly place
# dlb.h into <builddir>/src/apis

dlb_h = configure_file(
  input : 'dlb.h.in',
  output : 'dlb.h',
  configuration : {
    'DLB_VERSION_HEXA' : dlb_version_hexa,
  },
)

headers = [
  dlb_h,
  'dlb_sp.h',
  'dlb_stats.h',
  'dlb_drom.h',
  'dlb_talp.h',
  'dlb_types.h',
  'dlb_errors.h',
  'dlbf.h',
  'dlbf-errors.h',
  'dlbf_drom.h',
  'dlbf_talp.h',
]

install_headers(headers)
