project('dlb', 'c', 'fortran',
  version : run_command('scripts/git-version-gen', '.tarball-version',
    check : true).stdout().strip(),
  license: ' LGPL-3.0-or-later',
  default_options : [
  ]
)

cc = meson.get_compiler('c')
fc = meson.get_compiler('fortran')


### Dependencies

m_dep = cc.find_library('m', required : true)
rt_dep = cc.find_library('rt', required : true)
dl_dep = cc.find_library('dl', required : true)
thread_dep = dependency('threads')
hwloc_dep = dependency('hwloc', required : get_option('hwloc'))
papi_dep = dependency('papi', required : get_option('papi'))
common_deps = [m_dep, rt_dep, dl_dep, thread_dep, hwloc_dep, papi_dep]
mpi_dep = dependency('mpi', language : 'c', required : get_option('mpi'))
mpif_dep = dependency('mpi', language : 'fortran', required : get_option('mpi'))


### CFLAGS

add_project_arguments('-DHAVE_CONFIG_H', language : 'c')
add_project_arguments('-D_GNU_SOURCE', language : 'c')
if hwloc_dep.found()
  add_project_arguments('-DHWLOC_LIB', language : 'c')
endif
if papi_dep.found()
  add_project_arguments('-DPAPI_LIB', language : 'c')
endif

instr_cpp_flags = '-DINSTRUMENTATION_VERSION'

if get_option('buildtype') == 'debug'
  # C arguments
  extra_cc_arguments = [
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

  foreach arg : extra_cc_arguments
    if cc.has_argument(arg)
      add_project_arguments(arg, language : 'c')
    endif
  endforeach

  add_project_arguments('-DDEBUG_VERSION', language : 'c')

  # Fortran arguments
  extra_fc_arguments = [
    '-Wno-c-binding-type',
  ]

  foreach arg : extra_fc_arguments
    if fc.has_argument(arg)
      add_project_arguments(arg, language : 'fortran')
    endif
  endforeach
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
  'src/support/array_template.h',
  'src/support/env.c',
  'src/support/env.h',
  'src/support/error.c',
  'src/support/error.h',
  'src/support/gslist.c',
  'src/support/gslist.h',
  'src/support/gtree.c',
  'src/support/gtree.h',
  'src/support/gtypes.h',
  'src/support/mask_utils.c',
  'src/support/mask_utils.h',
  'src/support/mytime.c',
  'src/support/mytime.h',
  'src/support/options.c',
  'src/support/options.h',
  'src/support/perf_metrics.h',
  'src/support/queue_template.h',
  'src/support/queues.c',
  'src/support/queues.h',
  'src/support/small_array.h',
  'src/support/talp_output.c',
  'src/support/talp_output.h',
  'src/support/types.c',
  'src/support/types.h',
  'src/LB_comm/shmem.c',
  'src/LB_comm/shmem.h',
  'src/LB_comm/shmem_async.c',
  'src/LB_comm/shmem_async.h',
  'src/LB_comm/shmem_barrier.c',
  'src/LB_comm/shmem_barrier.h',
  'src/LB_comm/shmem_talp.c',
  'src/LB_comm/shmem_talp.h',
  'src/LB_core/lb_funcs.c',
  'src/LB_core/lb_funcs.h',
  'src/LB_core/spd.c',
  'src/LB_core/spd.h',
  'src/LB_core/talp_types.h',
  'src/LB_numThreads/omp-tools.h',
  'src/LB_numThreads/ompt-multiplex.h',
  'src/apis/DLB_interface.c',
  'src/apis/DLB_interfaceF.c',
  'src/apis/DLB_interface_drom.c',
  'src/apis/DLB_interface_sp.c',
  'src/apis/DLB_interface_stats.c',
  'src/apis/DLB_interface_talp.c',
]

# Convenience library to speed up compilation
libdlb_common = static_library(
  'dlb_common', common_srcs,
  include_directories : common_includes,
  gnu_symbol_visibility : 'hidden',
  install : false,
)

sources = [
  'src/support/debug.c',
  'src/support/debug.h',
  'src/support/tracing.c',
  'src/support/tracing.h',
  'src/LB_numThreads/numThreads.c',
  'src/LB_numThreads/numThreads.h',
  'src/LB_numThreads/omptm_omp5.c',
  'src/LB_numThreads/omptm_omp5.h',
  'src/LB_numThreads/omptm_free_agents.c',
  'src/LB_numThreads/omptm_free_agents.h',
  'src/LB_numThreads/omptm_role_shift.c',
  'src/LB_numThreads/omptm_role_shift.h',
  'src/LB_numThreads/omptool.c',
  'src/LB_numThreads/omptool.h',
  'src/LB_comm/comm_lend_light.c',
  'src/LB_comm/comm_lend_light.h',
  'src/LB_comm/shmem_cpuinfo.c',
  'src/LB_comm/shmem_cpuinfo.h',
  'src/LB_comm/shmem_lewi_async.c',
  'src/LB_comm/shmem_lewi_async.h',
  'src/LB_comm/shmem_procinfo.c',
  'src/LB_comm/shmem_procinfo.h',
  'src/LB_policies/lewi.c',
  'src/LB_policies/lewi.h',
  'src/LB_policies/lewi_async.c',
  'src/LB_policies/lewi_async.h',
  'src/LB_policies/lewi_mask.c',
  'src/LB_policies/lewi_mask.h',
  'src/LB_core/DLB_kernel.c',
  'src/LB_core/DLB_kernel.h',
  'src/LB_core/DLB_talp.c',
  'src/LB_core/DLB_talp.h',
  'src/LB_core/node_barrier.c',
  'src/LB_core/node_barrier.h',
]

libdlb = library(
  'dlb', sources,
  include_directories : common_includes,
  link_whole: libdlb_common,
  dependencies : common_deps,
  gnu_symbol_visibility : 'hidden',
  install : true,
)

libdlb_instr = library(
  'dlb_instr', sources,
  include_directories : common_includes,
  c_args : instr_cpp_flags,
  link_whole: libdlb_common,
  dependencies : common_deps,
  gnu_symbol_visibility : 'hidden',
  install : true,
)

# Static library only used for testing
libdlb_test = static_library(
  'dlb_test', sources,
  include_directories : common_includes,
  link_with: libdlb_common,
  dependencies : common_deps,
  install : false,
  build_by_default : false,
)


#################################################################################
### DLB MPI libraries                                                         ###
#################################################################################

if mpi_dep.found()
  mpi_cpp_flags = '-DMPI_LIB'

  check_mpi_version = '''
    #include <mpi.h>
    #include <stdio.h>
    #include <stdlib.h>
    int main(int argc, char *argv[])
    {
        printf("%d.%d", MPI_VERSION, MPI_SUBVERSION);
        return EXIT_SUCCESS;
    }
    '''
  runresult = cc.run(check_mpi_version,
    dependencies : mpi_dep,
  )
  mpi_version = runresult.stdout()

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

  prog_python = import('python').find_installation('python')
  gen = generator(prog_python,
    output : '@BASENAME@',
    arguments : [
      '@SOURCE_DIR@/scripts/pygen.py',
      '--ifile', '@INPUT@', '--ofile', '@OUTPUT@',
      '--json', '@SOURCE_DIR@/src/LB_MPI/mpicalls.json',
      '--std', mpi_version,
      '--lib', mpi_library_version,
    ]
  )

  mpi_common_sources = [
    'src/LB_MPI/process_MPI.c',
    'src/LB_MPI/process_MPI.h',
  ]

  mpi_gen_sources = gen.process(
    'src/LB_MPI/MPI_calls_coded.c.in',
    'src/LB_MPI/MPI_calls_coded.h.in',
    'src/LB_MPI/MPI_interceptF08.F90.in',
    'src/LB_MPI/MPI_interface.c.in',
    'src/LB_MPI/MPI_interface.h.in',
    'src/LB_MPI/MPI_interfaceF.c.in',
    'src/LB_MPI/MPI_interfaceF.h.in',
    preserve_path_from : '@0@/src'.format(meson.current_source_dir()),
  )

  mpi_gen_intercept_sources = gen.process(
    'src/LB_MPI/MPI_intercept.c.in',
    'src/LB_MPI/MPI_interceptF.c.in',
    preserve_path_from : '@0@/src'.format(meson.current_source_dir()),
  )

  mpi_sources = [sources, mpi_common_sources, mpi_gen_sources]

  libdlb_mpi = library(
    'dlb_mpi', [mpi_sources, mpi_gen_intercept_sources],
    include_directories : common_includes,
    c_args : mpi_cpp_flags,
    link_whole: libdlb_common,
    dependencies : [common_deps, mpi_dep, mpif_dep],
    gnu_symbol_visibility : 'hidden',
    install : true,
  )

  libdlb_mpi_instr = library(
    'dlb_mpi_instr', mpi_sources,
    include_directories : common_includes,
    c_args : [mpi_cpp_flags, instr_cpp_flags],
    link_whole: libdlb_common,
    dependencies : [common_deps, mpi_dep, mpif_dep],
    gnu_symbol_visibility : 'hidden',
    install : true,
  )
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

project_name_lowercase = meson.project_name().to_lower()
dlb_version_string = project_name_lowercase + ' ' + meson.project_version()
dlb_git_branch = ''

git_program = find_program('git', required : false)
if git_program.found()
  git_branch_result = run_command(
      [git_program, 'rev-parse','--abbrev-ref', 'HEAD'], capture : true, check : false
    )
  if git_branch_result.returncode() == 0
    dlb_git_branch = git_branch_result.stdout().strip()
    dlb_version_string += ' (' + dlb_git_branch + ')'
  endif
endif

conf_data = configuration_data()
conf_data.set_quoted('VERSION', meson.project_version())
conf_data.set_quoted('PACKAGE', project_name_lowercase)
conf_data.set_quoted('PACKAGE_BUGREPORT', 'dlb@bsc.es')
conf_data.set_quoted('PACKAGE_NAME', meson.project_name())
conf_data.set_quoted('PACKAGE_VERSION', meson.project_version())
conf_data.set_quoted('DLB_CONFIGURE_ARGS', 'meson setup ' + meson.build_options())
conf_data.set_quoted('DLB_VERSION_STRING', dlb_version_string)
conf_data.set('HAVE_EMMINTRIN_H', cc.has_header('emmintrin.h'))
conf_data.set('HAVE_EXECINFO_H', cc.has_header('execinfo.h'))
conf_data.set('HAVE_IMMINTRIN_H', cc.has_header('immintrin.h'))
conf_data.set('HAVE_STDATOMIC_H', cc.has_header('stdatomic.h'))
conf_data.set_quoted('MPI_LIBRARY_VERSION', mpi_library_version)
config_h = configure_file( output : 'config.h', configuration : conf_data)


#################################################################################
### dist                                                                      ###
#################################################################################

# Generate .tarbal-git-branch and .tarball-version files on a 'meson dist' command
meson.add_dist_script(
  'scripts/meson-dist-gen.sh',
  dlb_git_branch,
  meson.project_version()
)


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
         'PAPI': papi_dep.found(),
         'MPI': mpi_dep.found(),
        }, section: 'Configuration')
