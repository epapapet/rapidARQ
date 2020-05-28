#!/bin/bash

# Script for executing Tetrys experiment 4 multiple times
#For fixed propagation delay and loss rate, change bandwidth.
#For each experiment, give the experiment's number (1-4) in order to define filename. For the rest test cases, results will be stored in a tetrys.txt file.

SIMULATOR_PATH="./ns"
SIMULATION_FILE="arq/tetrys_cbr.tcl"
EXPERIMENT_ID="4"

#Number of iterations
NUM_ITERATIONS=5

bandwidth=(1M 2M 5M 10M 50M)
wnd=(25 50 125 250 1250)

#ack_period possible values [0.25RTT, 2RTT]
ack_period=(-4 -2 0 -0.66667 -0.5)

#cd $(dirname $(readlink -f $SIMULATOR_PATH))

for i in "${!bandwidth[@]}"; do 
	for ackper in "${ack_period[@]}"; do
		for iter in $(seq 1 $NUM_ITERATIONS); do 
			echo "Experiment 4| bandwidth: ${bandwidth[$i]} ARQ window: ${wnd[$i]} ack_period: $ackper Iteration $iter";
			RUN_SIMULATION="${SIMULATOR_PATH} ${SIMULATION_FILE} ${bandwidth[$i]} 100ms ${wnd[$i]} ${bandwidth[$i]} 1000 0.05 0.05 9 $ackper 0 1000 $iter"			
			${RUN_SIMULATION} ${EXPERIMENT_ID};
		done
	done
done


