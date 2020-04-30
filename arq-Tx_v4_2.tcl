# usage: ns <scriptfile> <bandwidth> <propagation_delay> <protocol> <window_size> <cbr_rate> <pkt_size> <err_rate> <ack_rate> <num_rtx> <rate_k> <coding_depth> <simulation_time> <seed>
# <bandwidth> : in bps, example: set to 5Mbps -> 5M or 5000000
# <propagation_delay> : in secs, example: set to 30ms -> 30ms or 0.03
# <protocol> : the protocol to be used (either Tetrys, Caterpillar and our protocol in any other case)
# <window_size> : aqr window size in pkts
# <cbr_rate> : the rate of the cbr applications, in bps, example: set to 3Mbps -> 3M or 3000000
# <pkt_size> : the size of udp pkt (including UDP and IP headers)
# <err_rate> : the error rate in the forward channel (error rate for frames)
# <ack_rate> : the error rate in the return channel (error rate for ACKs)
# <num_rtx> : the number of retransmissions allowed for a native pkt
# <rate_k> : the number of native pkts sent before creating a coded pkt (actually define the code rate)
# <coding_depth> : the number of coding cycles used to create a coded pkt
# <simulation_time> : the simulation time in secs
# <seed> : seed used to produce randomness

set protocol [lindex $argv 2]

set protocolTx "ARQTx"
set protocolAcker "ARQAcker"
set protocolNAcker "ARQNacker"


if {$protocol == "Tetrys"} { 
     set protocolTx "TetrysTx"
     set protocolAcker "TetrysAcker"
     set protocolNAcker "TetrysNacker"
} elseif {$protocol == "Caterpillar"} {
     set protocolTx "CaterpillarTx"
     set protocolAcker "CaterpillarAcker"
     set protocolNAcker "CaterpillarNacker"
} 

$protocolTx set retry_limit_ 100
$protocolTx set rate_k 1000
$protocolTx set coding_depth 0
$protocolTx set lnk_delay_ 30ms
$protocolTx set lnk_bw_ 10M
$protocolTx set app_pkt_Size_ 1000
$protocolTx set debug_ NULL
$protocolAcker set debug_ NULL
$protocolNAcker set debug_ NULL

SimpleLink instproc link-arq { wndsize apktsz ratekk coddpth limit vgseed ackerr } {
    $self instvar link_ link_errmodule_ queue_ drophead_ head_
    $self instvar tARQ_ acker_ nacker_
    global protocolTx protocolAcker protocolNAcker

    append attachTX "attach-" $protocolTx
    append setupAcker "setup-" $protocolAcker
    append setupNAcker "setup-" $protocolNAcker
 
    set tARQ_ [new $protocolTx]
    set acker_ [new $protocolAcker]
    set nacker_ [new $protocolNAcker]

    #Tx set up
    $tARQ_ set retry_limit_ $limit
    $tARQ_ set rate_k $ratekk
    $tARQ_ set coding_depth $coddpth
    $tARQ_ set lnk_bw_ [$self bw]
    $tARQ_ set lnk_delay_ [$self delay]
    $tARQ_ set app_pkt_Size_ [expr {8*$apktsz}]
    $tARQ_ setup-wnd $wndsize
    

    #Acker set up
    $acker_ $attachTX $tARQ_
    $acker_ $setupNAcker $nacker_
    $acker_ setup-wnd $wndsize
    $acker_ update-delays
    
    set vagrngn2 [new RNG]
    $vagrngn2 seed [expr {$vgseed + 1}]
    set vagranvarn2 [new RandomVariable/Uniform]
    $vagranvarn2 use-rng $vagrngn2
    $acker_ ranvar $vagranvarn2
    $acker_ set-err $ackerr    


    #Nacker set up
    $nacker_ $attachTX $tARQ_
	$nacker_ $setupAcker $acker_
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

Simulator instproc link-arq {wndsize apktsize ratek coddth limit from to vgseed ackerr} {
    set link [$self link $from $to]
    set acker [$link link-arq $wndsize $apktsize $ratek $coddth $limit $vgseed $ackerr]
	return $acker
}

proc print_stats {} {
	global receiver
	$receiver print-stats
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
set window [lindex $argv 3]
set em [new ErrorModel]
$em set rate_ [lindex $argv 6]

$em set enable_ 1
$em unit pkt
$em set bandwidth_ $link_bwd

set vagrng [new RNG]
$vagrng seed [lindex $argv 12]
set vagranvar [new RandomVariable/Uniform]
$vagranvar use-rng $vagrng

$em ranvar $vagranvar
$em drop-target [new Agent/Null]

$ns link-lossmodel $em $n1 $n3

set num_rtx [lindex $argv 8]
set rate_k [lindex $argv 9]
set cod_dpth [lindex $argv 10]
set apppktSize [lindex $argv 5]
set receiver [$ns link-arq $window $apppktSize $rate_k $cod_dpth $num_rtx $n1 $n3 [lindex $argv 12] [lindex $argv 7]]

#=== Set up a UDP connection ===
set udp [new Agent/UDP]
set sink [new Agent/Null]
set cbr [new Application/Traffic/CBR]

$cbr set type_ CBR
$cbr set packet_size_ $apppktSize
$cbr set rate_ [lindex $argv 4]
$cbr set random_ false

$ns attach-agent $n1 $udp
$ns attach-agent $n3 $sink
$cbr attach-agent $udp
$ns connect $udp $sink

$ns at 0.0 "$cbr start"
$ns at [lindex $argv 11] "$cbr stop"
$ns at [expr {[lindex $argv 11] + 0.5}] print_stats
$ns at [expr {[lindex $argv 11] + 1.0}] "exit 0"
$ns run
