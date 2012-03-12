#!/bin/bash

##### VARS ###############################
DLB_PATH=/home/bsc15/bsc15994/DLB/dlb
##########################################

profiling=NO

if [ $# != 10 ]
then
	echo "ERROR: Wrong number of parameters"
	echo "Usage: write_script.sh <script_name> <ini_dir> <mpi_procs> <procs per node> <params> <tracing> <version> <policy> <trace_path> <BITS>"
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
# @ wall_clock_limit = 00:10:00
# @ tracing = 1
# @class = bsc_cs 
### @class = benchmark

export MP_EUILIB=gm

srun ${DLB_PATH}/bin/set_dlb.sh ${params}

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

