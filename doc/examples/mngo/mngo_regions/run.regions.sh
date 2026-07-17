#!/bin/bash
#
#SBATCH --job-name="DLB-MNGO"
#SBATCH --output=dlb_mngo.%j.out
#
#SBATCH --nodes=1
#SBATCH --ntasks=4
#SBATCH --ntasks-per-node=4
#SBATCH --cpus-per-task=28
#
#SBATCH --time=00:30:00
#
# Fail if any variable is not set
set -u

# Clear Extrae files
rm -f -r "set-*"
rm -f TRACE.mpits 

set -v

#####################
### CONFIGURATION ###
#####################

## Required environment variables to the necessary paths
export EXTRAE_HOME=""
export EXTRAE_CONFIG_FILE=""
export DLB_HOME=""

## Configure tracing
TRACE=0

## Configure DLB
MNGO=1
DROM=1
LEWI=0
TALP=1

## Configure Verbose
VERBOSE=0

## If your DLB depends on system modules (or manual configuration) add it at all
## instances of:
# >>>>>> ADD REQUIRED MODULES <<<<

############################
### END OF CONFIGURATION ###
############################

# Enable dlb if any of the modules are
DLB=$(( MNGO + TALP + DROM + LEWI ))

# Prepare libraries to preload
preload=()

# Define DLB environment
dlb_args=()
dlb_verbose=()

if [[ $MNGO == 1 ]]; then
    TALP=1
    dlb_args+=("--mngo")
    dlb_args+=("--mngo-mode=regions")
    dlb_verbose+=("mngo")
    dlb_verbose+=("shmem")
fi

if [[ $TALP == 1 ]]; then
    dlb_args+=("--talp")
    dlb_args+=("--talp-openmp")
fi

if [[ $DROM == 1 ]]; then
    dlb_args+=("--drom")
fi

if [[ $LEWI == 1 ]]; then
    dlb_args+=("--lewi")
fi

if [[ $VERBOSE == 1 ]] && [[ ${#dlb_verbose[*]} -gt 0 ]]; then
    export IFS=":"
    dlb_args+=("--verbose=${dlb_verbose[*]}")
fi

if [[ $DLB -gt 0 ]]; then
    dlb_args+=("--ompt")
    dlb_args+=("--ompt-thread-manager=omp5")

    if [[ $TRACE == 1 ]]; then
        dlb_args+=("--instrument=none")
    fi

    export IFS=" "
    set -x
    export DLB_ARGS="${dlb_args[*]}"
    set +x
    export LD_LIBRARY_PATH="${DLB_HOME}/lib:${LD_LIBRARY_PATH}"
    preload+=("${DLB_HOME}/lib/libdlb_mpi_dbg.so")
fi

# Define Extrae environment
if [[ $TRACE == 1 ]]; then
    set +v
    #module load extrae/4.2.15
    export PATH="${EXTRAE_HOME}/bin:$PATH"
    set -v
    preload+=("${EXTRAE_HOME}/lib/libompitracecf.so")
fi

# Define OpneMP environment
export OMP_PROC_BIND=false
export OMP_WAIT_POLICY=passive

# Clean dlb shared memory for all nodes
clean_script=$(readlink -f $(mktemp clean_shmem.XXX.sh))
cat >"${clean_script}" <<EOF
# >>>>>> ADD REQUIRED MODULES <<<<
${DLB_HOME}/bin/dlb_shm -d
EOF
chmod +x "${clean_script}"

scontrol show hostname | while read node; do
  ssh -n "${node}" "${clean_script}"
done

rm "${clean_script}"

export IFS=":" 
export SLURM_CPUS_PER_TASK=${SLURM_CPUS_PER_TASK}
set -x
srun -n "${SLURM_NTASKS}" --cpus-per-task "${SLURM_CPUS_PER_TASK}"  \
    --cpu-bind=threads                                                     \
    env LD_PRELOAD="${preload[*]}"                                         \
    ./test_regions                                                         ;
set +x

if [[ $TRACE == 1 ]]; then
    mpi2prv -f TRACE.mpits -o test_regions.prv
fi
