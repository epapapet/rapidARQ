set arg_cnt [lindex $argc]
if {$arg_cnt != 11} {
    puts "# usage: ns <scriptfile> <bandwidth> <propagation_delay> <window_size> <cbr_rate> <pkt_size> <err_rate> <ack_rate> <num_rtx> <rate_k> <coding_depth> <simulation_time> <seed>"
    puts "# <bandwidth> : in bps, example: set to 5Mbps -> 5M or 5000000"
    puts "# <propagation_delay> : in secs, example: set to 30ms -> 30ms or 0.03"
    puts "# <window_size> : aqr window size in pkts"
    puts "# <cbr_rate> : the rate of the cbr applications, in bps, example: set to 3Mbps -> 3M or 3000000"
    puts "# <pkt_size> : the size of udp pkt (including UDP and IP headers)"
    puts "# <err_rate> : the error rate in the forward channel (error rate for frames)"
    puts "# <ack_rate> : the error rate in the return channel (error rate for ACKs)"
    puts "# <num_rtx> : the number of retransmissions allowed for a native pkt"
    puts "# <rate_k> : the number of native pkts sent before creating a coded pkt (actually define the code rate)"
    puts "# <coding_depth> : the number of coding cycles used to create a coded pkt"
    puts "# <simulation_time> : the simulation time in secs"
    puts "# <seed> : seed used to produce randomness"
    exit 1
}

CaterpillarTx set retry_limit_ 100
CaterpillarTx set rate_k 1000
CaterpillarTx set coding_depth 0
CaterpillarTx set lnk_delay_ 30ms
CaterpillarTx set lnk_bw_ 10M
CaterpillarTx set app_pkt_Size_ 1000
CaterpillarTx set debug_ NULL
CaterpillarAcker set debug_ NULL
CaterpillarNacker set debug_ NULL

SimpleLink instproc link-arq { wndsize apktsz ratekk coddpth limit vgseed ackerr } {
    $self instvar link_ link_errmodule_ queue_ drophead_ head_
    $self instvar tARQ_ acker_ nacker_
 
    set tARQ_ [new CaterpillarTx]
    set acker_ [new CaterpillarAcker]
    set nacker_ [new CaterpillarNacker]

    #Tx set up
	$tARQ_ setup-wnd $wndsize $ratekk $coddpth
    $tARQ_ set retry_limit_ $limit
    $tARQ_ set lnk_bw_ [$self bw]
    $tARQ_ set lnk_delay_ [$self delay]
    $tARQ_ set app_pkt_Size_ [expr {$apktsz + 40}]
    

    #Acker set up
    $acker_ attach-CaterpillarTx $tARQ_
    $acker_ setup-CaterpillarNacker $nacker_
    $acker_ setup-wnd $wndsize
    $acker_ update-delays
    
    set vagrngn2 [new RNG]
    $vagrngn2 seed [expr {$vgseed + 1}]
    set vagranvarn2 [new RandomVariable/Uniform]
    $vagranvarn2 use-rng $vagrngn2
    $acker_ ranvar $vagranvarn2
    $acker_ set-err $ackerr    


    #Nacker set up
    $nacker_ attach-CaterpillarTx $tARQ_
	$nacker_ setup-CaterpillarAcker $acker_
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

proc show_tcp_seqno {} {
    global tcp ns x
    puts "At [$ns now], The tcp sequence number is [$tcp set t_seqno_]"
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
set window [lindex $argv 2]
set em [new ErrorModel]
$em set rate_ [lindex $argv 4]

$em set enable_ 1
$em unit pkt
$em set bandwidth_ $link_bwd

set vagrng [new RNG]
$vagrng seed [lindex $argv 10]
set vagranvar [new RandomVariable/Uniform]
$vagranvar use-rng $vagrng

$em ranvar $vagranvar
$em drop-target [new Agent/Null]

$ns link-lossmodel $em $n1 $n3

set num_rtx [lindex $argv 6]
set rate_k [lindex $argv 7]
set cod_dpth [lindex $argv 8]
set apppktSize [lindex $argv 3]
set receiver [$ns link-arq $window $apppktSize $rate_k $cod_dpth $num_rtx $n1 $n3 [lindex $argv 10] [lindex $argv 5]]

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
$ns at [lindex $argv 9] "$ftp stop"
$ns at [expr {[lindex $argv 9] + 0.51}] show_tcp_seqno
$ns at [expr {[lindex $argv 9] + 0.5}] print_stats
$ns at [expr {[lindex $argv 9] + 1.0}] "exit 0"
$ns run
