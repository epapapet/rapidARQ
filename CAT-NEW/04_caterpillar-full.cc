#include "caterpillar.h"
#include "packet.h"

static class CaterpillarTxClass: public TclClass {
 public:
	CaterpillarTxClass() : TclClass("CaterpillarTx") {}
	TclObject* create(int, const char*const*) {
		return (new CaterpillarTx);
	}
} class_caterpillar_tx;

static class CaterpillarAckerClass: public TclClass {
 public:
	CaterpillarAckerClass() : TclClass("CaterpillarAcker") {}
	TclObject* create(int, const char*const*) {
		return (new CaterpillarAcker);
	}
} class_caterpillar_acker;

static class CaterpillarNackerClass: public TclClass {
 public:
	CaterpillarNackerClass() : TclClass("CaterpillarNacker") {}
	TclObject* create(int, const char*const*) {
		return (new CaterpillarNacker);
	}
} class_caterpillar_nacker;

void CaterpillarHandler::handle(Event* e)
{
	arq_tx_.resume();
}


//-------------------------------------------CaterpillarRx--------------------------------------------//
//--------------------------------------------------------------------------------------------//
CaterpillarTx::CaterpillarTx() : arqh_(*this)
{
	wnd_ = 0; //the size of the sender window
	sn_cnt = 0; //number of available sequence numbers
	last_acked_sq_ = -1; //the last acked sequence number
	most_recent_sq_ = 0; //the most recent sequence number to be used when sending a frame

	blocked_ = 0; //used to check whethet the channel is occupied with a transmission
	pending = NULL; //used to store a frame that arrives from the outgoing queue and finds the channel blocked
	coded = NULL; //used for storing a coded packet that finds the channel blocked_
	handler_ = 0; //pointer to the outgoing queue (upstream object)

	retry_limit_ = 0; //number of retransmisions allowed per frame
	bind("retry_limit_", &retry_limit_);
	num_pending_retrans_ = 0; //number of retransmissions scheduled and pending in CaterpillarTx

	rate_k = 0; //the number of native frames before the creation of a coded one
	lnk_bw_ = 10000000;
	bind("lnk_bw_", &lnk_bw_);
	lnk_delay_ = 0.03;
	bind("lnk_delay_", &lnk_delay_);
	app_pkt_Size_ = 8000;
	bind("app_pkt_Size_", &app_pkt_Size_);
  timeout_ = 0;

  ranvar_ = NULL; // random number generator used for simulating the loss rate for ACKs
	err_rate = 0.0; // the error rate for ACks

	native_counter = 0; //counter used for invoking the creation of a coded frame

	debug = false; //used to enable printing of diagnostic messages

	start_time = -1; //time when 1st packet arrived at CaterpillarTx::recv
	packets_sent = 0; //unique packets sent
  coded_pkts_sent = 0; ////total nu,ber of csent coded pkts
  pkt_rtxs = 0; //the total number of pkt retransmissions
  total_pause_time = 0; //the total time the sender spend paused because the window reached its limit
  start_of_pause_period = -1; //the start of a pause period, used to calculate the total_pause_time
} //end of constructor

int CaterpillarTx::command(int argc, const char*const* argv)
{
	Tcl& tcl = Tcl::instance();
  if (argc == 3) {
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
  } else if (argc == 5) {
		if (strcmp(argv[1], "setup-wnd") == 0) {
			if (*argv[2] == '0') {
				tcl.resultf("Cannot setup NULL wnd\n");
				return(TCL_ERROR);
			}
			wnd_ = atoi(argv[2]);
			sn_cnt = 4 * wnd_; //although 2*wnd_ is enough, we use 4*wnd_ or more to tackle the case that CaterpillarTx drops packets and advances its window without CaterpillarRx knowing
			pkt_buf = new Packet* [wnd_]; //buffer for storing pending frames
			status = new CaterpillarARQStatus[wnd_]; //the status for each frame: IDLE,SENT,ACKED,RTX,RTXPRE,DROP
			num_rtxs = new int[wnd_]; //the number of retransmissions executed for each frame
			pkt_uids = new int[wnd_]; //buffer for storing the uids of pending frames: used only for diagnostic purposes
			for(int i=0; i<wnd_; i++){ pkt_buf[i] = NULL; status[i] = IDLE; num_rtxs[i] = 0; pkt_uids[i]=-1; }
      timeout_events = new CaterpillarTimeoutEvent* [sn_cnt]; //buffer for storing pointers to timeout events
      for(int i=0; i<sn_cnt; i++){ timeout_events[i] = NULL; }
			rate_k = atoi(argv[3]);
			if (rate_k == 0){
				rate_k = 2147483647; //i.e., deactivate coding
			}
      timeout_ = atof(argv[4]);
      double max_ack_size = (wnd_ + 1)*4.0;
      //double minimum_rtt_time = 2*lnk_delay_ + 8.0*(app_pkt_Size_ + 1)/lnk_bw_;
      double rtt_time = 2*lnk_delay_ + 8.0*(app_pkt_Size_ + max_ack_size)/lnk_bw_;
      double minimum_rtt_time = rtt_time;
      if (timeout_ == 0){ timeout_ = rtt_time; }
      if (timeout_ < 0) { timeout_ = -(1.0/timeout_)*rtt_time; }
      if (timeout_ < minimum_rtt_time) {
        tcl.resultf("Timeout is too small.\n");
				return(TCL_ERROR);
      }
			return(TCL_OK);
		}
	} return Connector::command(argc, argv);
} //end of command


void CaterpillarTx::recv(Packet* p, Handler* h)
{

	//This procedure is invoked by the queue_ (i.e., output queue) to deliver a message to CaterpillarTx
	//The check whether the current window exceeds or not wnd_ has already be done at this point
  if (handler_ == 0) printf("Simulation time: %10.4f", Scheduler::instance().clock());
  if (handler_ != 0) printf("\b\b\b\b\b\b\b\b\b\b%10.4f", Scheduler::instance().clock());

	if(last_acked_sq_ == -1 && most_recent_sq_ == 0 && start_time == -1){ //first packet to be received will trigger the clock
		start_time = Scheduler::instance().clock();
	}

	//Sanity checks---------//
	if (&arqh_==0) {
		fprintf(stderr, "Error at CaterpillarTx::recv, Cannot transmit when &arqh_ is Null.\n");
		abort();
	}
	if ((status[most_recent_sq_%wnd_] != IDLE) && ((HDR_CMN(p))-> opt_num_forwards_ >= 0)) {
		fprintf(stderr, "Error at CaterpillarTx::recv, storing position should be in IDLE mode.\n");
		abort();
	}
	//---------------------//
	//Only in initialization-------//
	if (h != 0) handler_ = h;
	//----------------------------//

  if ((blocked_) && ((HDR_CMN(p))-> opt_num_forwards_ >= 0)) {
    if (pending) { fprintf(stderr, "Error at CaterpillarsTx::recv, Tx should not have a pending frame when recv is called.\n"); abort(); }
    pending = p; return;
  }
  if ((blocked_) && ((HDR_CMN(p))-> opt_num_forwards_ < 0)) {
    if (coded) { fprintf(stderr, "Error at CaterpillarTx::recv, Tx should not have a pending frame when recv is called.\n"); abort(); }
    coded = p; return;
  }

	hdr_cmn *ch = HDR_CMN(p);
	if (ch-> opt_num_forwards_ >= 0){

		ch-> opt_num_forwards_ = most_recent_sq_;
		if (pkt_buf[most_recent_sq_%wnd_]) {fprintf(stderr, "Error at CaterpillarTx::recv, storing position found non empty.\n"); abort();} //pkt_buf is cleared by reset_lastacked
		pkt_buf[most_recent_sq_%wnd_] = p;
		pkt_uids[most_recent_sq_%wnd_] = ch->uid();
		status[most_recent_sq_%wnd_] = SENT;
		num_rtxs[most_recent_sq_%wnd_] = 0;

    CaterpillarTimeoutEvent *etout = new CaterpillarTimeoutEvent();
    etout->type = TIMEOUT;
    etout->sn = most_recent_sq_;
    etout->uid = ch->uid();
    etout->expirationTime = Scheduler::instance().clock() + timeout_;
    etout->isCancelled = false;
    if (timeout_events[most_recent_sq_]) { fprintf(stderr, "Error at CaterpillarTx::recv, found non-empty position of timeout event.\n"); abort(); }
    timeout_events[most_recent_sq_] = etout;
    Scheduler::instance().schedule(this, etout, timeout_);

    ch->ts_arr_ = Scheduler::instance().clock(); //used to calculate delay, retransmitted pkts are not sent through recv(), so this is a new pkt

		most_recent_sq_ = (most_recent_sq_+1)%sn_cnt;

		native_counter++;
		if (native_counter == rate_k){ //prepare a coded frame
			coded = create_coded_packet();
			native_counter = 0;
		}

	}

	if (debug) printf("CaterpillarTx, recv: Sending pkt %d. SimTime=%.8f.\n", ch-> opt_num_forwards_, Scheduler::instance().clock());

	blocked_ = 1;
	if (ch-> opt_num_forwards_ >= 0){
		//In general, when sending a native frame we create a copy (even in retransmissions) to decouple CaterpillarTx and CaterpillarRx
		//CaterpillarTx is responsible for freeing frames sent from queue while CaterpillarRx delivers frames to upper layers and frees frames not eventually delivered
		packets_sent += 1;
		Packet *pnew = p->copy();
		send(pnew,&arqh_);
	} else {
    coded_pkts_sent++;
		send(p,&arqh_);
	}

} //end of recv

Packet* CaterpillarTx::create_coded_packet(){ //create a new coded packet with seq nums of <=k previous packets sent

	Packet *cpkt = Packet::alloc();
	hdr_cmn *ch2 = HDR_CMN(cpkt);
	ch2->opt_num_forwards_ = -10000; //indicates a coded packet, SHOULD BE ALWAYS < 0

	unsigned char *buffer = new unsigned char[sizeof(int)*(wnd_+1)]; //maximum of wnd_ sequence numbers plus a counter
	int runner_ = ((last_acked_sq_+1)%sn_cnt)%wnd_;
	int cnt_pkts = 0;
	do {
		if((status[runner_] != DROP) && (status[runner_] != ACKED)){ //should not include DROPPED pkts, no need to include ACKED pkts
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

/*void CaterpillarTx::ack(int rcv_sn, int rcv_uid)
{

	int fw_dis = (rcv_sn - last_acked_sq_ + sn_cnt)%sn_cnt;
	int fw_width = (most_recent_sq_ - last_acked_sq_ + sn_cnt)%sn_cnt;
	bool within_fww = ((fw_dis < fw_width) && (fw_dis > 0)) ? (true) : (false);
	if (!within_fww) return; //ARTx may receive multiple ACKs per frame due to coded frames, so ignore ACKs out of the active window

	//Sanity checks--------//
	if (status[rcv_sn%wnd_] == IDLE) { fprintf(stderr,"Error at CaterpillarTx::ack, an ACK is received when the status is not SENT. It is %d.\n", status[rcv_sn%wnd_]); abort(); }
	if (handler_ == 0) { fprintf(stderr,"Error at CaterpillarTx::ack, handler_ is null.\n"); abort(); }
	//--------------------//

	if (status[rcv_sn%wnd_] != ACKED){
		if (debug) printf("CaterpillarTx ack: Pkt %d with status %d is ACKED. The news status is %d. Pending retrans=%d. LA(MR) before is %d(%d). SimTime=%.8f.\n ", rcv_sn, status[rcv_sn%wnd_], ACKED, num_pending_retrans_, last_acked_sq_, most_recent_sq_, Scheduler::instance().clock());

		if((status[rcv_sn%wnd_] == RTX) || (status[rcv_sn%wnd_] == RTXPRE)) num_pending_retrans_--; //reduce the number of scheduled retransmissions
		status[rcv_sn%wnd_] = ACKED;
		if (rcv_sn%wnd_ == ((last_acked_sq_ + 1)%sn_cnt)%wnd_) {
      reset_lastacked();  //acked frame is next in order, so check whether the active window should advance
      if (!blocked_) handler_->handle(0); //ask queue_ to deliver next packet
    }
	}

} //end of ack
*/

void CaterpillarTx::parse_cumulative_ack(Packet *p){ //called when a coded ack arrives, reads the contents and checks to advance the window

  bool should_forward_window = false;
  bool valid_seqnum = true;
	unsigned char *contents = p->accessdata();
	int cnt_pkts = -1;
	cnt_pkts = *(contents);
	cnt_pkts = cnt_pkts << 8 | *(contents+1);
	cnt_pkts = cnt_pkts << 8 | *(contents+2);
	cnt_pkts = cnt_pkts << 8 | *(contents+3);


	if (debug) {
		int readint = -1;
		printf("CaterpillarTx, parse_cumulative_ack: Received ACK contains =");
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

    if (seq_number >= 0) {
      valid_seqnum = parse_ack(seq_number, true);
    } else { //this is a negative ack
      valid_seqnum = parse_nack (seq_number + 10000);
    }
    if (!valid_seqnum) continue;
    if (seq_number%wnd_ == ((last_acked_sq_ + 1)%sn_cnt)%wnd_) should_forward_window = true;
	}
  if (should_forward_window){
    reset_lastacked();  //acked frame is next in order, so check whether the active window should advance
    if (!blocked_) handler_->handle(0); //ask queue_ to deliver next packet
  }
	Packet::free(p);

} //end of parse_cumulative_ack

bool CaterpillarTx::parse_ack(int rcv_sn, bool batch)
{

  int fw_dis = (rcv_sn - last_acked_sq_ + sn_cnt)%sn_cnt;
	int fw_width = (most_recent_sq_ - last_acked_sq_ + sn_cnt)%sn_cnt;
	bool within_fww = ((fw_dis < fw_width) && (fw_dis > 0)) ? (true) : (false);
	if (!within_fww) {return false;} //ARTx may receive multiple ACKs per frame due to coded frames, so ignore ACKs out of the active window

	//Sanity checks--------//
	if (status[rcv_sn%wnd_] == IDLE) { fprintf(stderr,"Error at CaterpillarTx::ack, an ACK is received when the status is not SENT. It is %d.\n", status[rcv_sn%wnd_]); abort(); }
	if (handler_ == 0) { fprintf(stderr,"Error at CaterpillarTx::ack, handler_ is null.\n"); abort(); }
	//--------------------//

	if (status[rcv_sn%wnd_] != ACKED){
		if (debug) printf("CaterpillarTx ack: Pkt %d with status %d is ACKED. The news status is %d. Pending retrans=%d. LA(MR) before is %d(%d). SimTime=%.8f.\n ", rcv_sn, status[rcv_sn%wnd_], ACKED, num_pending_retrans_, last_acked_sq_, most_recent_sq_, Scheduler::instance().clock());
		if ((status[rcv_sn%wnd_] == RTX) || (status[rcv_sn%wnd_] == RTXPRE)) num_pending_retrans_--; //reduce the number of scheduled retransmissions
		status[rcv_sn%wnd_] = ACKED;
    if (timeout_events[rcv_sn]) (timeout_events[rcv_sn])->expirationTime = -1.0; //the timeout event may be NULL if pkt is DROPED and a late decode ACK is received
    if (timeout_events[rcv_sn]) (timeout_events[rcv_sn])->isCancelled = true;
    if (!batch){
      if (rcv_sn%wnd_ == ((last_acked_sq_ + 1)%sn_cnt)%wnd_) {
        reset_lastacked();  //acked frame is next in order, so check whether the active window should advance
        if (!blocked_) handler_->handle(0); //ask queue_ to deliver next packet
      }
    }
	}
  return true;

} //end of parse_ack


bool CaterpillarTx::parse_nack(int rcv_sn)
{

	int fw_dis = (rcv_sn - last_acked_sq_ + sn_cnt)%sn_cnt;
	int fw_width = (most_recent_sq_ - last_acked_sq_ + sn_cnt)%sn_cnt;
	bool within_fww = ((fw_dis < fw_width) && (fw_dis > 0)) ? (true) : (false);
	if (!within_fww) return false; //ARTx may receive multiple NACKs per frame due to coded frames, so ignore ACKs out of the active window

  if (Scheduler::instance().clock() - ((timeout_events[rcv_sn])->expirationTime - timeout_) < timeout_) return false; // time elapsed since last retransmission is not sufficient for the ack to arrive, so avoid sending again a retransmission

	//Sanity checks--------//
	if (status[rcv_sn%wnd_] == IDLE) { fprintf(stderr,"Error at CaterpillarTx::timeout_expired when the status is not SENT.\n"); abort(); }
	if (handler_ == 0) { fprintf(stderr,"Error at CaterpillarTx::timeout_expired, handler_ is null\n"); abort(); }
	if (&arqh_==0) { fprintf(stderr, "Error at CaterpillarTx::timeout_expired, Cannot transmit when &arqh_ is Null.\n"); abort(); }
	//--------------------//

	if (status[rcv_sn%wnd_] != SENT) { //it is possible to receive an NACK for a frame acked through a cumulative ACK (i.e., coded frame) when the cumulative ACK arives after the retransmission
		if (debug) printf("CaterpillarTx, timeout_expired for pkt %d with status %d. Ignoring. SimTime=%.8f.\n", rcv_sn, status[rcv_sn%wnd_], Scheduler::instance().clock());
		return false;
	} else {
		if (pkt_buf[rcv_sn%wnd_] == NULL) { fprintf(stderr,"Error at CaterpillarTx::timeout_expired but pkt is not found.\n"); abort(); }
		if (debug) printf("CaterpillarTx, timeout_expired: timeout_expired for pkt %d with status %d. SimTime=%.8f.\n", rcv_sn, status[rcv_sn%wnd_], Scheduler::instance().clock());
	}

	if ( num_rtxs[rcv_sn%wnd_] < retry_limit_) { //packet shoud be retransmitted
		status[rcv_sn%wnd_] = RTXPRE;
		if (!blocked_){ //if CaterpillarTx is available go on with retransmision
			if (debug) printf("CaterpillarTx, timeout_expired: Sending pkt %d. SimTime=%.8f.\n", rcv_sn, Scheduler::instance().clock());
			blocked_ = 1;
			num_rtxs[rcv_sn%wnd_]++;
			status[rcv_sn%wnd_] = SENT;
			Packet *newp = pkt_buf[rcv_sn%wnd_]->copy();
      timeout_events[rcv_sn]->expirationTime = Scheduler::instance().clock() + timeout_;
      timeout_events[rcv_sn]->isCancelled = true;
      Scheduler::instance().schedule(this, timeout_events[rcv_sn], timeout_);
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
    return false;
	} else {//packet should be dropped
		if (debug) printf("CaterpillarTx, nack: Dropping pkt %d. SimTime=%.8f.\n", rcv_sn, Scheduler::instance().clock());
		status[rcv_sn%wnd_] = DROP;
    timeout_events[rcv_sn]->expirationTime = -1.0; //do not delete event because it is already in the scheduler
    timeout_events[rcv_sn]->isCancelled = true;
    return true;
	}

}//parse_nack

void CaterpillarTx::timeout_expired(int rcv_sn, int rcv_uid)
{

	int fw_dis = (rcv_sn - last_acked_sq_ + sn_cnt)%sn_cnt;
	int fw_width = (most_recent_sq_ - last_acked_sq_ + sn_cnt)%sn_cnt;
	bool within_fww = ((fw_dis < fw_width) && (fw_dis > 0)) ? (true) : (false);
	if (!within_fww) return; //ARTx may receive multiple NACKs per frame due to coded frames, so ignore ACKs out of the active window

	//Sanity checks--------//
	if (status[rcv_sn%wnd_] == IDLE) { fprintf(stderr,"Error at CaterpillarTx::timeout_expired when the status is not SENT.\n"); abort(); }
	if (handler_ == 0) { fprintf(stderr,"Error at CaterpillarTx::timeout_expired, handler_ is null\n"); abort(); }
	if (&arqh_==0) { fprintf(stderr, "Error at CaterpillarTx::timeout_expired, Cannot transmit when &arqh_ is Null.\n"); abort(); }
	//--------------------//

	if (status[rcv_sn%wnd_] != SENT) { //it is possible to receive an NACK for a frame acked through a cumulative ACK (i.e., coded frame) when the cumulative ACK arives after the retransmission
		if (debug) printf("CaterpillarTx, timeout_expired for pkt %d with status %d. Ignoring. SimTime=%.8f.\n", rcv_sn, status[rcv_sn%wnd_], Scheduler::instance().clock());
		return;
	} else {
		if (pkt_buf[rcv_sn%wnd_] == NULL) { fprintf(stderr,"Error at CaterpillarTx::timeout_expired but pkt is not found.\n"); abort(); }
		if (debug) printf("CaterpillarTx, timeout_expired: timeout_expired for pkt %d with status %d. SimTime=%.8f.\n", rcv_sn, status[rcv_sn%wnd_], Scheduler::instance().clock());
	}

	if ( num_rtxs[rcv_sn%wnd_] < retry_limit_) { //packet shoud be retransmitted
		status[rcv_sn%wnd_] = RTX;
		if (!blocked_){ //if CaterpillarTx is available go on with retransmision
			if (debug) printf("CaterpillarTx, timeout_expired: Sending pkt %d. SimTime=%.8f.\n", rcv_sn, Scheduler::instance().clock());
			blocked_ = 1;
			num_rtxs[rcv_sn%wnd_]++;
			status[rcv_sn%wnd_] = SENT;
			//Packet *newp = pkt_buf[rcv_sn%wnd_]->copy();
      Packet *newp = create_coded_packet(); //instead of sending the packet send a coded one
      HDR_CMN(newp)->aomdv_salvage_count_ = rcv_uid; //add the uid of the original packet
      HDR_CMN(newp)->opt_num_forwards_ = HDR_CMN(newp)->opt_num_forwards_ + rcv_sn; //add the sn of the original packet
      timeout_events[rcv_sn]->expirationTime = Scheduler::instance().clock() + timeout_;
      timeout_events[rcv_sn]->isCancelled = false;
      Scheduler::instance().schedule(this, timeout_events[rcv_sn], timeout_);
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
		if (debug) printf("CaterpillarTx, nack: Dropping pkt %d. SimTime=%.8f.\n", rcv_sn, Scheduler::instance().clock());
		status[rcv_sn%wnd_] = DROP;
    delete (timeout_events[rcv_sn]);
    timeout_events[rcv_sn] = NULL;
		if (rcv_sn%wnd_ == ((last_acked_sq_ + 1)%sn_cnt)%wnd_) {
      reset_lastacked(); //droped frame is next in order so check whether the active window should advance
      if (!blocked_) handler_->handle(0); //ask queue_ to deliver next packet
    }
	}

}//timeout_expired

void CaterpillarTx::resume()
{
	//This procedure is invoked by link_ when a transmission is completed, i.e., it is invoked T secs after CaterpillarTx executes send() where T equals transmission_delay.
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
		int wrunner_ = findpos_retrans();
    int runner_ = wrunner_%wnd_;
		if (debug) printf("CaterpillarTx, resume: Sending pkt %d. SimTime=%.8f.\n", (HDR_CMN(pkt_buf[runner_]))->opt_num_forwards_, Scheduler::instance().clock());
		num_rtxs[runner_]++;
		num_pending_retrans_--;
		blocked_ = 1;
    Packet *pnew;
		if (status[runner_] == RTX) { //the packet has expired, so send a coded one instead of the packet
      pnew = create_coded_packet();
      HDR_CMN(pnew)->aomdv_salvage_count_ = HDR_CMN(pkt_buf[runner_])->uid(); //add the uid of the original
      HDR_CMN(pnew)->opt_num_forwards_ = HDR_CMN(pnew)->opt_num_forwards_ + HDR_CMN(pkt_buf[runner_])->opt_num_forwards_; //add the sn of original
    } else { //the case of RTXPRE: non expired, send original
      pnew = (pkt_buf[runner_])->copy();
    }
    status[runner_] = SENT;
    timeout_events[wrunner_]->expirationTime = Scheduler::instance().clock() + timeout_;
    timeout_events[wrunner_]->isCancelled = false;
    Scheduler::instance().schedule(this, timeout_events[wrunner_], timeout_);
    pkt_rtxs++;
    native_counter++;
		if (native_counter == rate_k){ //prepare a coded frame
			coded = create_coded_packet();
			native_counter = 0;
		}
		send(pnew,&arqh_);
	} else {//there are no pending retransmision, check whether it is possible to send a new packet
		//TO DO: check whether active window reaches wnd_ and CaterpillarTx is stuck without asking queue for the next frame
		int current_wnd_ = (most_recent_sq_ - last_acked_sq_ + sn_cnt)%sn_cnt;
		if (current_wnd_ <= wnd_) {
      handler_->handle(0); //ask queue_ to deliver next packet
    } else {
      if (start_of_pause_period == -1) start_of_pause_period = Scheduler::instance().clock(); //start logging a pause period
    }
	}

}//end of resume

int CaterpillarTx::findpos_retrans()
{
	//----------DEBUG------------------//
	if((last_acked_sq_+1)%sn_cnt == most_recent_sq_) { fprintf(stderr, "Error at CaterpillarTx::findpos_retrans, no packet is waiting (stored) for transmission.\n"); abort(); }
	//---------------------------------//

	bool found = FALSE;
	int runner_ = ((last_acked_sq_+1)%sn_cnt)%wnd_;
  int runner_wide_ = (last_acked_sq_+1)%sn_cnt;

	do {
		if ((status[runner_] == RTX) || (status[runner_] == RTXPRE)) {
			found = TRUE;
			break;
		}
		runner_ = (runner_+1)%wnd_;
    runner_wide_ = (runner_wide_ + 1)%sn_cnt;
	} while (runner_ != (most_recent_sq_%wnd_));

	//----------DEBUG------------------//
	if (!found){ fprintf(stderr, "Error at CaterpillarTx::findpos_retrans, packet with RTX or RTXPRE status NOT FOUND.\n"); abort(); }
	//---------------------------------//

	return runner_wide_;
} //end of findpos_retrans

void CaterpillarTx::reset_lastacked()
{

  bool window_advanced = false;
	if((last_acked_sq_+1)%sn_cnt == most_recent_sq_) return; //no need to reset last_ack because there is no packet stored (MOST RECENT - LAST ACKED = 1)

	int runner_ = ((last_acked_sq_+1)%sn_cnt)%wnd_;
  int wide_runner_ = (last_acked_sq_+1)%sn_cnt;
	do {
		if ((status[runner_] == RTX) || (status[runner_] == SENT)) break;
		if (pkt_buf[runner_]) Packet::free(pkt_buf[runner_]); //free frames not needed any more
		pkt_buf[runner_] = NULL;
    timeout_events[wide_runner_] = NULL;
		status[runner_] = IDLE;
		num_rtxs[runner_] = 0;
		pkt_uids[runner_] = -1;

    window_advanced = true;
		last_acked_sq_ = (last_acked_sq_ + 1)%sn_cnt;
		runner_ = (runner_ + 1)%wnd_;
    wide_runner_ = (wide_runner_ + 1)%sn_cnt;
	} while (runner_ != (most_recent_sq_%wnd_));

  if (window_advanced) {
    if (start_of_pause_period != -1) total_pause_time = total_pause_time + Scheduler::instance().clock() - start_of_pause_period;
    start_of_pause_period = -1;
  }
	if(debug) printf("CaterpillarTx, reset_lastacked: new LA(MR) are %d(%d). SimTime=%.8f.\n", last_acked_sq_, most_recent_sq_, Scheduler::instance().clock());

} // end of reset_lastacked

void CaterpillarTx::handle(Event* e){

  CaterpillarEvent *received_event = (CaterpillarEvent *)e;
  CaterpillarEventTypes rcv_type = received_event->type;
  if (rcv_type == TIMEOUT){ //handle a timeout expiration event

    CaterpillarTimeoutEvent *received_tevent = (CaterpillarTimeoutEvent *)e;
    //three cases for event: a) not cancelled -> timeout expired, b) cancelled and expirationTime set to -1.0 -> the frame has been ACKED, and
    //c) cancelled but expirationTime > current time -> should be rescheduled (an old expiration event cancelled and renewed by a retransmission)
    //Events are deleted ONLY BY a) timeout_expired when rtx_limit is reached or b) handle when the event has been cancleled by an ACK. This is
    //because we need to be sure that the event is not in the Scheduler (it is added when the packets is sent for the first time).
    if (received_tevent->isCancelled){

      if (received_tevent->expirationTime < 0){
        timeout_events[received_tevent->sn] = NULL;
        delete e;
      } else if ( received_tevent->expirationTime > Scheduler::instance().clock() ) {
        received_tevent->isCancelled = false;
        Scheduler::instance().schedule(this, received_tevent, received_tevent->expirationTime - Scheduler::instance().clock());
      }

    } else { //the event is not cancelled

      timeout_expired(received_tevent->sn, received_tevent->uid);

    }

  } else if (rcv_type == ACK) { //handle an ack related event

    CaterpillarACKEvent *received_aevent = (CaterpillarACKEvent *)e;
    if ( ranvar_->value() < err_rate ){ //ack is lost, no need to do anything just delete pkt if it is a coded_ack
      if (received_aevent->coded_ack == NULL){
        if (debug) printf("Tx, handle: ACK for %d is lost.\n", received_aevent->sn);
      } else { //this is a coded ack, not expected so ignore and delete
        if (debug) printf("Tx, handle: Coded ACK is lost.\n");
  			Packet::free(received_aevent->coded_ack);
      }
    } else { //ack is received correctly
      if (received_aevent->coded_ack == NULL){ //this is a simple ACK
        parse_ack(received_aevent->sn, false);
      } else { //this is a cumulative ACK
        Packet *rcvpkt = (received_aevent->coded_ack)->copy();
        Packet::free(received_aevent->coded_ack);
        parse_cumulative_ack(rcvpkt);
      }
    }
    delete e; //no more needed, delete

  }
} //end of handle
//--------------------------------------------------------------------------------------------//
//--------------------------------------------------------------------------------------------//





//-------------------------------------------CaterpillarRx--------------------------------------------//
//--------------------------------------------------------------------------------------------//
CaterpillarRx::CaterpillarRx()
{
	arq_tx_=0;
	delay_ = 0;
} //end of constructor


int CaterpillarRx::command(int argc, const char*const* argv)
{
	Tcl& tcl = Tcl::instance();
  if (argc == 2){
    if (strcmp(argv[1], "update-delays") == 0) {
      delay_ = arq_tx_->get_linkdelay(); //the propagation delay
			return(TCL_OK);
		}
  } else if (argc == 3) {
		if (strcmp(argv[1], "attach-CaterpillarTx") == 0) {
			if (*argv[2] == '0') {
				tcl.resultf("Cannot attach NULL CaterpillarTx\n");
				return(TCL_ERROR);
			}
			arq_tx_ = (CaterpillarTx*)TclObject::lookup(argv[2]);
			return(TCL_OK);
		}
	} return Connector::command(argc, argv);
} //end of command


CaterpillarAcker::CaterpillarAcker()
{
	wnd_ = 0; // the window size
	sn_cnt = 0; //the number of available sequence numbers
	last_fwd_sn_ = -1; //the sequence number of the last frame delivered to the upper layer
	most_recent_acked = 0; //the sequence number of the frame that was most recently ACKed

	nacker = NULL;

	debug = false; //used to enable printing of diagnostic messages

  finish_time = 0; //time when the last pkt was delivered to the receiver's upper layer, used to calculate throughput
	delivered_pkts = 0; //the total number of pkts delivered to the receiver's upper layer
	delivered_data = 0; //the total number of bytes delivered to the receiver's upper layer
	sum_of_delay = 0; //sum of delays for every packet delivered, used to calculate average delay
  sum_of_delay_jitter = 0; //sum of delay jitter for every packet delivered, used to calculate average delay
	avg_dec_matrix_size = 0; //the avg size of the decoding matrix when decoding is performed (used to estimate processing overhead)
  max_dec_matrix_size = 0; //the maximum size of the decoding matrix
  avg_inv_known_pkts_size = 0; //the avg size of the involved_known_packets when decoding is performed (part of decoding matrix already in diagonal form)
  max_inv_known_pkts_size = 0; //the maximum size of involved_known_packets
  avg_known_pkts_size = 0; //the avg size of the known_packets when decoding is performed (part of decoding matrix already in diagonal form)
  max_known_pkts_size = 0; //the maximum size of known_packets
	num_of_decodings = 0; //number of decoding operations
  avg_pkts_per_decoding = 0; //the average number of decoded packets per decoding
  max_delay = 0; //the maximum delay experienced by a packet
  min_delay = 100000000000; //the minimum delay experienced by a packet
  last_delay_sample = 0; //the last delay, used to calculate delay jitter
} //end of constructor



int CaterpillarAcker::command(int argc, const char*const* argv)
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
			sn_cnt = 4 * wnd_; //never less than 2*wnd_ but usually more than that (see note in CaterpillarTx command)
			pkt_buf = new Packet* [wnd_]; //buffer for storing out-of-order received frames
			lost_pkt_buf = new Packet* [sn_cnt]; //buffer for storing lost frames, used to find a frame if the frame is decoded (this is used as a hack)
			status = new CaterpillarPacketStatus[wnd_]; //the status of each pending frame: NONE,MISSING,RECEIVED,DECODED
			for(int i=0; i<wnd_; i++){ pkt_buf[i] = NULL; status[i] = NONE;}
			for(int i=0; i<sn_cnt; i++){ lost_pkt_buf[i] = NULL; }
			return(TCL_OK);
		}
		if (strcmp(argv[1], "setup-CaterpillarNacker") == 0) {
			if (*argv[2] == '0') {
				tcl.resultf("Cannot attach NULL CaterpillarNacker\n");
				return(TCL_ERROR);
			}
			nacker = (CaterpillarNacker*)TclObject::lookup(argv[2]);
			return(TCL_OK);
		}
	} return CaterpillarRx::command(argc, argv);
} //end of command

void CaterpillarAcker::recv(Packet* p, Handler* h)
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
	bool within_bww = (bw_dis < wnd_) ? (true) : (false); //bool to indicate whether the new frame is (or not) within the CaterpillarTx's active window as seen by CaterpillarRx (backward window)
	int oldest_sq_sender = (most_recent_acked - wnd_ + 1 + sn_cnt)%sn_cnt; //the lower bound of CaterpillarTx's active window as seen by CaterpillarRx before receiving seq_num
	int new_oldest_sq_sender = (seq_num - wnd_ + 1 + sn_cnt)%sn_cnt; //the lower bound of CaterpillarTx's active window as seen by CaterpillarRx after receiving seq_num

	int nxt_seq = (last_fwd_sn_ + 1)%sn_cnt; //the next expected seq_num

	bool should_check_for_decoding = true; //bool to indicate whether the new reception should trigger a decoding
  bool decoding_occured = false; //bool to check if the new arrival created a decoding

	if (within_fww){//frame belongs to the forward window

		if(seq_num == nxt_seq){ //frame arrives in order, ack and send frame and finally check if other frames are now in order
			if (debug) printf("CaterpillarRx, recv1 (LF=%d,MR=%d): received pkt %d. SimTime=%.8f.\n", last_fwd_sn_, most_recent_acked, seq_num, Scheduler::instance().clock());
			status[nxt_seq%wnd_] = NONE;
			if (pkt_buf[nxt_seq%wnd_]){ fprintf(stderr, "Error at CaterpillarRx::recv, position of in order frame found non empty.\n"); abort(); }
			pkt_buf[nxt_seq%wnd_] = NULL;
			lost_pkt_buf[nxt_seq] = NULL;

			finish_time = Scheduler::instance().clock();
			delivered_data += ch->size_;
			delivered_pkts++;
      if (last_delay_sample != 0.0) sum_of_delay_jitter = sum_of_delay_jitter + fabs(Scheduler::instance().clock() - ch->ts_arr_ - last_delay_sample);
      last_delay_sample = Scheduler::instance().clock() - ch->ts_arr_;
      if (last_delay_sample > max_delay) max_delay = last_delay_sample;
      if (last_delay_sample < min_delay) min_delay = last_delay_sample;
			sum_of_delay = sum_of_delay + last_delay_sample;

			send(p,h);

			last_fwd_sn_ = (last_fwd_sn_+1)%sn_cnt;
			deliver_frames((wnd_-1), true, h); //check whether other frames are now in order and should be delivered
		} else { //a new frame arrives out of order (either before most_recent_acked or after but within forward window), should be ACKEd and stored in the appropriate position
			if ((status[seq_num%wnd_] != RECEIVED) && (status[seq_num%wnd_] != DECODED)) {
				if (debug) printf("CaterpillarRx, recv2 (LF=%d,MR=%d): received pkt %d. SimTime=%.8f.\n", last_fwd_sn_, most_recent_acked, seq_num, Scheduler::instance().clock());
				status[seq_num%wnd_] = RECEIVED;
				if (pkt_buf[seq_num%wnd_]){ fprintf(stderr, "Error at CaterpillarRx::recv, position of not received and not decoded frame within forward window found non empty.\n"); abort(); }
				pkt_buf[seq_num%wnd_] = p;
				lost_pkt_buf[seq_num] = NULL;
			} else { //the frame has already been received, thus the ACK was lost and nothing needs to be done beyond ACKing the frame and deleting the newly received
				//Sanity-------------------------//
				if ((HDR_CMN(pkt_buf[seq_num%wnd_]))->uid() != (HDR_CMN(p))->uid()){ fprintf(stderr, "Error at CaterpillarRx::handle, received retransmission has not the same uid.\n"); abort(); }
				Packet::free(p);
				//-------------------------------//
			}
		}
		if (fw_dis > fw_width) { //the new frames is beyond the most_recent_acked
			for (int i = (most_recent_acked + 1)%sn_cnt; i != seq_num; i = (i + 1)%sn_cnt){ status[i%wnd_] = MISSING; } //update positions in between with MISSING state
			most_recent_acked = seq_num;
			clean_decoding_matrix(oldest_sq_sender, new_oldest_sq_sender); //remove from the coding structures the frames that are now out of CaterpillarTx's active window
		}

	} else if (within_bww && !within_fww) {//frame belongs to the backward window (i.e., CaterpillarTx's active window), so it is a retransmitted frame due to loss of ACK
		should_check_for_decoding = false;
		//ignore the packet and acknowledge
	} else { //frame is not in the forward and not in the backward window
		//This happens when one or more frames are dropped by CaterpillarTx (when exceeding the retrans limit) so it moves on to transmit new frames.
		//CaterpillarRx is not informed to advance its window and receives brand new frames that may consider as old ones
		//Using 2*wnd_ sequence numbers (= forward window + backward window) is enough for identifying new frames from retransmissions provided that CaterpillarTx advances its window only after an ACK by CaterpillarRx
		//When CaterpillarTx can advance its window and send new frames after dropping a old frame then we need more than 2*wnd_ sequence numbers to identify those the new frames because otherwise those new frames will appear to be in the backward window and will be mistaken by CaterpillarRx as retransmission frames
		if (debug) printf("CaterpillarRx, recv3 (LF=%d,MR=%d): received pkt %d. SimTime=%.8f.\n", last_fwd_sn_, most_recent_acked, seq_num, Scheduler::instance().clock());
		int first_st = ((seq_num - wnd_ + sn_cnt)%sn_cnt - last_fwd_sn_ + sn_cnt)%sn_cnt;
		deliver_frames(first_st, false, h); //deliver frames from last forwarded up to the new lower end of the backward window
		status[seq_num%wnd_] = RECEIVED;
		if (pkt_buf[seq_num%wnd_]){ fprintf(stderr, "Error at CaterpillarRx::recv, position of new frame beyond most recent found non empty.\n"); abort(); }
		pkt_buf[seq_num%wnd_] = p;
		lost_pkt_buf[seq_num] = NULL;
		deliver_frames(wnd_, true, h); //check if it is possible to deliver in order frames
		for (int i = (most_recent_acked + 1)%sn_cnt; i != seq_num; i = (i + 1)%sn_cnt){ status[i%wnd_] = MISSING; } //update positions between most_recent_acked and the new sequence number with MISSING state
		most_recent_acked = seq_num;
		clean_decoding_matrix(oldest_sq_sender, new_oldest_sq_sender); //remove from the coding structures the frames that are now out of CaterpillarTx's active window
	}

  if (should_check_for_decoding){ //check if decoding is now possible due to the reception of the new pkt (if it is a retransmitted one)
    involved_known_packets.insert(seq_num); //add newly received pkt
    known_packets.insert(seq_num); //add newly received pkt
    delete_lost_and_find_associated_coded_in_matrix(seq_num); //delete received pkt from lost and delete coded pkts containing only this lost
    repopulate_seen_packets(); //a change in the decoding matrix has occured so recalculate seen packets
    decoding_occured = decode(h, false); //Check if decoding is now possible
  }

  if (!decoding_occured){ //no need to send new ack since the decode process sent one
    //-----Schedule ACK----------------//
    CaterpillarACKEvent *new_ACKEvent = new CaterpillarACKEvent();
    Event *ack_e;
    new_ACKEvent->type = ACK;
    new_ACKEvent->sn = seq_num;
    new_ACKEvent->uid = pkt_uid;
    new_ACKEvent->coded_ack = create_coded_ack();
    ack_e = (Event *)new_ACKEvent;
    if (delay_ > 0)
      Scheduler::instance().schedule(arq_tx_, ack_e, (delay_ + 8.0*HDR_CMN(new_ACKEvent->coded_ack)->size_/arq_tx_->get_linkbw()));
    //---------------------------------//
  }

} //end of recv


void CaterpillarAcker::deliver_frames(int steps, bool mindgaps, Handler *h)
{

	int count = 0;
	while (count < steps){
		if (((status[((last_fwd_sn_+1)%sn_cnt)%wnd_] != RECEIVED) && (status[((last_fwd_sn_+1)%sn_cnt)%wnd_] != DECODED))&&(mindgaps)) break;

    if ((status[((last_fwd_sn_+1)%sn_cnt)%wnd_] == RECEIVED) || (status[((last_fwd_sn_+1)%sn_cnt)%wnd_] == DECODED) || (pkt_buf[((last_fwd_sn_+1)%sn_cnt)%wnd_])){
      //status may not be RECEIVED or DECODED when Tx drops pkts and Rx does not know, so a new lost pkt overrides the status of an old receieved pkt
			finish_time = Scheduler::instance().clock();
			delivered_data += (HDR_CMN(pkt_buf[((last_fwd_sn_+1)%sn_cnt)%wnd_]))->size_;
			delivered_pkts++;
      if (last_delay_sample != 0) sum_of_delay_jitter = sum_of_delay_jitter + fabs(Scheduler::instance().clock() - (HDR_CMN(pkt_buf[((last_fwd_sn_+1)%sn_cnt)%wnd_]))->ts_arr_ - last_delay_sample);
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

void CaterpillarAcker::print_stats()
{
  printf("\n//------------ STATS FOR CATERPILLAR --------------//\n");
	printf("Start time (sec):\t\t\t%f\n", arq_tx_->get_start_time());
	printf("Finish time (sec):\t\t\t%f\n", finish_time);

	printf("Total number of delivered pkts:\t\t%.0f\n", delivered_pkts);
	printf("Delivered data (in mega bytes):\t\t%.3f\n", delivered_data/1048576);
  if (delivered_pkts == 0) {finish_time = Scheduler::instance().clock();} //hack for the case that deliver_frames is not called
	double throughput = (delivered_data * 8) / (finish_time - arq_tx_->get_start_time());
	printf("Total throughput (Mbps):\t\t%f\n", throughput * 1.0e-6);
  printf("Total pause time (secs):\t\t%f\n", arq_tx_->get_total_pause_time());

	printf("Unique packets sent:\t\t\t%.0f\n", arq_tx_->get_total_packets_sent());
  printf("Coded packets sent:\t\t\t%.0f\n", arq_tx_->get_total_coded_packets_sent());
  double mean = (delivered_pkts == 0) ? (0.0) : (sum_of_delay / delivered_pkts);
	printf("Mean delay (msec):\t\t\t%f\n", mean * 1.0e+3);
  printf("Maximum delay (msec):\t\t\t%f\n", max_delay * 1.0e+3);
  printf("Minimum delay (msec):\t\t\t%f\n", min_delay * 1.0e+3);
  double meanjitter = (delivered_pkts <= 1) ? (0.0) : (sum_of_delay_jitter / (delivered_pkts -1));
	printf("Mean delay jitter (msec):\t\t%f\n", meanjitter * 1.0e+3);
  double avg_rtxs = arq_tx_->get_total_retransmissions() / arq_tx_->get_total_packets_sent();
	printf("Avg num of retransmissions:\t\t%f\n", avg_rtxs);
	printf("Packet loss rate:\t\t\t%f\n", 1 - (delivered_pkts / arq_tx_->get_total_packets_sent()));

	printf("Number of actual decodings:\t\t%.0f\n", num_of_decodings);
  printf("Avg num of decoded pkts per decoding:\t%f\n", (num_of_decodings == 0) ? (0):(avg_pkts_per_decoding / num_of_decodings));
	printf("Average decoding matrix size:\t\t%f\n", ( (num_of_decodings == 0) ? (0) : (avg_dec_matrix_size / num_of_decodings) ));
  printf("Max decoding matrix size:\t\t%.0f\n", max_dec_matrix_size);
  printf("Avg size of known_pkts (inv in dec):\t%f\n", ( (num_of_decodings == 0) ? (0) : (avg_inv_known_pkts_size / num_of_decodings) ));
  printf("Max size of known_pkts (inv in dec):\t%.0f\n", max_inv_known_pkts_size);
  printf("Average size of known_packets:\t\t%f\n", ( (num_of_decodings == 0) ? (0) : (avg_known_pkts_size / num_of_decodings) ));
  printf("Max size of known_packets:\t\t%.0f\n", max_known_pkts_size);
  printf("//-------------------------------------------------//\n");
} //end of print_stats

void CaterpillarAcker:: parse_coded_packet(Packet *cp, Handler* h){ //function that reads a coded packet and update the list with known packets

  set<int> intersect;
  set<int> known_intersect;
  set<int> unionset;
  set<int> old_lost_packets = lost_packets; //keep a copy of lost_packets before adding new ones
  lost_packets.clear();
  set<int> coded_pkt_contents; //used to store the contents of the coded pkt for short term
  vector<int> coded_pkt_contents_vector; //used store the contents of the coded pkt for long term
  bool found_first_lost = false;

	if (debug) {
		int start_pos = (last_fwd_sn_+1)%sn_cnt;
		int stop_pos = (most_recent_acked+1)%sn_cnt;
		if (start_pos != stop_pos){
			printf("CaterpillarRx, parse_coded_packet: The window contains: ");
			do{
				printf("%d(S:%d), ", start_pos, status[start_pos%wnd_]);
				start_pos = (start_pos + 1)%sn_cnt;
			} while (start_pos != stop_pos);
			printf(".\n");
		} else{
			printf("CaterpillarRx, parse_coded_packet: The window contains nothing.\n");
		}
		set<int>:: iterator it;
		printf("CaterpillarRx, parse_coded_packet: The known_pkts are: ");
		for(it = involved_known_packets.begin(); it != involved_known_packets.end(); ++it){
			printf("%d, ", *(it));
		}
		printf(".\n");
		printf("CaterpillarRx, parse_coded_packet: The lost_pkts are: ");
		for(it = lost_packets.begin(); it != lost_packets.end(); ++it){
			printf("%d, ", *(it));
		}
		printf(".\n");
	}

	unsigned char *contents = cp->accessdata();
	int pkt_cnt = -1;
	int readint = -1;
	pkt_cnt = *(contents);
	pkt_cnt = pkt_cnt << 8 | *(contents+1);
	pkt_cnt = pkt_cnt << 8 | *(contents+2);
	pkt_cnt = pkt_cnt << 8 | *(contents+3);
	if (debug) printf("CaterpillarRx, parse_coded_packet: The coded pkt contains: ");

	for(int j = 1; j <= pkt_cnt; j++){
		readint = *(contents+sizeof(int)*j);
		readint = readint << 8 | *(contents+sizeof(int)*j+1);
		readint = readint << 8 | *(contents+sizeof(int)*j+2);
		readint = readint << 8 | *(contents+sizeof(int)*j+3);
    coded_pkt_contents.insert(readint);
    coded_pkt_contents_vector.push_back(readint);
		if (debug) printf("%d", readint);
		if(involved_known_packets.find(readint) == involved_known_packets.end()){ //is not included in already known
			if (debug) printf("+");
			lost_packets.insert(readint);
      if ((!found_first_lost) && (seen_packets.count(readint) == 0)){ //find the first lost (but not seen pkt) contained in the coded
        found_first_lost = true;
        if (old_lost_packets.count(readint) == 0) seen_packets.insert(readint); //if the pkt is a new lost (not already in lost) then it is a "seen" one
      }
		}
		if (debug) printf(", ");
	}
	if (debug) printf(".\n");

  set_intersection(old_lost_packets.begin(),old_lost_packets.end(),coded_pkt_contents.begin(),coded_pkt_contents.end(), std::inserter(intersect,intersect.begin())); //take the intersection between coded_pkt_contents and old_lost_packets

  if (intersect.size() == 0) {
    //the new coded packet contains none of the old_lost_packets->the window of Tx has moved on, no further coded pkts containing those lost will be received so clean lost as well as coded_packets because decoding will not be possible
    old_lost_packets.clear();
    coded_packets.clear();
    //intersect.clear();
  }

  if (lost_packets.size() != 0) { //at least one lost pkt is contained in the coded packet so store it

    set_union(lost_packets.begin(),lost_packets.end(),old_lost_packets.begin(),old_lost_packets.end(), std::inserter(unionset,unionset.begin())); //take the union between coded_pkt_contents and lost_packets
    lost_packets = unionset; unionset.clear(); old_lost_packets.clear();
    //set<int>::iterator mmiter;
    //printf("parse, adding coded pkt. "); printf("Lost pkts are:"); for (mmiter = lost_packets.begin(); mmiter != lost_packets.end(); ++mmiter ){ printf(" %d", *(mmiter)); } printf("\n"); printf("Known pkts are:"); for (mmiter = involved_known_packets.begin(); mmiter != involved_known_packets.end(); ++mmiter ){ printf(" %d", *(mmiter)); } printf("\n");
    coded_packets.push_back(coded_pkt_contents_vector); //store new coded pkt
  }

	decode(h,true);

  if ((intersect.size() == 0) || (coded_packets.size() == 0)){
    //If the received coded pkt reset the lost_packets and coded_packets polls or if a decoding occured we need to reset involved_known_packets
    //to the known packets existing in the contents of the received coded pkt
    //All future coded pkts will contain involved_known_packets with greater seq_nums
    set_intersection(involved_known_packets.begin(),involved_known_packets.end(),coded_pkt_contents.begin(),coded_pkt_contents.end(), std::inserter(known_intersect,known_intersect.begin())); //take the intersection between coded_pkt_contents and involved_known_packets
    involved_known_packets.clear(); involved_known_packets = known_intersect; known_intersect.clear();
  }
  coded_pkt_contents.clear(); //delete contents of coded pkt, no more needed
  coded_pkt_contents_vector.clear();
  intersect.clear();
	Packet::free(cp);  //free coded packet, no more needed

} //end of parse_coded_packet

bool CaterpillarAcker::decode(Handler* h, bool afterCodedreception){

  if (debug) printf("CaterpillarRx, decode : Examining decoding (LPKTS=%d,RCODED=%d).\n", (int)lost_packets.size(), (int)coded_packets.size());

  bool decoding_is_possible = false;
	if (((int)lost_packets.size() <= (int)coded_packets.size()) && (lost_packets.size() > 0)){
			decoding_is_possible = true;
			avg_dec_matrix_size = avg_dec_matrix_size + (int) lost_packets.size() + (int) involved_known_packets.size();
      if (lost_packets.size() + involved_known_packets.size() > max_dec_matrix_size) max_dec_matrix_size = (int) lost_packets.size() + (int) involved_known_packets.size();
      avg_inv_known_pkts_size = avg_inv_known_pkts_size + (int) involved_known_packets.size();
      avg_known_pkts_size = avg_known_pkts_size + (int) known_packets.size();
      if (involved_known_packets.size() > max_inv_known_pkts_size) max_inv_known_pkts_size = (int) involved_known_packets.size();
      if (known_packets.size() > max_known_pkts_size) max_known_pkts_size = (int) known_packets.size();
			num_of_decodings ++;
      avg_pkts_per_decoding = avg_pkts_per_decoding + lost_packets.size();
	}
	if ((!decoding_is_possible) && (!afterCodedreception)) return false; //no need to decode and no need to send ack


  if (decoding_is_possible){
  	int fw_dis, fw_width, bw_dis, oldest_sq_sender, new_oldest_sq_sender, nxt_seq, first_st;
  	bool within_fww, within_bww;

  	set<int>:: iterator it;
  	for(it = lost_packets.begin(); it != lost_packets.end(); ++it){

  		int lost_sn = *it;
  		involved_known_packets.insert(*it); //move decoded frame into known ones
      known_packets.insert(*it); //move decoded frame into known ones
  		if (debug) printf("CaterpillarRx, decode: decoding pkt %d.\n", lost_sn);
  		//------------------------------------------//
  		//if ((status[lost_sn%wnd_] != MISSING)) { fprintf(stderr, "Error at CaterpillarRx::decode, decoded pkt cannot be found or has a wrong status.\n"); abort();}
  		//------------------------------------------//

  		fw_dis = (lost_sn - last_fwd_sn_ + sn_cnt)%sn_cnt;
  		fw_width = (most_recent_acked - last_fwd_sn_ + sn_cnt)%sn_cnt;
  		within_fww = ((fw_dis <= wnd_) && (fw_dis > 0)) ? (true) : (false);
  		bw_dis = (most_recent_acked - lost_sn + sn_cnt)%sn_cnt;
  		within_bww = (bw_dis < wnd_) ? (true) : (false);
  		oldest_sq_sender = (most_recent_acked - wnd_ + 1 + sn_cnt)%sn_cnt;
  		new_oldest_sq_sender = (lost_sn - wnd_ + 1 + sn_cnt)%sn_cnt;
  		nxt_seq = (last_fwd_sn_ + 1)%sn_cnt;

  		if (within_fww){ //decoded frame is in the forward window
  			if(lost_sn == nxt_seq){ //decoded frame is the next expected
  				status[lost_sn%wnd_] = NONE;
  				pkt_buf[lost_sn%wnd_] = lost_pkt_buf[lost_sn]; //copy frame from lost_pkt_buf
  				lost_pkt_buf[lost_sn] = NULL;
  				finish_time = Scheduler::instance().clock();
  				delivered_pkts++;
  				delivered_data += HDR_CMN(pkt_buf[lost_sn%wnd_])->size_;
          if (last_delay_sample !=0) sum_of_delay_jitter = sum_of_delay_jitter + fabs(Scheduler::instance().clock() - HDR_CMN(pkt_buf[lost_sn%wnd_])->ts_arr_ - last_delay_sample);
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
  				clean_decoding_matrix(oldest_sq_sender, new_oldest_sq_sender); //remove from the coding structures the frames that are now outside the backward window (i.e., CaterpillarTx's active window)
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
  			clean_decoding_matrix(oldest_sq_sender, new_oldest_sq_sender); //remove from the coding structures the frames that are now outside the backward window (i.e., CaterpillarTx's active window)
  		}
  	} //end for

    lost_packets.clear(); //clean because frames were decoded
    seen_packets.clear(); //clean because all frames were decoded
    coded_packets.clear(); //coded frames no more needed because decoding completed

  } //end if decoding_is_possible

	//Sent a cumulative ack =======================================//
	Event *ack_e;
	CaterpillarACKEvent *new_ACKEvent = new CaterpillarACKEvent();
  new_ACKEvent->type = ACK;
	new_ACKEvent->sn = -1;
	new_ACKEvent->uid = -1;
	new_ACKEvent->coded_ack = create_coded_ack();
	ack_e = (Event *)new_ACKEvent;
	if (delay_ > 0)
		Scheduler::instance().schedule(arq_tx_, ack_e, (delay_ + 8*HDR_CMN(new_ACKEvent->coded_ack)->size_ /arq_tx_->get_linkbw()));

	if (debug) printf("CaterpillarRx, decode: ACK sent.\n");
	//=============================================================//

  return decoding_is_possible;

} //end of decode

Packet* CaterpillarAcker::create_coded_ack(){

	Packet *cpkt = Packet::alloc();
	hdr_cmn *ch2 = HDR_CMN(cpkt);
	ch2->opt_num_forwards_ = -10001; //coded packet ACK
	unsigned char *buffer = new unsigned char[sizeof(int)*((int)(known_packets.size()) + (int)(lost_packets.size()) + 1)]; //4 bytes for each decoded frame plus a byte for the counter

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
  for (itr = lost_packets.begin(); itr != lost_packets.end(); itr++)
	{
    int sent_sq_num = (*itr - 10000); //this corresponds to a negative ack
    if (seen_packets.count(*itr) != 0) sent_sq_num = *itr; //seen packets are possitively acknowledged
    *(buffer+sizeof(int)*(cnt_pkts+1)) =  (sent_sq_num >> 24) & 0xFF;
		*(buffer+sizeof(int)*(cnt_pkts+1)+1) = (sent_sq_num >> 16) & 0xFF;
		*(buffer+sizeof(int)*(cnt_pkts+1)+2) = (sent_sq_num >> 8) & 0xFF;
		*(buffer+sizeof(int)*(cnt_pkts+1)+3) = sent_sq_num & 0xFF;
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


void CaterpillarAcker::clean_decoding_matrix(int from, int to)
{
  int runner_ = from;
	do {
		delete_known_from_matrix_strict(runner_); //delete known pkt only if it is not contained in any stored coded one
    delete_lost_and_associated_coded_from_matrix(runner_); //delete lost pkt and also all coded containing this packet
    repopulate_seen_packets(); //a change in decoding matrix has occured, so recalculater seen packets
		runner_ = (runner_ + 1)%sn_cnt;
	} while (runner_ != to);
} //end of clean_decoding_matrix

void CaterpillarAcker::delete_lost_and_associated_coded_from_matrix(int pkt_to_remove)
{
  //Should delete a lost_packet that is now out of the sender's coding window, so no more subsequent coded pkts will contain it
  //In doing so we also need to delete all coded pkts containing the deleted lost pkt because they will no be usefull for decodings
  //We do not need to update involved_known_packets: we could delete involved_known_packets that are only contained in the deleted coded ones but
  //the impact in reducing the size of involved_known_packets will be minimal and processing overhead high
  vector<vector<int> >::iterator itcodedpkts;
  vector<vector<int> > temp_coded;
  set<int>::iterator mmiter;
  set<int> intersect;

  int deleted_lost_pkt = lost_packets.erase(pkt_to_remove);
  if (deleted_lost_pkt > 0){
    //printf("Lost pkts are:"); for (mmiter = lost_packets.begin(); mmiter != lost_packets.end(); ++mmiter ){ printf(" %d", *(mmiter)); } printf("\n"); printf("Known pkts are:"); for (mmiter = involved_known_packets.begin(); mmiter != involved_known_packets.end(); ++mmiter ){ printf(" %d", *(mmiter)); } printf("\n"); printf("Num of coded pkts = %d. Lost pkt deleted is %d.\n", (int)coded_packets.size(), pkt_to_remove);
    for (itcodedpkts = coded_packets.begin(); itcodedpkts != coded_packets.end(); ++itcodedpkts){
        //printf("Coded pkt: "); for (mmiter = (*itcodedpkts).begin(); mmiter != (*itcodedpkts).end(); ++mmiter ){ printf(" %d", *(mmiter)); } printf(".\n");
        set<int> temp_set((*itcodedpkts).begin(), (*itcodedpkts).end());
        int erase_coded_cnt = temp_set.count(pkt_to_remove); //if deleted lost is in coded pkt erase_coded_cnt > 0
        if (erase_coded_cnt > 0){ //coded pkt contains the deleted lost one
          //printf("Coded pkt is deleted.\n");
        } else { // this only serves as a diagnostic: every other coded pkt should contain at least one lost packet
          set_intersection(lost_packets.begin(),lost_packets.end(),temp_set.begin(),temp_set.end(), std::inserter(intersect,intersect.begin()));
          if (intersect.size() == 0) {fprintf(stderr, "Error at CaterpillarRx::delete_lost_and_associated_coded_from_matrix, found coded pkt that does contain any lost pkt.\n"); abort();}
          intersect.clear();
          temp_coded.push_back((*itcodedpkts));
        }
        temp_set.clear();
    } // done cleaning coded_pkts
    coded_packets.clear();
    coded_packets = temp_coded;
    temp_coded.clear();
    //printf("The remaining coded_pkts are %d.\n", (int)coded_packets.size());
  } //done with this deleted lost packet
} //end of delete_lost_and_associated_coded_from_matrix


void CaterpillarAcker::delete_lost_and_find_associated_coded_in_matrix(int pkt_to_remove)
{
  //Should delete a lost_packet that turned into known (through a retransmission)
  //In doing so we also need to delete all coded pkts containing only this lost_packet because they are no more usefull in decodings
  //We do not need to update involved_known_packets: we could delete involved_known_packets that are only contained in the deleted coded ones but
  //the impact in reducing the size of involved_known_packets will be minimal and processing overhead high
  vector<vector<int> >::iterator itcodedpkts;
  vector<vector<int> > temp_coded;
  set<int>::iterator mmiter;
  set<int> intersect;

  int deleted_lost_pkt = lost_packets.erase(pkt_to_remove);
  if (deleted_lost_pkt > 0){
    //printf("Lost pkts are:"); for (mmiter = lost_packets.begin(); mmiter != lost_packets.end(); ++mmiter ){ printf(" %d", *(mmiter)); } printf("\n"); printf("Known pkts are:"); for (mmiter = involved_known_packets.begin(); mmiter != involved_known_packets.end(); ++mmiter ){ printf(" %d", *(mmiter)); } printf("\n"); printf("Num of coded pkts = %d. Lost pkt deleted is %d.\n", (int)coded_packets.size(), pkt_to_remove);
    for (itcodedpkts = coded_packets.begin(); itcodedpkts != coded_packets.end(); ++itcodedpkts){
        //printf("Coded pkt: "); for (mmiter = (*itcodedpkts).begin(); mmiter != (*itcodedpkts).end(); ++mmiter ){ printf(" %d", *(mmiter)); } printf(".\n");
        set<int> temp_set((*itcodedpkts).begin(), (*itcodedpkts).end());
        int exists_in_coded_cnt = temp_set.count(pkt_to_remove); //if deleted lost is in coded pkt exists_in_coded_cnt > 0
        if (exists_in_coded_cnt != 0){ //coded pkt contains the lost pkt turned to known, so check if coded contains at least one other lost
          set_intersection(lost_packets.begin(),lost_packets.end(),temp_set.begin(),temp_set.end(), std::inserter(intersect,intersect.begin()));
          if (intersect.size() == 0) { //coded pkt does not contain at least one other lost pkt
            //printf("Coded pkt is deleted.\n");
          } else { //coded pkt contains at least one other lost pkt
            temp_coded.push_back((*itcodedpkts));
          }
          intersect.clear();
        } else { //coded pkt does not contain the deleted lost pkt
          temp_coded.push_back((*itcodedpkts));
        }
        temp_set.clear();
    } // done cleaning coded_pkts
    coded_packets.clear();
    coded_packets = temp_coded;
    temp_coded.clear();
    //printf("The remaining coded_pkts are %d.\n", (int)coded_packets.size());
  } //done with this deleted lost packet
} //end of delete_lost_and_find_associated_coded_in_matrix

void CaterpillarAcker::delete_known_from_matrix(int pkt_to_remove){
  //Should delete a known_packet that is now out of the sender's coding window, so no more subsequent coded pkts will contain it
  //The deletion will take place only if this packet is not involved in a stored coded packet, in which case it is needed for decoding
  vector<vector<int> >::iterator itcodedpkts;
  known_packets.erase(pkt_to_remove);
  int should_delete = 0;
  for (itcodedpkts = coded_packets.begin(); itcodedpkts != coded_packets.end(); ++itcodedpkts){
    set<int> temp_set((*itcodedpkts).begin(), (*itcodedpkts).end());
    should_delete = temp_set.count(pkt_to_remove);
    temp_set.clear();
    if (should_delete > 0) return;
  }
  involved_known_packets.erase(pkt_to_remove);
} //end of delete_known_from_matrix

void CaterpillarAcker::delete_known_from_matrix_strict(int pkt_to_remove){
  //Should delete a known_packet that is now out of the sender's coding window
  //Should also delete affected coded packets as well as lost ones
  vector<vector<int> >::iterator itcodedpkts;
  vector<vector<int> > temp_coded;
  set <int> affected_pkts;
  set <int> not_affected_pkts;
  set <int> helper;
  set <int> ::iterator iterhelper;

  known_packets.erase(pkt_to_remove);
  int erased_one = involved_known_packets.erase(pkt_to_remove);
  if (erased_one){
    for (itcodedpkts = coded_packets.begin(); itcodedpkts != coded_packets.end(); ++itcodedpkts){
      set<int> temp_set((*itcodedpkts).begin(), (*itcodedpkts).end());
      if (temp_set.count(pkt_to_remove)){
        set_union(affected_pkts.begin(),affected_pkts.end(),temp_set.begin(),temp_set.end(), std::inserter(helper,helper.begin()));
        affected_pkts.clear();
        affected_pkts = helper;
        helper.clear();
      } else {
        set_union(not_affected_pkts.begin(),not_affected_pkts.end(),temp_set.begin(),temp_set.end(), std::inserter(helper,helper.begin()));
        not_affected_pkts.clear();
        not_affected_pkts = helper;
        helper.clear();
        temp_coded.push_back((*itcodedpkts));
      }
      temp_set.clear();
    }
    coded_packets.clear();
    coded_packets = temp_coded;
    temp_coded.clear();

    helper.clear();
    set_intersection(lost_packets.begin(),lost_packets.end(),affected_pkts.begin(),affected_pkts.end(), std::inserter(helper,helper.begin()));
    affected_pkts.clear();
    affected_pkts = helper;
    helper.clear();
    set_intersection(lost_packets.begin(),lost_packets.end(),not_affected_pkts.begin(),not_affected_pkts.end(), std::inserter(helper,helper.begin()));
    not_affected_pkts.clear();
    not_affected_pkts = helper;
    helper.clear();
    set_difference(affected_pkts.begin(),affected_pkts.end(),not_affected_pkts.begin(),not_affected_pkts.end(), std::inserter(helper,helper.begin()));
    for (iterhelper = helper.begin(); iterhelper != helper.end(); ++iterhelper){
      lost_packets.erase(*iterhelper);
    }
    affected_pkts.clear();
    not_affected_pkts.clear();
    helper.clear();
  }

} //end of delete_known_from_matrix

void CaterpillarAcker::repopulate_seen_packets(){

  set<int> repopulatelost;
  vector<vector<int> >::iterator itcodedpkts;
  vector<int>::iterator codedcontentsit;
  bool found_first_lost = false;

  seen_packets.clear();
  for (itcodedpkts = coded_packets.begin(); itcodedpkts != coded_packets.end(); ++itcodedpkts){ //for each coded pkt (each row in decoding matrix) - order of coded pkts is important
    for (codedcontentsit = (*itcodedpkts).begin(); codedcontentsit != (*itcodedpkts).end(); ++codedcontentsit){ //for each pkt contained in the coded one - order of contents is important
      if (involved_known_packets.count(*(codedcontentsit)) == 0){ //if native is not known
        if ((!found_first_lost) && (seen_packets.count(*(codedcontentsit)) == 0)){ //find the first lost pkt that is not seen
          found_first_lost = true;
          if (repopulatelost.count(*(codedcontentsit)) == 0) seen_packets.insert(*(codedcontentsit)); //if the first pkt is a new lost (not already in list of lost)
        }
        repopulatelost.insert(*(codedcontentsit)); //add found lost pkt in list of lost
      }
    }
    found_first_lost = false;
  }
  repopulatelost.clear();


} //end of populate_seen_packets

void CaterpillarAcker::log_lost_pkt(Packet *p){

	hdr_cmn *ch = HDR_CMN(p);
  int seq_num = ch->opt_num_forwards_;
  int rpktuid = ch->uid();
  if (ch->opt_num_forwards_ < 0){ seq_num = seq_num + 10000; rpktuid = ch->aomdv_salvage_count_; }


	if (debug) printf("CaterpillarRx, log_lost_pkt: Pkt %d is lost.\n", seq_num);
	if (debug) printf("CaterpillarRx, log_lost_pkt: Logging pkt %d. Position %d.\n", seq_num, seq_num%wnd_);

	if (lost_pkt_buf[seq_num]){
		hdr_cmn *chold = HDR_CMN(lost_pkt_buf[seq_num]);
		if (rpktuid != chold->uid()){ //this is an old frame, so delete it
      if (ch->opt_num_forwards_ < 0) { fprintf(stderr, "Error at CaterpillarAcker::log_lost_pkt, coded received instead of native but native is missing.\n"); abort(); } //a coded pkt cannot find a pkt that is not the corresponding native
			Packet::free (lost_pkt_buf[seq_num]);
			lost_pkt_buf[seq_num] = p;
		} else { //this is a retransmitted frame, so ignore
			Packet::free (p);
		}
	} else {
		if (ch->opt_num_forwards_ >= 0) lost_pkt_buf[seq_num] = p; //store only if this is a native pkt because otherwise the native pkt has already been delivered to the upper layer
	}

} // end of log_lost_pkt

CaterpillarNacker::CaterpillarNacker()
{
	debug = false;
	acker = NULL;
} //end of CaterpillarNacker

int CaterpillarNacker::command(int argc, const char*const* argv)
{
	Tcl& tcl = Tcl::instance();
	if (argc == 3) {
		if (strcmp(argv[1], "setup-CaterpillarAcker") == 0) {
			if (*argv[2] == '0') {
				tcl.resultf("Cannot attach NULL CaterpillarAcker\n");
				return(TCL_ERROR);
			}
			acker = (CaterpillarAcker*)TclObject::lookup(argv[2]);
			return(TCL_OK);
		}
	} return CaterpillarRx::command(argc, argv);
} //end of command


void CaterpillarNacker::recv(Packet* p, Handler* h)
{

	hdr_cmn *ch = HDR_CMN(p);
	if(ch->opt_num_forwards_ == -10000){ //coded packet lost, do nothing (but free packet memory)
		Packet::free(p);
		return;
	}

	acker->log_lost_pkt(p);

} //end of recv

//--------------------------------------------------------------------------------------------//
//--------------------------------------------------------------------------------------------//
