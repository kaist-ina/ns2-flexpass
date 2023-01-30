#ifndef _XPASS_EGDX_H_
#define _XPASS_EGDX_H_

#include "agent.h"
#include "packet.h"
#include "tcp.h"
#include "template.h"
#include <assert.h>
#include <math.h>
#include <map>
#include <unordered_map>
#include "tcp-full.h"
#include "../queue/queue.h"
#include "flags.h"

#define AIR 0
#define ECS 0

#define CFC_ORIG 0
#define CFC_BIC 1

#define CFC_ALG CFC_ORIG

#define AEOLUS 0

typedef enum EGDX_SEND_STATE_ {
  EGDX_SEND_CLOSED,
  EGDX_SEND_CLOSE_WAIT,
  EGDX_SEND_CREDIT_SENDING,
  EGDX_SEND_CREDIT_STOP_RECEIVED,
  EGDX_SEND_NSTATE,
} EGDX_SEND_STATE;

typedef enum EGDX_RECV_STATE_ {
  EGDX_RECV_CLOSED,
  EGDX_RECV_CLOSE_WAIT,
  EGDX_RECV_CREDIT_REQUEST_SENT,
  EGDX_RECV_CREDIT_RECEIVING,
  EGDX_RECV_CREDIT_STOP_SENT,
  EGDX_RECV_NSTATE,
} EGDX_RECV_STATE;

#if AEOLUS
typedef enum SENDER_FLOW_STATE_ {
  SENDER_WITHIN_FIRST_RTT,
  SENDER_AFTER_FIRST_RTT,
} SENDER_FLOW_STATE_;
#endif

struct hdr_egdx {
  // To measure RTT  
  double credit_sent_time_;

  // Credit sequence number
  seq_t credit_seq_;

  // temp variables for test
  int sendbuffer_;
  seq_t total_len_;

  int fid_;
  int payload_len_;

  seq_t dontcare_seq_;  // Reactive Flows should not consider seq below dontcare_seq_ as lost
  seq_t stat_rc3_bytes_total_recovery_;

  seq_t original_flow_seq_;

#if AEOLUS
  seq_t firstrtt_init_seq_;
  int credit_req_id_;
#endif

  // For header access
  static int offset_; // required by PacketHeaderManager
	inline static int& offset() { return offset_; }
  inline static hdr_egdx* access(const Packet* p) {
    return (hdr_egdx*)p->access(offset_);
  }

  /* per-field member access functions */
  double& credit_sent_time() { return (credit_sent_time_); }
  seq_t& credit_seq() { return (credit_seq_); }
#if AEOLUS
  seq_t& firstrtt_init_seq() { return (firstrtt_init_seq_); }
  int& credit_req_id() { return (credit_req_id_); }
#endif
  seq_t& total_len() {
    return (total_len_);
   }
  int& payload_len() {
    return (payload_len_);
  }
  seq_t& dontcare_seq() {
    return (dontcare_seq_);
  }

  seq_t& original_flow_seq() {
    return (original_flow_seq_);
  }
};

class EgdxAgent;
class EgdxSendCreditTimer: public TimerHandler {
public:
  EgdxSendCreditTimer(EgdxAgent *a): TimerHandler(), a_(a) { }
protected:
  virtual void expire(Event *);
  EgdxAgent *a_;
};

class EgdxCreditStopTimer: public TimerHandler {
public:
  EgdxCreditStopTimer(EgdxAgent *a): TimerHandler(), a_(a) { }
protected:
  virtual void expire(Event *);
  EgdxAgent *a_;
};

class EgdxSenderRetransmitTimer: public TimerHandler {
public:
  EgdxSenderRetransmitTimer(EgdxAgent *a): TimerHandler(), a_(a) { }
protected:
  virtual void expire(Event *);

  EgdxAgent *a_;
};

class EgdxReceiverRetransmitTimer: public TimerHandler {
public:
  EgdxReceiverRetransmitTimer(EgdxAgent *a): TimerHandler(), a_(a) { }
protected:
  virtual void expire(Event *);
  EgdxAgent *a_;
};

class EgdxFCTTimer: public TimerHandler {
public:
  EgdxFCTTimer(EgdxAgent *a): TimerHandler(), a_(a) { }
protected:
  virtual void expire(Event *);
  EgdxAgent *a_;
};

class EgdxAllocateTxTimer: public TimerHandler {
public:
  EgdxAllocateTxTimer(EgdxAgent *a, bool xpass): TimerHandler(), a_(a), xpass_(xpass) { }
protected:
  virtual void expire(Event *);
  EgdxAgent *a_;
  bool xpass_;
};

#if AEOLUS
class EgdxFirstRttSendTimer: public TimerHandler {
public:
  EgdxFirstRttSendTimer(EgdxAgent *a): TimerHandler(), a_(a) { }
protected:
  virtual void expire(Event *);
  EgdxAgent *a_;
};

class EgdxRetransmitTimer: public TimerHandler {
public:
  EgdxRetransmitTimer(EgdxAgent *a): TimerHandler(), a_(a) { }
protected:
  virtual void expire(Event *);
  EgdxAgent *a_;
};

class EgdxRestartFirstRttSendTimer: public TimerHandler {
public:
  EgdxRestartFirstRttSendTimer(EgdxAgent *a): TimerHandler(), a_(a) { }
protected:
  virtual void expire(Event *);
  EgdxAgent *a_;
};

#endif

class EgdxAgent : public SackFullTcpAgent
{
  friend class EgdxSendCreditTimer;
  friend class EgdxCreditStopTimer;
  friend class EgdxSenderRetransmitTimer;
  friend class EgdxReceiverRetransmitTimer;
  friend class EgdxFCTTimer;
  friend class EgdxAllocateTxTimer;
#if AEOLUS
  friend class EgdxFirstRttSendTimer;
  friend class EgdxRetransmitTimer;
  friend class EgdxRestartFirstRttSendTimer;
#endif
public:
  EgdxAgent(): SackFullTcpAgent(), credit_send_state_(EGDX_SEND_CLOSED),
                credit_recv_state_(EGDX_RECV_CLOSED), 
#if AEOLUS                
                sender_flow_state_(SENDER_WITHIN_FIRST_RTT),
#endif                
                last_credit_rate_update_(-0.0),
                credit_total_(0), credit_dropped_(0), can_increase_w_(false),
                send_credit_timer_(this), credit_stop_timer_(this), 
                sender_retransmit_timer_(this), receiver_retransmit_timer_(this),
                fct_timer_(this), 
#if AEOLUS                
                retransmit_timer_(this), restart_firstrtt_timer_(this),
#endif            
                curseq_(1), t_seqno_(1), recv_next_(1),
                c_seqno_(1), c_recv_next_(1), rtt_(-0.0), // rtt_(-0.0),
                wait_retransmission_(false), credit_wasted_(0), credit_recved_(0), 
                credit_recved_rtt_(0), last_credit_recv_update_(0), 
#if AEOLUS
                base_rtt_(0.00004), firstrtt_send_timer_(this), last_firstrtt_burst_(0), unscheduled_burst_period_(0), aeolus_firstrtt_burst_(0),credit_request_id_(0), sender_credit_request_id_(0),
#endif
                egdx_beta_(0.5), egdx_beta_min_(0.5), enable_ack_(1), credit_request_sent_time_(-1), egdx_xpass_rtt_(-1), 
                egdx_tcp_rtt_(-1), egdx_rtt_min_(-1), egdx_xpass_prioritized_bytes_(1538), egdx_xpass_prioritized_bytes_left_(0),
                egdx_tcp_reserve_(0), egdx_allocate_tx_timer_(this, false), egdx_allocate_tx_timer_xpass_(this, true)
                {
                  sendbuffer_ = new PacketQueue;
                  // srand(1);
                }

                ~EgdxAgent() {
                  delete sendbuffer_;
                }
                virtual int command(int argc, const char *const *argv);
                virtual void recv(Packet *, Handler *);
                virtual void trace(TracedVar *v);

              protected:
                virtual void delay_bind_init_all();
                virtual int delay_bind_dispatch(const char *varName, const char *localName, TclObject *tracer);

                virtual void traceVar(TracedVar *v);
                void traceAll();
                // credit send state
                EGDX_SEND_STATE credit_send_state_;
                // credit receive state
                EGDX_RECV_STATE credit_recv_state_;

                // minimum Ethernet frame size (= size of control packet such as credit)
                int min_ethernet_size_;
                // maximum Ethernet frame size (= maximum data packet size)
                int max_ethernet_size_;

#if AEOLUS
  // from Aeolus
  SENDER_FLOW_STATE_ sender_flow_state_;
#endif

  // If min_credit_size_ and max_credit_size_ are the same, 
  // credit size is determined statically. Otherwise, if
  // min_credit_size_ != max_credit_size_, credit sizes is
  // determined randomly between min and max.
  // minimum credit size (practically, should be > min_ethernet_size_)
  int min_credit_size_;
  // maximum credit size
  int max_credit_size_;

  // ExpressPass Header size
  int xpass_hdr_size_;

  // maximum credit rate (= lineRate * 84/(1538+84))
  // in Bytes/sec
  int64_t max_credit_rate_;
  // current credit rate (should be initialized ALPHA*max_credit_rate_)
  // should always less than or equal to max_credit_rate_.
  // in Bytes/sec
  int64_t cur_credit_rate_;
  TracedInt cur_credit_rate_tr_;

  // maximum credit rate for 10G NIC.
  int base_credit_rate_;
  // initial cur_credit_rate_ = alpha_ * max_credit_rate_
  double alpha_;
  //
  double target_loss_scaling_;
  // last time for cur_credit_rate_ update with feedback control.
  double last_credit_rate_update_;
  // total number of credit = # credit received + # credit dropped.
  int credit_total_;
  // number of credit dropped.
  int credit_dropped_;
  // aggressiveness factor
  // it determines how aggressively increase the credit sending rate.
  double w_;
  // initial value of w_
  double w_init_;
  // minimum value of w_
  double min_w_;
  // whether feedback control can increase w or not.
  bool can_increase_w_;
  // maximum jitter: -1.0 ~ 1.0 (wrt. inter-credit gap)
  double max_jitter_;
  // minimum jitter: -1.0 ~ 1.0 (wrt. inter-credit gap)
  double min_jitter_;

#if CFC_ALG == CFC_BIC
  int bic_target_rate_;
  int bic_s_min_;
  int bic_s_max_;
  double bic_beta_;
#endif

  EgdxSendCreditTimer send_credit_timer_;
  EgdxCreditStopTimer credit_stop_timer_;
  EgdxSenderRetransmitTimer sender_retransmit_timer_;
  EgdxReceiverRetransmitTimer receiver_retransmit_timer_;
  EgdxFCTTimer fct_timer_;

#if AEOLUS
  /* Aeolus */
  EgdxRetransmitTimer retransmit_timer_;
  EgdxRestartFirstRttSendTimer restart_firstrtt_timer_;
#endif

  // the highest sequence number produced by app.
  seq_t curseq_;
  // next sequence number to send
  seq_t t_seqno_;
  // next sequence number expected (acknowledging number)
  seq_t recv_next_;
  // next credit sequence number to send
  seq_t c_seqno_;
  // next credit sequence number expected
  seq_t c_recv_next_;

  // weighted-average round trip time
  double rtt_;
  // flow start time
  double fst_;
  double fct_;

  // retransmission time out
  double retransmit_timeout_;

  // timeout to ignore credits after credit stop
  double default_credit_stop_timeout_;

  // whether receiver is waiting for data retransmission
  bool wait_retransmission_;
  // temp variables
  int credit_wasted_;

  // counter to hold credit count;
  int credit_recved_;
  int credit_recved_rtt_;
  double last_credit_recv_update_;

#if AEOLUS
  /* =========== Aeolus =========== */
  // base rtt
  double base_rtt_;
  // maximum link rate, added for Aeolus
  // in bytes/sec
  double max_link_rate_;
  EgdxFirstRttSendTimer firstrtt_send_timer_;
  // used at receiver side, to record the state of packets sent in the first rtt
  int *pkt_received_;
  // used at receiver side, total number of packets to receive
  int pkt_number_to_receive;
  // the number of packets received
  int pkt_number_received;
  // the following variables are used at sender side
  //to record whether a packet sent in the first rtt is acked
  int *pkt_acked_;
  //to record size of transmitted packet
  int *pkt_payload_size_;
  // packet id of last processed ack
  int last_processed_ack_pkt_id;
  // number of lost packets detected

  int lost_pkt_num;
  int first_lost_pkt_id;
  int max_pktno_firstrtt;
  int probe_counter;
  int probe_frequent;
  seq_t last_sent_pkt_seqno;

  int test_retransmit_counter;
  int flow_finished_with_firstrtt_pkt;
  int sent_counter_firstrtt_pkt;
  int ack_counter_firstrtt_pkt;
  int credit_induced_data_received;
  int firstrtt_init_done;

  // packet transmission interval in the first rat， added for aeolus
  double firstrtt_transmit_interval;
  int already_sent_credit_stop;
  seq_t byte_num_to_send;
  seq_t last_firstrtt_burst_;
  int unscheduled_burst_period_;
  int aeolus_firstrtt_burst_;
  int credit_request_id_;
  int sender_credit_request_id_;
#endif 

  PacketQueue *sendbuffer_;

  double egdx_beta_;
  double egdx_beta_min_; // assumed to be weight of the queue
  int enable_ack_;
  double credit_request_sent_time_;
  double syn_sent_time_;
  double egdx_xpass_rtt_;
  double egdx_tcp_rtt_;
  double egdx_rtt_min_;
  seq_t pending_bytes_;
  seq_t egdx_xpass_prioritized_bytes_;
  seq_t egdx_xpass_prioritized_bytes_left_;
  seq_t egdx_tcp_reserve_;
  EgdxAllocateTxTimer egdx_allocate_tx_timer_;
  EgdxAllocateTxTimer egdx_allocate_tx_timer_xpass_;
  bool egdx_tcp_start_{false};
  std::unordered_map<unsigned, double> packet_sent_timestamp_;
  std::unordered_map<unsigned, double> packet_tcp_sent_timestamp_;
  bool fct_reported_{false};
  int debug_flowsize_{-1};
  seq_t total_bytes_ {0};
  seq_t total_bytes_send_{0}; // > 0 if sender
  seq_t rc3_tcp_snd_nxt{0};   // last byte seq of the packet that will be sent next
  seq_t rc3_xpass_snd_nxt{0}; // first byte seq of the packet that will be sent next
  seq_t rc3_bytes_needs_recovery_{0};
  int rc3_mode_;
  int credit_stop_sent_count_{0};
  seq_t stat_bytes_sent_xpass_{0};
  seq_t stat_bytes_sent_tcp_{0};
  seq_t stat_bytes_sent_rc3_{0};
  seq_t rc3_bytes_total_recovery_ {0};
  
  seq_t total_bytes_xpass_plus_rc3_{0}; // for checking integrity at receiver side.
  seq_t highest_reactive_ack_ {0}; // sender side highest reactive ack
  bool reactive_subflow_ready_{false}; // is received synack?
  int dontcare_skipped_bytes{0};
  int new_allocation_logic_;
  int reordering_measure_in_rc3_ {0}; // for motivation purpose. let measure reordering in RC3 method
  int static_allocation_{0};          // for motivation purpose.
  size_t stat_max_reordering_bytes_{0};
  seq_t stat_pending_head_{0};
  seq_t stat_unacked_head_{0};
  seq_t stat_recovery_head_{0};
  seq_t stat_next_proactive_orig_seq_{0};
  seq_t stat_next_reactive_orig_seq_{0};
  seq_t stat_highest_data_{0};
  std::map<seq_t, size_t> stat_empty_data_;

  inline void
  stat_reordering_detection(seq_t seq, size_t datalen) {
    if (seq <= stat_highest_data_ && (seq_t)(seq + datalen) >= stat_highest_data_) {
        stat_highest_data_ = seq + datalen;
        for (auto it = stat_empty_data_.begin(); it != stat_empty_data_.end();) {
            if (it->first > stat_highest_data_) break;
            if (it->first + it->second > stat_highest_data_)
                stat_highest_data_ = it->first + it->second;
            it = stat_empty_data_.erase(it);
        }
        // printf("MAX Reordering bytes: %lu\n", stat_max_reordering_bytes_);
    } else if (seq <= stat_highest_data_ && seq + datalen >= stat_highest_data_) {
      // do nothing
    } else if (seq > stat_highest_data_) {
      stat_empty_data_[seq] = datalen;

      size_t required_buf = seq + datalen - stat_highest_data_;
      if (stat_max_reordering_bytes_ < required_buf) {
          stat_max_reordering_bytes_ = required_buf;
          // printf("MAX Reordering bytes: %lu, seq+datalen=%lu, stat_heighest_data_=%lu\n", stat_max_reordering_bytes_, seq + datalen, stat_highest_data_);
      }

    }

    // printf("stat_highest_data_: %ld\n", stat_highest_data_);
  }
  /* ============================= */

  inline double now() { return Scheduler::instance().clock(); }
  seq_t datalen_remaining_xpass() { return curseq_ - t_seqno_; }
  seq_t datalen_remaining() const {
      seq_t len = pending_bytes_ + rc3_bytes_needs_recovery_ + (curseq_ - t_seqno_) + (SackFullTcpAgent::curseq_ - SackFullTcpAgent::highest_ack_ + 1);
      assert(len >= 0);
      // seq_t len = pending_bytes_ + rc3_bytes_needs_recovery_ + (curseq_ - t_seqno_) + (SackFullTcpAgent::curseq_ - highest_reactive_ack_ + 1);
      // printf("[%2.8lf: %p]pending_bytes_=%ld, rc3rec_=%ld, curseq_=%ld, t_seqno=%ld, curseq_tcp=%ld, ha=%ld, len=%ld\n", now(), this,
      // pending_bytes_, rc3_bytes_needs_recovery_, curseq_, t_seqno_,FullTcpAgent::curseq_, FullTcpAgent::highest_ack_, len);
      return len;
  }
  inline seq_t get_reactive_buffered_bytes() {
    return get_reactive_buffered_bytes(SackFullTcpAgent::highest_ack_);
  }
  
  inline seq_t get_reactive_buffered_bytes(seq_t new_highest_ack) {
      return (infinite_send_) ? TCP_MAXSEQ : ((SackFullTcpAgent::highest_ack_ < 0) ? SackFullTcpAgent::curseq_ : (SackFullTcpAgent::curseq_ - new_highest_ack + 1));
  }
  int max_segment() { return (max_ethernet_size_ - xpass_hdr_size_); }
  int pkt_remaining() { return ceil(datalen_remaining()/(double)max_segment()); }
  double avg_credit_size() { return (min_credit_size_ + max_credit_size_)/2.0; }
  void try_send(Packet *p, Handler *h);
  virtual void sendpacket(seq_t seq, seq_t ack, int flags, int dlen, int why, Packet *p = 0);
  virtual void report_fct();
  void write_fct();

  void init();
#if AEOLUS
  Packet* construct_credit_request(seq_t nb);
#else
  Packet* construct_credit_request();
#endif
  Packet* construct_credit_stop();
  Packet* construct_credit();
  Packet* construct_data(Packet *credit);
  Packet* construct_data_tcp(Packet *credit);
  
  Packet* construct_nack(seq_t seq_no, seq_t triggered_seq_no);
  Packet* construct_ack(seq_t seq_no, int datalen);
  

#if AEOLUS
  /* =========== Aeolus =========== */
  Packet* construct_firstrtt_data();
  Packet* construct_retransmitted_data(seq_t seqno, Packet *credit);
  Packet* construct_data_ack(Packet *rev_pkt);
  Packet* construct_probe();
  Packet* construct_probe_ack(Packet *rev_pkt);
  /* ============================== */
#endif

  void send_credit();
  void send_credit_stop();
  void advance_packet(Packet *pkt);
  void advance_bytes(seq_t nb);
  void allocate_tx_bytes(bool is_credit_available);
  void allocate_tx_bytes(bool is_credit_available, int step);
  void allocate_tx_bytes(bool is_credit_available, int step, seq_t new_highest_ack);

  void recv_credit_request(Packet *pkt);
  void recv_credit(Packet *pkt);
  void recv_data(Packet *pkt);
  void recv_credit_stop(Packet *pkt);
  void recv_nack(Packet *pkt);
  void recv_ack(Packet *pkt);

  void handle_sender_retransmit();
  void handle_receiver_retransmit();
  void handle_fct();
  void process_ack(Packet *pkt);
  void update_rtt(Packet *pkt);
  void update_sender_rtt(Packet *pkt);

  void credit_feedback_control();

  void dupack_action();
  void recover_lost_tcp_block(bool timeout, seq_t maxseg, bool force=false);
  void timeout_action();
  bool is_recv_complete();
  int build_options(hdr_tcp *tcph);

#if AEOLUS
  /* =========== Aeolus =========== */
  void send_firstrtt_data();
  void recv_retransmitted_data(Packet *pkt);
  void recv_firstrtt_data(Packet *rev_pkt);
  void recv_data_ack(Packet *rev_pkt);
  void recv_probe(Packet *rev_pkt);
  void recv_probe_ack(Packet *rev_pkt);
  int get_first_lost_pkt_id();
  int get_first_unacked_pkt_id();
  
  void init_for_firstrtt_transmit();
  void init_for_credit_transmit();

  void output_fct();
  void handle_retransmit();
  /* ============================== */
#endif

};



#endif
