#!/bin/bash

# Script for executing Caterpillar RLNC experiment 4 multiple times
#For fixed propagation delay and loss rate, change bandwidth.
#For each experiment, give the experiment's number (1-4) in order to define filename. For the rest test cases, results will be stored in a caterpillar.txt file.

SIMULATOR_PATH="./ns"
SIMULATION_FILE="arq/tcl_files/caterpillar_cbr.tcl"
EXPERIMENT_ID="4"

#Number of iterations
NUM_ITERATIONS=5

#Number of retransmissions
num_rtx=(0 3)

bandwidth=(1M 2M 5M 10M 50M)
wnd=(25 50 125 250 1250)

#cd $(dirname $(readlink -f $SIMULATOR_PATH))

for rtx in "${num_rtx[@]}"; do
	for i in "${!bandwidth[@]}"; do 
		for iter in $(seq 1 $NUM_ITERATIONS); do 
			echo "Experiment 4| num_rtx: $rtx bandwidth: ${bandwidth[$i]} ARQ window: ${wnd[$i]} Iteration $iter";
			RUN_SIMULATION="${SIMULATOR_PATH} ${SIMULATION_FILE} ${bandwidth[$i]} 100ms ${wnd[$i]} ${bandwidth[$i]} 1000 0.05 0.05 $rtx 9 0 1000 $iter"			
			${RUN_SIMULATION} ${EXPERIMENT_ID};
		done
	done
done

