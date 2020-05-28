#!/bin/bash

# Script for executing Caterpillar RLNC experiment 2 multiple times
#For fixed code_rate and loss_rate, study the impact of changing RTT (propagation delay) (also test different coding_depth values for C-ARQ).
#For each experiment, give the experiment's number (1-4) in order to define filename. For the rest test cases, results will be stored in a caterpillar.txt file.

SIMULATOR_PATH="./ns"
SIMULATION_FILE="arq/caterpillar_cbr.tcl"
EXPERIMENT_ID="2"

#Number of iterations
NUM_ITERATIONS=5

#Number of retransmissions
num_rtx=(0 3)

delay=(5ms 10ms 20ms 50ms 100ms)
wnd=(12 25 50 125 250)

#cd $(dirname $(readlink -f $SIMULATOR_PATH))

for rtx in "${num_rtx[@]}"; do
	for i in "${!delay[@]}"; do 
		for iter in $(seq 1 $NUM_ITERATIONS); do 		
			echo "Experiment 2| num_rtx: $rtx propagation delay: ${delay[$i]} ARQ window: ${wnd[$i]} Iteration $iter";
			RUN_SIMULATION="${SIMULATOR_PATH} ${SIMULATION_FILE} 10M ${delay[$i]} ${wnd[$i]} 10M 1000 0.05 0.05 $rtx 9 0 1000 $iter"			
			${RUN_SIMULATION} ${EXPERIMENT_ID};
		done
	done
done

