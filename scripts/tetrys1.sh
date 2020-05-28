#!/bin/bash

# Script for executing Tetrys experiment 1 multiple times
#For fixed loss_rate and propagation delay, study the impact of different code rates (also different coding depths in C-ARQ).
#For each experiment, give the experiment's number (1-4) in order to define filename. For the rest test cases, results will be stored in a tetrys.txt file.

SIMULATOR_PATH="./ns"
SIMULATION_FILE="arq/tetrys_cbr.tcl"
EXPERIMENT_ID="1"

#Number of iterations
NUM_ITERATIONS=5

ratek_values=(1 3 5 9 13)

#ack_period possible values [0.25RTT, 2RTT]
ack_period=(-4 -2 0 -0.66667 -0.5)

#cd $(dirname $(readlink -f $SIMULATOR_PATH))

for i in "${!ratek_values[@]}"; do
	for ackper in "${ack_period[@]}"; do
		for iter in $(seq 1 $NUM_ITERATIONS); do 
			echo "Experiment 1| rate_k: ${ratek_values[$i]} ack_period: $ackper Iteration $iter";
			RUN_SIMULATION="${SIMULATOR_PATH} ${SIMULATION_FILE} 10M 100ms 250 10M 1000 0.05 0.05 ${ratek_values[$i]} $ackper 0 1000 $iter"			
			${RUN_SIMULATION} ${EXPERIMENT_ID}; 
		done
	done
done


