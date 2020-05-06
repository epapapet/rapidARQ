#include "arq.h"
#include "packet.h"

static class ARQTxClass: public TclClass {
 public:
	ARQTxClass() : TclClass("ARQTx") {}
	TclObject* create(int, const char*const*) {
		return (new ARQTx);
	}
} class_arq_tx;

static class ARQAckerClass: public TclClass {
 public:
	ARQAckerClass() : TclClass("ARQAcker") {}
	TclObject* create(int, const char*const*) {
		return (new ARQAcker);
	}
} class_arq_acker;

static class ARQNackerClass: public TclClass {
 public:
	ARQNackerClass() : TclClass("ARQNacker") {}
	TclObject* create(int, const char*const*) {
		return (new ARQNacker);
	}
} class_arq_nacker;

void ARQHandler::handle(Event* e)
{
	arq_tx_.resume();
}


//-------------------------------------------ARQRx--------------------------------------------//
//--------------------------------------------------------------------------------------------//
ARQTx::ARQTx() : arqh_(*this)
{
	wnd_ = 0; //the size of the sender window
	sn_cnt = 0; //number of available sequence numbers
	last_acked_sq_ = -1; //the last acked sequence number
	most_recent_sq_ = 0; //the most recent sequence number to be used when sending a frame

	blocked_ = 0; //used to check whethet the channel is occupied with a transmission
	pending = NULL; //used to store a frame that arrives from the outgoing queue and finds the channel blocked
	handler_ = 0; //pointer to the outgoing queue (upstream object)

	retry_limit_ = 0; //number of retransmisions allowed per frame
	bind("retry_limit_", &retry_limit_);
	num_pending_retrans_ = 0; //number of retransmissions scheduled and pending in ARQTx

	rate_k = 0;
	bind("rate_k", &rate_k); //the number of native frames before the creation of a coded one
	coding_depth = 0;
	bind("coding_depth", &coding_depth); //the number of coding cycles used to create a coded frame
  coding_wnd = 0;
	lnk_bw_ = 10000000;
	bind("lnk_bw_", &lnk_bw_);
	lnk_delay_ = 0.03;
	bind("lnk_delay_", &lnk_delay_);
	app_pkt_Size_ = 8000;
	bind("app_pkt_Size_", &app_pkt_Size_);
	native_counter = 0; //counter used for invoking the creation of a coded frame

	debug = false; //used to enable printing of diagnostic messages

	start_time = -1; //time when 1st packet arrived at ARQTx::recv
	packets_sent = 0; //unique packets sent
  coded_pkts_sent = 0; ////total nu,ber of csent coded pkts
  pkt_rtxs = 0; //the total number of pkt retransmissions
} //end of constructor

int ARQTx::command(int argc, const char*const* argv)
{
	Tcl& tcl = Tcl::instance();
	if (argc == 3) {
		if (strcmp(argv[1], "setup-wnd") == 0) {
			if (*argv[2] == '0') {
				tcl.resultf("Cannot setup NULL wnd\n");
				return(TCL_ERROR);
			}
      if (rate_k == 0){
        rate_k = 2147483647; //i.e., deactivate coding
      }
			wnd_ = atoi(argv[2]);
			sn_cnt = 4 * wnd_; //although 2*wnd_ is enough, we use 4*wnd_ or more to tackle the case that ARQTx drops packets and advances its window without ARQRx knowing
			pkt_buf = new Packet* [wnd_]; //buffer for storing pending frames
			status = new ARQStatus[wnd_]; //the status for each frame: IDLE,SENT,ACKED,RTX,DROP
			num_rtxs = new int[wnd_]; //the number of retransmissions executed for each frame
			pkt_uids = new int[wnd_]; //buffer for storing the uids of pending frames: used only for diagnostic purposes
			for(int i=0; i<wnd_; i++){ pkt_buf[i] = NULL; status[i] = IDLE; num_rtxs[i] = 0; pkt_uids[i]=-1; }
      coding_wnd = (coding_depth == 0) ? (wnd_) : (coding_depth * rate_k);
      if (coding_wnd > wnd_) {
        tcl.resultf("The product coding_depth*rate_k should not exceed wnd\n");
				return(TCL_ERROR);
      }
			return(TCL_OK);
		}
	} return Connector::command(argc, argv);
} //end of command


void ARQTx::recv(Packet* p, Handler* h)
{

	//This procedure is invoked by the queue_ (i.e., output queue) to deliver a message to ARQTx
	//The check whether the current window exceeds or not wnd_ has already be done at this point
  if (handler_ == 0) printf("Simulation time: %10.4f", Scheduler::instance().clock());
  if (handler_ != 0) printf("\b\b\b\b\b\b\b\b\b\b%10.4f", Scheduler::instance().clock());

	if(last_acked_sq_ == -1 && most_recent_sq_ == 0 && start_time == -1){ //first packet to be received will trigger the clock
		start_time = Scheduler::instance().clock();
	}

	//Sanity checks---------//
	if (&arqh_==0) {
		fprintf(stderr, "Error at ARQTx::recv, Cannot transmit when &arqh_ is Null.\n");
		abort();
	}
	if ((status[most_recent_sq_%wnd_] != IDLE) && ((HDR_CMN(p))-> opt_num_forwards_ >= 0)) {
		fprintf(stderr, "Error at ARQTx::recv, storing position should be in IDLE mode.\n");
		abort();
	}
	//---------------------//
	//Only in initialization-------//
	if (h != 0) handler_ = h;
	//----------------------------//

	if ((blocked_) && ((HDR_CMN(p))-> opt_num_forwards_ >= 0)) {
    if (pending) { fprintf(stderr, "Error at ARQTx::recv, Tx should not have a pending frame when recv is called.\n"); abort(); }
    pending = p; return;
  }
  if ((blocked_) && ((HDR_CMN(p))-> opt_num_forwards_ < 0)) {
    if (coded) { fprintf(stderr, "Error at ARQTx::recv, Tx should not have a pending frame when recv is called.\n"); abort(); }
    coded = p; return;
  }

	hdr_cmn *ch = HDR_CMN(p);
	if (ch-> opt_num_forwards_ >= 0){

		ch-> opt_num_forwards_ = most_recent_sq_;
		if (pkt_buf[most_recent_sq_%wnd_]) {fprintf(stderr, "Error at ARQTx::recv, storing position found non empty.\n"); abort();} //pkt_buf is cleared by reset_lastacked
		pkt_buf[most_recent_sq_%wnd_] = p;
		pkt_uids[most_recent_sq_%wnd_] = ch->uid();
		status[most_recent_sq_%wnd_] = SENT;
		num_rtxs[most_recent_sq_%wnd_] = 0;

    ch->ts_arr_ = Scheduler::instance().clock(); //used to calculate delay, retransmitted pkts are not sent through recv(), so this is a new pkt

		most_recent_sq_ = (most_recent_sq_+1)%sn_cnt;

		native_counter++;
		if (native_counter == rate_k){ //prepare a coded frame
			coded = create_coded_packet();
			native_counter = 0;
		}

	}

	if (debug) printf("ARQTx, recv: Sending pkt %d. SimTime=%.8f.\n", ch-> opt_num_forwards_, Scheduler::instance().clock());

	blocked_ = 1;
	if (ch-> opt_num_forwards_ >= 0){
		//In general, when sending a native frame we create a copy (even in retransmissions) to decouple ARQTx and ARQRx
		//ARQTx is responsible for freeing frames sent from queue while ARQRx delivers frames to upper layers and frees frames not eventually delivered
    packets_sent += 1;
		Packet *pnew = p->copy();
		send(pnew,&arqh_);
	} else {
    coded_pkts_sent++;
		send(p,&arqh_);
	}

} //end of recv

Packet* ARQTx::create_coded_packet(){ //create a new coded packet with seq nums of <=k previous packets sent

	Packet *cpkt = Packet::alloc();
	hdr_cmn *ch2 = HDR_CMN(cpkt);
	ch2->opt_num_forwards_ = -10000 + (most_recent_sq_ - 1 + sn_cnt)%sn_cnt; //indicates a coded packet, SHOULD BE ALWAYS < 0

	unsigned char *buffer = new unsigned char[sizeof(int)*(wnd_+1)]; //maximum of wnd_ sequence numbers plus a counter
  int far_end_diff = ((most_recent_sq_ - coding_wnd + sn_cnt)%sn_cnt - (last_acked_sq_ + 1)%sn_cnt + sn_cnt)%sn_cnt;
  int runner_ = ((far_end_diff > 0) && (far_end_diff <= (wnd_ - coding_wnd))) ? (((most_recent_sq_ - coding_wnd + sn_cnt)%sn_cnt)%wnd_) : (((last_acked_sq_ + 1)%sn_cnt)%wnd_);
  //printf("MR: %d, LA: %d, FED: %d, runner: %d.\n", most_recent_sq_, last_acked_sq_, far_end_diff, fakerunner_);fflush(stdout);
	int cnt_pkts = 0;
	do {
		if(status[runner_] != DROP){ //We should consider which packets to include in an encoded packet. For example, should we incude RTX or ACKED pkts?
			hdr_cmn *ch = HDR_CMN(pkt_buf[runner_]);
			*(buffer+sizeof(int)*(cnt_pkts+1)) =  (ch->opt_num_forwards_ >> 24) & 0xFF;
			*(buffer+sizeof(int)*(cnt_pkts+1)+1) = (ch->opt_num_forwards_ >> 16) & 0xFF;
			*(buffer+sizeof(int)*(cnt_pkts+1)+2) = (ch->opt_num_forwards_ >> 8) & 0xFF;
			*(buffer+sizeof(int)*(cnt_pkts+1)+3) = ch->opt_num_forwards_ & 0xFF;
			cnt_pkts++;
		}
		runner_ = (runner_ + 1)%wnd_;
	} while (runner_ != (most_recent_sq_%wnd_));

	*(buffer) =  (cnt_pkts >> 24) & 0xFF;
	*(buffer+1) = (cnt_pkts >> 16) & 0xFF;
	*(buffer+2) = (cnt_pkts >> 8) & 0xFF;
	*(buffer+3) = cnt_pkts & 0xFF;

	ch2->size_ = sizeof(int)*(cnt_pkts+1) + app_pkt_Size_; //size of native frame plus the size of the encoding vector (bytes only for coefficients)
	PacketData *cp_data;
	cp_data = new PacketData(sizeof(int)*(cnt_pkts+1));
	unsigned char *pointer_to_data;
	pointer_to_data = cp_data->data();
	memcpy(pointer_to_data, buffer, sizeof(int)*(cnt_pkts+1));
	delete buffer;
	cpkt->setdata(cp_data);

	return cpkt;

} //end of create_coded_packet

void ARQTx::ack(int rcv_sn, int rcv_uid)
{

	int fw_dis = (rcv_sn - last_acked_sq_ + sn_cnt)%sn_cnt;
	int fw_width = (most_recent_sq_ - last_acked_sq_ + sn_cnt)%sn_cnt;
	bool within_fww = ((fw_dis < fw_width) && (fw_dis > 0)) ? (true) : (false);
	if (!within_fww) return; //ARTx may receive multiple ACKs per frame due to coded frames, so ignore ACKs out of the active window

	//Sanity checks--------//
	if (status[rcv_sn%wnd_] == IDLE) { fprintf(stderr,"Error at ARQTx::ack, an ACK is received when the status is not SENT. It is %d.\n", status[rcv_sn%wnd_]); abort(); }
	if (handler_ == 0) { fprintf(stderr,"Error at ARQTx::ack, handler_ is null.\n"); abort(); }
	//--------------------//

	if (status[rcv_sn%wnd_] != ACKED){
		if (debug) printf("ARQTx ack: Pkt %d with status %d is ACKED. The news status is %d. Pending retrans=%d. LA(MR) before is %d(%d). SimTime=%.8f.\n ", rcv_sn, status[rcv_sn%wnd_], ACKED, num_pending_retrans_, last_acked_sq_, most_recent_sq_, Scheduler::instance().clock());

		if(status[rcv_sn%wnd_] == RTX) num_pending_retrans_--; //reduce the number of scheduled retransmissions
		status[rcv_sn%wnd_] = ACKED;
		if (rcv_sn%wnd_ == ((last_acked_sq_ + 1)%sn_cnt)%wnd_){
      reset_lastacked();  //acked frame is next in order, so check whether the active window should advance
      if (!blocked_) handler_->handle(0); //ask queue_ to deliver next packet
    }
	}

} //end of ack

void ARQTx::ack(Packet *p){ //called when a coded ack arrives, reads the contents and checks to advance the window

	unsigned char *contents = p->accessdata();
	int cnt_pkts = -1;
	cnt_pkts = *(contents);
	cnt_pkts = cnt_pkts << 8 | *(contents+1);
	cnt_pkts = cnt_pkts << 8 | *(contents+2);
	cnt_pkts = cnt_pkts << 8 | *(contents+3);


	if (debug) {
		int readint = -1;
		printf("ARQTx, ack(cum): Received ACK contains =");
		for(int j = 1; j <= cnt_pkts; j++){
			readint = *(contents+sizeof(int)*j);
			readint = readint << 8 | *(contents+sizeof(int)*j+1);
			readint = readint << 8 | *(contents+sizeof(int)*j+2);
			readint = readint << 8 | *(contents+sizeof(int)*j+3);
			printf(" %d", readint);
		}
		printf(".\n");
	}

	int seq_number = -1;
	for(int j = 1; j <= cnt_pkts; j++){
		seq_number = *(contents+sizeof(int)*j);
		seq_number = seq_number << 8 | *(contents+sizeof(int)*j+1);
		seq_number = seq_number << 8 | *(contents+sizeof(int)*j+2);
		seq_number = seq_number << 8 | *(contents+sizeof(int)*j+3);

		ack(seq_number, -1);
	}
	Packet::free(p);

} //end of ack(cumulative)

void ARQTx::nack(int rcv_sn, int rcv_uid)
{

	int fw_dis = (rcv_sn - last_acked_sq_ + sn_cnt)%sn_cnt;
	int fw_width = (most_recent_sq_ - last_acked_sq_ + sn_cnt)%sn_cnt;
	bool within_fww = ((fw_dis < fw_width) && (fw_dis > 0)) ? (true) : (false);
	if (!within_fww) return; //ARTx may receive multiple NACKs per frame due to coded frames, so ignore ACKs out of the active window

	//Sanity checks--------//
	if (status[rcv_sn%wnd_] == IDLE) { fprintf(stderr,"Error at ARQTx::nack, a NACK is received when the status is not SENT.\n"); abort(); }
	if (handler_ == 0) { fprintf(stderr,"Error at ARQTx::nack, handler_ is null\n"); abort(); }
	if (&arqh_==0) { fprintf(stderr, "Error at ARQTx::nack, Cannot transmit when &arqh_ is Null.\n"); abort(); }
	//--------------------//

	if (status[rcv_sn%wnd_] != SENT) { //it is possible to receive an NACK for a frame acked through a cumulative ACK (i.e., coded frame) when the cumulative ACK arives after the retransmission
		if (debug) printf("ARQTx, nack: NACK for pkt %d with status %d. Ignoring. SimTime=%.8f.\n", rcv_sn, status[rcv_sn%wnd_], Scheduler::instance().clock());
		return;
	} else {
		if (pkt_buf[rcv_sn%wnd_] == NULL) { fprintf(stderr,"Error at ARQTx::nack, a NACK is received but pkt is not found.\n"); abort(); }
		if (debug) printf("ARQTx, nack: NACK for pkt %d with status %d. SimTime=%.8f.\n", rcv_sn, status[rcv_sn%wnd_], Scheduler::instance().clock());
	}

	if ( num_rtxs[rcv_sn%wnd_] < retry_limit_) { //packet shoud be retransmitted
		status[rcv_sn%wnd_] = RTX;
		if (!blocked_){ //if ARQTx is available go on with retransmision
			if (debug) printf("ARQTx, nack: Sending pkt %d. SimTime=%.8f.\n", rcv_sn, Scheduler::instance().clock());
			blocked_ = 1;
			num_rtxs[rcv_sn%wnd_]++;
			status[rcv_sn%wnd_] = SENT;
			Packet *newp = pkt_buf[rcv_sn%wnd_]->copy();
      pkt_rtxs++;
      native_counter++;
      if (native_counter == rate_k){ //prepare a coded frame
        coded = create_coded_packet();
        native_counter = 0;
      }
			send(newp,&arqh_);
		} else {
			num_pending_retrans_++;
		}
	} else {//packet should be dropped
		if (debug) printf("ARQTx, nack: Dropping pkt %d. SimTime=%.8f.\n", rcv_sn, Scheduler::instance().clock());
		status[rcv_sn%wnd_] = DROP;
		if (rcv_sn%wnd_ == ((last_acked_sq_ + 1)%sn_cnt)%wnd_) {
      reset_lastacked(); //droped frame is next in order so check whether the active window should advance
      if (!blocked_) handler_->handle(0); //ask queue_ to deliver next packet
    }
	}

}//end of nack

void ARQTx::resume()
{
	//This procedure is invoked by link_ when a transmission is completed, i.e., it is invoked T secs after ARQTx executes send() where T equals transmission_delay.
	blocked_ = 0;
  if (coded){
    Packet *fwpkt = coded;
		coded = NULL;
		recv(fwpkt, handler_);
  } else if (pending){
		Packet *fwpkt = pending;
		pending = NULL;
		recv(fwpkt, handler_);
	} else if (num_pending_retrans_ > 0) { //if there exist packets not ACKed that need to be retransmitted
		int runner_ = findpos_retrans();
		if (debug) printf("ARQTx, resume: Sending pkt %d. SimTime=%.8f.\n", (HDR_CMN(pkt_buf[runner_]))->opt_num_forwards_, Scheduler::instance().clock());
		num_rtxs[runner_]++;
		status[runner_] = SENT;
		num_pending_retrans_--;
		blocked_ = 1;
		Packet *pnew = (pkt_buf[runner_])->copy();
    pkt_rtxs++;
    native_counter++;
		if (native_counter == rate_k){ //prepare a coded frame
			coded = create_coded_packet();
			native_counter = 0;
		}
		send(pnew,&arqh_);
	} else {//there are no pending retransmision, check whether it is possible to send a new packet
		//TO DO: check whether active window reaches wnd_ and ARQTx is stuck without asking queue for the next frame
		int current_wnd_ = (most_recent_sq_ - last_acked_sq_ + sn_cnt)%sn_cnt;
		if (current_wnd_ <= wnd_) handler_->handle(0); //ask queue_ to deliver next packet
	}

}//end of resume

int ARQTx::findpos_retrans()
{
	//----------DEBUG------------------//
	if((last_acked_sq_+1)%sn_cnt == most_recent_sq_) { fprintf(stderr, "Error at ARQTx::findpos_retrans, no packet is waiting (stored) for transmission.\n"); abort(); }
	//---------------------------------//

	bool found = FALSE;
	int runner_ = ((last_acked_sq_+1)%sn_cnt)%wnd_;

	do {
		if (status[runner_] == RTX) {
			found = TRUE;
			break;
		}
		runner_ = (runner_+1)%wnd_;
	} while (runner_ != (most_recent_sq_%wnd_));

	//----------DEBUG------------------//
	if (!found){ fprintf(stderr, "Error at ARQTx::findpos_retrans, packet with RTX status NOT FOUND.\n"); abort(); }
	//---------------------------------//

	return runner_;
} //end of findpos_retrans

void ARQTx::reset_lastacked()
{

	if((last_acked_sq_+1)%sn_cnt == most_recent_sq_) return; //no need to reset last_ack because there is no packet stored (MOST RECENT - LAST ACKED = 1)

	int runner_ = ((last_acked_sq_+1)%sn_cnt)%wnd_;
	do {
		if ((status[runner_] == RTX) || (status[runner_] == SENT)) break;
		if (pkt_buf[runner_]) Packet::free(pkt_buf[runner_]); //free frames not needed any more
		pkt_buf[runner_] = NULL;
		status[runner_] = IDLE;
		num_rtxs[runner_] = 0;
		pkt_uids[runner_] = -1;

		last_acked_sq_ = (last_acked_sq_ + 1)%sn_cnt;
		runner_ = (runner_ + 1)%wnd_;
	} while (runner_ != (most_recent_sq_%wnd_));

	if(debug) printf("ARQTx, reset_lastacked: new LA(MR) are %d(%d). SimTime=%.8f.\n", last_acked_sq_, most_recent_sq_, Scheduler::instance().clock());

} // end of reset_lastacked

//--------------------------------------------------------------------------------------------//
//--------------------------------------------------------------------------------------------//





//-------------------------------------------ARQRx--------------------------------------------//
//--------------------------------------------------------------------------------------------//
ARQRx::ARQRx()
{
	arq_tx_=0;
	delay_ = 0;
	timeout_ = 0;
} //end of constructor


int ARQRx::command(int argc, const char*const* argv)
{
	Tcl& tcl = Tcl::instance();
	if (argc == 2) {
		if (strcmp(argv[1], "update-delays") == 0) {
      bool delayNack = false; //bool variable that controls the value assigned to timeout_. If false, keep timeout = delay.
			delay_ = arq_tx_->get_linkdelay(); //the propagation delay
      double nack_delay = (arq_tx_->get_codingdepth() * (arq_tx_->get_ratek() + 1) * arq_tx_->get_apppktsize())/arq_tx_->get_linkbw() + arq_tx_->get_linkdelay();
			timeout_ = (delayNack == false) ? (delay_ + 8.0/arq_tx_->get_linkbw()): nack_delay;
			return(TCL_OK);
		}
	} else if (argc == 3) {
		if (strcmp(argv[1], "attach-ARQTx") == 0) {
			if (*argv[2] == '0') {
				tcl.resultf("Cannot attach NULL ARQTx\n");
				return(TCL_ERROR);
			}
			arq_tx_ = (ARQTx*)TclObject::lookup(argv[2]);
			return(TCL_OK);
		}
	} return Connector::command(argc, argv);
} //end of command


ARQAcker::ARQAcker()
{
	wnd_ = 0; // the window size
	sn_cnt = 0; //the number of available sequence numbers
	last_fwd_sn_ = -1; //the sequence number of the last frame delivered to the upper layer
	most_recent_acked = 0; //the sequence number of the frame that was most recently ACKed

	ranvar_ = NULL; // random number generator used for simulating the loss rate for ACKs
	err_rate = 0.0; // the error rate for ACks

	nacker = NULL;

	debug = false; //used to enable printing of diagnostic messages

	finish_time = 0; //time when the last pkt was delivered to the receiver's upper layer, used to calculate throughput
	delivered_pkts = 0; //the total number of pkts delivered to the receiver's upper layer
	delivered_data = 0; //the total number of bytes delivered to the receiver's upper layer
	sum_of_delay = 0; //sum of delays for every packet delivered, used to calculate average delay
  sum_of_delay_jitter = 0; //sum of delay jitter for every packet delivered, used to calculate average delay
	avg_dec_matrix_size = 0; //the avg size of the decoding matrix when decoding is performed (used to estimate processing overhead)
  avg_known_pkts_size = 0; //the avg size of the known_packets when decoding is performed (part of decoding matrix already in diagonal form)
	num_of_decodings = 0; //number of decoding operations
  max_delay = 0; //the maximum delay experienced by a packet
  min_delay = 100000000000; //the minimum delay experienced by a packet
  last_delay_sample = 0; //the last delay, used to calculate delay jitter
} //end of constructor



int ARQAcker::command(int argc, const char*const* argv)
{
	Tcl& tcl = Tcl::instance();
	if (argc == 2) {
		if (strcmp(argv[1], "print-stats") == 0) {
			print_stats(); //used for collecting statistics, along with the corresponding tcl command
			return(TCL_OK);
		}
	} else if (argc == 3) {
		if (strcmp(argv[1], "setup-wnd") == 0) {
			if (*argv[2] == '0') {
				tcl.resultf("Cannot setup NULL wnd\n");
				return(TCL_ERROR);
			}
			wnd_ = atoi(argv[2]);
			sn_cnt = 4 * wnd_; //never less than 2*wnd_ but usually more than that (see note in ARQTx command)
			pkt_buf = new Packet* [wnd_]; //buffer for storing out-of-order received frames
			lost_pkt_buf = new Packet* [sn_cnt]; //buffer for storing lost frames, used to find a frame if the frame is decoded (this is used as a hack)
			event_buf = new ACKEvent* [sn_cnt]; //buffer for storing pointers to NACK events
			status = new PacketStatus[wnd_]; //the status of each pending frame: NONE,MISSING,RECEIVED,DECODED
			for(int i=0; i<wnd_; i++){ pkt_buf[i] = NULL; status[i] = NONE;}
			for(int i=0; i<sn_cnt; i++){ lost_pkt_buf[i] = NULL; event_buf[i] = NULL;}
			return(TCL_OK);
		}
		if (strcmp(argv[1], "setup-ARQNacker") == 0) {
			if (*argv[2] == '0') {
				tcl.resultf("Cannot attach NULL ARQNacker\n");
				return(TCL_ERROR);
			}
			nacker = (ARQNacker*)TclObject::lookup(argv[2]);
			return(TCL_OK);
		}
		if (strcmp(argv[1], "ranvar") == 0) {
			ranvar_ = (RandomVariable*) TclObject::lookup(argv[2]);
			return (TCL_OK);
		}
		if (strcmp(argv[1], "set-err") == 0) {
			if (atof(argv[2]) > 1) {
				tcl.resultf("Cannot set error more than 1.\n");
				return(TCL_ERROR);
			}
			if (atof(argv[2]) < 0) {
				tcl.resultf("Cannot set error less than 0.\n");
				return(TCL_ERROR);
			}
			err_rate = atof(argv[2]);
			return(TCL_OK);
		}
	} return ARQRx::command(argc, argv);
} //end of command

void ARQAcker::recv(Packet* p, Handler* h)
{

	if((HDR_CMN(p))->opt_num_forwards_ < 0){ //coded frames should be processed by parse_coded_packet
		parse_coded_packet(p, h);
		return;
	}

	hdr_cmn *ch = HDR_CMN(p);
	int seq_num = ch->opt_num_forwards_;
	int pkt_uid = ch->uid();

	int fw_dis = (seq_num - last_fwd_sn_ + sn_cnt)%sn_cnt; //distance of received seq_num from the last delivered frame
	int fw_width = (most_recent_acked - last_fwd_sn_ + sn_cnt)%sn_cnt; //the active receiver window
	bool within_fww = ((fw_dis <= wnd_) && (fw_dis > 0)) ? (true) : (false); //bool to indicate whether the new frame is within forward window or not
	int bw_dis = (most_recent_acked - seq_num + sn_cnt)%sn_cnt; //distance of the most recently acked seq_num from the received seq_num
	bool within_bww = (bw_dis < wnd_) ? (true) : (false); //bool to indicate whether the new frame is (or not) within the ARQTx's active window as seen by ARQRx (backward window)
	int oldest_sq_sender = (most_recent_acked - arq_tx_->get_coding_wnd() + 1 + sn_cnt)%sn_cnt; //the lower bound of ARQTx's active window as seen by ARQRx before receiving seq_num
	int new_oldest_sq_sender = (seq_num - arq_tx_->get_coding_wnd() + 1 + sn_cnt)%sn_cnt; //the lower bound of ARQTx's active window as seen by ARQRx after receiving seq_num

	int nxt_seq = (last_fwd_sn_ + 1)%sn_cnt; //the next expected seq_num

	bool should_check_for_decoding = true; //bool to indicate whether the new reception should trigger a decoding

	if (within_fww){//frame belongs to the forward window

		if(seq_num == nxt_seq){ //frame arrives in order, ack and send frame and finally check if other frames are now in order
			if (debug) printf("ARQRx, recv1 (LF=%d,MR=%d): received pkt %d. SimTime=%.8f.\n", last_fwd_sn_, most_recent_acked, seq_num, Scheduler::instance().clock());
			status[nxt_seq%wnd_] = NONE;
			if (pkt_buf[nxt_seq%wnd_]){ fprintf(stderr, "Error at ARQRx::recv, position of in order frame found non empty.\n"); abort(); }
			pkt_buf[nxt_seq%wnd_] = NULL;
			lost_pkt_buf[nxt_seq] = NULL;
			event_buf[nxt_seq] = NULL;

			finish_time = Scheduler::instance().clock();
			delivered_data += ch->size_;
			delivered_pkts++;
      if (last_delay_sample != 0.0) sum_of_delay_jitter = sum_of_delay_jitter + abs(Scheduler::instance().clock() - ch->ts_arr_ - last_delay_sample);
      last_delay_sample = Scheduler::instance().clock() - ch->ts_arr_;
      if (last_delay_sample > max_delay) max_delay = last_delay_sample;
      if (last_delay_sample < min_delay) min_delay = last_delay_sample;
			sum_of_delay = sum_of_delay + last_delay_sample;

			send(p,h);

			last_fwd_sn_ = (last_fwd_sn_+1)%sn_cnt;
			deliver_frames((wnd_-1), true, h); //check whether other frames are now in order and should be delivered
		} else { //a new frame arrives out of order (either before most_recent_acked or after but within forward window), should be ACKEd and stored in the appropriate position
			if ((status[seq_num%wnd_] != RECEIVED) && (status[seq_num%wnd_] != DECODED)) {
				if (debug) printf("ARQRx, recv2 (LF=%d,MR=%d): received pkt %d. SimTime=%.8f.\n", last_fwd_sn_, most_recent_acked, seq_num, Scheduler::instance().clock());
				status[seq_num%wnd_] = RECEIVED;
				if (pkt_buf[seq_num%wnd_]){ fprintf(stderr, "Error at ARQRx::recv, position of not received and not decoded frame within forward window found non empty.\n"); abort(); }
				pkt_buf[seq_num%wnd_] = p;
				lost_pkt_buf[seq_num] = NULL;
				event_buf[seq_num] = NULL;
			} else { //the frame has already been received, thus the ACK was lost and nothing needs to be done beyond ACKing the frame and deleting the newly received
				//Sanity-------------------------//
				if ((HDR_CMN(pkt_buf[seq_num%wnd_]))->uid() != (HDR_CMN(p))->uid()){ fprintf(stderr, "Error at ARQRx::handle, received retransmission has not the same uid.\n"); abort(); }
				Packet::free(p);
				//-------------------------------//
			}
		}
		if (fw_dis > fw_width) { //the new frames is beyond the most_recent_acked
			for (int i = (most_recent_acked + 1)%sn_cnt; i != seq_num; i = (i + 1)%sn_cnt){ status[i%wnd_] = MISSING; } //update positions in between with MISSING state
			most_recent_acked = seq_num;
			clean_decoding_matrix(oldest_sq_sender, new_oldest_sq_sender); //remove from the coding structures the frames that are now out of ARQTx's active window
		}

	} else if (within_bww && !within_fww) {//frame belongs to the backward window (i.e., ARQTx's active window), so it is a retransmitted frame due to loss of ACK
		should_check_for_decoding = false;
		//ignore the packet and acknowledge
	} else { //frame is not in the forward and not in the backward window
		//This happens when one or more frames are dropped by ARQTx (when exceeding the retrans limit) so it moves on to transmit new frames.
		//ARQRx is not informed to advance its window and receives brand new frames that may consider as old ones
		//Using 2*wnd_ sequence numbers (= forward window + backward window) is enough for identifying new frames from retransmissions provided that ARQTx advances its window only after an ACK by ARQRx
		//When ARQTx can advance its window and send new frames after dropping a old frame then we need more than 2*wnd_ sequence numbers to identify those the new frames because otherwise those new frames will appear to be in the backward window and will be mistaken by ARQRx as retransmission frames
		if (debug) printf("ARQRx, recv3 (LF=%d,MR=%d): received pkt %d. SimTime=%.8f.\n", last_fwd_sn_, most_recent_acked, seq_num, Scheduler::instance().clock());
		int first_st = ((seq_num - wnd_ + sn_cnt)%sn_cnt - last_fwd_sn_ + sn_cnt)%sn_cnt;
		deliver_frames(first_st, false, h); //deliver frames from last forwarded up to the new lower end of the backward window
		status[seq_num%wnd_] = RECEIVED;
		if (pkt_buf[seq_num%wnd_]){ fprintf(stderr, "Error at ARQRx::recv, position of new frame beyond most recent found non empty.\n"); abort(); }
		pkt_buf[seq_num%wnd_] = p;
		lost_pkt_buf[seq_num] = NULL;
		event_buf[seq_num] = NULL;
		deliver_frames(wnd_, true, h); //check if it is possible to deliver in order frames
		for (int i = (most_recent_acked + 1)%sn_cnt; i != seq_num; i = (i + 1)%sn_cnt){ status[i%wnd_] = MISSING; } //update positions between most_recent_acked and the new sequence number with MISSING state
		most_recent_acked = seq_num;
		clean_decoding_matrix(oldest_sq_sender, new_oldest_sq_sender); //remove from the coding structures the frames that are now out of ARQTx's active window
	}

	//-----Schedule ACK----------------//
	ACKEvent *new_ACKEvent = new ACKEvent();
	Event *ack_e;
	new_ACKEvent->ACK_sn = seq_num;
	new_ACKEvent->ACK_uid = pkt_uid;
	new_ACKEvent->coded_ack = NULL;
	new_ACKEvent->isCancelled = false;
	ack_e = (Event *)new_ACKEvent;
	if (delay_ > 0)
		Scheduler::instance().schedule(this, ack_e, (delay_ + 8.0/arq_tx_->get_linkbw()));
	else
		handle(ack_e);
	//---------------------------------//

	if (should_check_for_decoding){ //check if decoding is now possible due to the reception of the new pkt (if it is a retransmitted one)
		known_packets.insert(seq_num); //add newly received pkt
		delete_lost_and_find_associated_coded_in_matrix(seq_num); //delete received pkt from lost and delete coded pkts containing only this lost
		decode(h, false); //Check if decoding is now possible
	}

} //end of recv


void ARQAcker::deliver_frames(int steps, bool mindgaps, Handler *h)
{

	int count = 0;
	while (count < steps){
		if (((status[((last_fwd_sn_+1)%sn_cnt)%wnd_] != RECEIVED) && (status[((last_fwd_sn_+1)%sn_cnt)%wnd_] != DECODED))&&(mindgaps)) break;

		if ((status[((last_fwd_sn_+1)%sn_cnt)%wnd_] == RECEIVED) || (status[((last_fwd_sn_+1)%sn_cnt)%wnd_] == DECODED) || (pkt_buf[((last_fwd_sn_+1)%sn_cnt)%wnd_])){
      //status may not be RECEIVED or DECODED when Tx drops pkts and Rx does not know, so a new lost pkt overrides the status of an old receieved pkt
			finish_time = Scheduler::instance().clock();
			delivered_data += (HDR_CMN(pkt_buf[((last_fwd_sn_+1)%sn_cnt)%wnd_]))->size_;
			delivered_pkts++;
      if (last_delay_sample != 0) sum_of_delay_jitter = sum_of_delay_jitter + abs(Scheduler::instance().clock() - (HDR_CMN(pkt_buf[((last_fwd_sn_+1)%sn_cnt)%wnd_]))->ts_arr_ - last_delay_sample);
      last_delay_sample = Scheduler::instance().clock() - (HDR_CMN(pkt_buf[((last_fwd_sn_+1)%sn_cnt)%wnd_]))->ts_arr_;
      if (last_delay_sample > max_delay) max_delay = last_delay_sample;
      if (last_delay_sample < min_delay) min_delay = last_delay_sample;
			sum_of_delay = sum_of_delay + last_delay_sample;

			send(pkt_buf[((last_fwd_sn_+1)%sn_cnt)%wnd_],h);
		}
		pkt_buf[((last_fwd_sn_+1)%sn_cnt)%wnd_] = NULL;
		status[((last_fwd_sn_+1)%sn_cnt)%wnd_] = NONE;

		last_fwd_sn_ = (last_fwd_sn_+1)%sn_cnt;
		count++;
	}

}// end of deliver_frames

void ARQAcker::print_stats()
{
	printf("\n//--------------- STATISTICS ---------------//\n");
	printf("Start time (sec):\t\t%f\n", arq_tx_->get_start_time());
	printf("Finish time (sec):\t\t%f\n", finish_time);

	printf("Total number of delivered pkts:\t%.0f\n", delivered_pkts);
	printf("Delivered data (in mega bytes):\t%.3f\n", delivered_data/1048576);
  if (delivered_pkts == 0) {finish_time = Scheduler::instance().clock();} //hack for the case that deliver_frames is not called
	double throughput = (delivered_data * 8) / (finish_time - arq_tx_->get_start_time());
	printf("Total throughput (Mbps):\t%f\n", throughput * 1.0e-6);

	printf("Unique packets sent:\t\t%.0f\n", arq_tx_->get_total_packets_sent());
  printf("Coded packets sent:\t\t%.0f\n", arq_tx_->get_total_coded_packets_sent());
  double mean = (delivered_pkts == 0) ? (0.0) : (sum_of_delay / delivered_pkts);
	printf("Mean delay (msec):\t\t%f\n", mean * 1.0e+3);
  printf("Maximum delay (msec):\t\t%f\n", max_delay * 1.0e+3);
  printf("Minimum delay (msec):\t\t%f\n", min_delay * 1.0e+3);
  double meanjitter = (delivered_pkts <= 1) ? (0.0) : (sum_of_delay_jitter / (delivered_pkts -1));
	printf("Mean delay jitter (msec):\t%f\n", meanjitter * 1.0e+3);
  double avg_rtxs = arq_tx_->get_total_retransmissions() / arq_tx_->get_total_packets_sent();
	printf("Avg num of retransmissions:\t%f\n", avg_rtxs);
	printf("Packet loss rate:\t\t%f\n", 1 - (delivered_pkts / arq_tx_->get_total_packets_sent()));

	printf("Number of actual decodings:\t%.0f\n", num_of_decodings);
	printf("Average decoding matrix size:\t%f\n", ( (num_of_decodings == 0) ? (0) : (avg_dec_matrix_size / num_of_decodings) ));
  printf("Average size of known_packets:\t%f\n", ( (num_of_decodings == 0) ? (0) : (avg_known_pkts_size / num_of_decodings) ));
  printf("//------------------------------------------//\n");
} //end of print_stats

void ARQAcker:: parse_coded_packet(Packet *cp, Handler* h){ //function that reads a coded packet and update the list with known packets

  set<int> intersect;
  set<int> unionset;
  set<int> old_lost_packets = lost_packets; //keep a copy of lost_packets before adding new ones
  lost_packets.clear();
  set<int> coded_pkt_contents; //used to store the contents of the coded pkt

	if (debug) {
		int start_pos = (last_fwd_sn_+1)%sn_cnt;
		int stop_pos = (most_recent_acked+1)%sn_cnt;
		if (start_pos != stop_pos){
			printf("ARQRx, parse_coded_packet: The window contains: ");
			do{
				printf("%d(S:%d), ", start_pos, status[start_pos%wnd_]);
				start_pos = (start_pos + 1)%sn_cnt;
			} while (start_pos != stop_pos);
			printf(".\n");
		} else{
			printf("ARQRx, parse_coded_packet: The window contains nothing.\n");
		}
		set<int>:: iterator it;
		printf("ARQRx, parse_coded_packet: The known_pkts are: ");
		for(it = known_packets.begin(); it != known_packets.end(); ++it){
			printf("%d, ", *(it));
		}
		printf(".\n");
		printf("ARQRx, parse_coded_packet: The lost_pkts are: ");
		for(it = lost_packets.begin(); it != lost_packets.end(); ++it){
			printf("%d, ", *(it));
		}
		printf(".\n");
	}

	unsigned char *contents = cp->accessdata();
	int pkt_cnt = -1;
	int readint = -1;
  int first_read_sq = -1;
	pkt_cnt = *(contents);
	pkt_cnt = pkt_cnt << 8 | *(contents+1);
	pkt_cnt = pkt_cnt << 8 | *(contents+2);
	pkt_cnt = pkt_cnt << 8 | *(contents+3);
	if (debug) printf("ARQRx, parse_coded_packet: The coded pkt contains: ");

	for(int j = 1; j <= pkt_cnt; j++){
		readint = *(contents+sizeof(int)*j);
		readint = readint << 8 | *(contents+sizeof(int)*j+1);
		readint = readint << 8 | *(contents+sizeof(int)*j+2);
		readint = readint << 8 | *(contents+sizeof(int)*j+3);
    if (j == 1) first_read_sq = readint;
    coded_pkt_contents.insert(readint);
		if (debug) printf("%d", readint);
		if(known_packets.find(readint) == known_packets.end()){ //is not included in already known
			if (debug) printf("+");
			lost_packets.insert(readint);
		}
		if (debug) printf(", ");
	}
	if (debug) printf(".\n");

  set_intersection(old_lost_packets.begin(),old_lost_packets.end(),coded_pkt_contents.begin(),coded_pkt_contents.end(), std::inserter(intersect,intersect.begin())); //take the intersection between coded_pkt_contents and old_lost_packets

  if (intersect.size() == 0) {
    //the new coded packet contains none of the old_lost_packets->the window of Tx has moved on, no further coded pkts containing those lost will be received so clean lost as well as coded_packets because decoding will not be possible
    old_lost_packets.clear();
    coded_packets.clear();
    //TODO: take care of known_packets: if possible clear those packets not needed any more
    intersect.clear();
  }

  if (lost_packets.size() != 0) { //at least one lost pkt is contained in the coded packet so store it

    set_union(lost_packets.begin(),lost_packets.end(),old_lost_packets.begin(),old_lost_packets.end(), std::inserter(unionset,unionset.begin())); //take the union between coded_pkt_contents and lost_packets
    lost_packets = unionset; unionset.clear(); old_lost_packets.clear();
    //set<int>::iterator mmiter;
    //printf("parse, adding coded pkt %d. ", first_read_sq); printf("Lost pkts are:"); for (mmiter = lost_packets.begin(); mmiter != lost_packets.end(); ++mmiter ){ printf(" %d", *(mmiter)); } printf("\n"); printf("Known pkts are:"); for (mmiter = known_packets.begin(); mmiter != known_packets.end(); ++mmiter ){ printf(" %d", *(mmiter)); } printf("\n");
    coded_packets.insert(pair<int, set<int> >(first_read_sq, coded_pkt_contents)); //store new coded pkt
  }

  coded_pkt_contents.clear(); //delete contents of coded pkt, no more needed

	decode(h,true);
	Packet::free(cp);  //free coded packet, no more needed

} //end of parse_coded_packet

void ARQAcker::decode(Handler* h, bool afterCodedreception){

	if (debug) printf("ARQRx, decode : Examining decoding (LPKTS=%d,RCODED=%d).\n", (int)lost_packets.size(), (int)coded_packets.size());

	bool decoding_is_possible = false;
	if (((int)lost_packets.size() <= (int)coded_packets.size()) && (lost_packets.size() > 0)){
			decoding_is_possible = true;
			avg_dec_matrix_size = avg_dec_matrix_size + (int) lost_packets.size() + (int) known_packets.size();
      avg_known_pkts_size = avg_known_pkts_size + (int) known_packets.size();
			num_of_decodings ++;
	}
	if ((!decoding_is_possible) && (!afterCodedreception)) return; //no need to decode and no need to send ack


  if (decoding_is_possible){
    int fw_dis, fw_width, bw_dis, oldest_sq_sender, new_oldest_sq_sender, nxt_seq, first_st;
    bool within_fww, within_bww;

  	set<int>:: iterator it;
  	for(it = lost_packets.begin(); it != lost_packets.end(); ++it){

  		int lost_sn = *it;
  		known_packets.insert(*it); //move decoded frame into known ones
  		if (debug) printf("ARQRx, decode: decoding pkt %d.\n", lost_sn);
  		//------------------------------------------//
  		//if ((status[lost_sn%wnd_] != MISSING)) { fprintf(stderr, "Error at ARQRx::decode, decoded pkt cannot be found or has a wrong status.\n"); abort();}
  		//------------------------------------------//

  		fw_dis = (lost_sn - last_fwd_sn_ + sn_cnt)%sn_cnt;
  		fw_width = (most_recent_acked - last_fwd_sn_ + sn_cnt)%sn_cnt;
  		within_fww = ((fw_dis <= wnd_) && (fw_dis > 0)) ? (true) : (false);
  		bw_dis = (most_recent_acked - lost_sn + sn_cnt)%sn_cnt;
  		within_bww = (bw_dis < wnd_) ? (true) : (false);
      oldest_sq_sender = (most_recent_acked - arq_tx_->get_coding_wnd() + 1 + sn_cnt)%sn_cnt;
  		new_oldest_sq_sender = (lost_sn - arq_tx_->get_coding_wnd() + 1 + sn_cnt)%sn_cnt;
  		nxt_seq = (last_fwd_sn_ + 1)%sn_cnt;

  		if (within_fww){ //decoded frame is in the forward window
  			if(lost_sn == nxt_seq){ //decoded frame is the next expected
  				status[lost_sn%wnd_] = NONE;
  				pkt_buf[lost_sn%wnd_] = lost_pkt_buf[lost_sn]; //copy frame from lost_pkt_buf
  				lost_pkt_buf[lost_sn] = NULL;
  				finish_time = Scheduler::instance().clock();
  				delivered_pkts++;
  				delivered_data += HDR_CMN(pkt_buf[lost_sn%wnd_])->size_;
          if (last_delay_sample !=0) sum_of_delay_jitter = sum_of_delay_jitter + abs(Scheduler::instance().clock() - HDR_CMN(pkt_buf[lost_sn%wnd_])->ts_arr_ - last_delay_sample);
          last_delay_sample = Scheduler::instance().clock() - HDR_CMN(pkt_buf[lost_sn%wnd_])->ts_arr_;
          if(last_delay_sample > max_delay) max_delay = last_delay_sample;
          if(last_delay_sample < min_delay) min_delay = last_delay_sample;
  				sum_of_delay = sum_of_delay + Scheduler::instance().clock() - HDR_CMN(pkt_buf[lost_sn%wnd_])->ts_arr_;

  				send(pkt_buf[lost_sn%wnd_],h);
  				pkt_buf[lost_sn%wnd_] = NULL;
  				last_fwd_sn_ = (last_fwd_sn_+1)%sn_cnt;
  				deliver_frames((wnd_-1), true, h); //check if other frames are now in order and deliver
  			} else { //decoded frame is in forward window but not the next in order
  				status[lost_sn%wnd_] = DECODED;
  				pkt_buf[lost_sn%wnd_] = lost_pkt_buf[lost_sn]; //copy frame from lost_pkt_buf
  				lost_pkt_buf[lost_sn] = NULL;
  			}
  			if (fw_dis > fw_width) { //if the decoded frame is beyond the most_recent_acked
  				for (int j = (most_recent_acked + 1)%sn_cnt; j != lost_sn; j = (j + 1)%sn_cnt){ status[j%wnd_] = MISSING; } //updtae the positions in between with the MISSING state
  				most_recent_acked = lost_sn;
  				clean_decoding_matrix(oldest_sq_sender, new_oldest_sq_sender); //remove from the coding structures the frames that are now outside the backward window (i.e., ARQTx's active window)
  			}
  		} else if (within_bww && !within_fww){ //decode frame is in the backward window, so do nothing beyond ACKing the frame

  		} else { //the case of a new frame that is beyond the forward window
  			first_st = ((lost_sn - wnd_ + sn_cnt)%sn_cnt - last_fwd_sn_ + sn_cnt)%sn_cnt;
  			deliver_frames(first_st, false, h); //deliver frames from last_fwd_sn_ up to the lower end of the backward window
  			status[lost_sn%wnd_] = DECODED;
  			pkt_buf[lost_sn%wnd_] = lost_pkt_buf[lost_sn]; //copy frame from the lost_pkt_buf
  			lost_pkt_buf[lost_sn] = NULL;
  			deliver_frames(wnd_, true, h); //check if it is possible to deliver in order frames
  			for (int j = (most_recent_acked + 1)%sn_cnt; j != lost_sn; j = (j + 1)%sn_cnt){ status[j%wnd_] = MISSING; } //update positions beyond most_recent_acked up to the decoded frame with the MISSING state
  			most_recent_acked = lost_sn;
  			clean_decoding_matrix(oldest_sq_sender, new_oldest_sq_sender); //remove from the coding structures the frames that are now outside the backward window (i.e., ARQTx's active window)
  		}
  	} //end for
  } //end if

	//Sent a cumulative ack =======================================//
	Event *ack_e;
	ACKEvent *new_ACKEvent = new ACKEvent();
	new_ACKEvent->ACK_sn = -1;
	new_ACKEvent->ACK_uid = -1;
	new_ACKEvent->coded_ack = create_coded_ack();
	new_ACKEvent->isCancelled = false;
	ack_e = (Event *)new_ACKEvent;
	if (delay_ > 0)
		Scheduler::instance().schedule(this, ack_e, (delay_ + HDR_CMN(new_ACKEvent->coded_ack)->size_ /arq_tx_->get_linkbw()));
	else
		handle(ack_e);

	if (debug) printf("ARQRx, decode: ACK sent.\n");
	//=============================================================//

  if (decoding_is_possible){
    lost_packets.clear(); //clean because frames were decoded
    coded_packets.clear(); //coded frames no more needed because decoding completed
  }

} //end of decode

Packet* ARQAcker::create_coded_ack(){

	Packet *cpkt = Packet::alloc();
	hdr_cmn *ch2 = HDR_CMN(cpkt);
	ch2->opt_num_forwards_ = -10001; //coded packet ACK
	unsigned char *buffer = new unsigned char[sizeof(int)*((int)(known_packets.size()) + 1)]; //4 bytes for each decoded frame plus a byte for the counter

	int cnt_pkts = 0;
	set <int> :: iterator itr;
	for (itr = known_packets.begin(); itr != known_packets.end(); itr++)
	{
		*(buffer+sizeof(int)*(cnt_pkts+1)) =  (*itr >> 24) & 0xFF;
		*(buffer+sizeof(int)*(cnt_pkts+1)+1) = (*itr >> 16) & 0xFF;
		*(buffer+sizeof(int)*(cnt_pkts+1)+2) = (*itr >> 8) & 0xFF;
		*(buffer+sizeof(int)*(cnt_pkts+1)+3) = *itr & 0xFF;
		cnt_pkts++;
	}
	*(buffer) =  (cnt_pkts >> 24) & 0xFF;
	*(buffer+1) = (cnt_pkts >> 16) & 0xFF;
	*(buffer+2) = (cnt_pkts >> 8) & 0xFF;
	*(buffer+3) = cnt_pkts & 0xFF;

	ch2->size_ = (cnt_pkts+1)*sizeof(int); //one byte for each seq_num and one byte for the counter
	PacketData *cp_data;
	cp_data = new PacketData(sizeof(int)*(cnt_pkts+1)); //4 bytes for each decoded frame plus a byte for the counter
	unsigned char *pointer_to_data;
	pointer_to_data = cp_data->data();
	memcpy(pointer_to_data, buffer, sizeof(int)*(cnt_pkts+1));
	delete buffer;
	cpkt->setdata(cp_data);

	return cpkt;

} //end of create_coded_ack


void ARQAcker::clean_decoding_matrix(int from, int to)
{

	int runner_ = from;
	do {
		known_packets.erase(runner_); //TODO: if deleted known pkt is in at least one coded pkt we should not delete
    delete_lost_and_associated_coded_from_matrix(runner_); //delete lost pkt and also all coded containing this packet

		runner_ = (runner_ + 1)%sn_cnt;
	} while (runner_ != to);

} //end of clean_decoding_matrix

void ARQAcker::delete_lost_and_associated_coded_from_matrix(int pkt_to_remove)
{

  multimap<int, set<int>>::iterator itcodedpkts;
  multimap<int, set<int>> temp_coded;
  set<int>::iterator mmiter;
  set<int> intersect;

  int deleted_lost_pkt = lost_packets.erase(pkt_to_remove);
  if (deleted_lost_pkt > 0){ //delete all coded pkts containing the deleted lost pkt, TODO: update the known_packets based on the remaining coded
    //printf("Lost pkts are:"); for (mmiter = lost_packets.begin(); mmiter != lost_packets.end(); ++mmiter ){ printf(" %d", *(mmiter)); } printf("\n"); printf("Known pkts are:"); for (mmiter = known_packets.begin(); mmiter != known_packets.end(); ++mmiter ){ printf(" %d", *(mmiter)); } printf("\n"); printf("Num of coded pkts = %d. Lost pkt deleted is %d.\n", (int)coded_packets.size(), pkt_to_remove);
    for (itcodedpkts = coded_packets.begin(); itcodedpkts != coded_packets.end(); ++itcodedpkts){
        //printf("Coded pkt %d:", itcodedpkts->first); for (mmiter = (itcodedpkts->second).begin(); mmiter != (itcodedpkts->second).end(); ++mmiter ){ printf(" %d", *(mmiter)); } printf(".\n");
        int erase_coded_cnt = (itcodedpkts->second).count(pkt_to_remove); //if deleted lost is in coded pkt erase_coded_cnt > 0
        if (erase_coded_cnt > 0){ //coded pkt contains the deleted lost one
          //printf("Coded pkt is deleted.\n");
        } else { // this only serves as a diagnostic: every other coded pkt should contain at least one lost packet
          set_intersection(lost_packets.begin(),lost_packets.end(),(itcodedpkts->second).begin(),(itcodedpkts->second).end(), std::inserter(intersect,intersect.begin()));
          if (intersect.size() == 0) {fprintf(stderr, "Error at ARQRx::delete_lost_and_associated_coded_from_matrix, found coded pkt that does contain any lost pkt.\n"); abort();}
          intersect.clear();
          temp_coded.insert(pair<int, set<int> >(itcodedpkts->first, itcodedpkts->second));
        }
    } // done cleaning coded_pkts
    coded_packets.clear();
    coded_packets = temp_coded;
    temp_coded.clear();
    //printf("The remaining coded_pkts are %d.\n", (int)coded_packets.size());
  } //done with this deleted lost packet

} //end of delete_lost_and_associated_coded_from_matrix


void ARQAcker::delete_lost_and_find_associated_coded_in_matrix(int pkt_to_remove)
{
  multimap<int, set<int>>::iterator itcodedpkts;
  multimap<int, set<int>> temp_coded;
  set<int>::iterator mmiter;
  set<int> intersect;

  int deleted_lost_pkt = lost_packets.erase(pkt_to_remove);
  if (deleted_lost_pkt > 0){ //find if an exist coded pkt contained only the deleted lost pkt and if so delete coded, TODO: update the known_packets based on the remaining coded
    //printf("Lost pkts are:"); for (mmiter = lost_packets.begin(); mmiter != lost_packets.end(); ++mmiter ){ printf(" %d", *(mmiter)); } printf("\n"); printf("Known pkts are:"); for (mmiter = known_packets.begin(); mmiter != known_packets.end(); ++mmiter ){ printf(" %d", *(mmiter)); } printf("\n"); printf("Num of coded pkts = %d. Lost pkt deleted is %d.\n", (int)coded_packets.size(), pkt_to_remove);
    for (itcodedpkts = coded_packets.begin(); itcodedpkts != coded_packets.end(); ++itcodedpkts){
        //printf("Coded pkt %d:", itcodedpkts->first); for (mmiter = (itcodedpkts->second).begin(); mmiter != (itcodedpkts->second).end(); ++mmiter ){ printf(" %d", *(mmiter)); } printf(".\n");
        int exists_in_coded_cnt = (itcodedpkts->second).count(pkt_to_remove); //if deleted lost is in coded pkt exists_in_coded_cnt > 0
        if (exists_in_coded_cnt != 0){ //coded pkt contains the lost pkt turned to known, so check if coded contains at least one other lost
          set_intersection(lost_packets.begin(),lost_packets.end(),(itcodedpkts->second).begin(),(itcodedpkts->second).end(), std::inserter(intersect,intersect.begin()));
          if (intersect.size() == 0) { //coded pkt does not contain at least one other lost pkt
            //printf("Coded pkt is deleted.\n");
          } else { //coded pkt contains at least one other lost pkt
            temp_coded.insert(pair<int, set<int> >(itcodedpkts->first, itcodedpkts->second));
          }
          intersect.clear();
        } else { //coded pkt does not contain the deleted lost pkt
          temp_coded.insert(pair<int, set<int> >(itcodedpkts->first, itcodedpkts->second));
        }
    } // done cleaning coded_pkts
    coded_packets.clear();
    coded_packets = temp_coded;
    temp_coded.clear();
    //printf("The remaining coded_pkts are %d.\n", (int)coded_packets.size());
  } //done with this deleted lost packet
} //end of delete_lost_and_find_associated_coded_in_matrix

void ARQAcker::delete_nack_event(int seq_num){
	event_buf[seq_num] = NULL;
} //end of delete_nack_event

void ARQAcker::log_lost_pkt(Packet *p, ACKEvent *e){

	hdr_cmn *ch = HDR_CMN(p);
	int seq_num = ch->opt_num_forwards_;

	if (debug) printf("ARQRx, log_lost_pkt: Pkt %d is lost.\n", seq_num);
	if (debug) printf("ARQRx, log_lost_pkt: Logging pkt %d. Position %d.\n", seq_num, seq_num%wnd_);

	if (lost_pkt_buf[seq_num]){
		hdr_cmn *chold = HDR_CMN(lost_pkt_buf[seq_num]);
		if (ch->uid() != chold->uid()){ //this is an old frame, so delete it
			Packet::free (lost_pkt_buf[seq_num]);
			lost_pkt_buf[seq_num] = p;
		} else { //this is a retransmitted frame, so ignore
			Packet::free (p);
		}
	} else {
		lost_pkt_buf[seq_num] = p;
	}
	if (event_buf[seq_num] != NULL) { fprintf(stderr, "Error at ARQRx::log_lost_pkt, event_buf position found non empty.\n"); abort(); }
	//Should never find this position with a NACK event since if this is a RTX the NACK event should have been deleted
	event_buf[seq_num] = e;

} // end of log_lost_pkt

void ARQAcker::parse_coded_ack(Packet *p){

	unsigned char *contents = p->accessdata();
	int cnt_pkts = -1;
	cnt_pkts = *(contents);
	cnt_pkts = cnt_pkts << 8 | *(contents+1);
	cnt_pkts = cnt_pkts << 8 | *(contents+2);
	cnt_pkts = cnt_pkts << 8 | *(contents+3);

/*	if (debug) {
		int readint = -1;
		printf("ARQNacker, parse_coded_ack: Coded ACK contains =");
		for(int j = 1; j <= cnt_pkts; j++){
			readint = *(contents+sizeof(int)*j);
			readint = readint << 8 | *(contents+sizeof(int)*j+1);
			readint = readint << 8 | *(contents+sizeof(int)*j+2);
			readint = readint << 8 | *(contents+sizeof(int)*j+3);
			printf(" %d", readint);
		}
		printf(".\n");
	}
*/

	int seq_number = -1;
	for(int j = 1; j <= cnt_pkts; j++){
		seq_number = *(contents+sizeof(int)*j);
		seq_number = seq_number << 8 | *(contents+sizeof(int)*j+1);
		seq_number = seq_number << 8 | *(contents+sizeof(int)*j+2);
		seq_number = seq_number << 8 | *(contents+sizeof(int)*j+3);

		if(event_buf[seq_number]) {
			event_buf[seq_number]->isCancelled = true;
			if (debug) printf("ARQNacker, parse_coded_ack: NACK for pkt %d is cancelled.\n", seq_number);
		}
		//arq_tx_->ack(seq_number, -1);

	}
	//Packet::free(p);

} //end of parse_coded_ack

void ARQAcker::handle(Event* e)
{

	ACKEvent *rcv_ack = (ACKEvent *)e;
	int rcv_sn = rcv_ack->ACK_sn;
	int rcv_uid = rcv_ack->ACK_uid;
	Packet *rcv_p = rcv_ack->coded_ack;
	delete e;

	if ( ranvar_->value() < err_rate ){
		if (rcv_p == NULL){ //a coded ack is not expected so no need to call arq_tx->nack() at the appropriate time
			if (debug) printf("Acker, handle: ACK for pkt %d is lost.\n", rcv_sn);
			if (timeout_ - (delay_ + 8.0/arq_tx_->get_linkbw())==  0) { //if timeout is the same as the time needed for ACK to be delivered then just call nack() instead of ack()
				arq_tx_->nack(rcv_sn, rcv_uid);
			} else { //schedule the end of timeout (nack()) since the ACK is lost and the timeout expires later
				ACKEvent *ACKEvent_helper = new ACKEvent();
				Event *ack_e_helper;
				ACKEvent_helper->ACK_sn = rcv_sn;
				ACKEvent_helper->ACK_uid = rcv_uid;
				ACKEvent_helper->coded_ack = NULL;
				ACKEvent_helper->isCancelled = false;
				ack_e_helper = (Event *)ACKEvent_helper;
				Scheduler::instance().schedule(nacker, ack_e_helper, timeout_ - (delay_ + 8.0/arq_tx_->get_linkbw()));
			}
		} else { //the coded ack is lost, so free
			if (debug) printf("Acker, handle: Coded ACK is lost.\n");
			Packet::free(rcv_p);
		}
	} else {
		if(rcv_p){ //different call based on the type of received packet
			parse_coded_ack(rcv_p);
			arq_tx_->ack(rcv_p);
		}else{
			arq_tx_->ack(rcv_sn,rcv_uid);
		}
	}

} //end of handle

ARQNacker::ARQNacker()
{
	debug = false;
	acker = NULL;
} //end of ARQNacker

int ARQNacker::command(int argc, const char*const* argv)
{
	Tcl& tcl = Tcl::instance();
	if (argc == 3) {
		if (strcmp(argv[1], "setup-ARQAcker") == 0) {
			if (*argv[2] == '0') {
				tcl.resultf("Cannot attach NULL ARQAcker\n");
				return(TCL_ERROR);
			}
			acker = (ARQAcker*)TclObject::lookup(argv[2]);
			return(TCL_OK);
		}
	} return ARQRx::command(argc, argv);
} //end of command


void ARQNacker::recv(Packet* p, Handler* h)
{

	hdr_cmn *ch = HDR_CMN(p);
	if(ch->opt_num_forwards_ < 0){ //coded packet lost, do nothing (but free packet memory)
		Packet::free(p);
		return;
	}

	ACKEvent *new_ACKEvent = new ACKEvent();
	Event *ack_e;
	new_ACKEvent->ACK_sn = ch->opt_num_forwards_;
	new_ACKEvent->ACK_uid = ch->uid();
	new_ACKEvent->coded_ack = NULL;
	new_ACKEvent->isCancelled = false;
	ack_e = (Event *)new_ACKEvent;

	acker->log_lost_pkt(p, new_ACKEvent);

	if (delay_ > 0)
		Scheduler::instance().schedule(this, ack_e, timeout_);
	else
		handle(ack_e);

} //end of recv

void ARQNacker::handle(Event* e)
{
	ACKEvent *rcv_ack = (ACKEvent *)e;
	int rcv_sn = rcv_ack->ACK_sn;
	int rcv_uid = rcv_ack->ACK_uid;
	delete e;
	if (debug) printf("Nacker, handler: Packet id is %d. SimTime=%.8f.\n", rcv_sn, Scheduler::instance().clock());
	if (!rcv_ack->isCancelled) arq_tx_->nack(rcv_sn,rcv_uid);
	acker->delete_nack_event(rcv_sn);
} //end of handle

//--------------------------------------------------------------------------------------------//
//--------------------------------------------------------------------------------------------//
