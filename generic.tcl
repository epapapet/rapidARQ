#Default values
set opt(protocol) CARQ
set opt(bw) 5M
set opt(delay) 50ms
set opt(wnd) 65
set opt(transport) "CBR"
set opt(cbrrate) 5M
set opt(pktsize) 1000
set opt(err) 0.1
set opt(burstduration) 0.5
set opt(ackerr) 0.1
set opt(rtx) 100
set opt(coderate) 9
set opt(codingdepth) 3
set opt(timeout) 0
set opt(ackperiod) 0
set opt(simtime) 1000
set opt(seed) 1
set opt(resfile) "multiarq"

set enabled(protocol) 0
set enabled(bw) 0
set enabled(delay) 0
set enabled(wnd) 0
set enabled(transport) 0
set enabled(cbrrate) 0
set enabled(pktsize) 0
set enabled(err) 0
set enabled(burstduration) 0
set enabled(ackerr) 0
set enabled(rtx) 0
set enabled(coderate) 0
set enabled(codingdepth) 0
set enabled(timeout) 0
set enabled(ackperiod) 0
set enabled(simtime) 0
set enabled(seed) 0
set enabled(resfile) 0

if { $argc == 0} { puts "Using default values"}

for {set i 0} {$i < $argc} {incr i} {

    set argument_prefix [lindex $argv $i]
    if {$argument_prefix == "-PROTOCOL"} {
        set parameter_index "protocol"
    } elseif {$argument_prefix == "-BW"} {
        set parameter_index "bw"
    } elseif {$argument_prefix == "-DELAY"} {
        set parameter_index "delay"
    } elseif {$argument_prefix == "-WND"} {
        set parameter_index "wnd"
    } elseif {$argument_prefix == "-TRANSPORT"} {
        set parameter_index "transport"
    } elseif {$argument_prefix == "-CBR_RATE"} {
        set parameter_index "cbrrate"
    } elseif {$argument_prefix == "-PKT_SIZE"} {
        set parameter_index "pktsize"
    } elseif {$argument_prefix == "-ERR"} {
        set parameter_index "err"
    } elseif {$argument_prefix == "-BURST_DURATION"} {
        set parameter_index "burstduration"
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
    } elseif {$argument_prefix == "-ACKPERIOD"} {
        set parameter_index "ackperiod"
    } elseif {$argument_prefix == "-SIMTIME"} {
        set parameter_index "simtime"
    } elseif {$argument_prefix == "-SEED"} {
        set parameter_index "seed"
    } elseif {$argument_prefix == "-RESULTSFILE"} {
        set parameter_index "resfile"
    } else {
    puts "# usage: ns <scriptfile> -OPTION1 value1, -OPTION2 value2, -OPTION3 value3, .... where OPTION1, OPTION2, OPTION3, .... can be one of the following options:"
    puts "# -PROTOCOL <pr_value> : set the protocol to be used, <pr_value> can be CARQ, Caterpillar, Tetrys or SRARQ"
    puts "# -BW <b_value> : set bandwidth, <b_value> in bps, example: set to 5Mbps -> 5M or 5000000"
    puts "# -DELAY <d_value> : set propagation delay, <d_value> in secs, example: set to 30ms -> 30ms or 0.03"
    puts "# -WND <w_value> : set the arq window size, <w_value> in pkts"
    puts "# -TRANSPORT <tr_value>: set the transport protocol to be used, <tr_value> is either FTP or CBR"
    puts "# -CBR_RATE <cr_value> : set the rate of the cbr applications, <cr_value> in bps, example: set to 3Mbps -> 3M or 3000000"
    puts "# -PKT_SIZE <ps_value> : set the size of udp pkt (including UDP and IP headers), <ps_value> in bytes"
    puts "# -ERR <e_value> : set the error rate in the forward channel (error rate for frames), 0 <= <e_value> < 1"
    puts "# -BURST_DURATION <bd_value> : 0,..,1 -> the percentage of time that the channel is in an error burst state, usage enables a two-state error channel"
    puts "# -ACK_ERR <ae_value> : set the error rate in the return channel (error rate for ACKs), 0 <= <ae_value> < 1"
    puts "# -RTX <r_value> : set the number of retransmissions allowed for a native pkt, <r_value> is an integer >= 0"
    puts "# -CODERATE <cr_value> : set the number of native pkts sent before creating a coded pkt (actually define the code rate), <cr_value> is an integer >= 0, 0 deactivates coding"
	puts "# -CODING_DEPTH <cd_value> : set the number of coding cycles used to create a coded pkt, <cd_value> is an integer \in \[0, floor(<w_value/<c_value>>)\], 0 set the coding window equal to the arq window"    
	puts "# -TIMEOUT <t_value> : set the time for expiring an non acked pkt, <t_value> in secs, example: set to 30ms->30ms or 0.03, 0 sets timeout=RTT, a value v<0 will set the timeout=-(RTT)/v"
    puts "# -ACKPERIOD <ackp_value> : set the period for sending periodic acks, <ackp_value> in secs, example: set to 30ms->30ms or 0.03, 0 sets ack_period=RTT, a value v<0 will set the ack_period=-(window duration)/v"
    puts "# -SIMTIME <s_time> : set the simulation time, <s_time> in secs"
    puts "# -SEED <se_value> : set the seed to control randomness"
	puts "# -RESULTSFILE <res_value> : set the filename to store results, <res_value> is a string, setting <res_value> to the values 1-4 will produce predefined file names"
    
        exit 1
    }

    set indexofvalue [expr {$i + 1}]
    set opt($parameter_index) [lindex $argv $indexofvalue]
    set enabled($parameter_index) 1

    incr i
}

    #Checks for argument correctness
    if { $opt(protocol) != "CARQ" &&  $opt(protocol) != "Caterpillar" && $opt(protocol) != "Tetrys" && $opt(protocol) != "SRARQ" } { 
        puts "Not a valid protocol. Exiting..." 
        exit 1
    }
    if { $opt(protocol) == "SRARQ" } {
        if { $enabled(coderate) == 1 || $enabled(codingdepth) == 1 || $enabled(ackperiod) == 1 } {
            puts "Not compatible parameters..."
            exit 1
        }
    } elseif { $opt(protocol) == "Tetrys" } {
        if { $enabled(codingdepth) == 1 || $enabled(rtx) == 1 } {
            puts "Not compatible parameters..."
            exit 1
        }
    } elseif { $opt(protocol) == "Caterpillar" } {
        if { $enabled(codingdepth) == 1 || $enabled(ackperiod) == 1 } {
            puts "Not compatible parameters..."
            exit 1
        }
    } elseif { $opt(protocol) == "CARQ" } {
        if { $enabled(ackperiod) == 1 } {
            puts "Not compatible parameters..."
            exit 1
        }
    }
    if {[string first "ms" $opt(timeout)] != -1} {
        set timeout_per_string [string map {"ms" ""} $opt(timeout)]
        set opt(timeout) [expr {double($timeout_per_string)/1000}]
    }
    if {[string first "ms" $opt(ackperiod)] != -1} {
        set ackperiod_per_string [string map {"ms" ""} $opt(ackperiod)]
        set opt(ackperiod) [expr {double($ackperiod_per_string)/1000}]
    }
    if { $opt(transport) != "FTP" &&  $opt(transport) != "CBR" } {
            puts "Not a valid transport protocol..."
            exit 1
    }
    if { $opt(transport) == "FTP" && $enabled(cbrrate) == 1 } {
        puts "Not compatible parameters..."
        exit 1
    }

set protocolTx "$opt(protocol)Tx"
set protocolAcker "$opt(protocol)Acker"
set protocolNAcker "$opt(protocol)Nacker"

if { $opt(protocol) != "Tertys" } { 
    $protocolTx set retry_limit_ $opt(rtx) 
}
if { $opt(protocol) != "SRARQ" } { 
    $protocolTx set rate_k $opt(coderate) 
}
if { $opt(protocol) == "CARQ" } { 
    $protocolTx set coding_depth $opt(codingdepth) 
}
$protocolTx set lnk_delay_ $opt(delay)
$protocolTx set lnk_bw_ $opt(bw)
$protocolTx set app_pkt_Size_ $opt(pktsize)
$protocolTx set debug_ NULL
$protocolAcker set debug_ NULL
$protocolNAcker set debug_ NULL

SimpleLink instproc link-arq {} {
    $self instvar link_ link_errmodule_ queue_ drophead_ head_
    $self instvar tARQ_ acker_ nacker_
    global opt protocolTx protocolAcker protocolNAcker

 
    set tARQ_ [new $protocolTx]
    set acker_ [new $protocolAcker]
    set nacker_ [new $protocolNAcker]

    #Tx set up
    if { $opt(protocol) != "Tertys" } {
        $tARQ_ set retry_limit_ $opt(rtx) 
    }
    $tARQ_ set lnk_bw_ [$self bw]
    $tARQ_ set lnk_delay_ [$self delay]
    $tARQ_ set app_pkt_Size_ $opt(pktsize)
    if { $opt(protocol) == "CARQ" } {
        $tARQ_ setup-wnd $opt(wnd) $opt(coderate) $opt(codingdepth) $opt(timeout)
    } elseif { $opt(protocol) == "Tetrys" || $opt(protocol) == "Caterpillar" } {
        $tARQ_ setup-wnd $opt(wnd) $opt(coderate) $opt(timeout)
    } elseif { $opt(protocol) == "SRARQ" } {
        $tARQ_ setup-wnd $opt(wnd) $opt(timeout)
    }

    set vagrngn2 [new RNG]
    $vagrngn2 seed [expr {$opt(seed) + 1}]
    set vagranvarn2 [new RandomVariable/Uniform]
    $vagranvarn2 use-rng $vagrngn2
    $tARQ_ ranvar $vagranvarn2
    $tARQ_ set-err $opt(ackerr)    

    #Acker set up
    $acker_ attach-$protocolTx $tARQ_
    $acker_ setup-$protocolNAcker $nacker_
    $acker_ setup-wnd $opt(wnd)
	$acker_ setup-filename $opt(resfile)
    if { $opt(protocol) == "Tetrys" } {
       $acker_ update-delays $opt(ackperiod)
    } else {
        $acker_ update-delays
    }
    
    
    #Nacker set up
    $nacker_ attach-$protocolTx $tARQ_
	$nacker_ setup-$protocolAcker $acker_
    if { $opt(protocol) == "Tetrys" } {
       $acker_ update-delays $opt(ackperiod)
    } else {
        $acker_ update-delays
    }

    
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

if { $enabled(burstduration) == 0 } {
    #Create a simple uniform Errormodel
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
} else {
    #Create uniform Errormodel representing first state
    set tmp [new ErrorModel]
    $tmp set rate_ 0
    $tmp set enable_ 1
    $tmp set bandwidth_ $opt(bw)
    set vagrng00 [new RNG]
    $vagrng00 seed [expr {$opt(seed) + 10}]
    set vagranvar00 [new RandomVariable/Uniform]
    $vagranvar00 use-rng $vagrng00
    $tmp ranvar $vagranvar00
    #Create uniform Errormodel representing second state
    set tmp1 [new ErrorModel]
    $tmp1 set rate_ $opt(err)
    $tmp1 set enable_ 1
    $tmp1 set bandwidth_ $opt(bw)
    set vagrng01 [new RNG]
    $vagrng01 seed $opt(seed)
    set vagranvar01 [new RandomVariable/Uniform]
    $vagranvar01 use-rng $vagrng01
    $tmp ranvar $vagranvar01

    if {$opt(burstduration) > 1 || $opt(burstduration) < 0} {
        puts "Burst duration percentage should be in \[0, 1\]"
        exit 1;
    }
    if {[string first "M" $opt(bw)] != -1} {
        set bwd_per_string [string map {"M" ""} $opt(bw)]
        set bwdcalc [expr {double($bwd_per_string)*1000000}]
    } else {
        set bwdcalc $opt(bw)
    }
    set bduration [lindex $argv 6]
    set wndduration [expr {8.0*$opt(wnd)*$opt(pktsize)/$bwdcalc}]
    set state1nduration [expr {(1- $opt(burstduration))*$wndduration}]
    set state2nduration [expr {$opt(burstduration)*$wndduration}]


    # Array of states (error models)
    set m_states [list $tmp $tmp1]
    # Durations for each of the states, tmp, tmp1 and tmp2, respectively
    set m_periods [list $state1nduration $state2nduration]
    # Transition state model matrix
    set m_transmx { {0 1} {1 0}}
    set m_trunit pkt
    # Use time-based transition
    set m_sttype time
    set m_nstates 2
    set m_nstart [lindex $m_states 0]
    set em [new ErrorModel/MultiState $m_states $m_periods $m_transmx $m_trunit $m_sttype $m_nstates $m_nstart]

}

$ns link-lossmodel $em $n1 $n3
set receiver [$ns link-arq $n1 $n3]

if { $opt(transport) == "CBR" } { 
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

} else {

    #=== Set up a TCP connection ===
    set tcp [new Agent/TCP]
    $tcp set packetSize_ $opt(pktsize)
    set sink [new Agent/TCPSink]
    set ftp [new Application/FTP]
    $ns attach-agent $n1 $tcp

    $ns attach-agent $n3 $sink
    $ftp attach-agent $tcp
    $ns connect $tcp $sink

}

if { $enabled(burstduration) == 1 } {
    $ns at 0.0 "$receiver print-parameters [expr {double(round(100*$opt(err))) + double(round(100*$opt(burstduration)))/100}] $opt(ackerr) $opt(simtime) $opt(seed)"
} else {
    $ns at 0.0 "$receiver print-parameters $opt(err) $opt(ackerr) $opt(simtime) $opt(seed)"
}

if { $opt(transport) == "CBR" } { 
    $ns at 0.0 "$cbr start"
    $ns at $opt(simtime) "$cbr stop"
} else {
    $ns at 0.0 "$ftp start"
    $ns at $opt(simtime) "$ftp stop"
}

$ns at [expr {$opt(simtime) + 0.5}] "print_stats"
$ns at [expr {$opt(simtime) + 1.0}] "exit 0"
$ns run
