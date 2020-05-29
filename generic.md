# Running a simulation with the generic script

You can use the generic.tcl script to run a simulation for any of the following ARQ protocols:

* Selective Repeat (SRARQ)
* Tetrys (Tetrys)
* Caterpillar-FB (Caterpillar)
* C-ARQ (CARQ)

The following command can be used:
```
./ns generic.tcl -OPTION1 value1, -OPTION2 value2, -OPTION3 value3, .... 
```

where OPTION1, OPTION2, OPTION3, .... can be one of the following options regardless of reported order:

> * -PROTOCOL _\<pr_value\>_: sets the protocol to be used in the simulation, _\<pr_value\>_ can be CARQ, Caterpillar, Tetrys or SRARQ
> * -BW _\<b_value\>_: sets the bandwidth of the link, _\<b_value\>_ is in bps, example value: 5M or 5000000 sets bandwidth to 5Mbps 
> * -DELAY _\<d_value\>_: sets the propagation delay, _\<d_value\>_ in in secs, example value: 30ms sets delay to 30ms or 0.03 secs
> * -WND _\<w_value\>_ : sets the arq window size, _\<w_value\>_ is in pkts
> * -APP_TYPE _\<appt_value\>_: sets the type of application to be used for generating data, _\<appt_value\>_ is either FTP or CBR
> * -CBR_RATE _\<cr_value\>_: sets the rate of the cbr applications, _\<cr_value\>_ is in bps, example value: 3M or 3000000 sets the rate to 3Mbps"
> * -PKT_SIZE _\<ps_value\>_: sets the size of pkts, _\<ps_value\>_ is in bytes, if CBR is used _\<ps_value\>_ includes the UDP and IP headers while if FTP is used _\<ps_value\>_ does not include the TCP and IP headers
> * -ERR _\<e_value\>_ : sets the error rate in the forward channel (error rate for frames), 0 <= _\<e_value\>_ < 1
> * -BURST_DURATION <bd_value> : 0,..,1 -> the percentage of time that the channel is in an error burst state, usage enables a two-state error channel
> * -ACK_ERR _\<ae_value\>_: sets the error rate in the return channel (error rate for ACKs), 0 <= _\<ae_value\>_ < 1
> * -RTX _\<r_value\>_: sets the number of retransmissions allowed for a native packet, _\<r_value\>_ is an integer >= 0
> * -CODERATE _\<cr_value\>_: sets the number of native packtes sent before creating a coded packet (actually defines the code rate), _\<cr_value\>_ is an integer >= 0, 0 deactivates coding, i.e., no coded packets are created
> * -CODING_DEPTH _\<cd_value\>_: sets the number of coding cycles used to create a coded packet, _\<cd_value\>_ is an integer \in \[0, floor(_\<w_value\>_/_\<c_value\>_)\], 0 sets the coding window equal to the arq window    
> * -TIMEOUT _\<t_value\>_: sets the time for expiring an non acked pkt, _\<t_value\>_ is in secs, example: 30ms or 0.03 sets the timeout to 30 ms, 0 sets timeout = RTT, a value v<0 will set the timeout= RTT/|v|
> * -ACK_PERIOD _\<ackp_value\>_: sets the period for sending periodic acks, _\<ackp_value\>_ is in secs, example value: 30ms or 0.03 sets the period to 30 ms, 0 sets ack_period = RTT, a value v<0 will set the ack_period=(window duration)/|v|
> * -SIMTIME _\<s_time\>_: sets the simulation time, _\<s_time\>_ is in secs
> * -SEED _\<se_value\>_: sets the seed for controling randomness
> * -RESULTSFILE _\<res_value\>_: sets the filename for storing results, _\<res_value\>_ is a string, setting _\<res_value\>_ to the values 1-4 will produce predefined file names

Note that:

* options CODERATE -CODING_DEPTH and ACK_PERIOD cannot be used with SRARQ
* options CODING_DEPTH and RTX cannot be used with Tetrys
* options CODING_DEPTH and ACK_PERIOD cannot be used with Catepillar
* options ACK_PERIOD cannot be used with CARQ"

Finally, note that for any parameter that is not defined by the user that script will use a default value. For helpon how to use the script you can type:
```
./ns generic.tcl -HELP
```