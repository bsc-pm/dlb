
tests = {
  '00_support' : {
    'atomic_00'           : {},
    'debug_00'            : {},
    'env_00'              : {},
    'errors_00'           : {},
    'gtree_00'            : {},
    'mask_00'             : {},
    'mask_01'             : {},
    'mask_02'             : {},
    'mask_03'             : {},
    'mask_04'             : {},
    'mytime_00'           : {},
    'options_00'          : {},
    'queues_00'           : {},
    'talp_output_00'      : {},
    'types_00'            : {},
  },
  '01_pm' : {
    # FIXME: omptm_free_agents_00.c not yet supported in meson test
    'omptool_00'          : {},
    'pm_00'               : {},
  },
  '02_shmem' : {
    'async_00' : {},
    'barrier_00'          : {},
    'barrier_01'          : {},
    'cpuinfo_00'          : {},
    'cpuinfo_01_async'    : {'source' : 'cpuinfo_01.c', 'dlb_args' : '--mode=async'},
    'cpuinfo_01_poll'     : {'source' : 'cpuinfo_01.c', 'dlb_args' : '--mode=polling'},
    'cpuinfo_02_async'    : {'source' : 'cpuinfo_02.c', 'dlb_args' : '--mode=async'},
    'cpuinfo_02_poll'     : {'source' : 'cpuinfo_02.c', 'dlb_args' : '--mode=polling'},
    'cpuinfo_03_async'    : {'source' : 'cpuinfo_03.c', 'dlb_args' : '--mode=async'},
    'cpuinfo_03_poll'     : {'source' : 'cpuinfo_03.c', 'dlb_args' : '--mode=polling'},
    'cpuinfo_get_binding_00'    : {},
    'cpuinfo_get_binding_01'    : {},
    'cpuinfo_procinfo_sync_00'  : {},
    'cpuinfo_procinfo_sync_01'  : {},
    'printer_00'          : {},
    'procinfo_00'         : {},
    'procinfo_01'         : {},
    'procinfo_03'         : {},
    'procinfo_04'         : {},
    'shmem_00'            : {},
    'shmem_01'            : {},
    'shmem_abort_00'      : {},
    'shmem_abort_01'      : {},
    'shmem_cleanup_00'    : {},
    'shmem_fail_00'       : {'should_fail': true},
    'shmem_fail_01'       : {'should_fail': true},
    'shmem_versions_00'   : {},
  },
  '03_policies' : {
    'lewi_00'             : {},
    'lewi_mask_00'        : {},
    'lewi_mask_01_async'  : {'source' : 'lewi_mask_01.c', 'dlb_args' : '--mode=async'},
    'lewi_mask_01_poll'   : {'source' : 'lewi_mask_01.c', 'dlb_args' : '--mode=polling'},
    'lewi_mask_02'        : {},
  },
  '04_core' : {
    'drom_00'             : {},
    'spd_00'              : {},
    'talp_00'             : {},
    'talp_01'             : {},
    'talp_01_lewi'        : {'source' : 'talp_01.c', 'dlb_args' : '--lewi'},
    'talp_02'             : {},
  },
  '05_api' : {
    'api_00'              : {},
    'api_01'              : {},
    'api_barrier_00'      : {},
    'api_drom_00'         : {},
    'api_drom_01'         : {},
    'api_drom_02'         : {},
    'api_monitor_00'      : {},
    'api_sp_00'           : {},
    'api_sp_01_async'     : {'source' : 'api_sp_01.c', 'dlb_args' : '--mode=async'},
    'api_sp_01_poll'      : {'source' : 'api_sp_01.c', 'dlb_args' : '--mode=polling'},
    'api_sp_03_async'     : {'source' : 'api_sp_03.c', 'dlb_args' : '--mode=async'},
    'api_sp_03_poll'      : {'source' : 'api_sp_03.c', 'dlb_args' : '--mode=polling'},
    'fortran_api_00'      : {'source' : 'fortran_api_00.f90'},
    'fortran_api_talp_00' : {'source' : 'fortran_api_talp_00.f90'},
  },
}

test_includes = [
  common_includes,
  include_directories('..'),          # parent subdir for <config.h>
  include_directories('test'),        # helper test headers
  include_directories('../src/apis'), # for fortran tests
]

initial_env = environment()

foreach suite, suite_tests : tests
  foreach test_name, test_args : suite_tests
    test_file = test_args.get('source', test_name + '.c')
    test_path = 'test/' + suite + '/' + test_file
    exe = executable(test_name, test_path,
      include_directories : test_includes,
      link_whole: libdlb_test,
      dependencies : common_deps,
      build_by_default: false,
      install : false
    )

    test_env = initial_env
    if (test_args.has_key('dlb_args'))
      test_env.append('DLB_ARGS', test_args.get('dlb_args'), separator: ' ')
    endif
    test(test_name, exe,
      env : test_env,
      should_fail : test_args.get('should_fail', false),
      suite : suite,
    )
  endforeach
endforeach
