#       http://ns2ultimate.tumblr.com/post/1539258323/arq-module-program-update


ARQTx set delay_ 300ms
ARQTx set retry_limit_ 100
ARQTx set rate_k 1000
ARQTx set debug_ NULL

ARQAcker set delay_ 300ms
ARQAcker set debug_ NULL

ARQNacker set delay_ 300ms
ARQNacker set debug_ NULL


# ==== arq.tcl ====

# usage: ns <scriptfile> <window_size> <err_rate> <ack_rate> <num_rtx> <seed> <source_pkts>
SimpleLink instproc link-arq { limit wndsize vgseed ackerr source_pkts} {
    $self instvar link_ link_errmodule_ queue_ drophead_ head_
    $self instvar tARQ_ acker_ nacker_
 
    set tARQ_ [new ARQTx]
    set acker_ [new ARQAcker]
    set nacker_ [new ARQNacker]
    $tARQ_ set retry_limit_ $limit
	$tARQ_ set rate_k $source_pkts

    $tARQ_ setup-wnd $wndsize
    $acker_ attach-ARQTx $tARQ_
    $nacker_ attach-ARQTx $tARQ_
    #$acker_ setup-wnd $wndsize $source_pkts
	$acker_ setup-wnd $wndsize
	$nacker_ setup-ARQAcker $acker_

    $tARQ_ target [$queue_ target]
    $queue_ target $tARQ_
    $acker_ target [$link_errmodule_ target]
    $link_errmodule_ target $acker_
    $link_errmodule_ drop-target $nacker_
    $tARQ_ drop-target $drophead_

    $acker_ set delay_ [$self delay]
    $nacker_ set delay_ [$self delay]

    set vagrngn2 [new RNG]
    $vagrngn2 seed [expr {$vgseed + 1}]
    set vagranvarn2 [new RandomVariable/Uniform]
    $vagranvarn2 use-rng $vagrngn2

    $acker_ ranvar $vagranvarn2
    
    $acker_ set-err $ackerr
    
	return $acker_
}

Simulator instproc link-arq {wndsize source_pkts limit from to vgseed ackerr} {
    set link [$self link $from $to]
    set acker [$link link-arq $limit $wndsize $vgseed $ackerr $source_pkts]
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
$ns duplex-link $n1 $n2 5M 30ms DropTail
$ns duplex-link $n2 $n3 5M 30ms DropTail
$ns duplex-link $n1 $n3 5M 30ms DropTail

#=== Create error and ARQ module ===
set window [lindex $argv 0]
set em [new ErrorModel]
$em set rate_ [lindex $argv 1]

$em set enable_ 1
$em unit pkt
$em set bandwidth_ 5M

set vagrng [new RNG]
$vagrng seed [lindex $argv 4]
set vagranvar [new RandomVariable/Uniform]
$vagranvar use-rng $vagrng

$em ranvar $vagranvar
$em drop-target [new Agent/Null]

$ns link-lossmodel $em $n1 $n3

set num_rtx [lindex $argv 3]
set source_pkts [lindex $argv 5]
set receiver [$ns link-arq $window $source_pkts $num_rtx $n1 $n3 [lindex $argv 4] [lindex $argv 2]]

#=== Set up a UDP connection ===
set udp [new Agent/UDP]
set sink [new Agent/Null]
set cbr [new Application/Traffic/CBR]

$cbr set type_ CBR
$cbr set packet_size_ 1000
$cbr set rate_ 3mb
$cbr set random_ false

$ns attach-agent $n1 $udp
$ns attach-agent $n3 $sink
$cbr attach-agent $udp
$ns connect $udp $sink

$ns at 0.0 "$cbr start"
$ns at 100.0 print_stats
$ns at 100.1 "exit 0"
$ns run
