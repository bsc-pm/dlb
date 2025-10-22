dlb_py_conf = configuration_data()
dlb_py_conf.set('prefix', get_option('prefix'))
dlb_py_conf.set('SIZEOF_CPU_SET_T', cc.sizeof('cpu_set_t', prefix: '#include <sched.h>'))

dlb_py_file = configure_file(
  input: 'dlb.py.in',
  output: 'dlb.py',
  configuration: dlb_py_conf
)

py = import('python').find_installation('python3')
py.install_sources(
  [dlb_py_file, 'dlb_mpi.py']
)
