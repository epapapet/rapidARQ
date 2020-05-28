#!/bin/bash

# Script for executing Tetrys experiment 3 multiple times
#For a fixed, relatively-large value of propagation delay and bandwidth, change code rate and loss rate.
#For each experiment, give the experiment's number (1-4) in order to define filename. For the rest test cases, results will be stored in a tetrys.txt file.

SIMULATOR_PATH="./ns"
SIMULATION_FILE="arq/tetrys_cbr.tcl"
EXPERIMENT_ID="3"

#Number of iterations
NUM_ITERATIONS=5

#following error rates correspond to total loss rate of 2%, 5%, 10% and 20% respectively
err_rate=(0.01 0.025 0.05 0.105)
ratek_values=(49 19 9 4)

#ack_period possible values [0.25RTT, 2RTT]
ack_period=(-4 -2 0 -0.66667 -0.5)

#cd $(dirname $(readlink -f $SIMULATOR_PATH))
 
for i in "${!ratek_values[@]}"; do
	for ackper in "${ack_period[@]}"; do 
		for iter in $(seq 1 $NUM_ITERATIONS); do
			echo "Experiment 3| Error rate: ${err_rate[$i]} rate_k: ${ratek_values[$i]} ack_period: $ackper Iteration $iter";
			RUN_SIMULATION="${SIMULATOR_PATH} ${SIMULATION_FILE} 10M 100ms 250 10M 1000 ${err_rate[$i]} ${err_rate[$i]} ${ratek_values[$i]} $ackper 0 1000 $iter"			
			${RUN_SIMULATION} ${EXPERIMENT_ID};
		done
	done
done


