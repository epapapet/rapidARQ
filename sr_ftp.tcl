set arg_cnt [lindex $argc]
if {$arg_cnt != 10} {
    puts "# usage: ns <scriptfile> <bandwidth> <propagation_delay> <window_size> <pkt_size> <err_rate> <ack_rate> <num_rtx> <timeout> <simulation_time> <seed>"
    puts "# <bandwidth> : in bps, example: set to 5Mbps -> 5M or 5000000"
    puts "# <propagation_delay> : in secs, example: set to 30ms -> 30ms or 0.03"
    puts "# <window_size> : arq window size in pkts"
    puts "# <pkt_size> : the size of a TCP segment (not including the TCP and IP headers)"
    puts "# <err_rate> : the error rate in the forward channel (error rate for frames)"
    puts "# <ack_rate> : the error rate in the return channel (error rate for ACKs)"
    puts "# <num_rtx> : the number of retransmissions allowed for a native pkt"
    puts "# <timeout> : the time for expiring an non acked pkt, example: set to 30ms->30ms or 0.03, 0 sets timeout=RTT, a value v<0 will set the timeout=-(RTT)/v"
    puts "# <simulation_time> : the simulation time in secs"
    puts "# <seed> : seed used to produce randomness"
    exit 1
}

SRARQTx set retry_limit_ 100
SRARQTx set rate_k 1000
SRARQTx set lnk_delay_ 30ms
SRARQTx set lnk_bw_ 10M
SRARQTx set app_pkt_Size_ 1000
SRARQTx set debug_ NULL
SRARQAcker set debug_ NULL
SRARQNacker set debug_ NULL

SimpleLink instproc link-arq { wndsize apktsz timeoutt limit vgseed ackerr } {
    $self instvar link_ link_errmodule_ queue_ drophead_ head_
    $self instvar tARQ_ acker_ nacker_
 
    set tARQ_ [new SRARQTx]
    set acker_ [new SRARQAcker]
    set nacker_ [new SRARQNacker]

    #Tx set up
    $tARQ_ set retry_limit_ $limit
    $tARQ_ set lnk_bw_ [$self bw]
    $tARQ_ set lnk_delay_ [$self delay]
    $tARQ_ set app_pkt_Size_ [expr {$apktsz + 40}]
    $tARQ_ setup-wnd $wndsize $timeoutt

    set vagrngn2 [new RNG]
    $vagrngn2 seed [expr {$vgseed + 1}]
    set vagranvarn2 [new RandomVariable/Uniform]
    $vagranvarn2 use-rng $vagrngn2
    $tARQ_ ranvar $vagranvarn2
    $tARQ_ set-err $ackerr    

    #Acker set up
    $acker_ attach-SRARQTx $tARQ_
    $acker_ setup-SRARQNacker $nacker_
    $acker_ setup-wnd $wndsize
    $acker_ update-delays
    
    #Nacker set up
    $nacker_ attach-SRARQTx $tARQ_
	$nacker_ setup-SRARQAcker $acker_
    $nacker_ update-delays

    
    #Connections between Tx, Acker, Nacker, queue, drop-target and Acker target
    $tARQ_ target [$queue_ target]
    $queue_ target $tARQ_
    $acker_ target [$link_errmodule_ target]
    $link_errmodule_ target $acker_
    $link_errmodule_ drop-target $nacker_
    $tARQ_ drop-target $drophead_
    
	return $acker_
}

Simulator instproc link-arq {wndsize apktsize timeout limit from to vgseed ackerr} {
    set link [$self link $from $to]
    set acker [$link link-arq $wndsize $apktsize $timeout $limit $vgseed $ackerr]
	return $acker
}

proc show_tcp_seqno {} {
    global tcp ns x
    puts "At [$ns now], The tcp sequence number is [$tcp set t_seqno_]"
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
set em [new ErrorModel]
$em set rate_ [lindex $argv 4]

$em set enable_ 1
$em unit pkt
$em set bandwidth_ $link_bwd

set vagrng [new RNG]
$vagrng seed [lindex $argv 9]
set vagranvar [new RandomVariable/Uniform]
$vagranvar use-rng $vagrng

$em ranvar $vagranvar
$em drop-target [new Agent/Null]

$ns link-lossmodel $em $n1 $n3

set num_rtx [lindex $argv 6]
if {[string first "ms" [lindex $argv 7]] != -1} {
    set timeout_per_string [string map {"ms" ""} [lindex $argv 7]]
    set timeout_period [expr {double($timeout_per_string)/1000}]
} else {
    set timeout_period [lindex $argv 7]
}
set apppktSize [lindex $argv 3]
set receiver [$ns link-arq $window $apppktSize $timeout_period $num_rtx $n1 $n3 [lindex $argv 9] [lindex $argv 5]]

#=== Set up a UDP connection ===
set tcp [new Agent/TCP]
$tcp set packetSize_ $apppktSize
set sink [new Agent/TCPSink]
set ftp [new Application/FTP]
$ns attach-agent $n1 $tcp

$ns attach-agent $n3 $sink
$ftp attach-agent $tcp
$ns connect $tcp $sink

$ns at 0.0 "$ftp start"
$ns at [lindex $argv 8] "$ftp stop"
$ns at [expr {[lindex $argv 8] + 0.51}] show_tcp_seqno
$ns at [expr {[lindex $argv 8] + 0.5}] "print_stats [lindex $argv 4] [lindex $argv 5] [lindex $argv 8] [lindex $argv 9]"
$ns at [expr {[lindex $argv 8] + 1.0}] "exit 0"
$ns run
