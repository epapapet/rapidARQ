set arg_cnt [lindex $argc]
if {$arg_cnt != 14} {
    puts "# usage: ns <scriptfile> <bandwidth> <propagation_delay> <window_size> <cbr_rate> <pkt_size> <err_rate> <burst_duration> <ack_rate> <rate_k> <ack_period> <timeout> <simulation_time> <seed> <experiment_id>"
    puts "* <bandwidth> : in bps, example: set to 5Mbps -> 5M or 5000000"
    puts "* <propagation_delay> : in secs, example: set to 30ms -> 30ms or 0.03"
    puts "* <window_size> : arq window size in pkts"
    puts "* <cbr_rate> : the rate of the cbr applications, in bps, example: set to 3Mbps -> 3M or 3000000"
    puts "* <pkt_size> : the size of udp pkt (including UDP and IP headers)"
    puts "# <err_rate> : the error rate in the forward channel (error rate for frames) during a burst period"
    puts "# <burst_duration> : 0,..,1 -> the percentage of time that the channel is in an error burst state"
    puts "* <ack_rate> : the error rate in the return channel (error rate for ACKs)"
    puts "* <rate_k> : the number of native pkts sent before creating a coded pkt (actually defines the code rate), 0 corresponds to deactivating coding"
    puts "* <ack_period> : the period for sending acks, example: set to 30ms->30ms or 0.03, 0 sets ack_period=window duration, a value v<0 will set the ack_period=-(window duration)/v"
    puts "* <timeout> : the time for expiring an non acked pkt, example: set to 30ms->30ms or 0.03, 0 sets timeout=RTT, a value v<0 will set the timeout=-(RTT)/v"
    puts "* <simulation_time> : the simulation time in secs"
    puts "* <seed> : seed used to produce randomness"
	puts "* <experiment_id> : an id that determines the filename where simulation's results will be stored. For experiments 1-4, use values 1-4, otherwise results will be saved in a txt file determined by user's value."
    exit 1
}

TetrysTx set rate_k 1000
TetrysTx set lnk_delay_ 30ms
TetrysTx set lnk_bw_ 10M
TetrysTx set app_pkt_Size_ 1000
TetrysTx set debug_ NULL
TetrysAcker set debug_ NULL
TetrysNacker set debug_ NULL

SimpleLink instproc link-arq { wndsize apktsz ratekk ackperr timeoutt vgseed ackerr eid} {
    $self instvar link_ link_errmodule_ queue_ drophead_ head_
    $self instvar tARQ_ acker_ nacker_
 
    set tARQ_ [new TetrysTx]
    set acker_ [new TetrysAcker]
    set nacker_ [new TetrysNacker]

    #Tx set up
    $tARQ_ set lnk_bw_ [$self bw]
    $tARQ_ set lnk_delay_ [$self delay]
    $tARQ_ set app_pkt_Size_ $apktsz
	$tARQ_ setup-wnd $wndsize $ratekk $timeoutt

	set vagrngn2 [new RNG]
    $vagrngn2 seed [expr {$vgseed + 1}]
    set vagranvarn2 [new RandomVariable/Uniform]
    $vagranvarn2 use-rng $vagrngn2
    $tARQ_ ranvar $vagranvarn2
    $tARQ_ set-err $ackerr 
    
    #Acker set up
    $acker_ attach-TetrysTx $tARQ_
    $acker_ setup-TetrysNacker $nacker_
    $acker_ setup-wnd $wndsize
	$acker_ setup-filename $eid
    $acker_ update-delays $ackperr

    #Nacker set up
    $nacker_ attach-TetrysTx $tARQ_
	$nacker_ setup-TetrysAcker $acker_
    $nacker_ update-delays $ackperr

    
    #Connections between Tx, Acker, Nacker, queue, drop-target and Acker target
    $tARQ_ target [$queue_ target]
    $queue_ target $tARQ_
    $acker_ target [$link_errmodule_ target]
    $link_errmodule_ target $acker_
    $link_errmodule_ drop-target $nacker_
    $tARQ_ drop-target $drophead_
    
	return $acker_
}

Simulator instproc link-arq {wndsize apktsize ratek ackper timeout from to vgseed ackerr id} {
    set link [$self link $from $to]
    set acker [$link link-arq $wndsize $apktsize $ratek $ackper $timeout $vgseed $ackerr $id]
	return $acker
}

proc print_stats {err_rate ack_rate sim_time seed} {
	global receiver
	$receiver print-stats $err_rate $ack_rate $sim_time $seed
}

#=== Create the Simulator, Nodes, and Links ===
set ns [new Simulator]
set n1 [$ns node]
set n2 [$ns node]
set n3 [$ns node]

set link_bwd [lindex $argv 0]
set link_delay [lindex $argv 1]

$ns duplex-link $n1 $n2 $link_bwd $link_delay DropTail
$ns duplex-link $n2 $n3 $link_bwd $link_delay DropTail
$ns duplex-link $n1 $n3 $link_bwd $link_delay DropTail

#=== Create error and ARQ module ===
set window [lindex $argv 2]


#Create uniform Errormodel representing first state
set tmp [new ErrorModel]
$tmp set rate_ 0
$tmp set enable_ 1
$tmp set bandwidth_ $link_bwd
set vagrng00 [new RNG]
$vagrng00 seed [expr {[lindex $argv 12] + 10}]
set vagranvar00 [new RandomVariable/Uniform]
$vagranvar00 use-rng $vagrng00
$tmp ranvar $vagranvar00

#Create uniform Errormodel representing second state
set tmp1 [new ErrorModel]
$tmp1 set rate_ [lindex $argv 5]
$tmp1 set enable_ 1
$tmp1 set bandwidth_ $link_bwd
set vagrng01 [new RNG]
$vagrng01 seed [lindex $argv 12]
set vagranvar01 [new RandomVariable/Uniform]
$vagranvar01 use-rng $vagrng01
$tmp1 ranvar $vagranvar01

if {[lindex $argv 6] > 1 || [lindex $argv 6] < 0} {
    puts "Burst duration percentage should be in \[0, 1\]"
    exit 1;
}
if {[string first "M" [lindex $argv 0]] != -1} {
    set bwd_per_string [string map {"M" ""} [lindex $argv 0]]
    set bwdcalc [expr {double($bwd_per_string)*1000000}]
} else {
    set bwdcalc [lindex $argv 0]
}
set bduration [lindex $argv 6]
set wndduration [expr {8.0*$window*[lindex $argv 4]/$bwdcalc}]
set state1nduration [expr {(1- $bduration)*$wndduration}]
set state2nduration [expr {$bduration*$wndduration}]


# Array of states (error models)
set m_states [list $tmp $tmp1]
# Durations for each of the states, tmp and tmp1, respectively
set m_periods [list $state1nduration $state2nduration]
# Transition state model matrix
set m_transmx { {0 1} {1 0}}
set m_trunit pkt
 # Use time-based transition
set m_sttype time
set m_nstates 2
set m_nstart [lindex $m_states 0]
set em [new ErrorModel/MultiState $m_states $m_periods $m_transmx $m_trunit $m_sttype $m_nstates $m_nstart]

$em drop-target [new Agent/Null]
$ns link-lossmodel $em $n1 $n3

set rate_k [lindex $argv 8]
if {[string first "ms" [lindex $argv 9]] != -1} {
    set ack_per_string [string map {"ms" ""} [lindex $argv 9]]
    set ack_period [expr {double($ack_per_string)/1000}]
} else {
    set ack_period [lindex $argv 9]
}
if {[string first "ms" [lindex $argv 10]] != -1} {
    set timeout_per_string [string map {"ms" ""} [lindex $argv 10]]
    set timeout_period [expr {double($timeout_per_string)/1000}]
} else {
    set timeout_period [lindex $argv 10]
}
set apppktSize [lindex $argv 4]
set receiver [$ns link-arq $window $apppktSize $rate_k $ack_period $timeout_period $n1 $n3 [lindex $argv 12] [lindex $argv 7] [lindex $argv 13]]

#=== Set up a UDP connection ===
set udp [new Agent/UDP]
set sink [new Agent/Null]
set cbr [new Application/Traffic/CBR]

$cbr set type_ CBR
$cbr set packet_size_ $apppktSize
$cbr set rate_ [lindex $argv 3]
$cbr set random_ false

$ns attach-agent $n1 $udp
$ns attach-agent $n3 $sink
$cbr attach-agent $udp
$ns connect $udp $sink

$ns at 0.0 "$receiver print-parameters [expr {double(round(100*[lindex $argv 5])) + double(round(100*[lindex $argv 6]))/100}] [lindex $argv 7] [lindex $argv 11] [lindex $argv 12]"
$ns at 0.0 "$cbr start"
$ns at [lindex $argv 11] "$cbr stop"
$ns at [expr {[lindex $argv 11] + 0.5}] "print_stats [lindex $argv 5] [lindex $argv 7] [lindex $argv 11] [lindex $argv 12]"
$ns at [expr {[lindex $argv 11] + 1.0}] "exit 0"
$ns run
