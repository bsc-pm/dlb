
/*<testinfo>
test_generator="gens/single-generator"
</testinfo>*/

#include <stdio.h>
#include "dlb/dlb.h"
#include "dlb/DLB_interface.h"

int main( int argc, char **argv )
{
   Init( 0, 1, 0 );
   DLB_disable();
   DLB_enable();
   DLB_UpdateResources();
   return 0;
}
