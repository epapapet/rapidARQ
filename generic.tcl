#Default values
set opt(protocol) CARQ
set opt(bw) 5M
set opt(delay) 50ms
set opt(wnd) 65
set opt(apptype) "CBR"
set opt(cbrrate) 5M
set opt(pktsize) 1000
set opt(err) 0.1
set opt(burstduration) 0.5
set opt(ackerr) 0.1
set opt(ackburstduration) 0.5
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
set enabled(apptype) 0
set enabled(cbrrate) 0
set enabled(pktsize) 0
set enabled(err) 0
set enabled(burstduration) 0
set enabled(ackerr) 0
set enabled(ackburstduration) 0
set enabled(rtx) 0
set enabled(coderate) 0
set enabled(codingdepth) 0
set enabled(timeout) 0
set enabled(ackperiod) 0
set enabled(simtime) 0
set enabled(seed) 0
set enabled(resfile) 0

if { $argc == 0} { puts -nonewline "\033\[1;31m"; puts "Using default values. Type generic.tcl -HELP for usage instructions."; puts -nonewline "\033\[0m"; puts ""}

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
    } elseif {$argument_prefix == "-APP_TYPE"} {
        set parameter_index "apptype"
    } elseif {$argument_prefix == "-CBR_RATE"} {
        set parameter_index "cbrrate"
    } elseif {$argument_prefix == "-PKT_SIZE"} {
        set parameter_index "pktsize"
    } elseif {$argument_prefix == "-ERR"} {
        set parameter_index "err"
    } elseif {$argument_prefix == "-BURST_DURATION"} {
        set parameter_index "burstduration"
    } elseif {$argument_prefix == "-ACK_BURST_DURATION"} {
        set parameter_index "ackburstduration"
    } elseif {$argument_prefix == "-ACK_ERR"} {
        set parameter_index "ackerr"
    } elseif {$argument_prefix == "-RTX"} {
        set parameter_index "rtx"
    } elseif {$argument_prefix == "-CODERATE"} {
        set parameter_index "coderate"
    } elseif {$argument_prefix == "-CODING_DEPTH"} {
        set parameter_index "codingdepth"
    } elseif {$argument_prefix == "-TIMEOUT"} {
        set parameter_index "timeout"
    } elseif {$argument_prefix == "-ACK_PERIOD"} {
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
        puts "# -APP_TYPE <appt_value>: set the type of application to be used for generating data, <appt_value> is either FTP or CBR"
        puts "# -CBR_RATE <cr_value> : set the rate of the cbr applications, <cr_value> in bps, example: set to 3Mbps -> 3M or 3000000"
        puts "# -PKT_SIZE <ps_value> : set the size of udp pkt (including UDP and IP headers), <ps_value> in bytes"
        puts "# -ERR <e_value> : set the error rate in the forward channel (error rate for frames), 0 <= <e_value> < 1"
        puts "# -BURST_DURATION <bd_value> : 0,..,1 -> the percentage of time that the channel is in an error burst state, usage enables a two-state error channel"
        puts "# -ACK_BURST_DURATION <bd_value> : 0,..,1 -> the percentage of time that the backward channel is in an error burst state, usage enables a two-state error channel"
        puts "# -ACK_ERR <ae_value> : set the error rate in the return channel (error rate for ACKs), 0 <= <ae_value> < 1"
        puts "# -RTX <r_value> : set the number of retransmissions allowed for a native pkt, <r_value> is an integer >= 0"
        puts "# -CODERATE <cr_value> : set the number of native pkts sent before creating a coded pkt (actually define the code rate), <cr_value> is an integer >= 0, 0 deactivates coding"
        puts "# -CODING_DEPTH <cd_value> : set the number of coding cycles used to create a coded pkt, <cd_value> is an integer \in \[0, floor(<w_value/<c_value>>)\], 0 set the coding window equal to the arq window"
        puts "# -TIMEOUT <t_value> : set the time for expiring an non acked pkt, <t_value> in secs, example: set to 30ms->30ms or 0.03, 0 sets timeout=RTT, a value v<0 will set the timeout=-(RTT)/v"
        puts "# -ACK_PERIOD <ackp_value> : set the period for sending periodic acks, <ackp_value> in secs, example: set to 30ms->30ms or 0.03, 0 sets ack_period=RTT, a value v<0 will set the ack_period=-(window duration)/v"
        puts "# -SIMTIME <s_time> : set the simulation time, <s_time> in secs"
        puts "# -SEED <se_value> : set the seed to control randomness"
        puts "# -RESULTSFILE <res_value> : set the filename to store results, <res_value> is a string, setting <res_value> to the values 1-4 will produce predefined file names"

        puts "# Note that:"
        puts "# - options CODERATE -CODING_DEPTH and ACK_PERIOD cannot be used with SRARQ"
        puts "# - options CODING_DEPTH and RTX cannot be used with Tetrys"
        puts "# - options CODING_DEPTH and ACK_PERIOD cannot be used with Catepillar"
        puts "# - options ACK_PERIOD cannot be used with CARQ"
    
        exit 1
    }

    set indexofvalue [expr {$i + 1}]
    set opt($parameter_index) [lindex $argv $indexofvalue]
    set enabled($parameter_index) 1

    incr i
}

    #Checks for argument correctness
    if { $opt(protocol) != "CARQ" &&  $opt(protocol) != "Caterpillar" && $opt(protocol) != "Tetrys" && $opt(protocol) != "SRARQ" } { 
        puts -nonewline "\033\[1;31m"; puts "Not a valid protocol. Exiting..."; puts -nonewline "\033\[0m";
        exit 1
    }
    if { $opt(protocol) == "SRARQ" } {
        if { $enabled(coderate) == 1 || $enabled(codingdepth) == 1 || $enabled(ackperiod) == 1 } {
            puts -nonewline "\033\[1;31m"; puts "SRARQ is not compatible with the CODERATE, CODING_DEPTH and ACK_PERIOD options. Exiting...";  puts -nonewline "\033\[0m";
            exit 1
        }
    } elseif { $opt(protocol) == "Tetrys" } {
        if { $enabled(codingdepth) == 1 || $enabled(rtx) == 1 } {
            puts -nonewline "\033\[1;31m"; puts "Tetrys is not compatible with the CODING_DEPTH and RTX options. Exiting...";  puts -nonewline "\033\[0m";
            exit 1
        }
    } elseif { $opt(protocol) == "Caterpillar" } {
        if { $enabled(codingdepth) == 1 || $enabled(ackperiod) == 1 } {
            puts -nonewline "\033\[1;31m"; puts "Caterpillar is not compatible with the CODINGDEPTH and ACK_PERIOD options. Exiting..."; puts -nonewline "\033\[0m";
            exit 1
        }
    } elseif { $opt(protocol) == "CARQ" } {
        if { $enabled(ackperiod) == 1 } {
            puts -nonewline "\033\[1;31m"; puts "CARQ is not compatible with the ACK_PERIOD option. Exiting...";  puts -nonewline "\033\[0m";
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
    if { $opt(apptype) != "FTP" &&  $opt(apptype) != "CBR" } {
            puts "Not a valid application type. Exiting..."
            exit 1
    }
    if { $opt(apptype) == "FTP" && $enabled(cbrrate) == 1 } {
        puts "CBR_RATE and FTP are not compatible parameters. Exiting..."
        exit 1
    }

set protocolTx "$opt(protocol)Tx"
set protocolAcker "$opt(protocol)Acker"
set protocolNAcker "$opt(protocol)Nacker"
set protocolACKRx "$opt(protocol)ACKRx"

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
$protocolACKRx set debug_ NULL

SimpleLink instproc link-arq { } {
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


SimpleLink instproc link-barq { acker lnk_fwd} {
    $self instvar link_ link_errmodule_ queue_ drophead_ head_
    $self instvar ackreceiver_
    global opt protocolACKRx protocolTx

    set ackreceiver_ [new $protocolACKRx]
    set lnk_fwd_queue_ [$lnk_fwd queue]
    $ackreceiver_ attach-$protocolTx [$lnk_fwd_queue_ target]
    $ackreceiver_ target [$link_errmodule_ target]
    $link_errmodule_ target $ackreceiver_
    $acker attach-oppositeQueue $queue_
    
}

Simulator instproc link-arq { from to } {
    set link [$self link $from $to]
    set acker [$link link-arq]
	return $acker
}


Simulator instproc link-barq { from to acker} {
    set link_back [$self link $from $to]
    set link_fwd [$self link $to $from]
    $link_back link-barq $acker $link_fwd
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

if { $enabled(burstduration) == 0 &&  $enabled(ackburstduration) == 0 } {
    #Create a simple uniform Errormodel
    set em [new ErrorModel]
    set em_back [new ErrorModel]
    $em set rate_ $opt(err)
    $em_back set rate_ $opt(ackerr)
    $em set enable_ 1
    $em_back set enable_ 1
    $em unit pkt
    $em_back unit pkt
    $em set bandwidth_ $opt(bw)
    $em_back set bandwidth_ $opt(bw)
    set vagrng [new RNG]
    set vagrng_back [new RNG]
    $vagrng seed $opt(seed)
    $vagrng_back seed [expr {$opt(seed) + 1}]
    set vagranvar [new RandomVariable/Uniform]
    set vagranvar_back [new RandomVariable/Uniform]
    $vagranvar use-rng $vagrng
    $vagranvar_back use-rng $vagrng_back
    $em ranvar $vagranvar
    $em_back ranvar $vagranvar_back
    $em drop-target [new Agent/Null]
    $em_back drop-target [new Agent/Null]
} else {
    #Create uniform Errormodel representing first state
    set tmp [new ErrorModel]
    set tmp_back [new ErrorModel]
    $tmp set rate_ 0
    $tmp_back set rate_ 0
    $tmp set enable_ 1
    $tmp_back set enable_ 1
    $tmp set bandwidth_ $opt(bw)
    $tmp_back set bandwidth_ $opt(bw)
    set vagrng00 [new RNG]
    set vagrng00_back [new RNG]
    $vagrng00 seed [expr {$opt(seed) + 10}]
    $vagrng00_back seed [expr {$opt(seed) + 110}]
    set vagranvar00 [new RandomVariable/Uniform]
    set vagranvar00_back [new RandomVariable/Uniform]
    $vagranvar00 use-rng $vagrng00
    $vagranvar00_back use-rng $vagrng00_back
    $tmp ranvar $vagranvar00
    $tmp_back ranvar $vagranvar00_back
    #Create uniform Errormodel representing second state
    set tmp1 [new ErrorModel]
    set tmp1_back [new ErrorModel]
    $tmp1 set rate_ $opt(err)
    $tmp1_back set rate_ $opt(ackerr)
    $tmp1 set enable_ 1
    $tmp1_back set enable_ 1
    $tmp1 set bandwidth_ $opt(bw)
    $tmp1_back set bandwidth_ $opt(bw)
    set vagrng01 [new RNG]
    set vagrng01_back [new RNG]
    $vagrng01 seed $opt(seed)
    $vagrng01_back seed [expr {$opt(seed) + 100}]
    set vagranvar01 [new RandomVariable/Uniform]
    set vagranvar01_back [new RandomVariable/Uniform]
    $vagranvar01 use-rng $vagrng01
    $vagranvar01_back use-rng $vagrng01_back
    $tmp1 ranvar $vagranvar01
    $tmp1_back ranvar $vagranvar01

    if {$opt(burstduration) > 1 || $opt(burstduration) < 0} {
        puts "Burst duration percentage should be in \[0, 1\]"
        exit 1;
    }
    if {$opt(ackburstduration) > 1 || $opt(ackburstduration) < 0} {
        puts "Burst duration percentage in the backward channel should be in \[0, 1\]"
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
    set state1nduration_back [expr {(1- $opt(ackburstduration))*$wndduration}]
    set state2nduration_back [expr {$opt(ackburstduration)*$wndduration}]    


    # Array of states (error models)
    set m_states [list $tmp $tmp1]
    set m_states_back [list $tmp_back $tmp1_back]
    # Durations for each of the states, tmp, tmp1 and tmp2, respectively
    set m_periods [list $state1nduration $state2nduration_back]
    set m_periods_back [list $state1nduration $state2nduration_back]
    # Transition state model matrix
    set m_transmx { {0 1} {1 0}}
    set m_trunit pkt
    # Use time-based transition
    set m_sttype time
    set m_nstates 2
    set m_nstart [lindex $m_states 0]
    set m_nstart_back [lindex $m_states_back 0]
    set em [new ErrorModel/MultiState $m_states $m_periods $m_transmx $m_trunit $m_sttype $m_nstates $m_nstart]
    set em_back [new ErrorModel/MultiState $m_states_back $m_periods_back $m_transmx $m_trunit $m_sttype $m_nstates $m_nstart_back]

}

$ns link-lossmodel $em $n1 $n3
$ns link-lossmodel $em_back $n3 $n1
set receiver [$ns link-arq $n1 $n3]
$ns link-barq $n3 $n1 $receiver

if { $opt(apptype) == "CBR" } {
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

if { $enabled(burstduration) == 1 || $enabled(ackburstduration) == 1 } {
    $ns at 0.0 "$receiver print-parameters [expr {double(round(100*$opt(err))) + double(round(100*$opt(burstduration)))/100}] [expr {double(round(100*$opt(ackerr))) + double(round(100*$opt(ackburstduration)))/100}] $opt(simtime) $opt(seed)"
} else {
    $ns at 0.0 "$receiver print-parameters $opt(err) $opt(ackerr) $opt(simtime) $opt(seed)"
}

if { $opt(apptype) == "CBR" } {
    $ns at 0.0 "$cbr start"
    $ns at $opt(simtime) "$cbr stop"
} else {
    $ns at 0.0 "$ftp start"
    $ns at $opt(simtime) "$ftp stop"
}

$ns at [expr {$opt(simtime) + 0.5}] "print_stats"
$ns at [expr {$opt(simtime) + 1.0}] "exit 0"
$ns run
