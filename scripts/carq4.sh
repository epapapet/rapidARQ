#!/bin/bash

# Script for executing C-ARQ experiment 4 multiple times
#For fixed propagation delay and loss rate, change bandwidth.
#For each experiment, give the experiment's number (1-4) in order to define filename. For the rest test cases, results will be stored in a carq.txt file.

SIMULATOR_PATH="./ns"
SIMULATION_FILE="arq/carq_cbr.tcl"
EXPERIMENT_ID="4"

#Number of iterations
NUM_ITERATIONS=5

#Number of retransmissions
num_rtx=(0 3)

bandwidth=(1M 2M 5M 10M 50M)
wnd=(25 50 125 250 1250)

coding_depths0=(1 2) 
coding_depths1=(1 2 3 4 5)
coding_depths2=(1 3 5 7 9 11 13)
coding_depths3=(1 3 5 7 9 11 13 16 20 24)
coding_depths4=(1 3 5 7 9 11 13 20 50 75 100 125)

arrays=(coding_depths0 coding_depths1 coding_depths2 coding_depths3 coding_depths4)

#cd $(dirname $(readlink -f $SIMULATOR_PATH))

for rtx in "${num_rtx[@]}"; do
	for i in "${!bandwidth[@]}"; do 
		declare -n temp="${arrays[i]}"
		for cdepth in "${temp[@]}";
		do 
			for iter in $(seq 1 $NUM_ITERATIONS); do 
				echo "Experiment 4| num_rtx: $rtx bandwidth: ${bandwidth[$i]} ARQ window: ${wnd[$i]} coding_depth: $cdepth Iteration $iter";
				RUN_SIMULATION="${SIMULATOR_PATH} ${SIMULATION_FILE} ${bandwidth[$i]} 100ms ${wnd[$i]} ${bandwidth[$i]} 1000 0.05 0.05 $rtx 9 $cdepth 0 1000 $iter"			
				${RUN_SIMULATION} ${EXPERIMENT_ID};
			done 
		done
	done
done


