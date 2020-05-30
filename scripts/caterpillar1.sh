#!/bin/bash

# Script for executing Caterpillar RLNC experiment 1 multiple times
#For fixed loss_rate and propagation delay, study the impact of different code rates (also different coding depths in C-ARQ).
#For each experiment, give the experiment's number (1-4) in order to define filename. For the rest test cases, results will be stored in a caterpillar.txt file.

SIMULATOR_PATH="./ns"
SIMULATION_FILE="arq/tcl_files/caterpillar_cbr.tcl"
EXPERIMENT_ID="1"

#Number of iterations
NUM_ITERATIONS=5

#Number of retransmissions
num_rtx=(0 3)

ratek_values=(1 3 5 9 13)

#cd $(dirname $(readlink -f $SIMULATOR_PATH))

for rtx in "${num_rtx[@]}"; do
	for i in "${!ratek_values[@]}"; do 
		for iter in $(seq 1 $NUM_ITERATIONS); do 
			echo "Experiment 1| num_rtx: $rtx rate_k: ${ratek_values[$i]} Iteration $iter";
			RUN_SIMULATION="${SIMULATOR_PATH} ${SIMULATION_FILE} 10M 100ms 250 10M 1000 0.05 0.05 $rtx ${ratek_values[$i]} 0 1000 $iter"			
			${RUN_SIMULATION} ${EXPERIMENT_ID};
		done
	done
done


