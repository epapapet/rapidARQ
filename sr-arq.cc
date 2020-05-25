#include "sr-arq.h"
#include "packet.h"

static class SRARQTxClass: public TclClass {
 public:
	SRARQTxClass() : TclClass("SRARQTx") {}
	TclObject* create(int, const char*const*) {
		return (new SRARQTx);
	}
} class_SRARQ_tx;

static class SRARQAckerClass: public TclClass {
 public:
	SRARQAckerClass() : TclClass("SRARQAcker") {}
	TclObject* create(int, const char*const*) {
		return (new SRARQAcker);
	}
} class_SRARQ_acker;

static class SRARQNackerClass: public TclClass {
 public:
	SRARQNackerClass() : TclClass("SRARQNacker") {}
	TclObject* create(int, const char*const*) {
		return (new SRARQNacker);
	}
} class_SRARQ_nacker;

void SRARQHandler::handle(Event* e)
{
	arq_tx_.resume();
}


//-------------------------------------------SRARQRx--------------------------------------------//
//--------------------------------------------------------------------------------------------//
SRARQTx::SRARQTx() : arqh_(*this)
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
	num_pending_retrans_ = 0; //number of retransmissions scheduled and pending in SRARQTx

	lnk_bw_ = 10000000;
	bind("lnk_bw_", &lnk_bw_);
	lnk_delay_ = 0.03;
	bind("lnk_delay_", &lnk_delay_);
	app_pkt_Size_ = 8000;
	bind("app_pkt_Size_", &app_pkt_Size_);
  timeout_ = 0;

  ranvar_ = NULL; // random number generator used for simulating the loss rate for ACKs
	err_rate = 0.0; // the error rate for ACks

	debug = false; //used to enable printing of diagnostic messages

	start_time = -1; //time when 1st packet arrived at SRARQTx::recv
	packets_sent = 0; //unique packets sent
  pkt_rtxs = 0; //the total number of pkt retransmissions
  total_pause_time = 0; //the total time the sender spend paused because the window reached its limit
  start_of_pause_period = -1; //the start of a pause period, used to calculate the total_pause_time
} //end of constructor

int SRARQTx::command(int argc, const char*const* argv)
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
  } else if (argc == 4) {
		if (strcmp(argv[1], "setup-wnd") == 0) {
			if (*argv[2] == '0') {
				tcl.resultf("Cannot setup NULL wnd\n");
				return(TCL_ERROR);
			}
			wnd_ = atoi(argv[2]);
			sn_cnt = 4 * wnd_; //although 2*wnd_ is enough, we use 4*wnd_ or more to tackle the case that SRARQTx drops packets and advances its window without SRARQRx knowing
			pkt_buf = new Packet* [wnd_]; //buffer for storing pending frames
			status = new SRARQARQStatus[wnd_]; //the status for each frame: IDLE,SENT,ACKED,RTX,RTXPRE,DROP
			num_rtxs = new int[wnd_]; //the number of retransmissions executed for each frame
			pkt_uids = new int[wnd_]; //buffer for storing the uids of pending frames: used only for diagnostic purposes
			for(int i=0; i<wnd_; i++){ pkt_buf[i] = NULL; status[i] = IDLE; num_rtxs[i] = 0; pkt_uids[i]=-1; }
      timeout_events = new SRARQTimeoutEvent* [sn_cnt]; //buffer for storing pointers to timeout events
      for(int i=0; i<sn_cnt; i++){ timeout_events[i] = NULL; }
      timeout_ = atof(argv[3]);
      double rtt_time = 2*lnk_delay_ + 8.0*(app_pkt_Size_ + 1)/lnk_bw_;
      if (timeout_ == 0){ timeout_ = rtt_time; }
      if (timeout_ < 0) { timeout_ = -(1.0/timeout_)*rtt_time; }
      if (timeout_ < rtt_time) {
        tcl.resultf("Timeout is too small.\n");
				return(TCL_ERROR);
      }
			return(TCL_OK);
		}
	} return Connector::command(argc, argv);
} //end of command


void SRARQTx::recv(Packet* p, Handler* h)
{

	//This procedure is invoked by the queue_ (i.e., output queue) to deliver a message to SRARQTx
	//The check whether the current window exceeds or not wnd_ has already be done at this point
  if (handler_ == 0) printf("Simulation time: %10.4f", Scheduler::instance().clock());
  if (handler_ != 0) printf("\b\b\b\b\b\b\b\b\b\b%10.4f", Scheduler::instance().clock());

	if(last_acked_sq_ == -1 && most_recent_sq_ == 0 && start_time == -1){ //first packet to be received will trigger the clock
		start_time = Scheduler::instance().clock();
	}

	//Sanity checks---------//
	if (&arqh_==0) {
		fprintf(stderr, "Error at SRARQTx::recv, Cannot transmit when &arqh_ is Null.\n");
		abort();
	}
	if ((status[most_recent_sq_%wnd_] != IDLE) && ((HDR_CMN(p))-> opt_num_forwards_ >= 0)) {
		fprintf(stderr, "Error at SRARQTx::recv, storing position should be in IDLE mode.\n");
		abort();
	}
	//---------------------//
	//Only in initialization-------//
	if (h != 0) handler_ = h;
	//----------------------------//

  if ((blocked_) && ((HDR_CMN(p))-> opt_num_forwards_ >= 0)) {
    if (pending) { fprintf(stderr, "Error at SRARQsTx::recv, Tx should not have a pending frame when recv is called.\n"); abort(); }
    pending = p; return;
  }

	hdr_cmn *ch = HDR_CMN(p);

	ch-> opt_num_forwards_ = most_recent_sq_;
	if (pkt_buf[most_recent_sq_%wnd_]) {fprintf(stderr, "Error at SRARQTx::recv, storing position found non empty.\n"); abort();} //pkt_buf is cleared by reset_lastacked
	pkt_buf[most_recent_sq_%wnd_] = p;
	pkt_uids[most_recent_sq_%wnd_] = ch->uid();
	status[most_recent_sq_%wnd_] = SENT;
	num_rtxs[most_recent_sq_%wnd_] = 0;

  SRARQTimeoutEvent *etout = new SRARQTimeoutEvent();
  etout->type = TIMEOUT;
  etout->sn = most_recent_sq_;
  etout->uid = ch->uid();
  etout->expirationTime = Scheduler::instance().clock() + timeout_;
  etout->isCancelled = false;
  if (timeout_events[most_recent_sq_]) { fprintf(stderr, "Error at SRARQTx::recv, found non-empty position of timeout event.\n"); abort(); }
  timeout_events[most_recent_sq_] = etout;
  Scheduler::instance().schedule(this, etout, timeout_);

  ch->ts_arr_ = Scheduler::instance().clock(); //used to calculate delay, retransmitted pkts are not sent through recv(), so this is a new pkt

	most_recent_sq_ = (most_recent_sq_+1)%sn_cnt;

  if (debug) printf("SRARQTx, recv: Sending pkt %d. SimTime=%.8f.\n", ch-> opt_num_forwards_, Scheduler::instance().clock());

  blocked_ = 1;
  //In general, when sending a frame we create a copy (even in retransmissions) to decouple SRARQTx and SRARQRx
  //SRARQTx is responsible for freeing frames sent from queue while SRARQRx delivers frames to upper layers and frees frames not eventually delivered
  packets_sent += 1;
  Packet *pnew = p->copy();
  send(pnew,&arqh_);


} //end of recv

void SRARQTx::parse_ack(int rcv_sn, int rcv_uid)
{

	int fw_dis = (rcv_sn - last_acked_sq_ + sn_cnt)%sn_cnt;
	int fw_width = (most_recent_sq_ - last_acked_sq_ + sn_cnt)%sn_cnt;
	bool within_fww = ((fw_dis < fw_width) && (fw_dis > 0)) ? (true) : (false);
	if (!within_fww) return; //ARTx may receive multiple ACKs per frame due to coded frames, so ignore ACKs out of the active window

	//Sanity checks--------//
	if (status[rcv_sn%wnd_] == IDLE) { fprintf(stderr,"Error at SRARQTx::ack, an ACK is received when the status is not SENT. It is %d.\n", status[rcv_sn%wnd_]); abort(); }
	if (handler_ == 0) { fprintf(stderr,"Error at SRARQTx::ack, handler_ is null.\n"); abort(); }
	//--------------------//

	if (status[rcv_sn%wnd_] != ACKED){
		if (debug) printf("SRARQTx ack: Pkt %d with status %d is ACKED. The news status is %d. Pending retrans=%d. LA(MR) before is %d(%d). SimTime=%.8f.\n ", rcv_sn, status[rcv_sn%wnd_], ACKED, num_pending_retrans_, last_acked_sq_, most_recent_sq_, Scheduler::instance().clock());

		if((status[rcv_sn%wnd_] == RTX) || (status[rcv_sn%wnd_] == RTXPRE)) num_pending_retrans_--; //reduce the number of scheduled retransmissions
		status[rcv_sn%wnd_] = ACKED;
    if (timeout_events[rcv_sn]) (timeout_events[rcv_sn])->expirationTime = -1.0; //the timeout event may be NULL if pkt is DROPED and a late decode ACK is received
    if (timeout_events[rcv_sn]) (timeout_events[rcv_sn])->isCancelled = true;
		if (rcv_sn%wnd_ == ((last_acked_sq_ + 1)%sn_cnt)%wnd_) {
      reset_lastacked();  //acked frame is next in order, so check whether the active window should advance
      if (!blocked_) handler_->handle(0); //ask queue_ to deliver next packet
    }
	}

} //end of parse_ack


void SRARQTx::parse_nack(int rcv_sn)
{

	int fw_dis = (rcv_sn - last_acked_sq_ + sn_cnt)%sn_cnt;
	int fw_width = (most_recent_sq_ - last_acked_sq_ + sn_cnt)%sn_cnt;
	bool within_fww = ((fw_dis < fw_width) && (fw_dis > 0)) ? (true) : (false);
	if (!within_fww) return; //ARTx may receive multiple NACKs per frame due to coded frames, so ignore ACKs out of the active window

  if (Scheduler::instance().clock() - ((timeout_events[rcv_sn])->expirationTime - timeout_) < timeout_) return; // time elapsed since last retransmission is not sufficient for the ack to arrive, so avoid sending again a retransmission

	//Sanity checks--------//
	if (status[rcv_sn%wnd_] == IDLE) { fprintf(stderr,"Error at SRARQTx::parse_nack when the status is not SENT.\n"); abort(); }
	if (handler_ == 0) { fprintf(stderr,"Error at SRARQTx::parse_nack, handler_ is null\n"); abort(); }
	if (&arqh_==0) { fprintf(stderr, "Error at SRARQTx::parse_nack, Cannot transmit when &arqh_ is Null.\n"); abort(); }
	//--------------------//

	if (status[rcv_sn%wnd_] != SENT) { //it is possible to receive an NACK for a frame acked through a cumulative ACK (i.e., coded frame) when the cumulative ACK arives after the retransmission
		if (debug) printf("SRARQTx, parse_nack for pkt %d with status %d. Ignoring. SimTime=%.8f.\n", rcv_sn, status[rcv_sn%wnd_], Scheduler::instance().clock());
		return;
	} else {
		if (pkt_buf[rcv_sn%wnd_] == NULL) { fprintf(stderr,"Error at SRARQTx::parse_nack but pkt is not found.\n"); abort(); }
		if (debug) printf("SRARQTx, parse_nack: timeout_expired for pkt %d with status %d. SimTime=%.8f.\n", rcv_sn, status[rcv_sn%wnd_], Scheduler::instance().clock());
	}

	if ( num_rtxs[rcv_sn%wnd_] < retry_limit_) { //packet shoud be retransmitted
		status[rcv_sn%wnd_] = RTXPRE;
		if (!blocked_){ //if SRARQTx is available go on with retransmision
			if (debug) printf("SRARQTx, parse_nack: Sending pkt %d. SimTime=%.8f.\n", rcv_sn, Scheduler::instance().clock());
			blocked_ = 1;
			num_rtxs[rcv_sn%wnd_]++;
			status[rcv_sn%wnd_] = SENT;
			Packet *newp = pkt_buf[rcv_sn%wnd_]->copy();
      timeout_events[rcv_sn]->expirationTime = Scheduler::instance().clock() + timeout_;
      timeout_events[rcv_sn]->isCancelled = true;
      //Scheduler::instance().schedule(this, timeout_events[rcv_sn], timeout_);
      pkt_rtxs++;

			send(newp,&arqh_);
		} else {
			num_pending_retrans_++;
		}
	} else {//packet should be dropped
		if (debug) printf("SRARQTx, nack: Dropping pkt %d. SimTime=%.8f.\n", rcv_sn, Scheduler::instance().clock());
		status[rcv_sn%wnd_] = DROP;
    timeout_events[rcv_sn]->expirationTime = -1.0; //do not delete event because it is already in the scheduler
    timeout_events[rcv_sn]->isCancelled = true;
    if (rcv_sn%wnd_ == ((last_acked_sq_ + 1)%sn_cnt)%wnd_) {
      reset_lastacked(); //droped frame is next in order so check whether the active window should advance
      if (!blocked_) handler_->handle(0); //ask queue_ to deliver next packet
    }
	}

}//parse_nack

void SRARQTx::timeout_expired(int rcv_sn, int rcv_uid)
{

	int fw_dis = (rcv_sn - last_acked_sq_ + sn_cnt)%sn_cnt;
	int fw_width = (most_recent_sq_ - last_acked_sq_ + sn_cnt)%sn_cnt;
	bool within_fww = ((fw_dis < fw_width) && (fw_dis > 0)) ? (true) : (false);
	if (!within_fww) return; //ARTx may receive multiple NACKs per frame due to coded frames, so ignore ACKs out of the active window

	//Sanity checks--------//
	if (status[rcv_sn%wnd_] == IDLE) { fprintf(stderr,"Error at SRARQTx::timeout_expired when the status is not SENT.\n"); abort(); }
	if (handler_ == 0) { fprintf(stderr,"Error at SRARQTx::timeout_expired, handler_ is null\n"); abort(); }
	if (&arqh_==0) { fprintf(stderr, "Error at SRARQTx::timeout_expired, Cannot transmit when &arqh_ is Null.\n"); abort(); }
	//--------------------//

	if (status[rcv_sn%wnd_] != SENT) { //it is possible to receive an NACK for a frame acked through a cumulative ACK (i.e., coded frame) when the cumulative ACK arives after the retransmission
		if (debug) printf("SRARQTx, timeout_expired for pkt %d with status %d. Ignoring. SimTime=%.8f.\n", rcv_sn, status[rcv_sn%wnd_], Scheduler::instance().clock());
		return;
	} else {
		if (pkt_buf[rcv_sn%wnd_] == NULL) { fprintf(stderr,"Error at SRARQTx::timeout_expired but pkt is not found.\n"); abort(); }
		if (debug) printf("SRARQTx, timeout_expired: timeout_expired for pkt %d with status %d. SimTime=%.8f.\n", rcv_sn, status[rcv_sn%wnd_], Scheduler::instance().clock());
	}

	if ( num_rtxs[rcv_sn%wnd_] < retry_limit_) { //packet shoud be retransmitted
		status[rcv_sn%wnd_] = RTX;
		if (!blocked_){ //if SRARQTx is available go on with retransmision
			if (debug) printf("SRARQTx, timeout_expired: Sending pkt %d. SimTime=%.8f.\n", rcv_sn, Scheduler::instance().clock());
			blocked_ = 1;
			num_rtxs[rcv_sn%wnd_]++;
			status[rcv_sn%wnd_] = SENT;
			Packet *newp = pkt_buf[rcv_sn%wnd_]->copy();
      timeout_events[rcv_sn]->expirationTime = Scheduler::instance().clock() + timeout_;
      timeout_events[rcv_sn]->isCancelled = false;
      Scheduler::instance().schedule(this, timeout_events[rcv_sn], timeout_);
      pkt_rtxs++;

			send(newp,&arqh_);
		} else {
			num_pending_retrans_++;
		}
	} else {//packet should be dropped
		if (debug) printf("SRARQTx, nack: Dropping pkt %d. SimTime=%.8f.\n", rcv_sn, Scheduler::instance().clock());
		status[rcv_sn%wnd_] = DROP;
    delete (timeout_events[rcv_sn]);
    timeout_events[rcv_sn] = NULL;
		if (rcv_sn%wnd_ == ((last_acked_sq_ + 1)%sn_cnt)%wnd_) {
      reset_lastacked(); //droped frame is next in order so check whether the active window should advance
      if (!blocked_) handler_->handle(0); //ask queue_ to deliver next packet
    }
	}

}//timeout_expired

void SRARQTx::resume()
{
	//This procedure is invoked by link_ when a transmission is completed, i.e., it is invoked T secs after SRARQTx executes send() where T equals transmission_delay.
	blocked_ = 0;

if (pending){
		Packet *fwpkt = pending;
		pending = NULL;
		recv(fwpkt, handler_);
	} else if (num_pending_retrans_ > 0) { //if there exist packets not ACKed that need to be retransmitted
		int wrunner_ = findpos_retrans();
    int runner_ = wrunner_%wnd_;
		if (debug) printf("SRARQTx, resume: Sending pkt %d. SimTime=%.8f.\n", (HDR_CMN(pkt_buf[runner_]))->opt_num_forwards_, Scheduler::instance().clock());
		num_rtxs[runner_]++;
		num_pending_retrans_--;
		blocked_ = 1;
    Packet *pnew;
    pnew = (pkt_buf[runner_])->copy();
		if (status[runner_] == RTX) { //the packet has expired, so send a coded one instead of the packet
      timeout_events[wrunner_]->expirationTime = Scheduler::instance().clock() + timeout_;
      timeout_events[wrunner_]->isCancelled = false;
      Scheduler::instance().schedule(this, timeout_events[wrunner_], timeout_);
    } else { //the case of RTXPRE: non expired, send original
      timeout_events[wrunner_]->isCancelled = true;
      Scheduler::instance().schedule(this, timeout_events[wrunner_], timeout_);
    }
    status[runner_] = SENT;
    pkt_rtxs++;
		send(pnew,&arqh_);
	} else {//there are no pending retransmision, check whether it is possible to send a new packet
		//TO DO: check whether active window reaches wnd_ and SRARQTx is stuck without asking queue for the next frame
		int current_wnd_ = (most_recent_sq_ - last_acked_sq_ + sn_cnt)%sn_cnt;
		if (current_wnd_ <= wnd_) {
      handler_->handle(0); //ask queue_ to deliver next packet
    } else {
      if (start_of_pause_period == -1) start_of_pause_period = Scheduler::instance().clock(); //start logging a pause period
    }
	}

}//end of resume

int SRARQTx::findpos_retrans()
{
	//----------DEBUG------------------//
	if((last_acked_sq_+1)%sn_cnt == most_recent_sq_) { fprintf(stderr, "Error at SRARQTx::findpos_retrans, no packet is waiting (stored) for transmission.\n"); abort(); }
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
	if (!found){ fprintf(stderr, "Error at SRARQTx::findpos_retrans, packet with RTX or RTXPRE status NOT FOUND.\n"); abort(); }
	//---------------------------------//

	return runner_wide_;
} //end of findpos_retrans

void SRARQTx::reset_lastacked()
{

  bool window_advanced = false;
	if((last_acked_sq_+1)%sn_cnt == most_recent_sq_) return; //no need to reset last_ack because there is no packet stored (MOST RECENT - LAST ACKED = 1)

	int runner_ = ((last_acked_sq_+1)%sn_cnt)%wnd_;
  int wide_runner_ = (last_acked_sq_+1)%sn_cnt;
	do {
		if ((status[runner_] == RTX) || (status[runner_] == RTXPRE) || (status[runner_] == SENT)) break;
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
	if(debug) printf("SRARQTx, reset_lastacked: new LA(MR) are %d(%d). SimTime=%.8f.\n", last_acked_sq_, most_recent_sq_, Scheduler::instance().clock());

} // end of reset_lastacked

void SRARQTx::handle(Event* e){

  SRARQEvent *received_event = (SRARQEvent *)e;
  SRARQEventTypes rcv_type = received_event->type;
  if (rcv_type == TIMEOUT){ //handle a timeout expiration event

    SRARQTimeoutEvent *received_tevent = (SRARQTimeoutEvent *)e;
    //three cases for event: a) not cancelled -> timeout expired, b) cancelled and expirationTime set to -1.0 -> the frame has been ACKED, and
    //c) cancelled but expirationTime > current time -> should be rescheduled (an old expiration event cancelled and renewed by a retransmission)
    //Events are deleted ONLY BY a) timeout_expired when rtx_limit is reached or b) handle when the event has been cancelled by an ACK. This is
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

    if ( ranvar_->value() < err_rate ){ //ack is lost, no need to do anything just delete pkt if it is a coded_ack
      if (debug) printf("Tx, handle: ACK for %d is lost.\n", received_event->sn);
    } else { //ack is received correctly
      parse_ack(received_event->sn, received_event->uid);
    }
    delete e; //no more needed, delete

  }
} //end of handle
//--------------------------------------------------------------------------------------------//
//--------------------------------------------------------------------------------------------//





//-------------------------------------------SRARQRx--------------------------------------------//
//--------------------------------------------------------------------------------------------//
SRARQRx::SRARQRx()
{
	arq_tx_=0;
	delay_ = 0;
} //end of constructor


int SRARQRx::command(int argc, const char*const* argv)
{
	Tcl& tcl = Tcl::instance();
  if (argc == 2){
    if (strcmp(argv[1], "update-delays") == 0) {
      delay_ = arq_tx_->get_linkdelay(); //the propagation delay
			return(TCL_OK);
		}
  } else if (argc == 3) {
		if (strcmp(argv[1], "attach-SRARQTx") == 0) {
			if (*argv[2] == '0') {
				tcl.resultf("Cannot attach NULL SRARQTx\n");
				return(TCL_ERROR);
			}
			arq_tx_ = (SRARQTx*)TclObject::lookup(argv[2]);
			return(TCL_OK);
		}
	} return Connector::command(argc, argv);
} //end of command


SRARQAcker::SRARQAcker()
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
  max_delay = 0; //the maximum delay experienced by a packet
  min_delay = 100000000000; //the minimum delay experienced by a packet
  last_delay_sample = 0; //the last delay, used to calculate delay jitter
} //end of constructor



int SRARQAcker::command(int argc, const char*const* argv)
{
	Tcl& tcl = Tcl::instance();
	if (argc == 3) {
		if (strcmp(argv[1], "setup-wnd") == 0) {
			if (*argv[2] == '0') {
				tcl.resultf("Cannot setup NULL wnd\n");
				return(TCL_ERROR);
			}
			wnd_ = atoi(argv[2]);
			sn_cnt = 4 * wnd_; //never less than 2*wnd_ but usually more than that (see note in SRARQTx command)
			pkt_buf = new Packet* [wnd_]; //buffer for storing out-of-order received frames
			status = new SRARQPacketStatus[wnd_]; //the status of each pending frame: NONE,MISSING,RECEIVED
			for(int i=0; i<wnd_; i++){ pkt_buf[i] = NULL; status[i] = NONE;}
			return(TCL_OK);
		}
		if (strcmp(argv[1], "setup-SRARQNacker") == 0) {
			if (*argv[2] == '0') {
				tcl.resultf("Cannot attach NULL SRARQNacker\n");
				return(TCL_ERROR);
			}
			nacker = (SRARQNacker*)TclObject::lookup(argv[2]);
			return(TCL_OK);
		}
	} else if (argc == 6) {
		if (strcmp(argv[1], "print-stats") == 0) {
			print_stats(atof(argv[2]), atof(argv[3]), atof(argv[4]), atoi(argv[5])); //used for collecting statistics, along with the corresponding tcl command
			return(TCL_OK);
		}
	} return SRARQRx::command(argc, argv);
} //end of command

void SRARQAcker::recv(Packet* p, Handler* h)
{

	hdr_cmn *ch = HDR_CMN(p);
	int seq_num = ch->opt_num_forwards_;
	int pkt_uid = ch->uid();

	int fw_dis = (seq_num - last_fwd_sn_ + sn_cnt)%sn_cnt; //distance of received seq_num from the last delivered frame
	int fw_width = (most_recent_acked - last_fwd_sn_ + sn_cnt)%sn_cnt; //the active receiver window
	bool within_fww = ((fw_dis <= wnd_) && (fw_dis > 0)) ? (true) : (false); //bool to indicate whether the new frame is within forward window or not
	int bw_dis = (most_recent_acked - seq_num + sn_cnt)%sn_cnt; //distance of the most recently acked seq_num from the received seq_num
	bool within_bww = (bw_dis < wnd_) ? (true) : (false); //bool to indicate whether the new frame is (or not) within the SRARQTx's active window as seen by SRARQRx (backward window)

	int nxt_seq = (last_fwd_sn_ + 1)%sn_cnt; //the next expected seq_num

	if (within_fww){//frame belongs to the forward window

		if(seq_num == nxt_seq){ //frame arrives in order, ack and send frame and finally check if other frames are now in order
			if (debug) printf("SRARQRx, recv1 (LF=%d,MR=%d): received pkt %d. SimTime=%.8f.\n", last_fwd_sn_, most_recent_acked, seq_num, Scheduler::instance().clock());
			status[nxt_seq%wnd_] = NONE;
			if (pkt_buf[nxt_seq%wnd_]){ fprintf(stderr, "Error at SRARQRx::recv, position of in order frame found non empty.\n"); abort(); }
			pkt_buf[nxt_seq%wnd_] = NULL;

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
			if (status[seq_num%wnd_] != RECEIVED) {
				if (debug) printf("SRARQRx, recv2 (LF=%d,MR=%d): received pkt %d. SimTime=%.8f.\n", last_fwd_sn_, most_recent_acked, seq_num, Scheduler::instance().clock());
				status[seq_num%wnd_] = RECEIVED;
				if (pkt_buf[seq_num%wnd_]){ fprintf(stderr, "Error at SRARQRx::recv, position of not received and not decoded frame within forward window found non empty.\n"); abort(); }
				pkt_buf[seq_num%wnd_] = p;
			} else { //the frame has already been received, thus the ACK was lost and nothing needs to be done beyond ACKing the frame and deleting the newly received
				//Sanity-------------------------//
				if ((HDR_CMN(pkt_buf[seq_num%wnd_]))->uid() != (HDR_CMN(p))->uid()){ fprintf(stderr, "Error at SRARQRx::handle, received retransmission has not the same uid.\n"); abort(); }
				Packet::free(p);
				//-------------------------------//
			}
		}
		if (fw_dis > fw_width) { //the new frames is beyond the most_recent_acked
			for (int i = (most_recent_acked + 1)%sn_cnt; i != seq_num; i = (i + 1)%sn_cnt){ status[i%wnd_] = MISSING; } //update positions in between with MISSING state
			most_recent_acked = seq_num;
		}

	} else if (within_bww && !within_fww) {//frame belongs to the backward window (i.e., SRARQTx's active window), so it is a retransmitted frame due to loss of ACK
		//ignore the packet and acknowledge
	} else { //frame is not in the forward and not in the backward window
		//This happens when one or more frames are dropped by SRARQTx (when exceeding the retrans limit) so it moves on to transmit new frames.
		//SRARQRx is not informed to advance its window and receives brand new frames that may consider as old ones
		//Using 2*wnd_ sequence numbers (= forward window + backward window) is enough for identifying new frames from retransmissions provided that SRARQTx advances its window only after an ACK by SRARQRx
		//When SRARQTx can advance its window and send new frames after dropping a old frame then we need more than 2*wnd_ sequence numbers to identify those the new frames because otherwise those new frames will appear to be in the backward window and will be mistaken by SRARQRx as retransmission frames
		if (debug) printf("SRARQRx, recv3 (LF=%d,MR=%d): received pkt %d. SimTime=%.8f.\n", last_fwd_sn_, most_recent_acked, seq_num, Scheduler::instance().clock());
		int first_st = ((seq_num - wnd_ + sn_cnt)%sn_cnt - last_fwd_sn_ + sn_cnt)%sn_cnt;
		deliver_frames(first_st, false, h); //deliver frames from last forwarded up to the new lower end of the backward window
		status[seq_num%wnd_] = RECEIVED;
		if (pkt_buf[seq_num%wnd_]){ fprintf(stderr, "Error at SRARQRx::recv, position of new frame beyond most recent found non empty.\n"); abort(); }
		pkt_buf[seq_num%wnd_] = p;
		deliver_frames(wnd_, true, h); //check if it is possible to deliver in order frames
		for (int i = (most_recent_acked + 1)%sn_cnt; i != seq_num; i = (i + 1)%sn_cnt){ status[i%wnd_] = MISSING; } //update positions between most_recent_acked and the new sequence number with MISSING state
		most_recent_acked = seq_num;
	}


  //-----Schedule ACK----------------//
  SRARQEvent *new_ACKEvent = new SRARQEvent();
  Event *ack_e;
  new_ACKEvent->type = ACK;
  new_ACKEvent->sn = seq_num;
  new_ACKEvent->uid = pkt_uid;
  ack_e = (Event *)new_ACKEvent;
  if (delay_ > 0)
    Scheduler::instance().schedule(arq_tx_, ack_e, (delay_ + 8.0/arq_tx_->get_linkbw()));
  //---------------------------------//

} //end of recv


void SRARQAcker::deliver_frames(int steps, bool mindgaps, Handler *h)
{

	int count = 0;
	while (count < steps){
		if ((status[((last_fwd_sn_+1)%sn_cnt)%wnd_] != RECEIVED)&&(mindgaps)) break;

    if ((status[((last_fwd_sn_+1)%sn_cnt)%wnd_] == RECEIVED) || (pkt_buf[((last_fwd_sn_+1)%sn_cnt)%wnd_])){
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

void SRARQAcker::print_stats(double err, double ack, double sim_time, int seed)
{
  printf("\n//------------ STATS FOR SRARQ --------------//\n");
	printf("Start time (sec):\t\t\t%f\n", arq_tx_->get_start_time());
	printf("Finish time (sec):\t\t\t%f\n", finish_time);

	printf("Total number of delivered pkts:\t\t%.0f\n", delivered_pkts);
	printf("Delivered data (in mega bytes):\t\t%.3f\n", delivered_data/1048576);
  if (delivered_pkts == 0) {finish_time = Scheduler::instance().clock();} //hack for the case that deliver_frames is not called
	double throughput = (delivered_data * 8) / (finish_time - arq_tx_->get_start_time());
	printf("Total throughput (Mbps):\t\t%f\n", throughput * 1.0e-6);
  printf("Total pause time (secs):\t\t%f\n", arq_tx_->get_total_pause_time());

	printf("Unique packets sent:\t\t\t%.0f\n", arq_tx_->get_total_packets_sent());
  double mean = (delivered_pkts == 0) ? (0.0) : (sum_of_delay / delivered_pkts);
	printf("Mean delay (msec):\t\t\t%f\n", mean * 1.0e+3);
  printf("Maximum delay (msec):\t\t\t%f\n", max_delay * 1.0e+3);
  printf("Minimum delay (msec):\t\t\t%f\n", min_delay * 1.0e+3);
  double meanjitter = (delivered_pkts <= 1) ? (0.0) : (sum_of_delay_jitter / (delivered_pkts -1));
	printf("Mean delay jitter (msec):\t\t%f\n", meanjitter * 1.0e+3);
  double avg_rtxs = arq_tx_->get_total_retransmissions() / arq_tx_->get_total_packets_sent();
	printf("Avg num of retransmissions:\t\t%f\n", avg_rtxs);
  double packet_loss_rate = 1 - (delivered_pkts / arq_tx_->get_total_packets_sent());
  printf("Packet loss rate:\t\t\t%f\n", packet_loss_rate);
  printf("//-------------------------------------------------//\n");

  //Append statistics if file exists, otherwise create new file and insert headers as well
  FILE *fp;
  fp = fopen("arq/results/sr-arq.txt", "a+");
  if(fp == NULL){
    printf("Error in creating file.\n");
    exit(0);
  }
  fseek(fp, 0, SEEK_END);
  if(ftell(fp) == 0) {
      char* header =  "bandwidth  propagation_delay window_size cbr_rate  pkt_size  err_rate  ack_rate  num_rtx timeout simulation_time seed  Start time (sec) Finish time (sec) Total number of delivered pkts  Delivered data (in mega bytes)  Total throughput (Mbps) Total pause time (secs) Unique packets sent  Mean delay (msec) Maximum delay (msec)  Minimum delay (msec)  Mean delay jitter (msec)  Avg num of retransmissions  Packet loss rate";
    fprintf(fp, "%s\n", header);
  }
  fprintf(fp, "%.0f %.3f  %d  %.0f  %d  %.3f  %.3f  %d  %.3f  %.0f  %d  %f %f  %.0f  %.3f  %f  %f  %.0f  %.0f  %f  %f  %f  %f  %f\n", arq_tx_->get_linkbw(), arq_tx_->get_linkdelay(), wnd_, arq_tx_->get_linkbw(), arq_tx_->get_apppktsize(), err, ack, arq_tx_->get_retry_limit(), arq_tx_->get_timeout(), sim_time, seed, arq_tx_->get_start_time(), finish_time, delivered_pkts, delivered_data/1048576, throughput * 1.0e-6, arq_tx_->get_total_pause_time(), arq_tx_->get_total_packets_sent(), mean * 1.0e+3, max_delay * 1.0e+3, min_delay * 1.0e+3, meanjitter * 1.0e+3, avg_rtxs, packet_loss_rate);
  fclose(fp);
} //end of print_stats

SRARQNacker::SRARQNacker()
{
	debug = false;
	acker = NULL;
} //end of SRARQNacker

int SRARQNacker::command(int argc, const char*const* argv)
{
	Tcl& tcl = Tcl::instance();
	if (argc == 3) {
		if (strcmp(argv[1], "setup-SRARQAcker") == 0) {
			if (*argv[2] == '0') {
				tcl.resultf("Cannot attach NULL SRARQAcker\n");
				return(TCL_ERROR);
			}
			acker = (SRARQAcker*)TclObject::lookup(argv[2]);
			return(TCL_OK);
		}
	} return SRARQRx::command(argc, argv);
} //end of command


void SRARQNacker::recv(Packet* p, Handler* h)
{
  //this is the place to schedule a NACK
} //end of recv

//--------------------------------------------------------------------------------------------//
//--------------------------------------------------------------------------------------------//
