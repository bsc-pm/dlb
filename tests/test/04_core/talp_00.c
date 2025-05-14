/*********************************************************************************/
/*  Copyright 2009-2024 Barcelona Supercomputing Center                          */
/*                                                                               */
/*  This file is part of the DLB library.                                        */
/*                                                                               */
/*  DLB is free software: you can redistribute it and/or modify                  */
/*  it under the terms of the GNU Lesser General Public License as published by  */
/*  the Free Software Foundation, either version 3 of the License, or            */
/*  (at your option) any later version.                                          */
/*                                                                               */
/*  DLB is distributed in the hope that it will be useful,                       */
/*  but WITHOUT ANY WARRANTY; without even the implied warranty of               */
/*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                */
/*  GNU Lesser General Public License for more details.                          */
/*                                                                               */
/*  You should have received a copy of the GNU Lesser General Public License     */
/*  along with DLB.  If not, see <https://www.gnu.org/licenses/>.                */
/*********************************************************************************/

/*<testinfo>
    test_generator="gens/basic-generator"
</testinfo>*/

#include "extra_tests.h"
#include "unique_shmem.h"

#include "LB_core/spd.h"
#include "LB_comm/shmem_talp.h"
#include "apis/dlb_talp.h"
#include "apis/dlb_errors.h"
#include "support/atomic.h"
#include "support/mask_utils.h"
#include "support/options.h"
#include "talp/regions.h"
#include "talp/talp.h"
#include "talp/talp_mpi.h"
#include "talp/talp_types.h"

#include <sched.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <assert.h>

/* Test simple TALP & Monitoring Regions functionalities */

enum { USLEEP_TIME = 1000 };

int main(int argc, char *argv[]) {

    enum { NUM_MEASUREMENTS = 10 };

    int cpu = sched_getcpu();
    cpu_set_t process_mask;
    CPU_ZERO(&process_mask);
    CPU_SET(cpu, &process_mask);

    char options[64] = "--talp --talp-papi --shm-key=";
    strcat(options, SHMEM_KEY);
    subprocess_descriptor_t spd = {.id = 111};
    options_init(&spd.options, options);
    memcpy(&spd.process_mask, &process_mask, sizeof(cpu_set_t));
    spd_enter_dlb(&spd);
    talp_init(&spd);

    talp_info_t *talp_info = spd.talp_info;
    dlb_monitor_t *global_monitor = talp_info->monitor;

    /* Register custom monitoring region */
    dlb_monitor_t *monitor = region_register(&spd, "Test monitor");
    assert( monitor != NULL );

    /* Check for forbidden names */
    assert( region_register(&spd, "ALL") == NULL );
    assert( region_register(&spd, "all") == NULL );
    assert( region_register(&spd, DLB_LAST_OPEN_REGION) == NULL );

    /* Check that registering "global_region_name" or DLB_GLOBAL_REGION_NAME is
     * allowed and that it does not reset the region */
    {
        /* Force num_measurements = 1 */
        assert( region_start(&spd, global_monitor) == DLB_SUCCESS );
        assert( region_stop(&spd, global_monitor) == DLB_SUCCESS );
        assert( global_monitor->num_measurements == 1 );

        dlb_monitor_t *another_global_1 = region_register(&spd,
                region_get_global_name());
        dlb_monitor_t *another_global_2 = region_register(&spd,
                DLB_GLOBAL_REGION_NAME);
        assert( another_global_1 == global_monitor);
        assert( another_global_2 == global_monitor);
        assert( global_monitor->num_measurements == 1 );

        /* Reset region for subsequent tests */
        assert( region_reset(&spd, global_monitor) == DLB_SUCCESS );
    }

    /* Test MPI + nested user defined region */
    {
        /* TALP initial values */
        assert( global_monitor->mpi_time == 0 );
        assert( global_monitor->useful_time == 0 );
        assert( region_get_global(&spd) == global_monitor );

        /* Start global monitor */
        talp_mpi_init(&spd);

        /* Start and Stop custom monitoring region */
        region_start(&spd, monitor);
        region_stop(&spd, monitor);

        /* Stop global monitor */
        talp_mpi_finalize(&spd);

        /* Test global monitor values are correct and greater than custom monitor */
        assert( global_monitor->mpi_time == 0 );
        assert( global_monitor->useful_time > 0 );
        assert( global_monitor->useful_time > monitor->useful_time );
        assert( global_monitor->elapsed_time > monitor->elapsed_time );
        if (talp_info->flags.papi) {
            assert( global_monitor->instructions > monitor->instructions );
            assert( global_monitor->cycles > monitor->cycles );
        }

        /* Test custom monitor values */
        assert( monitor->num_measurements == 1 );
        assert( monitor->mpi_time == 0 );
        assert( monitor->useful_time != 0 );
        assert( region_reset(&spd, monitor) == DLB_SUCCESS );
        assert( monitor->num_resets == 1 );
        assert( monitor->mpi_time == 0 );
        assert( monitor->useful_time == 0 );
        if (talp_info->flags.papi) {
            assert( monitor->instructions == 0 );
            assert( monitor->cycles == 0 );
        }
    }

    /* Test that reset/start/stop/report may also be invoked with the global DLB_GLOBAL_REGION */
    {
        /* Reset global monitoring region */
        assert( region_reset(&spd, DLB_GLOBAL_REGION) == DLB_SUCCESS );
        assert( !region_is_started(global_monitor) );
        assert( global_monitor->elapsed_time == 0 );
        assert( global_monitor->num_measurements == 0 );

        /* Start global monitor */
        talp_mpi_init(&spd);

        /* Stop global monitoring region */
        assert( region_stop(&spd, DLB_GLOBAL_REGION) == DLB_SUCCESS );
        assert( !region_is_started(global_monitor) );
        assert( global_monitor->elapsed_time > 0 );
        assert( global_monitor->num_measurements == 1 );

        /* Start global monitoring region */
        assert( region_start(&spd, DLB_GLOBAL_REGION) == DLB_SUCCESS );
        assert( region_is_started(global_monitor) );

        /* Stop global monitor */
        talp_mpi_finalize(&spd);

        /* Report global region */
        assert( region_report(&spd, DLB_GLOBAL_REGION) == DLB_SUCCESS );
    }

    /* Test number of measurements */
    {
        for (int i = 0; i < NUM_MEASUREMENTS; ++i) {
            region_start(&spd, monitor);
            region_stop(&spd, monitor);
        }
        assert( monitor->num_measurements == NUM_MEASUREMENTS );
        int64_t last_elapsed_measurement = monitor->stop_time - monitor->start_time;
        assert( last_elapsed_measurement > 0 );
        assert( last_elapsed_measurement * NUM_MEASUREMENTS/10 < monitor->elapsed_time );
    }

    /* Test invalid start/stop call order */
    {
        assert( region_stop(&spd, monitor) == DLB_NOUPDT );
        assert( region_start(&spd, monitor) == DLB_SUCCESS );
        assert( region_start(&spd, monitor) == DLB_NOUPDT );
        assert( region_stop(&spd, monitor) == DLB_SUCCESS );
        assert( region_stop(&spd, monitor) == DLB_NOUPDT );
    }

    /* Test reset with open region */
    {
        assert( talp_info->open_regions == NULL );
        assert( region_start(&spd, monitor) == DLB_SUCCESS );
        assert( talp_info->open_regions &&
                talp_info->open_regions->data == monitor );
        assert( region_reset(&spd, monitor) == DLB_SUCCESS );
        assert( talp_info->open_regions == NULL );
        assert( region_stop(&spd, monitor) == DLB_NOUPDT );
    }

    /* Test nested regions */
    {
        dlb_monitor_t *monitor1 = region_register(&spd, "Test nested 1");
        dlb_monitor_t *monitor2 = region_register(&spd, "Test nested 2");
        dlb_monitor_t *monitor3 = region_register(&spd, "Test nested 3");
        region_start(&spd, monitor1);
        usleep(USLEEP_TIME);
        for (int i = 0; i < NUM_MEASUREMENTS; ++i) {
            region_start(&spd, monitor2);
            usleep(USLEEP_TIME);
            for (int j = 0; j < NUM_MEASUREMENTS; ++j) {
                region_start(&spd, monitor3);
                usleep(USLEEP_TIME);
                region_stop(&spd, monitor3);
            }
            usleep(USLEEP_TIME);
            region_stop(&spd, monitor2);
        }
        usleep(USLEEP_TIME);
        region_stop(&spd, monitor1);
        assert( monitor1->num_measurements == 1 );
        assert( monitor2->num_measurements == NUM_MEASUREMENTS );
        assert( monitor3->num_measurements == NUM_MEASUREMENTS * NUM_MEASUREMENTS );
        assert( monitor1->elapsed_time > monitor2->elapsed_time );
        assert( monitor2->elapsed_time > monitor3->elapsed_time );
    }

    /* Test monitor register with repeated or NULL names */
    {
        dlb_monitor_t *monitor3 = region_register(&spd, "Test nested 3");
        dlb_monitor_t *monitor4 = region_register(&spd, "Test nested 3");
        assert( monitor4 == monitor3 );
        dlb_monitor_t *monitor5 = region_register(&spd, NULL);
        assert( monitor5 != NULL );
        dlb_monitor_t *monitor6 = region_register(&spd, NULL);
        assert( monitor6 != NULL );
        assert( monitor6 != monitor5 );
        assert( strcmp(monitor5->name, "Anonymous Region 1") == 0 );
        assert( strcmp(monitor6->name, "Anonymous Region 2") == 0 );
    }

    /* Test monitor name length */
    {
        const char *long_name = "This is a very long name. It is so long that"
            " it is more than DLB_MONITOR_NAME_MAX which, at the time of writing,"
            " should be 128 characters.";
        dlb_monitor_t *monitor7 = region_register(&spd, long_name);
        dlb_monitor_t *monitor8 = region_register(&spd, long_name);
        assert( monitor7 == monitor8 );
        region_report(&spd, monitor7);
        region_report(&spd, global_monitor);
    }

    /* Test internal monitor */
    {
        dlb_monitor_t *monitor9 = region_register(&spd, "Hidden monitor");
        region_set_internal(monitor9, true);
        region_report(&spd, monitor9);
    }

    /* Test --talp-region-select */
    {
        strcpy(spd.options.talp_region_select, "exclude:all");
        dlb_monitor_t *monitor1 = region_register(&spd, "disabled_monitor");
        assert( region_start(&spd, monitor1) == DLB_NOUPDT );
        assert( region_stop(&spd, monitor1) == DLB_NOUPDT );
        assert( monitor1->num_measurements == 0 );

        strcpy(spd.options.talp_region_select, "other_monitor");
        dlb_monitor_t *monitor2 = region_register(&spd, "not_enabled_monitor");
        assert( region_start(&spd, monitor2) == DLB_NOUPDT );
        assert( region_stop(&spd, monitor2) == DLB_NOUPDT );
        assert( monitor2->num_measurements == 0 );

        strcpy(spd.options.talp_region_select, "monitor4,monitor5");
        dlb_monitor_t *monitor3 = region_register(&spd, "monitor3");
        assert( region_start(&spd, monitor3) == DLB_NOUPDT );
        assert( region_stop(&spd, monitor3) == DLB_NOUPDT );
        assert( monitor3->num_measurements == 0 );
        dlb_monitor_t *monitor4 = region_register(&spd, "monitor4");
        assert( region_start(&spd, monitor4) == DLB_SUCCESS );
        assert( region_stop(&spd, monitor4) == DLB_SUCCESS );
        assert( monitor4->num_measurements == 1 );
        dlb_monitor_t *monitor5 = region_register(&spd, "monitor5");
        assert( region_start(&spd, monitor5) == DLB_SUCCESS );
        assert( region_stop(&spd, monitor5) == DLB_SUCCESS );
        assert( monitor5->num_measurements == 1 );

        strcpy(spd.options.talp_region_select, "exclude:monitor7,monitor8");
        dlb_monitor_t *monitor6 = region_register(&spd, "monitor6");
        assert( region_start(&spd, monitor6) == DLB_SUCCESS );
        assert( region_stop(&spd, monitor6) == DLB_SUCCESS );
        assert( monitor6->num_measurements == 1 );
        dlb_monitor_t *monitor7 = region_register(&spd, "monitor7");
        assert( region_start(&spd, monitor7) == DLB_NOUPDT );
        assert( region_stop(&spd, monitor7) == DLB_NOUPDT );
        assert( monitor7->num_measurements == 0 );
        dlb_monitor_t *monitor8 = region_register(&spd, "monitor8");
        assert( region_start(&spd, monitor8) == DLB_NOUPDT );
        assert( region_stop(&spd, monitor8) == DLB_NOUPDT );
        assert( monitor8->num_measurements == 0 );

        strcpy(spd.options.talp_region_select, "");
        dlb_monitor_t *monitor9 = region_register(&spd, "monitor9");
        assert( region_start(&spd, monitor9) == DLB_SUCCESS );
        assert( region_stop(&spd, monitor9) == DLB_SUCCESS );
        assert( monitor9->num_measurements == 1 );

        strcpy(spd.options.talp_region_select, "all");
        dlb_monitor_t *monitor10 = region_register(&spd, "monitor10");
        assert( region_start(&spd, monitor10) == DLB_SUCCESS );
        assert( region_stop(&spd, monitor10) == DLB_SUCCESS );
        assert( monitor10->num_measurements == 1 );

        strcpy(spd.options.talp_region_select, "include:all");
        dlb_monitor_t *monitor11 = region_register(&spd, "monitor11");
        assert( region_start(&spd, monitor11) == DLB_SUCCESS );
        assert( region_stop(&spd, monitor11) == DLB_SUCCESS );
        assert( monitor11->num_measurements == 1 );

        strcpy(spd.options.talp_region_select, "include:monitor13");
        dlb_monitor_t *monitor12 = region_register(&spd, "monitor12");
        assert( region_start(&spd, monitor12) == DLB_NOUPDT );
        assert( region_stop(&spd, monitor12) == DLB_NOUPDT );
        assert( monitor12->num_measurements == 0 );
        dlb_monitor_t *monitor13 = region_register(&spd, "monitor13");
        assert( region_start(&spd, monitor13) == DLB_SUCCESS );
        assert( region_stop(&spd, monitor13) == DLB_SUCCESS );
        assert( monitor13->num_measurements == 1 );

        // Reset talp_region_select for the following tests
        strcpy(spd.options.talp_region_select, "");
    }

    /* Test creation of N regions */
    {
        enum { N = 1000 };
        char name[DLB_MONITOR_NAME_MAX];
        for (int i = 0; i < N; ++i) {
            snprintf(name, DLB_MONITOR_NAME_MAX, "Loop Region %d", i);
            dlb_monitor_t *loop_monitor = region_register(&spd, name);
            assert( loop_monitor != NULL );
            assert( strncmp(name, loop_monitor->name, DLB_MONITOR_NAME_MAX) == 0 );
            assert( region_start(&spd, loop_monitor) == DLB_SUCCESS );
            assert( region_stop(&spd, loop_monitor) == DLB_SUCCESS );
        }
    }

    /* Test DLB_LAST_OPEN_REGION */
    {
        dlb_monitor_t *first = region_register(&spd, "first");
        dlb_monitor_t *last = region_register(&spd, "last");

        assert( region_start(&spd, first) == DLB_SUCCESS );
        assert( region_start(&spd, last) == DLB_SUCCESS );
        assert( last->num_measurements == 0 );
        assert( first->num_measurements == 0 );

        /* Stop last */
        assert( region_stop(&spd, DLB_LAST_OPEN_REGION) == DLB_SUCCESS );
        assert( last->num_measurements == 1 );

        /* Re-open and stop */
        assert( region_start(&spd, last) == DLB_SUCCESS );
        assert( region_stop(&spd, DLB_LAST_OPEN_REGION) == DLB_SUCCESS );
        assert( last->num_measurements == 2 );

        /* Stop first */
        assert( region_stop(&spd, DLB_LAST_OPEN_REGION) == DLB_SUCCESS );
        assert( first->num_measurements == 1 );

        /* No open regions */
        assert( region_stop(&spd, DLB_LAST_OPEN_REGION) == DLB_ERR_NOENT );

        dlb_monitor_t *mid = region_register(&spd, "mid");
        assert( region_start(&spd, first) == DLB_SUCCESS );
        assert( region_start(&spd, mid) == DLB_SUCCESS );
        assert( region_start(&spd, last) == DLB_SUCCESS );

        /* Not properly nested, weird but still supported */
        assert( region_stop(&spd, mid) == DLB_SUCCESS );
        assert( mid->num_measurements == 1 );
        assert( region_stop(&spd, DLB_LAST_OPEN_REGION) == DLB_SUCCESS );
        assert( last->num_measurements == 3 );
        assert( region_stop(&spd, DLB_LAST_OPEN_REGION) == DLB_SUCCESS );
        assert( first->num_measurements == 2 );
    }

    /* Test function not compatble without MPI support */
    {
        dlb_pop_metrics_t pop_metrics;
        assert( talp_collect_pop_metrics(&spd, global_monitor, &pop_metrics) == DLB_ERR_NOCOMP );
    }

    talp_finalize(&spd);

    /* Test --talp-region-select with global region, and finalize with open regions  */
    {
        strcpy(spd.options.talp_region_select, "region1,global");
        talp_init(&spd);
        global_monitor = region_get_global(&spd);
        assert( region_start(&spd, global_monitor) == DLB_SUCCESS );
        assert( region_is_started(global_monitor) );
        talp_finalize(&spd);
        strcpy(spd.options.talp_region_select, "");
    }

    /* Test --talp-region-select without global region */
    {
        strcpy(spd.options.talp_region_select, "region1");
        talp_init(&spd);
        global_monitor = region_get_global(&spd);
        assert( region_start(&spd, global_monitor) == DLB_NOUPDT );
        assert( !region_is_started(global_monitor) );
        talp_finalize(&spd);
        strcpy(spd.options.talp_region_select, "");
    }

    /* Test --shm-size-multiplier */
    if (DLB_EXTRA_TESTS)
    {
        enum { KNOWN_NUM_REGIONS_PER_PROC = 100 };
        char name[DLB_MONITOR_NAME_MAX];
        spd.options.talp_external_profiler = true;
        spd.options.talp_summary = SUMMARY_NONE;

        /* Choose multipliers based on the size of the current node.
         * This test can significantly increase execution time on big nodes.
         * The following numbers are chosen somewhat arbitrarily:
         */
        int initial_multiplier, final_multiplier;
        if (mu_get_system_size() < 64 ) {
            initial_multiplier = 1;
            final_multiplier = 3;
        } else {
            initial_multiplier = 2;
            final_multiplier = 2;
        }

        /* Try multipliers */
        for (int multiplier = initial_multiplier; multiplier <= final_multiplier; ++multiplier) {
            spd.options.shm_size_multiplier = multiplier;
            talp_init(&spd);
            int expected_max_regions = KNOWN_NUM_REGIONS_PER_PROC
                * mu_get_system_size() * multiplier;
            /* start at 1 because of global region */
            for (int i = 1; i < expected_max_regions; ++i) {
                snprintf(name, DLB_MONITOR_NAME_MAX, "Loop Region %d", i);
                dlb_monitor_t *loop_monitor = region_register(&spd, name);
                assert( loop_monitor != NULL );
                assert( strncmp(name, loop_monitor->name, DLB_MONITOR_NAME_MAX) == 0 );
                assert( region_start(&spd, loop_monitor) == DLB_SUCCESS );
                assert( region_stop(&spd, loop_monitor) == DLB_SUCCESS );
                assert( shmem_talp__get_num_regions() == i+1 );
            }

            assert( shmem_talp__get_num_regions() == expected_max_regions );
            snprintf(name, DLB_MONITOR_NAME_MAX,
                    "Doesn't fit in shmem (multiplier: %d)", multiplier);
            assert( region_register(&spd, name) != NULL );
            assert( shmem_talp__get_num_regions() == expected_max_regions );
            talp_finalize(&spd);
        }
    }

    return 0;
}
