#!/bin/bash

##### VARS ###############################
DLB_PATH=/home/bsc15/bsc15994/DLB/dlb
##########################################

profiling=NO

if [ $# != 12 ]
then
	echo "ERROR: Wrong number of parameters"
	echo "Usage: write_script.sh <script_name> <ini_dir> <mpi_procs> <procs per node> <params> <tracing> <version> <policy> <trace_path> <BITS> <distribution> <duration>"
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
	duration=${12}
fi

if [ $distribution == "CYCLIC" ]
then
	dist="--distribution=cyclic"
elif [ $distribution == "BASE" ]
then
	dist=""
elif [ $distribution == "FOLD" ]
then
	dist="--distribution=arbitrary"
else
	dist="--distribution=arbitrary"
	dist_file="export SLURM_HOSTFILE=$distribution"
fi

 cat > ${ini_dir}/MN_SCRIPTS/${script_name}.mn <<EOF
#!/bin/bash

# @ initialdir = ${ini_dir}
# @ output = ${ini_dir}/OUTS/${script_name}.%j.out
# @ error =  ${ini_dir}/OUTS/${script_name}.%j.err
# @ total_tasks =  ${mpi_procs}
# @ tasks_per_node = ${procs_node}
# @ cpus_per_task = $((4/$procs_node))
# @ node_usage = not_shared
# @ wall_clock_limit = ${duration}
# @ tracing = 1
# @class = bsc_cs 
### @class = benchmark

export MP_EUILIB=gm

$dist_file

if [ $distribution == "FOLD" ]
then
	times=$(($procs_node/2))

	/opt/perf/bin/sl_get_machine_list > node-list-\$SLURM_JOBID

	for i in \$(seq 1 \$times)
	do
		first="\$(seq 1 $procs_node $mpi_procs) \$first"
	done
	second=\$(echo \$first | rev) 

	for i in \$first \$second 
	do
		cat node-list-\$SLURM_JOBID | head -\$i |tail -1 >> my_node-list.\$SLURM_JOBID
	done
	
	export SLURM_HOSTFILE=my_node-list.\$SLURM_JOBID
fi

srun $dist ${DLB_PATH}/bin/set_dlb.sh ${params}

if [ $distribution == "FOLD" ]
then
	rm node-list-\$SLURM_JOBID
	rm my_node-list.\$SLURM_JOBID
fi

if [ $tracing == "YES" ]
then
	if [ $version == "SMPSS" ]
	then
		export EXTRAE_LABELS=StarSs.pcf
	fi

	if [ $mpi_procs == "1" ]
	then
		srun ${MPITRACE_HOME}/64/bin/mpi2prv -f ${trace_path}/TRACE.mpits -syn -o ${trace_path}/$script_name.prv
	else
		srun ${MPITRACE_HOME}/64/bin/mpimpi2prv -f ${trace_path}/TRACE.mpits -syn -o ${trace_path}/$script_name.prv
	fi
	rm ${trace_path}/set-0/TRACE*


	if [ $policy != "ORIG" ]
	then
		$DLB_PATH/bin/adderDLB ${trace_path}/$script_name.pcf
	fi

fi



EOF

mnsubmit ${ini_dir}/MN_SCRIPTS/${script_name}.mn
echo Submiting ${ini_dir}/MN_SCRIPTS/${script_name}.mn

