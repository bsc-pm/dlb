/*********************************************************************************/
/*  Copyright 2009-2026 Barcelona Supercomputing Center                          */
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

#include "mngo/mngo_balancer.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <string.h>
#include <sys/wait.h>

int main(int argc, char *argv[]) {

    /* Test precompute_levels */
    int deltas[4] = {2, -2, 3, 4};
    {
        int *count_add = calloc(5, sizeof(int));
        int *count_sub = calloc(5, sizeof(int));
  
        mngo_balancer_testing__precompute_levels(deltas, 4, count_add, count_sub, 5);

        int count_add_correct[5] =  {3, 3, 3, 2, 1};
        if (memcmp(count_add, count_add_correct, 5) != 0) {
            assert(false && "count_add unexpected result");
        };

        int count_sub_correct[5] =  {1, 1, 1, 0, 0};
        if (memcmp(count_sub, count_sub_correct, 5) != 0) {
            assert(false && "count_sub unexpected result");
        };

        free(count_add);
        free(count_sub);
    }

    {
        int *count_add = calloc(7, sizeof(int));
        int *count_sub = calloc(7, sizeof(int));
  
        mngo_balancer_testing__precompute_levels(deltas, 4, count_add, count_sub, 7);

        int count_add_correct[7] =  {3, 3, 3, 2, 1, 0, 0};
        if (memcmp(count_add, count_add_correct, 7) != 0) {
            assert(false && "count_add unexpected result");
        };

        int count_sub_correct[7] =  {1, 1, 1, 0, 0, 0, 0};
        if (memcmp(count_sub, count_sub_correct, 7) != 0) {
            assert(false && "count_sub unexpected result");
        };

        /* Test pack_levels */
        {
          int self_deltas[7] = {1, 1, 1, 0, 0, 0, 0};
          mngo_balancer_testing__pack_levels(2, count_sub, self_deltas, 2);

          int count_sub_packed_correct[7] =  {2, 2, 0, 0, 0, 0, 0};
          if (memcmp(count_sub, count_sub_packed_correct, 7) != 0) {
              assert(false && "apply pack_levels wrong levels");
          };

          int correct_self_deltas[7] =  {1, 2, 0, 0, 0, 0, 0};
          if (memcmp(self_deltas, correct_self_deltas, 7) != 0) {
              assert(false && "apply pack_levels wrong deltas");
          };
        }

        {
          int self_deltas[7] = {1, 1, 1, 0, 0, 0, 0};
          int correct_self_deltas[7] = {1, 1, 1, 0, 0, 0, 0};
          mngo_balancer_testing__pack_levels(3, count_add, self_deltas, 4);

          int count_add_packed_correct[7] =  {3, 3, 3, 3, 0, 0, 0};
          if (memcmp(count_add, count_add_packed_correct, 7) != 0) {
              assert(false && "apply pack_levels wrong levels");
          };

          if (memcmp(self_deltas, correct_self_deltas, 7) != 0) {
              assert(false && "apply pack_levels wrong deltas");
          };

        }

        free(count_add);
        free(count_sub);
    }

    {
        bool i_am_a_giver = false;
        int my_abs_delta = 2;

        int count_add[3] = {2, 2, 1};
        int count_sub[3] = {3, 3, 0};
        int count_size = 3;
        int my_delta = mngo_balancer_testing__coherent_redistribution(
            i_am_a_giver, my_abs_delta, count_add, count_sub, count_size-1);
        assert(my_delta == 2);
    }

    {
        bool i_am_a_giver = true;
        int my_abs_delta = 2;

        int count_add[3] = {3, 3, 0};
        int count_sub[3] = {2, 2, 1};
        int count_size = 3;
        int my_delta = mngo_balancer_testing__coherent_redistribution(
            i_am_a_giver, my_abs_delta, count_add, count_sub, count_size-1);
        assert(my_delta == -2);
    }

    {
        bool i_am_a_giver = false;
        int my_abs_delta = 1;

        int count_add[3] = {2, 2, 0};
        int count_sub[3] = {3, 3, 0};
        int count_size = 3;
        int my_delta = mngo_balancer_testing__coherent_redistribution(
            i_am_a_giver, my_abs_delta, count_add, count_sub, count_size-1);
        assert(my_delta == 0);
    }
  
    {
        bool i_am_a_giver = true;
        int my_abs_delta = 1;

        int count_add[3] = {2, 2, 1};
        int count_sub[3] = {3, 3, 0};
        int count_size = 3;
        int my_delta = mngo_balancer_testing__coherent_redistribution(
            i_am_a_giver, my_abs_delta, count_add, count_sub, count_size-1);
        assert(my_delta == -1);
    }
}
