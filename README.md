# codedARQ

This is an (under development) implementation of RLNC-enabled ARQ protocols. The implementation works in the context of [_ns2_](https://www.isi.edu/nsnam/ns/). It has been tested for v2.35 of ns2.

The folder contains:
* A Makefile that can be used for incorporating the code into ns2 and compiling it.
* Files arq.h and arq.cc constitute the implementation's core.
* A bunch of \*.tcl files used for running example simulations. The available arguments can be found within the tcl file.

Currently the following examples for running a simulation can be used:

```
./ns arq/arq-Tx_v4_1.tcl <bandwidth> <propagation_delay> <protocol> <window_size> <pkt_size> <err_rate> <ack_rate> <num_rtx> <rate_k> <coding_depth> <simulation_time> <seed>
```

where:

* \<bandwidth\> : in bps, example: set to 5Mbps -> 5M or 5000000
* \<propagation_delay\> : in secs, example: set to 30ms -> 30ms or 0.03
* \<protocol\> : the protocol to be used (either Tetrys, Caterpillar and our protocol in any other case)
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
./ns arq/arq-Tx_v4_2.tcl <bandwidth> <propagation_delay> <protocol> <window_size> <cbr_rate> <pkt_size> <err_rate> <ack_rate> <num_rtx> <rate_k> <coding_depth> <simulation_time> <seed>
```

where:

* \<cbr_rate\> : the rate of the cbr applications, in bps, example: set to 3Mbps -> 3M or 3000000
* \<pkt_size\> : the size of udp pkt (including UDP and IP headers)

and all the other parameters have the same usage as in the arq-Tx_v4_1.tcl case.
