#include "connector.h"
#include "queue.h"
#include <math.h>
#include <set>
#include <vector>
#include <map>
#include <algorithm>
#include <unistd.h>

class SRARQTx;
enum SRARQARQStatus {IDLE,SENT,ACKED,RTX,RTXPRE,DROP}; //statuses for packets sent by SRARQTx
enum SRARQPacketStatus {NONE,MISSING,RECEIVED}; //for SRARQAcker, in order to tell apart different types of packets
enum SRARQEventTypes {TIMEOUT}; //types of events

class SRARQHandler : public Handler {
public:
    SRARQHandler(SRARQTx& arq) : arq_tx_(arq) {};
    void handle(Event*);
private:
    SRARQTx& arq_tx_;
};

class SRARQEvent : public Event {
 public:
   SRARQEventTypes type;
   int sn;
   int uid;
};

class SRARQTimeoutEvent : public SRARQEvent {
 public:
   double expirationTime;
   bool isCancelled;
};

class SRARQTx : public Connector {
 public:
	SRARQTx();
	void recv(Packet*, Handler*);
	void timeout_expired(int rcv_sn, int rcv_uid);
	void parse_ack(int rcv_sn, int rcv_uid);
  void parse_nack(int rcv_sn);
	void resume();
	virtual void handle(Event* e);
  void handle_ack(Packet *p);
	int command(int argc, const char*const* argv);
	//functions for setting protocol parameters
	double get_linkdelay() {return lnk_delay_;}
	double get_linkbw() {return lnk_bw_;}
	//functions used in statistics logging
	double get_start_time() {return start_time;}
	double get_total_packets_sent() {return packets_sent;}
  double get_total_retransmissions() {return pkt_rtxs;}
  double get_total_pause_time() {return total_pause_time;}
  //functions used in statistics passed to files
  int get_wnd(){return wnd_;}
  int get_apppktsize(){return app_pkt_Size_;}
  int get_retry_limit() {return retry_limit_;}
  double get_timeout() {return timeout_;}
 protected:
	SRARQHandler arqh_;
	Handler* handler_;

	Packet *pending; //used for storing a native packet from queue that finds the channel blocked_

	int wnd_;  //window size
	int sn_cnt; //the total count of used sequence numbers
	int retry_limit_; //maximum number of retransmissions allowed for each frame

	Packet **pkt_buf; //buffer used for storing frames under transmission (maximum size of wnd_)
	SRARQARQStatus *status; //the status of each frame under transmission
	int *num_rtxs; //number of retransmisions for each frame under transmission
	int *pkt_uids; //used for debugging purposes
	SRARQTimeoutEvent **timeout_events; //buffer for storing pointers to timeout events

	int blocked_; //switch set to 1 when Tx engaged in transmiting a frame, 0 otherwise
	int last_acked_sq_; //sequence number of last acked frame
	int most_recent_sq_; //sequence number of most recent frame to be sent
	int num_pending_retrans_; //number of frames needed to be retransmitted (after the first attempt)

	double lnk_bw_; //the bandwidth of the link_
	double lnk_delay_; //the delay of the link_
	int app_pkt_Size_; //the size of the pkt created by the app_pkt_Size_
	double timeout_; //the time used to trigger nack()

	bool debug;

	//Statistics
	double start_time; //time when 1st packet arrived at SRARQTx::recv
	double packets_sent; //unique packets sent
  double pkt_rtxs; //the total number of retransmissions
  double total_pause_time; //the total time the sender spend paused because the window reached its limit
  double start_of_pause_period; //the start of a pause period, used to calculate the total_pause_time

	int findpos_retrans();
	void reset_lastacked();

};

class SRARQRx : public Connector {
 public:
	SRARQRx();
	int command(int argc, const char*const* argv);
	virtual void recv(Packet*, Handler*) = 0;
 protected:
	SRARQTx* arq_tx_;
  Queue * opposite_queue;

	double delay_; //delay for returning feedback
};

class SRARQNacker;

class SRARQAcker : public SRARQRx {
public:
	SRARQAcker();
	void recv(Packet*, Handler*);
	int command(int argc, const char*const* argv);
	void print_stats(double err, double ack, double sim_time, int seed);
  void print_parameters(double err, double ack, double sim_time, int seed);
	void log_lost_pkt(Packet *p);
 protected:
	int wnd_;  //window size
	int sn_cnt; //the total count of used sequence numbers
	Packet **pkt_buf; //buffer used for storing packets arriving out of order
	int last_fwd_sn_; //sequence number of the last frame forwarded to the upper layer
	int most_recent_acked; //sequence number of the last frame for which an ack has been sent

	SRARQPacketStatus *status; //status of received packets

	SRARQNacker* nacker;

	bool debug;

	//Statistics
  double finish_time; //time when the last pkt was delivered to the receiver's upper layer, used to calculate throughput
	double delivered_pkts; //the total number of pkts delivered to the receiver's upper layer
	double delivered_data; //the total number of bytes delivered to the receiver's upper layer
	double sum_of_delay; //sum of delays for every packet delivered, used to calculate average delay
  double sum_of_delay_jitter; //sum of delay jitter for every packet delivered, used to calculate average delay
  double max_delay; //the maximum delay experienced by a packet
  double min_delay; //the minimum delay experienced by a packet
  double last_delay_sample; //the last delay, used to calculate delay jitter

	void deliver_frames(int steps, bool mindgaps, Handler *h);
 private:
  char* filename; //file where results will be stored
};

class SRARQNacker : public SRARQRx {
 public:
	SRARQNacker();
	void recv(Packet*, Handler*);
	int command(int argc, const char*const* argv);
 protected:
	SRARQAcker* acker;
	bool debug;
};


class SRARQACKRx : public Connector {
 public:
	SRARQACKRx();
	int command(int argc, const char*const* argv);
	void recv(Packet*, Handler*);
 protected:
	SRARQTx* arq_tx_;
};
