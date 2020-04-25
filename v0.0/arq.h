#include "connector.h"
#include "ranvar.h"
#include <set>

class ARQTx;
enum ARQStatus {IDLE,SENT,ACKED,RTX,DROP}; //statuses for packets sent by ARQTx
enum PacketStatus {NONE,MISSING,RECEIVED,DECODED}; //for ARQAcker, in order to tell apart different types of packets

class ARQHandler : public Handler {
public:
    ARQHandler(ARQTx& arq) : arq_tx_(arq) {};
    void handle(Event*);
private:
    ARQTx& arq_tx_;
};

class ARQTx : public Connector {
public:
    ARQTx();
    void recv(Packet*, Handler*);
    void nack(int rcv_sn, int rcv_uid);
    void ack(int rcv_sn, int rcv_uid);
    void ack(Packet *p); //overloaded ack method
    void resume();
    int command(int argc, const char*const* argv);
protected:
    ARQHandler arqh_;
    Handler* handler_;

    Packet *pending; //used for storing a packet from queue that finds the channel blocked_

    int wnd_;  //window size
    int sn_cnt; //the total count of used sequence numbers
    int retry_limit_; //maximum number of retransmissions allowed for each frame

    Packet **pkt_buf; //buffer used for storing frames under transmission (maximum size of wnd_)
    ARQStatus *status; //the status of each frame under transmission
    int *num_rtxs; //number of retransmisions for each frame under transmission
    int *pkt_uids; //used for debugging purposes

    int blocked_; //switch set to 1 when Tx engaged in transmiting a frame, 0 otherwise
    int last_acked_sq_; //sequence number of last acked frame
    int most_recent_sq_; //sequence number of most recent frame to be sent
    int num_pending_retrans_; //number of frames needed to be retransmitted (after the first attempt)
    int rate_k; //number of native packets sent before coded

    bool debug;

    int findpos_retrans();
    void reset_lastacked();
    Packet* create_coded_packet();

private:
    int native_counter; //counts the number of native packets sent before sending a coded one

};

class ARQRx : public Connector {
public:
    ARQRx();
    //ARQRx()  {arq_tx_=0; };
    int command(int argc, const char*const* argv);
    virtual void recv(Packet*, Handler*) = 0;
protected:
    ARQTx* arq_tx_;
    double delay_; //delay for returning feedback
};

class ARQAcker : public ARQRx {
public:
    ARQAcker();
    //ARQAcker() {};
    virtual void handle(Event*);
    void recv(Packet*, Handler*);
    int command(int argc, const char*const* argv);
    void print_stats();
    void log_lost_pkt(Packet *p);
protected:
    int wnd_;  //window size
    int sn_cnt; //the total count of used sequence numbers
    Packet **pkt_buf; //buffer used for storing packets arriving out of order
    Packet **lost_pkt_buf; //buffer used for storing lost packets
    int last_fwd_sn_; //sequence number of the last frame forwarded to the upper layer
    int most_recent_acked; //sequence number of the last frame for which an ack has been sent

    RandomVariable *ranvar_; //a random variable for generating errors in ACK delivery
    double err_rate; //the rate of errors in ACK delivery

    PacketStatus *status; //status of received packets
    set<int> known_packets; //already correctly received packets
    set<int> lost_packets; //how many packets are lost
    int received_coded_cnt; //counter for received coded packets that are available for decoding

    bool debug;

    //Statistics
    int delivered_pkts;

    void deliver_frames(int steps, bool mindgaps, Handler *h);
    void clean_decoding_matrix(int from, int to);

private:
    Packet* create_coded_ack();
    void parse_coded_packet(Packet *p, Handler *h);
    void decode(Handler* h);

};

class ARQNacker : public ARQRx {
  public:
    ARQNacker() {debug = false;};
    virtual void handle(Event*);
    void recv(Packet*, Handler*);
    int command(int argc, const char*const* argv);
  protected:
  	ARQAcker* acker;
    bool debug;
};

class ACKEvent : public Event {
public:
  int ACK_sn;
  int ACK_uid;
  Packet* coded_ack;
};
