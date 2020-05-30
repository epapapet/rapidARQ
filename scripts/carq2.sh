#!/bin/bash

# Script for executing C-ARQ experiment 2 multiple times
#For fixed code_rate and loss_rate, study the impact of changing RTT (propagation delay) (also test different coding_depth values for C-ARQ).
#For each experiment, give the experiment's number (1-4) in order to define filename. For the rest test cases, results will be stored in a carq.txt file.

SIMULATOR_PATH="./ns"
SIMULATION_FILE="arq/tcl_files/carq_cbr.tcl"
EXPERIMENT_ID="2"

#Number of iterations
NUM_ITERATIONS=5

#Number of retransmissions
num_rtx=(0 3)

delay=(5ms 10ms 20ms 50ms 100ms)
wnd=(12 25 50 125 250)

coding_depths0=(1) 
coding_depths1=(1 2)
coding_depths2=(1 2 3 4 5)
coding_depths3=(1 3 5 7 9 11 13)
coding_depths4=(1 3 5 7 9 11 13 16 20 24)

arrays=(coding_depths0 coding_depths1 coding_depths2 coding_depths3 coding_depths4)

#cd $(dirname $(readlink -f $SIMULATOR_PATH))

for rtx in "${num_rtx[@]}"; do
	for i in "${!delay[@]}"; do 
		declare -n temp="${arrays[i]}"
		for cdepth in "${temp[@]}";
		do 
			for iter in $(seq 1 $NUM_ITERATIONS); do 
				echo "Experiment 2| num_rtx: $rtx propagation delay: ${delay[$i]} ARQ window: ${wnd[$i]} coding_depth: $cdepth Iteration $iter";
				RUN_SIMULATION="${SIMULATOR_PATH} ${SIMULATION_FILE} 10M ${delay[$i]} ${wnd[$i]} 10M 1000 0.05 0.05 $rtx 9 $cdepth 0 1000 $iter"			
				${RUN_SIMULATION} ${EXPERIMENT_ID};
			done 
		done
	done
done


