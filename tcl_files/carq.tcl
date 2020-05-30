#Default values
set opt(bw) 5M
set opt(delay) 50ms
set opt(wnd) 65
set opt(cbrrate) 5M
set opt(pktsize) 1000
set opt(err) 0.1
set opt(ackerr) 0.1
set opt(rtx) 100
set opt(coderate) 9
set opt(codingdepth) 3
set opt(timeout) 0
set opt(simtime) 1000
set opt(seed) 1
set opt(resfile) "carq"

if { $argc == 0} { puts "Using default values"}

for {set i 0} {$i < $argc} {incr i} {

    set argument_prefix [lindex $argv $i]
    if {$argument_prefix == "-BW"} {
        set parameter_index "bw"
    } elseif {$argument_prefix == "-DELAY"} {
        set parameter_index "delay"
    } elseif {$argument_prefix == "-WND"} {
        set parameter_index "wnd"
    } elseif {$argument_prefix == "-CBR_RATE"} {
        set parameter_index "cbrrate"
    } elseif {$argument_prefix == "-PKT_SIZE"} {
        set parameter_index "pktsize"
    } elseif {$argument_prefix == "-ERR"} {
        set parameter_index "err"
    } elseif {$argument_prefix == "-ACK_ERR"} {
        set parameter_index "ackerr"
    } elseif {$argument_prefix == "-RTX"} {
        set parameter_index "rtx"
    } elseif {$argument_prefix == "-CODERATE"} {
        set parameter_index "coderate"
    } elseif {$argument_prefix == "-CODINGDEPTH"} {
        set parameter_index "codingdepth"
    } elseif {$argument_prefix == "-TIMEOUT"} {
        set parameter_index "timeout"
    } elseif {$argument_prefix == "-SIMTIME"} {
        set parameter_index "simtime"
    } elseif {$argument_prefix == "-SEED"} {
        set parameter_index "seed"
    } elseif {$argument_prefix == "-RESULTSFILE"} {
        set parameter_index "resfile"
    } else {
    puts "# usage: ns <scriptfile> -OPTION1 value1, -OPTION2 value2, -OPTION3 value3, .... where OPTION1, OPTION2, OPTION3, .... can be one of the following options:"
    puts "# -BW <b_value> : set bandwidth, <b_value> in bps, example: set to 5Mbps -> 5M or 5000000"
    puts "# -DELAY <d_value> : set propagation delay, <d_value> in secs, example: set to 30ms -> 30ms or 0.03"
    puts "# -WND <w_value> : set the arq window size, <w_value> in pkts"
    puts "# -CBR_RATE <cr_value> : set the rate of the cbr applications, <cr_value> in bps, example: set to 3Mbps -> 3M or 3000000"
    puts "# -PKT_SIZE <ps_value> : set the size of udp pkt (including UDP and IP headers), <ps_value> in bytes"
    puts "# -ERR <e_value> : set the error rate in the forward channel (error rate for frames), 0 <= <e_value> < 1"
    puts "# -ACK_ERR <ae_value> : set the error rate in the return channel (error rate for ACKs), 0 <= <ae_value> < 1"
    puts "# -RTX <r_value> : set the number of retransmissions allowed for a native pkt, <r_value> is an integer >= 0"
    puts "# -CODERATE <cr_value> : set the number of native pkts sent before creating a coded pkt (actually define the code rate), <cr_value> is an integer >= 0, 0 deactivates coding"
	puts "# -CODING_DEPTH <cd_value> : set the number of coding cycles used to create a coded pkt, <cd_value> is an integer \in \[0, floor(<w_value/<c_value>>)\], 0 set the coding window equal to the arq window"    
	puts "# -TIMEOUT <t_value> : set the time for expiring an non acked pkt, <t_value> in secs, example: set to 30ms->30ms or 0.03, 0 sets timeout=RTT, a value v<0 will set the timeout=-(RTT)/v"
    puts "# -SIMTIME <s_time> : set the simulation time, <s_time> in secs"
    puts "# -SEED <se_value> : set the seed to control randomness"
	puts "# -RESULTSFILE <res_value> : set the filename to store results, <res_value> is a string, setting <res_value> to the values 1-4 will produce predefined file names"
    
        exit 1
    }

    set indexofvalue [expr {$i + 1}]
    set opt($parameter_index) [lindex $argv $indexofvalue]
    
    if {$parameter_index == "timeout"} {
        if {[string first "ms" $opt(timeout)] != -1} {
            set timeout_per_string [string map {"ms" ""} $opt(timeout)]
            set opt(timeout) [expr {double($timeout_per_string)/1000}]
        }
    }

    incr i
}

CARQTx set retry_limit_ $opt(rtx)
CARQTx set rate_k $opt(coderate)
CARQTx set coding_depth $opt(codingdepth)
CARQTx set lnk_delay_ $opt(delay)
CARQTx set lnk_bw_ $opt(bw)
CARQTx set app_pkt_Size_ $opt(pktsize)
CARQTx set debug_ NULL
CARQAcker set debug_ NULL
CARQNacker set debug_ NULL

SimpleLink instproc link-arq {} {
    $self instvar link_ link_errmodule_ queue_ drophead_ head_
    $self instvar tARQ_ acker_ nacker_
    global opt

    puts "$opt(rtx)"
 
    set tARQ_ [new CARQTx]
    set acker_ [new CARQAcker]
    set nacker_ [new CARQNacker]

    #Tx set up
    $tARQ_ set retry_limit_ $opt(rtx)
    $tARQ_ set lnk_bw_ [$self bw]
    $tARQ_ set lnk_delay_ [$self delay]
    $tARQ_ set app_pkt_Size_ $opt(pktsize)
    $tARQ_ setup-wnd $opt(wnd) $opt(coderate) $opt(codingdepth) $opt(timeout)

    set vagrngn2 [new RNG]
    $vagrngn2 seed [expr {$opt(seed) + 1}]
    set vagranvarn2 [new RandomVariable/Uniform]
    $vagranvarn2 use-rng $vagrngn2
    $tARQ_ ranvar $vagranvarn2
    $tARQ_ set-err $opt(ackerr)    

    #Acker set up
    $acker_ attach-CARQTx $tARQ_
    $acker_ setup-CARQNacker $nacker_
    $acker_ setup-wnd $opt(wnd)
	$acker_ setup-filename $opt(resfile)
    $acker_ update-delays
    
    #Nacker set up
    $nacker_ attach-CARQTx $tARQ_
	$nacker_ setup-CARQAcker $acker_
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

Simulator instproc link-arq { from to } {
    set link [$self link $from $to]
    set acker [$link link-arq ]
	return $acker
}

proc print_stats { } {
	global receiver
    global opt
	$receiver print-stats $opt(err) $opt(ackerr) $opt(simtime) $opt(seed)
}

#=== Create the Simulator, Nodes, and Links ===
set ns [new Simulator]
set n1 [$ns node]
set n2 [$ns node]
set n3 [$ns node]

$ns duplex-link $n1 $n2 $opt(bw) $opt(delay) DropTail
$ns duplex-link $n2 $n3 $opt(bw) $opt(delay) DropTail
$ns duplex-link $n1 $n3 $opt(bw) $opt(delay) DropTail

#=== Create error and ARQ module ===
set window $opt(wnd)
set em [new ErrorModel]
$em set rate_ $opt(err)

$em set enable_ 1
$em unit pkt
$em set bandwidth_ $opt(bw)

set vagrng [new RNG]
$vagrng seed $opt(seed)
set vagranvar [new RandomVariable/Uniform]
$vagranvar use-rng $vagrng

$em ranvar $vagranvar
$em drop-target [new Agent/Null]

$ns link-lossmodel $em $n1 $n3

set receiver [$ns link-arq $n1 $n3]

#=== Set up a UDP connection ===
set udp [new Agent/UDP]
set sink [new Agent/Null]
set cbr [new Application/Traffic/CBR]

$cbr set type_ CBR
$cbr set packet_size_ $opt(pktsize)
$cbr set rate_ $opt(cbrrate)
$cbr set random_ false

$ns attach-agent $n1 $udp
$ns attach-agent $n3 $sink
$cbr attach-agent $udp
$ns connect $udp $sink

$ns at 0.0 "$receiver print-parameters $opt(err) $opt(ackerr) $opt(simtime) $opt(seed)"
$ns at 0.0 "$cbr start"
$ns at $opt(simtime) "$cbr stop"
$ns at [expr {$opt(simtime) + 0.5}] "print_stats"
$ns at [expr {$opt(simtime) + 1.0}] "exit 0"
$ns run
