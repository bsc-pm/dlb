
/*<testinfo>
test_generator="gens/single-generator"
</testinfo>*/

#include <stdio.h>
#include "LB_core/DLB_kernel.h"
#include "LB_core/DLB_interface.h"

int main( int argc, char **argv )
{
   Init();
   DLB_disable();
   DLB_enable();
   DLB_UpdateResources();
   return 0;
}
