
#include <stdio.h>
#include <dlb.h>

int main()
{
    #pragma omp parallel
    {}

    int err = DLB_Init(0,0,0);
    switch (err) {
        case DLB_SUCCESS:
            printf("DLB has not been initialized through OpenMP.\n"
                    "Make sure that the OpenMP runtime supports OMPT and that"
                    " DLB_ARGS is correctly set.\n");
            DLB_Finalize();
            break;
        case DLB_ERR_INIT:
            printf("DLB has correctly been registered as an OpenMP Tool.\n");
            break;
        default:
            printf("Unknown error, please report.");
            return 1;
    }

    return 0;
}
