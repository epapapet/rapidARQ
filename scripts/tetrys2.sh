#!/bin/bash

# Script for executing Tetrys experiment 2 multiple times
#For fixed code_rate and loss_rate, study the impact of changing RTT (propagation delay) (also test different coding_depth values for C-ARQ).
#For each experiment, give the experiment's number (1-4) in order to define filename. For the rest test cases, results will be stored in a tetrys.txt file.

SIMULATOR_PATH="./ns"
SIMULATION_FILE="arq/tetrys_cbr.tcl"
EXPERIMENT_ID="2"

#Number of iterations
NUM_ITERATIONS=5

delay=(5ms 10ms 20ms 50ms 100ms)
wnd=(12 25 50 125 250)

#ack_period possible values [0.25RTT, 2RTT]
ack_period=(-4 -2 0 -0.66667 -0.5)

#cd $(dirname $(readlink -f $SIMULATOR_PATH))

for i in "${!delay[@]}"; do 
	for ackper in "${ack_period[@]}"; do
		for iter in $(seq 1 $NUM_ITERATIONS); do 
			echo "Experiment 2| propagation delay: ${delay[$i]} ARQ window: ${wnd[$i]} ack_period: $ackper Iteration $iter";
			RUN_SIMULATION="${SIMULATOR_PATH} ${SIMULATION_FILE} 10M ${delay[$i]} ${wnd[$i]} 10M 1000 0.05 0.05 9 $ackper 0 1000 $iter"			
			${RUN_SIMULATION} ${EXPERIMENT_ID};
		done
	done
done


