
/*<testinfo>
test_generator="gens/single-generator"
</testinfo>*/

#include <stdio.h>
#include "support/mask_utils.h"

int main( int argc, char **argv ) {
    mu_init();
    fprintf( stdout, "System size: %d\n", mu_get_system_size() );
    mu_finalize();
    return 0;
}
