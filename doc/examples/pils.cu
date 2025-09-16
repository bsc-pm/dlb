#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <mpi.h>
#include <cuda_runtime.h>
#include <sys/time.h>
#include <nvToolsExt.h>

#define DEFAULT_CPU_LOAD 1000000
#define DEFAULT_ACCEL_LOAD 1000000
#define DEFAULT_MEMCPY_SIZE (10 * 1024 * 1024)  // 10 MB

__device__ float gpu_compute_op(float p, int x) {
    for (int i = 1; i < x; i++) {
        p += 0.99999f * i;
        p = p / i;
    }
    return p;
}

__global__ void gpu_computation(float *p, int x, int *iters, long long target_time) {
    if (threadIdx.x == 0 && blockIdx.x == 0) {
        long long start = clock64();
        long long now;
        int count = 0;
        float val = *p;
        do {
            val = gpu_compute_op(val, x);
            count++;
            now = clock64();
        } while ((now - start) < target_time);
        *p = val;
        *iters = count;
    }
}

long long int cpu_fibonacci(int n) {
    if (n <= 1) return n;
    return cpu_fibonacci(n - 1) + cpu_fibonacci(n - 2);
}

long long usecs() {
    struct timeval t;
    gettimeofday(&t, NULL);
    return t.tv_sec * 1000000LL + t.tv_usec;
}

void print_usage(char *prog_name) {
     printf("Usage: %s -n <fib_n> -a <accel_usec,...> -c <cpu_usec,...> -m <mem_usec,...> [-s <buffer_size>]\n", prog_name);
}


void parse_loads(char *str, int *loads, int mpi_size, int default_value) {
    for (int i = 0; i < mpi_size; ++i) {
        loads[i] = default_value;
    }
    if (!str) return;
    char *token = strtok(str, ",");
    for (int i = 0; token && i < mpi_size; ++i) {
        loads[i] = atoi(token);
        token = strtok(NULL, ",");
    }
}


static int cpu_computation(int num, int usec) {
    char range_name[64];
    snprintf(range_name, sizeof(range_name), "Rank CPU Loop");
    nvtxRangeId_t range_id = nvtxRangeStartA(range_name);

    float a = 0.99999f;
    float p = num;
    int x = 145;
    int i;
    long long t_start = usecs();
    long long t_now;
    do {
        for (i = 1; i < x; i++) {
            p += a * i;
            p = p / i;
        }
        t_now = usecs();
    } while ((t_now - t_start) < usec);

    nvtxRangeEnd(range_id);
    return (int)p;
}

static int gpu_timed_computation(int num, int usec) {
    float *d_p;
    int *d_total_iters;
    float h_p = num;
    int h_total_iters = 0;
    int threadsPerBlock = 256;
    int blocksPerGrid = 1;
    long long target_time = usec * 1000LL;

    cudaMalloc((void**)&d_p, sizeof(float));
    cudaMalloc((void**)&d_total_iters, sizeof(int));
    cudaMemcpy(d_p, &h_p, sizeof(float), cudaMemcpyHostToDevice);
    cudaMemcpy(d_total_iters, &h_total_iters, sizeof(int), cudaMemcpyHostToDevice);

    printf("DEBUG: Launching kernel with %d blocks, %d threads\n",
           blocksPerGrid, threadsPerBlock);
    fflush(stdout);

    gpu_computation<<<blocksPerGrid, threadsPerBlock>>>(d_p, num, d_total_iters, target_time);
    cudaDeviceSynchronize();
        cudaError_t err = cudaGetLastError();
    if (err != cudaSuccess) {
        printf("Kernel launch failed: %s\n", cudaGetErrorString(err));
        fflush(stdout);
    }

    err = cudaDeviceSynchronize();
    if (err != cudaSuccess) {
        printf("Kernel execution failed: %s\n", cudaGetErrorString(err));
        fflush(stdout);
    }


    cudaMemcpy(&h_p, d_p, sizeof(float), cudaMemcpyDeviceToHost);
    cudaMemcpy(&h_total_iters, d_total_iters, sizeof(int), cudaMemcpyDeviceToHost);

    cudaFree(d_p);
    cudaFree(d_total_iters);

    return (int)h_p;
}
static void gpu_memcpy_timed(int usec, size_t buffer_size) {
    char *h_buf = (char *)malloc(buffer_size);
    char *d_buf;

    cudaMalloc((void**)&d_buf, buffer_size);

    // Initialize host buffer to some data
    memset(h_buf, 1, buffer_size);

    long long t_start = usecs();
    long long t_now;
    do {
        cudaMemcpy(d_buf, h_buf, buffer_size, cudaMemcpyHostToDevice);
        cudaMemcpy(h_buf, d_buf, buffer_size, cudaMemcpyDeviceToHost);
        t_now = usecs();
    } while ((t_now - t_start) < usec);

    cudaFree(d_buf);
    free(h_buf);
}

int main(int argc, char *argv[]) {
    int provided;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_FUNNELED, &provided);

    int mpi_rank, mpi_size;
    MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);

    int fib_n = 35;
    char *accel_str = NULL;
    char *cpu_str = NULL;
    char *mem_str = NULL;
    size_t buffer_size = DEFAULT_MEMCPY_SIZE;
    double mem_time = 0.0;

    int opt;
        while ((opt = getopt(argc, argv, "n:a:c:m:s:")) != -1) {
        switch (opt) {
            case 'n': fib_n = atoi(optarg); break;
            case 'a': accel_str = strdup(optarg); break;
            case 'c': cpu_str = strdup(optarg); break;
            case 'm': mem_str = strdup(optarg); break;
            case 's': buffer_size = atol(optarg); break;
            default:
                if (mpi_rank == 0) print_usage(argv[0]);
                MPI_Finalize();
                return 1;
        }
    }
   

    int *cpu_loads = (int *)calloc(mpi_size, sizeof(int));
    int *accel_loads = (int *)calloc(mpi_size, sizeof(int));
    int *mem_loads = (int *)calloc(mpi_size, sizeof(int));


    parse_loads(cpu_str, cpu_loads, mpi_size, DEFAULT_CPU_LOAD);
    parse_loads(accel_str, accel_loads, mpi_size, DEFAULT_ACCEL_LOAD);
    parse_loads(mem_str, mem_loads, mpi_size, 0);  // Default to 0 (no memory ops)

    double cpu_time = 0.0, accel_time = 0.0;
    long long t_start, t_end;

    if (accel_loads[mpi_rank] > 0) {
        char range_name[64];
        snprintf(range_name, sizeof(range_name), "Rank %d GPU Load", mpi_rank);
        nvtxRangeId_t id = nvtxRangeStartA(range_name);

        t_start = usecs();
        gpu_timed_computation(fib_n, accel_loads[mpi_rank]);
        t_end = usecs();

        nvtxRangeEnd(id);
        accel_time = (t_end - t_start) / 1e6;
	cudaDeviceSynchronize(); // Ensure kernels complete
    }

    if (cpu_loads[mpi_rank] > 0) {
        char range_name[64];
        snprintf(range_name, sizeof(range_name), "Rank %d CPU Load", mpi_rank);
        nvtxRangeId_t id = nvtxRangeStartA(range_name);

        t_start = usecs();
        cpu_computation(fib_n, cpu_loads[mpi_rank]);
        t_end = usecs();

        nvtxRangeEnd(id);
        cpu_time = (t_end - t_start) / 1e6;
    }
    if (mem_loads[mpi_rank] > 0) {
        char range_name[64];
        snprintf(range_name, sizeof(range_name), "Rank %d Memory Copy", mpi_rank);
        nvtxRangeId_t id = nvtxRangeStartA(range_name);

        t_start = usecs();
        gpu_memcpy_timed(mem_loads[mpi_rank], buffer_size);
        t_end = usecs();

        nvtxRangeEnd(id);
        mem_time = (t_end - t_start) / 1e6;
    }
    printf("Rank %d completed CPU dummy computation in %.3f s, Accelerator dummy computation in %.3f s, and Memory copies in %.3f s\n",
           mpi_rank, cpu_time, accel_time, mem_time);

    // Synchronize all ranks before finalizing
    MPI_Barrier(MPI_COMM_WORLD);

    free(cpu_loads);
    free(accel_loads);
    free(mem_loads);
    if (mem_str) free(mem_str);
    if (cpu_str) free(cpu_str);
    if (accel_str) free(accel_str);

    MPI_Finalize();
    return 0;
}
