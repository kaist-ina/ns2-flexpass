#ifndef _QUEUE_DWRR_XPASS_RED_H_
#define _QUEUE_DWRR_XPASS_RED_H_
#include <list>
#include <map>
#include <unordered_map>
#include "red.h"
#include "config.h"
#include "trace.h"
#include "broadcom-node.h"
#include "packet.h"

#define LOG_QUEUE 0
#if LOG_QUEUE
#define LOG_GRAN 0.0001
#endif

/*Maximum queue number */
#define MAX_QUEUE_NUM 64

/* Per-queue ECN marking */
#define PER_QUEUE_MARKING 0
/* Per-port ECN marking */
#define PER_PORT_MARKING 1
/* MQ-ECN */
#define MQ_MARKING 2

class PacketDWRR;
class DWRR;

class DwrrXpassCreditTimer: public TimerHandler {
public:
  DwrrXpassCreditTimer(DWRR *a): TimerHandler(), a_(a) {}
protected:
  virtual void expire (Event *);
  DWRR *a_;
};

#if LOG_QUEUE
class DwrrEXPQueueLogTimer : public TimerHandler {
public:
  DwrrEXPQueueLogTimer(DWRR *a) : TimerHandler(), a_(a) {}
  virtual void expire(Event *a);
protected:
  DWRR *a_;
};
#endif

class PacketDWRR: public REDQueue
{
	public:
		PacketDWRR(): quantum(1500), deficitCounter(0), thresh(0), active(false), current(false), start_time(0), next(NULL), REDQueue()  {}
		
		int id;	//queue ID
		int quantum;	//quantum (weight) of this queue
		int deficitCounter;	//deficit counter for this queue
		double thresh;	//per-queue ECN marking threshold (pkts)
		bool active;	//whether this queue is active (qlen>0)
		bool current;	//whether this queue is currently being served (deficitCounter has been updated for thie round)
		double start_time;	//time when this queue is inserted to active list
		PacketDWRR *next;	//pointer to next node

        int stat_max_byte {0};

        friend class DWRR;
};

class DWRR : public Queue
{
  	friend class DwrrXpassCreditTimer;
	friend class DwrrEXPQueueLogTimer;
	public:
		DWRR();
		~DWRR();
		virtual int command(int argc, const char*const* argv);

	protected:
		Packet *deque(void);
		void enque(Packet *pkt);
		
		int TotalByteLength();	//Get total length of all queues in bytes
		bool MarkingECN(int q);	//Determine whether we need to mark ECN, q is queue index
		void trace_total_qlen();	//routine to write total qlen records
		void trace_qlen();	//routine to write per-queue qlen records

  		void updateTokenBucket();
  		void expire();
		
		/* Variables */
		PacketDWRR *activeList;	//list for active queues
		PacketDWRR *queues;	//underlying multi-FIFO (CoS) queues
		double round_time;	//estimation value for round time
		double last_idle_time;	//Last time when link becomes idle

		Tcl_Channel total_qlen_tchan_;	//place to write total_qlen records
		Tcl_Channel qlen_tchan_;	//place to write per-queue qlen records

		int queue_num_;	//number of queues
		int mean_pktsize_;	//MTU in bytes
		double port_thresh_;	//per-port ECN marking threshold (pkts)
		int marking_scheme_;	//ECN marking policy
		double estimate_round_alpha_;	//factor between 0 and 1 for round time estimation
		int estimate_round_idle_interval_bytes_;	//Time interval (divided by link capacity) to update round time when link is idle.
		double link_capacity_;	//Link capacity
		int deque_marking_;	//shall we enable dequeue ECN/RED marking
		int debug_;	//debug more(true) or not(false)
		
		// ExpressPass
		int credit_q_limit_;
		int64_t tokens_;
		int64_t max_tokens_;
		double token_bucket_clock_;
		double token_refresh_rate_;
 		DwrrXpassCreditTimer credit_timer_;
  		int exp_id_;
		int data_q_limit_; // sum of non-credit queues
		int qid_;
		int selective_dropping_threshold_;
		int unscheduled_queue_len_;
		int enable_tos_;
		int enable_non_xpass_selective_dropping_{0};
		int selective_packet_queue_len_{0};
		int egdx_queue_scheme_;
		int credit_drop_count_;
    	BroadcomNode *broadcom_node_ingress_;
    	BroadcomNode *broadcom_node_egress_;
        int broadcom_node_ingress_port_;
		int broadcom_node_egress_port_;
        int enable_shared_buffer_;
        int strict_high_priority_;

        double credit_last_deque_{0};
		double debug_thpt_estimate_{0};
		std::map<int, int> debug_credit_drop_per_flow_;

#if LOG_QUEUE
#define LOG_QUEUE_MAX_CNT 8
		double last_log_;
		double last_sample_;
		double qavg_[LOG_QUEUE_MAX_CNT];
		int qmax_[LOG_QUEUE_MAX_CNT];
		int trace_;
		int qidx_;
		DwrrEXPQueueLogTimer LogTimer;
		void logQueue();
#endif
};

#endif