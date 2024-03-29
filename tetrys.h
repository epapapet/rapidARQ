#include "connector.h"
#include "ranvar.h"
#include <set>
#include <vector>
#include <map>
#include <algorithm>
#include <unistd.h>

class TetrysTx;
enum TetrysARQStatus {IDLE,SENT,ACKED,DROP}; //statuses for packets sent by TetrysTx
enum TetrysPacketStatus {NONE,MISSING,RECEIVED,DECODED}; //for TetrysAcker, in order to tell apart different types of packets
enum TetrysEventTypes {TIMEOUT,ACK, ACKSCHEDULE}; //types of events

class TetrysHandler : public Handler {
public:
    TetrysHandler(TetrysTx& arq) : arq_tx_(arq) {};
    void handle(Event*);
private:
    TetrysTx& arq_tx_;
};

class TetrysEvent : public Event {
 public:
   TetrysEventTypes type;
   int sn;
   int uid;
};

class TetrysACKEvent : public TetrysEvent {
 public:
   Packet* coded_ack;
};

class TetrysTimeoutEvent : public TetrysEvent {
 public:
   double expirationTime;
   bool isCancelled;
};

class TetrysTx : public Connector {
 public:
	TetrysTx();
	void recv(Packet*, Handler*);
	void timeout_expired(int rcv_sn, int rcv_uid);
	void parse_cumulative_ack(Packet *p); //overloaded ack method
  bool parse_ack(int rcv_sn, bool batch);
	void resume();
	virtual void handle(Event* e);
	int command(int argc, const char*const* argv);
	//functions for setting protocol parameters
  int get_wnd() {return wnd_;}
  int get_ratek() {return rate_k;}
  int get_apppktsize() {return app_pkt_Size_;}
  double get_timeout() {return timeout_;}
  void set_timeout(double timeval) { timeout_ = timeval; }
	double get_linkdelay() {return lnk_delay_;}
	double get_linkbw() {return lnk_bw_;}
	//functions used in statistics logging
	double get_start_time() {return start_time;}
	double get_total_packets_sent() {return packets_sent;}
  double get_total_coded_packets_sent() {return coded_pkts_sent;}
  double get_total_pause_time() {return total_pause_time;}
 protected:
	TetrysHandler arqh_;
	Handler* handler_;

	Packet *pending; //used for storing a native packet from queue that finds the channel blocked_
  Packet *coded; //used for storing a coded packet that finds the channel blocked_

	int wnd_;  //window size
	int sn_cnt; //the total count of used sequence numbers

	Packet **pkt_buf; //buffer used for storing frames under transmission (maximum size of wnd_)
	TetrysARQStatus *status; //the status of each frame under transmission
	int *pkt_uids; //used for debugging purposes
	TetrysTimeoutEvent **timeout_events; //buffer for storing pointers to timeout events

	int blocked_; //switch set to 1 when Tx engaged in transmiting a frame, 0 otherwise
	int last_acked_sq_; //sequence number of last acked frame
	int most_recent_sq_; //sequence number of most recent frame to be sent
	int rate_k; //number of native packets sent before coded

	double lnk_bw_; //the bandwidth of the link_
	double lnk_delay_; //the delay of the link_
	int app_pkt_Size_; //the size of the pkt created by the app_pkt_Size_
	double timeout_; //the time used to trigger nack()

  RandomVariable *ranvar_; //a random variable for generating errors in ACK delivery
	double err_rate; //the rate of errors in ACK delivery

	bool debug;

	//Statistics
	double start_time; //time when 1st packet arrived at TetrysTx::recv
	double packets_sent; //unique packets sent
  double coded_pkts_sent; //total number of sent coded pkts
  double total_pause_time; //the total time the sender spend paused because the window reached its limit
  double start_of_pause_period; //the start of a pause period, used to calculate the total_pause_time

	void reset_lastacked();
	Packet* create_coded_packet();

 private:
	int native_counter; //counts the number of native packets sent before sending a coded one

};

class TetrysRx : public Connector {
 public:
	TetrysRx();
	int command(int argc, const char*const* argv);
	virtual void recv(Packet*, Handler*) = 0;
 protected:
	TetrysTx* arq_tx_;

	double delay_; //delay for returning feedback
  double ack_period; //the period for sending cumulative ACKs
};

class TetrysNacker;

class TetrysAcker : public TetrysRx {
public:
	TetrysAcker();
	void recv(Packet*, Handler*);
	int command(int argc, const char*const* argv);
	void print_stats(double err, double ack, double sim_time, int seed);
  void print_parameters(double err, double ack, double sim_time, int seed);
	void log_lost_pkt(Packet *p);
  virtual void handle(Event* e);
 protected:
	int wnd_;  //window size
	int sn_cnt; //the total count of used sequence numbers
	Packet **pkt_buf; //buffer used for storing packets arriving out of order
	Packet **lost_pkt_buf; //buffer used for storing lost packets
	int last_fwd_sn_; //sequence number of the last frame forwarded to the upper layer
	int most_recent_acked; //sequence number of the last frame for which an ack has been sent

  bool first_cum_ack_scheduled; //flag for starting the sequence of periodic acks

	TetrysPacketStatus *status; //status of received packets
	set<int> involved_known_packets; //already correctly received packets involved in coded_packets
  set<int> known_packets; //correctly received packets (used for creating a cumulative ACK)
	set<int> lost_packets; //how many packets are lost
  vector<vector<int> > coded_packets; //the received coded pkts that are useful for decoding

	TetrysNacker* nacker;

	bool debug;

	//Statistics
  double finish_time; //time when the last pkt was delivered to the receiver's upper layer, used to calculate throughput
	double delivered_pkts; //the total number of pkts delivered to the receiver's upper layer
	double delivered_data; //the total number of bytes delivered to the receiver's upper layer
	double sum_of_delay; //sum of delays for every packet delivered, used to calculate average delay
  double sum_of_delay_jitter; //sum of delay jitter for every packet delivered, used to calculate average delay
  double avg_dec_matrix_size; //the avg size of the decoding matrix when decoding is performed (used to estimate processing overhead)
  double max_dec_matrix_size; //the maximum size of decoding matrix
  double avg_inv_known_pkts_size; //the avg size of the involved_known_packets when decoding is performed
  double max_inv_known_pkts_size; //the maximum size of involved_known_packets
  double avg_known_pkts_size; //the avg size of the known_packets when decoding is performed (part of decoding matrix already in diagonal form)
  double max_known_pkts_size; //the maximum size of known_packets
	double num_of_decodings; //number of decoding operations
  double avg_pkts_per_decoding; //the average number of decoded packets per decoding
  double max_delay; //the maximum delay experienced by a packet
  double min_delay; //the minimum delay experienced by a packet
  double last_delay_sample; //the last delay, used to calculate delay jitter

	void deliver_frames(int steps, bool mindgaps, Handler *h);
  void clean_decoding_matrix(int from, int to);
  void delete_lost_and_affected_coded(set<int> packets_to_remove);
  void delete_known_and_affected_coded_and_lost(set<int> packets_to_remove);

 private:
  char* filename; //file where results will be stored
	Packet* create_coded_ack();
	void parse_coded_packet(Packet *p, Handler *h);
	bool decode(Handler* h, bool afterCoded);

};

class TetrysNacker : public TetrysRx {
 public:
	TetrysNacker();
	void recv(Packet*, Handler*);
	int command(int argc, const char*const* argv);
 protected:
	TetrysAcker* acker;
	bool debug;
};
