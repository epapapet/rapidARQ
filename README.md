# codedARQ

This is an (under development) implementation of RLNC-enabled ARQ protocols. The implementation works in the context of [_ns2_](https://www.isi.edu/nsnam/ns/). It has been tested for v2.35 of ns2.

The folder contains:
* A Makefile that can be used for incorporating the code into ns2 and compiling it.
* Files arq.h and arq.cc constitute the implementation's core.
* A bunch of \*.tcl files used for running example simulations. The available arguments can be found within the tcl file.

## Running a simulation

### _C-ARQ and Caterpillar-FB_

Currently the following examples for running a simulation can be used:

```
./ns arq/<scriptfile> <bandwidth> <propagation_delay> <window_size> <pkt_size> <err_rate> <ack_rate> <num_rtx> <rate_k> <coding_depth> <simulation_time> <seed>
```

where:

* \<scriptfile\> : carq_ftp.tcl->C-ARQ algorithm, caterpillar_ftp.tcl->Caterpillar-FB
* \<bandwidth\> : in bps, example: set to 5Mbps -> 5M or 5000000
* \<propagation_delay\> : in secs, example: set to 30ms -> 30ms or 0.03
* \<window_size\> : aqr window size in pkts
* \<pkt_size\> : the size of a TCP segment (not including the TCP and IP headers)
* \<err_rate\> : the error rate in the forward channel (error rate for frames)
* \<ack_rate\> : the error rate in the return channel (error rate for ACKs)
* \<num_rtx\> : the number of retransmissions allowed for a native pkt
* \<rate_k\> : the number of native pkts sent before creating a coded pkt (actually define the code rate)
* \<coding_depth\> : the number of coding cycles used to create a coded pkt
* \<simulation_time\> : the simulation time in secs
* \<seed\> : seed used to produce randomness

or:

```
./ns arq/<scriptfile> <bandwidth> <propagation_delay> <window_size> <cbr_rate> <pkt_size> <err_rate> <ack_rate> <num_rtx> <rate_k> <coding_depth> <simulation_time> <seed>
```

where:

* \<scriptfile\> : carq_cbr.tcl->C-ARQ algorithm, caterpillar_cbr.tcl->Caterpillar-FB
* \<cbr_rate\> : the rate of the cbr applications, in bps, example: set to 3Mbps -> 3M or 3000000
* \<pkt_size\> : the size of udp pkt (including UDP and IP headers)

and all the other parameters have the same usage as in the \*_ftp.tcl files.

### _Tetrys_

To execute a simulation with Tetrys, you can use the following syntax:

```
./ns arq/tetrys_cbr.tcl <bandwidth> <propagation_delay> <window_size> <cbr_rate> <pkt_size> <err_rate> <ack_rate> <rate_k> <ack_period> <timeout> <simulation_time> <seed>
```

where:

* \<bandwidth\> : in bps, example: set to 5Mbps -> 5M or 5000000
* \<propagation_delay\> : in secs, example: set to 30ms -> 30ms or 0.03
* \<window_size\> : aqr window size in pkts
* \<cbr_rate\> : the rate of the cbr applications, in bps, example: set to 3Mbps -> 3M or 3000000
* \<pkt_size\> : the size of udp pkt (including UDP and IP headers) when UDP is used or the size of a TCP segment (not including the TCP and IP headers) when TCP is used
* \<err_rate\> : the error rate in the forward channel (error rate for frames)
* \<ack_rate\> : the error rate in the return channel (error rate for ACKs)
* \<rate_k\> : the number of native pkts sent before creating a coded pkt (actually define the code rate)
* \<ack_period\> : the period for sending acks, example: set to 30ms->30ms or 0.03, 0 sets ack_period=window duration, a value v<0 will set the ack_period=-(window duration)/v
* \<timeout\> : the time for expiring an non acked pkt, example: set to 30ms->30ms or 0.03, 0 sets timeout=RTT, a value v<0 will set the timeout=-(RTT)/v
* \<simulation_time\> : the simulation time in secs
* \<seed\> : seed used to produce randomness

or you can replace *tetrys_cbr.tcl* with *tetrys_ftp.tcl* with the only differences being that the \<cbr_rate\> parameter is not available 

