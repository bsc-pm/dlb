project('DLB', 'c', 'fortran',
  version : '3.1a',
  license: ' LGPL-3.0-or-later',
  default_options : [
  ]
)

cc = meson.get_compiler('c')


### Dependencies

m_dep = cc.find_library('m', required : true)
rt_dep = cc.find_library('rt', required : true)
dl_dep = cc.find_library('dl', required : true)
thread_dep = dependency('threads')
hwloc_dep = dependency('hwloc', required : get_option('hwloc'))
common_deps = [m_dep, rt_dep, dl_dep, thread_dep, hwloc_dep]
mpi_dep = dependency('mpi', language : 'c', required : get_option('mpi'))
mpif_dep = dependency('mpi', language : 'fortran', required : get_option('mpi'))


### CFLAGS

add_project_arguments('-DHAVE_CONFIG_H', language : 'c')
add_project_arguments('-D_GNU_SOURCE', language : 'c')
if hwloc_dep.found()
  add_project_arguments('-DHWLOC_LIB', language : 'c')
endif

instr_cpp_flags = '-DINSTRUMENTATION_VERSION'

if get_option('buildtype') == 'debug'
  extra_arguments = [
    # '-Wall',  # default warning_level is already 1
    '-Werror',
    '-Wshadow',
    '-Wno-error=shadow',
    '-Wno-unused-result',
    '-Wformat-security',
    '-Wsign-compare',
    '-Wimplicit-fallthrough',
    '-Wvla',
  ]

  foreach arg : extra_arguments
    if cc.has_argument(arg)
      add_project_arguments(arg, language : 'c')
    endif
  endforeach

  add_project_arguments('-DDEBUG_VERSION', language : 'c')
endif


#################################################################################
### Headers                                                                   ###
#################################################################################

headers = [
  'src/apis/dlb.h',
  'src/apis/dlb_sp.h',
  'src/apis/dlb_stats.h',
  'src/apis/dlb_drom.h',
  'src/apis/dlb_types.h',
  'src/apis/dlb_errors.h',
  'src/apis/dlbf.h',
  'src/apis/dlbf-errors.h',
  'src/apis/dlb_talp.h',
  'src/apis/dlbf_talp.h',
]

install_headers(headers)


#################################################################################
### DLB libraries                                                             ###
#################################################################################

common_includes = include_directories('src')

common_srcs = [
  'src/support/atomic.h',
  'src/support/env.c',
  'src/support/env.h',
  'src/support/error.c',
  'src/support/error.h',
  'src/support/gtree.c',
  'src/support/gtree.h',
  'src/support/mask_utils.c',
  'src/support/mask_utils.h',
  'src/support/mytime.c',
  'src/support/mytime.h',
  'src/support/options.c',
  'src/support/options.h',
  'src/support/queues.c',
  'src/support/queues.h',
  'src/support/small_array.h',
  'src/support/types.c',
  'src/support/types.h',
  'src/LB_comm/shmem.c',
  'src/LB_comm/shmem.h',
  'src/LB_comm/shmem_async.c',
  'src/LB_comm/shmem_async.h',
  'src/LB_comm/shmem_barrier.c',
  'src/LB_comm/shmem_barrier.h',
  'src/LB_core/lb_funcs.c',
  'src/LB_core/lb_funcs.h',
  'src/LB_core/spd.c',
  'src/LB_core/spd.h',
  'src/apis/DLB_interface.c',
  'src/apis/DLB_interfaceF.c',
  'src/apis/DLB_interface_drom.c',
  'src/apis/DLB_interface_sp.c',
  'src/apis/DLB_interface_stats.c',
  'src/apis/DLB_interface_talp.c',
]

libdlb_common = static_library(
  'dlb_common', common_srcs,
  include_directories : common_includes,
  install : false,
)

dlb_common_objects = libdlb_common.extract_all_objects(recursive : true)

sources = [
  'src/support/debug.c',
  'src/support/debug.h',
  'src/support/tracing.c',
  'src/support/tracing.h',
  'src/LB_numThreads/numThreads.c',
  'src/LB_numThreads/numThreads.h',
  'src/LB_numThreads/omp_thread_manager.c',
  'src/LB_numThreads/omp_thread_manager.h',
  'src/LB_numThreads/omp_tool.c',
  'src/LB_numThreads/ompt.h',
  'src/LB_comm/comm_lend_light.c',
  'src/LB_comm/comm_lend_light.h',
  'src/LB_comm/shmem_cpuinfo.c',
  'src/LB_comm/shmem_cpuinfo.h',
  'src/LB_comm/shmem_procinfo.c',
  'src/LB_comm/shmem_procinfo.h',
  'src/LB_policies/lewi.c',
  'src/LB_policies/lewi.h',
  'src/LB_policies/lewi_mask.c',
  'src/LB_policies/lewi_mask.h',
  'src/LB_core/DLB_kernel.c',
  'src/LB_core/DLB_kernel.h',
  'src/LB_core/DLB_talp.c',
  'src/LB_core/DLB_talp.h',
]

libdlb = library(
  'dlb', sources,
  include_directories : common_includes,
  objects : dlb_common_objects,
  dependencies : common_deps,
  gnu_symbol_visibility : 'hidden',
  install : true,
)

libdlb_instr = library(
  'dlb_instr', sources,
  include_directories : common_includes,
  c_args : instr_cpp_flags,
  objects : dlb_common_objects,
  dependencies : common_deps,
  gnu_symbol_visibility : 'hidden',
  install : true,
)


#################################################################################
### DLB MPI libraries                                                         ###
#################################################################################

if mpi_dep.found()
  mpi_cpp_flags = '-DMPI_LIB'

  prog_python = import('python').find_installation('python')
  gen = generator(prog_python,
    output : '@BASENAME@',
    arguments : [
      '@SOURCE_DIR@/scripts/pygen.py',
      '--ifile', '@INPUT@', '--ofile', '@OUTPUT@',
      '--json', '@SOURCE_DIR@/src/LB_MPI/mpicalls.json',
    ]
  )

  mpi_common_sources = [
    'src/LB_MPI/DPD.c',
    'src/LB_MPI/DPD.h',
    'src/LB_MPI/process_MPI.c',
    'src/LB_MPI/process_MPI.h',
  ]

  mpi_common_gen = gen.process(
    'src/LB_MPI/MPI_calls_coded.c.in',
    'src/LB_MPI/MPI_calls_coded.h.in',
    preserve_path_from : '@0@/src'.format(meson.current_source_dir()),
  )

  mpi_gen_sources = gen.process(
    'src/LB_MPI/MPI_interface.c.in',
    'src/LB_MPI/MPI_interface.h.in',
    preserve_path_from : '@0@/src'.format(meson.current_source_dir()),
  )

  mpif_gen_sources = gen.process(
    'src/LB_MPI/MPI_interfaceF.c.in',
    'src/LB_MPI/MPI_interfaceF.h.in',
    preserve_path_from : '@0@/src'.format(meson.current_source_dir()),
  )

  mpi_gen_intercept_sources = gen.process(
    'src/LB_MPI/MPI_intercept.c.in',
    preserve_path_from : '@0@/src'.format(meson.current_source_dir()),
  )

  mpif_gen_intercept_sources = gen.process(
    'src/LB_MPI/MPI_interceptF.c.in',
    preserve_path_from : '@0@/src'.format(meson.current_source_dir()),
  )

  mpi_sources = [sources, mpi_common_sources, mpi_common_gen, mpi_gen_sources]
  mpif_sources = [sources, mpi_common_sources, mpi_common_gen, mpif_gen_sources]

  libdlb_mpi = library(
    'dlb_mpi', [mpi_sources, mpi_gen_intercept_sources],
    include_directories : common_includes,
    c_args : mpi_cpp_flags,
    objects : dlb_common_objects,
    dependencies : [common_deps, mpi_dep],
    gnu_symbol_visibility : 'hidden',
    install : true,
  )

  libdlb_mpi_instr = library(
    'dlb_mpi_instr', mpi_sources,
    include_directories : common_includes,
    c_args : [mpi_cpp_flags, instr_cpp_flags],
    objects : dlb_common_objects,
    dependencies : [common_deps, mpi_dep],
    gnu_symbol_visibility : 'hidden',
    install : true,
  )

  libdlb_mpif = library(
    'dlb_mpif', [mpif_sources, mpif_gen_intercept_sources],
    include_directories : common_includes,
    c_args : mpi_cpp_flags,
    objects : dlb_common_objects,
    dependencies : [common_deps, mpif_dep],
    gnu_symbol_visibility : 'hidden',
    install : true,
  )

  libdlb_mpif_instr = library(
    'dlb_mpif_instr', mpif_sources,
    include_directories : common_includes,
    c_args : [mpi_cpp_flags, instr_cpp_flags],
    objects : dlb_common_objects,
    dependencies : [common_deps, mpif_dep],
    gnu_symbol_visibility : 'hidden',
    install : true,
  )

  check_mpi_library_version = '''
    #include <mpi.h>
    #include <stdio.h>
    #include <stdlib.h>
    int main(int argc, char *argv[])
    {
        char version[MPI_MAX_LIBRARY_VERSION_STRING];
        int resultlen;
        MPI_Get_library_version(version, &resultlen);
        char *newline = strchr(version, '\n');
        if (newline) *newline = '\0';
        printf("%s", version);
        return EXIT_SUCCESS;
    }
    '''
  runresult = cc.run(check_mpi_library_version,
    dependencies : mpi_dep,
  )
  mpi_library_version = runresult.stdout()
else
  mpi_library_version = ''
endif


#################################################################################
### Binaries                                                                  ###
#################################################################################

binaries = [
  'dlb',
  'dlb_run',
  'dlb_shm',
  'dlb_taskset',
]

foreach bin : binaries
  executable(bin, 'src/utils/' + bin + '.c',
    include_directories : common_includes,
    link_with : libdlb,
    dependencies : common_deps,
    install : true,
    install_rpath : '$ORIGIN/../' + get_option('libdir'),
  )
endforeach


#################################################################################
### Data                                                                      ###
#################################################################################
#********************************************************************************
# Paraver cfgs
#********************************************************************************

# paraver_cfgs = [
#   'doc/paraver_cfgs/DLB_cpus(4).cfg',
#   'doc/paraver_cfgs/DLB_cpus(8).cfg',
#   'doc/paraver_cfgs/DLB_cpus(16).cfg',
#   'doc/paraver_cfgs/DLB_idle_cpus.cfg',
#   'doc/paraver_cfgs/DLB_mode.cfg',
#   'doc/paraver_cfgs/DLB_runtime.cfg',
#   'doc/paraver_cfgs/DLB_num_threads.cfg',
#   'doc/paraver_cfgs/DLB_rebind_threads.cfg',
#   'doc/paraver_cfgs/DLB_loop.cfg',
#   'doc/paraver_cfgs/DLB_regions.cfg',
#   'doc/paraver_cfgs/DLB_Give_Want_number_of_cpus.cfg',
#   'doc/paraver_cfgs/DLB_callbacks.cfg',
# ]
#
# paraver_cfgs_dir =  get_option('datadir') + '/paraver_cfgs/DLB'
# install_data(paraver_cfgs, install_dir : paraver_cfgs_dir)

#********************************************************************************
# Examples
#********************************************************************************

# docdir = get_option('datadir') + '/share/doc/dlb'

### DROM ###
# dromexamples_files = [
#   'doc/examples/drom/drom_01.c',
#   'doc/examples/drom/README',
# ]

# dromexamples_gen = custom_target('drom_makefile',
#   input : ,
#   output : ,
#   command : ,
# )

# dromexamples_dir = docdir + 'examples/DROM'
# install_data(
#   [dromexamples_files, dromexamples_gen],
#   install_dir : dromexamples_dir,
# )

#********************************************************************************
# User scripts
#********************************************************************************

#********************************************************************************
# Man pages
#********************************************************************************

#################################################################################
### Tests                                                                     ###
#################################################################################

subdir('tests')


#################################################################################
### config.h                                                                  ###
#################################################################################

conf_data = configuration_data()
conf_data.set_quoted('VERSION', meson.project_version())
conf_data.set_quoted('PACKAGE', meson.project_name())
conf_data.set_quoted('PACKAGE_BUGREPORT', 'pm-tools@bsc.es')
conf_data.set_quoted('DLB_BUILD_VERSION', 'meson')
conf_data.set_quoted('DLB_CONFIGURE_ARGS', 'not supported')
conf_data.set('HAVE_EMMINTRIN_H', cc.has_header('emmintrin.h'))
conf_data.set('HAVE_EXECINFO_H', cc.has_header('execinfo.h'))
conf_data.set('HAVE_IMMINTRIN_H', cc.has_header('immintrin.h'))
conf_data.set('HAVE_STDATOMIC_H', cc.has_header('stdatomic.h'))
conf_data.set_quoted('MPI_LIBRARY_VERSION', mpi_library_version)
config_h = configure_file( output : 'config.h', configuration : conf_data)


#################################################################################
### Summary                                                                   ###
#################################################################################

debug_flag = ''
if get_option('debug')
  debug_flag = ', -g'
endif
build_type_descr = '@0@ (-O@1@@2@)'.format(
  get_option('buildtype'),
  get_option('optimization'),
  debug_flag,
  )

summary({'prefix': get_option('prefix'),
        }, section: 'Directories')
summary({'CC': cc.get_id() + ' ' + cc.version(),
         'Build': build_type_descr,
        }, section: 'Compilation')
summary({'HWLOC': hwloc_dep.found(),
         'MPI': mpi_dep.found(),
        }, section: 'Configuration')
