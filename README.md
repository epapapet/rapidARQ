# codedARQ

This is an (under development) implementation of RLNC-enabled ARQ protocols. The implementation works in the context of [_ns2_](https://www.isi.edu/nsnam/ns/). It has been tested for v2.35 of ns2.

The folder contains:
* A Makefile that can be used for incorporating the code into ns2 and compiling it.
* Files arq.h and arq.cc constitute the implementation's core.
* A bunch of \*.tcl files used for running example simulations. The available arguments can be found within the tcl file.

Currently the following examples for running a simulation can be used:
* ./ns arq/arq-Tx_v4_1.tcl 5M 30ms 20 1000 0.2 0.2 3 5 5 6
* ./ns arq/arq-Tx_v4_2.tcl 5M 30ms 20 1000 0.2 0.2 3 5 5 6
