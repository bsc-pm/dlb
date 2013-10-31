#!/bin/bash

##### VARS ###############################
DLB_PATH=/home/bsc15/bsc15994/MN3/dlb/install
##########################################


if [ $# != 16 ]
then
	echo "ERROR: Wrong number of parameters"
	echo "Usage: write_script.sh <script_name> <ini_dir> <mpi_procs> <procs per node> <params> <tracing> <version> <policy> <trace_path> <BITS> <distribution> <block_mode> <extrae_xml> <duration> <submit> <thread distribution>"
	echo $* 
	exit
else
	script_name=$1	
	ini_dir=$2
	mpi_procs=$3
	procs_node=$4
	params=$5
	tracing=$6
	version=$7
	policy=$8
	trace_path=$9
	bits=${10}
	distribution=${11}
	block_mode=${12}
	extrae_xml=${13}
	duration=${14}
	submit=${15}
	thread_distrib=${16}
fi

if [ $distribution == "CYCLIC" ]
then
	dist="--bynode"
elif [ $distribution == "BASE" ]
then
	dist=""
else
	dist="--hostfile $distribution"
fi

CPUS_NODE=16
CPUS_PROC=$(($CPUS_NODE/$procs_node))

if [ $policy == "LeWI_mask" -o $policy == "RaL" ]
then
        binding="--cpus-per-proc $CPUS_PROC"

	if [ $CPUS_PROC == 1 ]
	then
		binding="$binding --bind-to-core"
	fi
else
	binding="--bind-to-none "
fi

binding="$binding "

program=$(echo $params | cut -d"\"" -f2 | cut -d " " -f1 )


output="${ini_dir}/OUTS/${script_name}"

 cat > ${ini_dir}/MN_SCRIPTS/${script_name}.mn <<EOF
#!/bin/bash

### Job Name
#BSUB -J ${script_name}

### Initial Dir
#BSUB -cwd ${ini_dir}

### Output and Error files
#BSUB -oo ${ini_dir}/OUTS/${script_name}.%J.out
#BSUB -eo ${ini_dir}/OUTS/${script_name}.%J.err

### Number of processes
#BSUB -n ${mpi_procs}

### Number of processes per node
#BSUB -R"span[ptile=${procs_node}]"

### Exclusive use
#BSUB -x 

### Wall clock time in HH:MM
#BSUB -W ${duration}

### DEBUG queue ####
#BSUB -q debug
###BSUB -q sequential 

ulimit -c unlimited
#export TMPDIR=$TMPDIR/extrae
#mkdir -p $TMPDIR

export LB_AGGRESSIVE_INIT=1
 
export OMPI_MCA_mpi_warn_on_fork=0

output="${output}.\$LSB_JOBID.app"

/usr/bin/time mpirun -n ${mpi_procs}  $binding $dist ${DLB_PATH}/bin/set_dlb_mn3.sh ${procs_node} ${version} ${policy} ${tracing} ${block_mode} ${extrae_xml} ${thread_distrib} "${params}" > \$output


if [ $tracing == "YES" ]
then
	if [ $version == "SMPSS" ]
	then
		export EXTRAE_LABELS=StarSs.pcf
	fi

##	if [ $mpi_procs == "1" ]
##	then
		${MPITRACE_HOME}/bin/mpi2prv -e ${program} -f ${trace_path}/TRACE.mpits -syn -o ${trace_path}/$script_name.prv >> \$output
##	else
##		mpirun  -n ${mpi_procs} ${MPITRACE_HOME}/bin/mpimpi2prv -e ${program} -f ${trace_path}/TRACE.mpits -syn -o ${trace_path}/$script_name.prv >> \$output
##	fi

	rm ${trace_path}/set-0/TRACE*
	rm -r ${ini_dir}/trace* 

fi



EOF

if [ ${submit} == "YES" ]
then
	bsub <  ${ini_dir}/MN_SCRIPTS/${script_name}.mn
	echo Submiting ${ini_dir}/MN_SCRIPTS/${script_name}.mn
else
	chmod a+x MN_SCRIPTS/${script_name}.mn
	./MN_SCRIPTS/${script_name}.mn
	echo Output in $output..app
fi
