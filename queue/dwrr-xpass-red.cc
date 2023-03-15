#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <math.h>
#include "dwrr-xpass-red.h"
#include "flags.h"
#include "packet.h"
#include "xpass/flexpass.h"
#include "xpass/xpass.h"

#define max(arg1,arg2) (arg1>arg2 ? arg1 : arg2)
#define min(arg1,arg2) (arg1<arg2 ? arg1 : arg2)

#define MIN(A,B) ((A<B)?A:B)
#define SHARED_BUFFER_MODEL 0

#define CREDIT_QUEUE_STRICT_PRIORITY true

/* Insert a queue to the tail of an active list. Return true if insert succeeds */
static void InsertTailList(PacketDWRR* list, PacketDWRR *q)
{
	if (q && list)
	{
		PacketDWRR* tmp = list;
		while (true)
		{
			/* Arrive at the tail of this list */
			if (!(tmp->next))
			{
				tmp->next = q;
				q->next = NULL;
				return;
			}
			/* Move to next node */
			else
			{
				tmp = tmp->next;
			}
		}
	}
}

/* Remove and return the head node from the active list */
static PacketDWRR* RemoveHeadList(PacketDWRR* list)
{
	if (list)
	{
		PacketDWRR* tmp = list->next;
		if (tmp)
		{
			list->next = tmp->next;
			return tmp;
		}
		/* This list is empty */
		else
		{
			return NULL;
		}
	}
	else
	{
		return NULL;
	}
}

static class DWRRClass : public TclClass
{
	public:
		DWRRClass() : TclClass("Queue/DwrrXPassRED") { }
		TclObject* create(int argc, const char*const* argv)
		{
			return (new DWRR);
		}
} class_dwrr;

#if LOG_QUEUE
DWRR::DWRR(): credit_timer_(this) , LogTimer(this) {
#else
DWRR::DWRR(): credit_timer_(this) {
#endif
	static int queue_id = 0;
	qid_ = queue_id++;
	
	queues = new PacketDWRR[MAX_QUEUE_NUM];
	for (int i = 0; i < MAX_QUEUE_NUM; i++)
		queues[i].id = i;

	activeList = new PacketDWRR();
	round_time = 0;
	last_idle_time = 0;

	total_qlen_tchan_ = NULL;
	qlen_tchan_ = NULL;

	queue_num_ = 8;
	mean_pktsize_ = 1500;	//MTU
	port_thresh_ = 65;
	marking_scheme_ = PER_QUEUE_MARKING;
	estimate_round_alpha_ = 0.75;
	estimate_round_idle_interval_bytes_ = 1500;	//MTU by default
	link_capacity_ = 10000000000;	//10Gbps by default
	deque_marking_ = 0;	//perform enqueue ECN/RED marking by default
	debug_ = 0;
	unscheduled_queue_len_ = 0;
	selective_packet_queue_len_ = 0;
	credit_drop_count_ = 0;
	broadcom_node_ingress_ = NULL;
	broadcom_node_egress_ = NULL;

	/* bind variables */
	bind("queue_num_", &queue_num_);
	bind("mean_pktsize_", &mean_pktsize_);
	bind("port_thresh_", &port_thresh_);
	bind("marking_scheme_", &marking_scheme_);
	bind("estimate_round_alpha_", &estimate_round_alpha_);
	bind("estimate_round_idle_interval_bytes_", &estimate_round_idle_interval_bytes_);
	bind_bw("link_capacity_", &link_capacity_);
	bind_bool("deque_marking_", &deque_marking_);
	bind_bool("debug_", &debug_);

	bind("credit_limit_", &credit_q_limit_);
    bind("max_tokens_", &max_tokens_);
    bind("token_refresh_rate_", &token_refresh_rate_);
    bind("exp_id_", &exp_id_);
	bind("data_limit_", &data_q_limit_);
	bind("selective_dropping_threshold_", &selective_dropping_threshold_);
	bind("enable_non_xpass_selective_dropping_", &enable_non_xpass_selective_dropping_);
	bind("enable_tos_", &enable_tos_);
    bind("flexpass_queue_scheme_", &flexpass_queue_scheme_);
    bind("enable_shared_buffer_", &enable_shared_buffer_);
    bind("strict_high_priority_", &strict_high_priority_);
    tokens_ = 0;
    token_bucket_clock_ = 0;
#if LOG_QUEUE
    bind("qidx_", &qidx_);
    bind("trace_", &trace_);
    last_log_ = 0.0;
    last_sample_ = 0.0;
	for(int i=0; i< LOG_QUEUE_MAX_CNT; i++) {
		qavg_[i] = 0.0;
		qmax_[i] = 0;
	}
    LogTimer.resched(LOG_GRAN);
#endif
}

DWRR::~DWRR()
{
  delete activeList;
  delete [] queues;
}


/* Get total length of all queues in bytes */
int DWRR::TotalByteLength()
{
	int result = 0;

	for (int i = 0; i < queue_num_; i++)
		result+=queues[i].byteLength();

	return result;
}

/* Determine whether a packet needs to get ECN marked.
   q is queue index of the packet.
   Return true if the packet should be marked */
bool DWRR::MarkingECN(int q)
{
	if (q < 0 || q >= queue_num_)
	{
		fprintf(stderr, "illegal queue number\n");
		return false;
	}

	/* Per-queue ECN marking */
	if (marking_scheme_ == PER_QUEUE_MARKING)
	{
		if (queues[q].byteLength() > queues[q].thresh * mean_pktsize_)
			return true;
		else
			return false;
	}
	/* Per-port ECN marking */
	else if (marking_scheme_ == PER_PORT_MARKING)
	{
		if (TotalByteLength() > port_thresh_ * mean_pktsize_)
			return true;
		else
			return false;
	}
	/* MQ-ECN */
	else if (marking_scheme_ == MQ_MARKING)
	{
		double thresh = port_thresh_;
		if (round_time >= 0.000000001 && link_capacity_ > 0)
			thresh = min(queues[q].quantum * 8 / round_time / link_capacity_, 1) * port_thresh_;

		//For debug
		if(debug_)
			printf("round time: %.9f threshold of queue %d: %f\n", round_time, q, thresh);

		if (queues[q].byteLength() > thresh * mean_pktsize_)
			return true;
		else
			return false;
	}
	/* Unknown ECN marking scheme */
	else
	{
		fprintf(stderr, "Unknown ECN marking scheme\n");
		return false;
	}
}

/*
 *  entry points from OTcL to set per queue state variables
 *  - $q set-quantum queue_id queue_quantum (quantum is actually weight)
 *  - $q set-thresh queue_id queue_thresh
 *  - $q attach-total file
 *	- $q attach-queue file
 *
 *  NOTE: $q represents the discipline queue variable in OTcl.
 */
int DWRR::command(int argc, const char*const* argv)
{
	if (argc == 2) {
		
		if (strcmp(argv[1], "print-stat") == 0) {
            // for (auto it = debug_credit_drop_per_flow_.begin(); it != debug_credit_drop_per_flow_.end(); ++it) {
            //     printf("credit_drop_cnt_[%d]=%d\n", it->first, it->second);
            // }
            printf("credit_drop_cnt\t%d\n", credit_drop_count_);
			
            return (TCL_OK);
		} else if (strcmp(argv[1], "print-stat-queue") == 0) {
            for (int i = 0; i < queue_num_; i++) {
                printf("max_queue_buildup_%d\t%d\n", i, queues[i].stat_max_byte);
            }
            return (TCL_OK);
		}
	}
	if (argc == 3)
	{
		// attach a file to trace total queue length
		if (strcmp(argv[1], "attach-total") == 0)
		{
			int mode;
			const char* id = argv[2];
			Tcl& tcl = Tcl::instance();
			total_qlen_tchan_ = Tcl_GetChannel(tcl.interp(), (char*)id, &mode);
			if (total_qlen_tchan_ == 0)
			{
				tcl.resultf("DWRR: trace: can't attach %s for writing", id);
				return (TCL_ERROR);
			}
			return (TCL_OK);
		}
		else if (strcmp(argv[1], "attach-queue") == 0)
		{
			int mode;
			const char* id = argv[2];
			Tcl& tcl = Tcl::instance();
			qlen_tchan_ = Tcl_GetChannel(tcl.interp(), (char*)id, &mode);
			if (qlen_tchan_ == 0)
			{
				tcl.resultf("DWRR: trace: can't attach %s for writing", id);
				return (TCL_ERROR);
			}
			return (TCL_OK);
		}
		else if (strcmp(argv[1], "link") == 0)
		{
			// to mitigate dirty implementation in tcl/lib/ns-lib.tcl that checks for string "RED" in class name
			return (TCL_OK);
		}
		else if (strcmp(argv[1], "ingress-node") == 0) {
			TclObject *obj;
			if( (obj = TclObject::lookup(argv[2])) == 0) {
				fprintf(stderr, "%s lookup failed\n", argv[1]);
				return TCL_ERROR;
			}
            broadcom_node_ingress_ = reinterpret_cast<BroadcomNode *>(obj);
            broadcom_node_ingress_port_ = broadcom_node_ingress_->RegisterPort(this, true);
            return TCL_OK;
        }
		else if (strcmp(argv[1], "egress-node") == 0) {
			TclObject *obj;
			if( (obj = TclObject::lookup(argv[2])) == 0) {
				fprintf(stderr, "%s lookup failed\n", argv[1]);
				return TCL_ERROR;
			}
            broadcom_node_egress_ = reinterpret_cast<BroadcomNode *>(obj);
            broadcom_node_egress_port_ = broadcom_node_egress_->RegisterPort(this, false);
            return TCL_OK;
        }
	}
	else if (argc == 4)
	{
		if (strcmp(argv[1], "set-quantum")==0)
		{
			int queue_id = atoi(argv[2]);
			if (queue_id < queue_num_ && queue_id >= 0)
			{
				int quantum = atoi(argv[3]);
				if (quantum > 0)
				{
					queues[queue_id].quantum = quantum;
					return (TCL_OK);
				}
				else
				{
					fprintf(stderr,"illegal quantum value %s for queue %s\n", argv[3], argv[2]);
					exit(1);
				}
			}
			/* Exceed the maximum queue number or smaller than 0*/
			else
			{
				fprintf(stderr,"no such queue %s\n",argv[2]);
				exit(1);
			}
		}
		else if (strcmp(argv[1], "set-thresh") == 0)
		{
			int queue_id = atoi(argv[2]);
			if (queue_id < queue_num_ && queue_id >= 0)
			{
				double thresh = atof(argv[3]);
				if (thresh >= 0)
				{
					queues[queue_id].thresh = thresh;
					queues[queue_id].edp_.th_min = thresh * queues[queue_id].edp_.mean_pktsize;
					queues[queue_id].edp_.th_max = thresh * queues[queue_id].edp_.mean_pktsize;
					return (TCL_OK);
				}
				else
				{
					fprintf(stderr,"illegal thresh value %s for queue %s\n", argv[3],argv[2]);
					exit(1);
				}
			}
			/* Exceed the maximum queue number or smaller than 0*/
			else
			{
				fprintf(stderr,"no such queue %s\n",argv[2]);
				exit(1);
			}
		}
	}
	return (Queue::command(argc, argv));
}

static inline bool isPacketUnscheduled (Packet *p) {
	hdr_cmn *cmnh = hdr_cmn::access(p);
	int ptype = (int)cmnh->ptype();
	// printf("Packet type = %d\n", ptype);
	if(cmnh->ptype() == PT_XPASS_FIRSTRTT_DATA)
		return true;
	return false;
}

inline bool isPacketSelectiveDropTarget(int flexpass_queue_scheme_, hdr_cmn *cmnh) {
	if(flexpass_queue_scheme_ == 2) 
    	return cmnh->tos() == TOS_FLEXPASS_REACTIVE;
	else if (flexpass_queue_scheme_ == 4) 
    	return cmnh->tos() == TOS_FLEXPASS_REACTIVE || cmnh->tos() == TOS_DATA_TCP || cmnh->tos() == 0;
  return false;
}

static DWRR* debugNode = NULL;
/* Receive a new packet */
void DWRR::enque(Packet *p)
{
	hdr_ip *iph = hdr_ip::access(p);
	int prio = iph->prio();
	hdr_flags* hf = hdr_flags::access(p);
	int pktSize = hdr_cmn::access(p)->size();
	#if SHARED_BUFFER_MODEL
	int qlimBytes = qlim_*mean_pktsize_;
	#endif
	double now = Scheduler::instance().clock();
	queue_num_ = max(min(queue_num_, MAX_QUEUE_NUM), 1);

	// if (broadcom_node_egress_) {
	// 	broadcom_node_egress_->RegisterSwitchingTable(iph->daddr(), broadcom_node_egress_port_);
	// }

    if (enable_shared_buffer_ && __glibc_unlikely(!broadcom_node_ingress_ && !broadcom_node_egress_)) {
        fprintf(stderr, "ERROR: Nither ingress Broadcom Node nor egress Broadcom Node has been specified. This should be not what you want.\n");
        abort();
    }

    if (TotalByteLength() == 0)
	{
		double idle_interval = now - last_idle_time;
		int idle_slot_num = 0;

		/* Update round_time */
		if (estimate_round_idle_interval_bytes_ > 0 && link_capacity_ > 0)
		{
			idle_slot_num = int(idle_interval / (estimate_round_idle_interval_bytes_ * 8 / link_capacity_));
			round_time = round_time * pow(estimate_round_alpha_, idle_slot_num);
		}
		else
		{
			round_time = 0;
		}

		if (debug_ && marking_scheme_ == MQ_MARKING)
			printf("%.9f round time is reset to %.9f after %d idle time slots\n", now, round_time, idle_slot_num);
	}

	/* Set priority: Credit = 0, XPassData = 1, TCP = 2 */
	hdr_cmn* cmnh = hdr_cmn::access(p);

	if (cmnh->ptype() == PT_XPASS_CREDIT) {
		if (queues[0].byteLength() + pktSize  > credit_q_limit_) {
			hdr_flexpass* flexpassh = hdr_flexpass::access(p);
			hdr_xpass* xpassh = hdr_xpass::access(p);
      		int fid = flexpassh->fid_ ? flexpassh->fid_ : xpassh->fid_;
			if (debug_credit_drop_per_flow_.find(fid) == debug_credit_drop_per_flow_.end())
				debug_credit_drop_per_flow_[fid] = 0;
            debug_credit_drop_per_flow_[fid] += 1;
            // printf("Drop fid=%d\n", fid);
            drop(p);
			credit_drop_count_++;
			return;
		}
		prio = 0;
	} else {
		if (enable_tos_) {
			if (flexpass_queue_scheme_ == 1) {
				// Pro / Rea,  Legacy
				if (cmnh->tos() == TOS_FLEXPASS_PROACTIVE) 
					prio = 1;
				else
					prio = 2;
            } else if (flexpass_queue_scheme_ == 2) {
                // Pro, Rea / Legacy
                if (cmnh->tos() == TOS_FLEXPASS_REACTIVE || cmnh->tos() == TOS_FLEXPASS_PROACTIVE)
                    prio = 1;
                else
                    prio = 2;
            } else if (flexpass_queue_scheme_ == 3) {
                // Pro / Rea / Legacy
                assert(queue_num_ >= 3);
                if (cmnh->tos() == TOS_FLEXPASS_PROACTIVE)
                    prio = 2;
                else if (cmnh->tos() == TOS_FLEXPASS_REACTIVE)
                    prio = 1;
                else
                    prio = 3;
            } else if (flexpass_queue_scheme_ == 4) {
                // Pro, Rea, Legacy
                prio = 2;
            } else {
                fprintf(stderr, "Unexpected flexpass_queue_scheme_ %d\n", flexpass_queue_scheme_);
				abort();
			}
			// for comparison purpose
			if (cmnh->tos() == TOS_XPASS_DATA) {
                prio = 1;
            }
        } else {
            prio = 2;
        }
    }

	if(prio == 0 && cmnh->ptype() != PT_XPASS_CREDIT)
		abort();


    bool buffer_drop = 0;
    int dest_device = -1;

    if (enable_shared_buffer_ && __glibc_likely(broadcom_node_ingress_ != 0)) {
        dest_device = broadcom_node_ingress_->FindDestinationPort(iph->daddr());
		if (dest_device < 0) {
			dest_device = broadcom_node_ingress_->EstimateNextHop(this, p); 
			if (dest_device >= 0) {
                broadcom_node_ingress_->RegisterSwitchingTable(iph->daddr(), dest_device);
            }
            // printf("%lf: Enqueue packet on %p, %p Physical Port %d -> %d (Estimated) \n", now, this, broadcom_node_ingress_, broadcom_node_ingress_port_ ,dest_device);
		} else {
            // printf("%lf: Enqueue packet on %p, %p Physical Port %d -> %d (Cached)\n", now, this, broadcom_node_ingress_, broadcom_node_ingress_port_ ,dest_device);
		}
        buffer_drop = !(broadcom_node_ingress_->CheckIngressAdmission(broadcom_node_ingress_port_, prio, pktSize) && broadcom_node_ingress_->CheckEgressAdmission(dest_device, prio, pktSize));
    } else {
        /* The shared buffer is overfilld */
		#if SHARED_BUFFER_MODEL
			buffer_drop = (data_q_limit_ == 0 ? (TotalByteLength() + pktSize > qlimBytes) : (TotalByteLength() + pktSize > data_q_limit_));
		#else
			buffer_drop = (queues[prio].byteLength() + pktSize > data_q_limit_);
		#endif
    }
    bool unimportant = false;

    if (isPacketUnscheduled(p)) {
		printf("FIRSTRTT_DATA\n");
		if (pktSize + unscheduled_queue_len_ > selective_dropping_threshold_)
		{
			printf("Dropping unscheduled_queue (unscheduled_queue_len_=%d)\n", unscheduled_queue_len_);
			buffer_drop = 1;
		} else {
			unscheduled_queue_len_ += pktSize;
		}
	} else if (enable_non_xpass_selective_dropping_ && isPacketSelectiveDropTarget(flexpass_queue_scheme_, cmnh)) {
        unimportant = true;
        if (enable_shared_buffer_ && broadcom_node_ingress_) {
            if (!(broadcom_node_ingress_->CheckIngressTLT(broadcom_node_ingress_port_, prio, pktSize) && broadcom_node_ingress_->CheckEgressTLT(dest_device, prio, pktSize))) {
                buffer_drop = true;
            }
        } else {
            if (selective_packet_queue_len_ + pktSize > selective_dropping_threshold_) {
                buffer_drop = 1;
            } else {
                selective_packet_queue_len_ += pktSize;
            }
        }
	}

	/* Hard drop */
    if (prio != 0 && buffer_drop)
	{
		drop(p);
		return;
	}
	
	assert(prio == 0 || prio == 1 || prio == 2 || prio == 3);
  	int enque_result = queues[prio].enque_(p);

    if (enable_shared_buffer_ && broadcom_node_ingress_) {
        broadcom_node_ingress_->UpdateIngressAdmission(broadcom_node_ingress_port_, prio, pktSize, p);
	    broadcom_node_ingress_->UpdateEgressAdmission(dest_device, prio, pktSize, p);
		if (unimportant) {
            broadcom_node_ingress_->UpdateIngressTLT(dest_device, prio, pktSize, p);
            broadcom_node_ingress_->UpdateEgressTLT(dest_device, prio, pktSize, p);
        }
	}

    if (enable_shared_buffer_ && enque_result != 0 && broadcom_node_ingress_) {
        fprintf(stderr, "ERROR: BroadcomNode should instruct to drop, not RED queue itself. limit_ of RED queue should be higher than shared buffer size.\n");
        abort();
    }
	
	if(queues[prio].byteLength() > queues[prio].stat_max_byte) {
        queues[prio].stat_max_byte = queues[prio].byteLength();
    }

    /* if queues[prio] is not in activeList */
	#if CREDIT_QUEUE_STRICT_PRIORITY
	/* Don't add queue 0 (Strict Priority) to activeList */
	if (prio != 0 && queues[prio].active == false)
	#else
	if (queues[prio].active == false)
	#endif
	{
		queues[prio].deficitCounter = 0;
		queues[prio].active = true;
		queues[prio].current = false;
		queues[prio].start_time = max(now, queues[prio].start_time);	//Start time of this round
		InsertTailList(activeList, &queues[prio]);
	}
	/* Enqueue ECN marking */
	/* Don't mark ECN to queue 0 (Strict Priority) */
	/* Don't mark here. red.cc will mark */
	// if (prio != 0 && deque_marking_ == 0 && MarkingECN(prio) && hf->ect())
	// 	hf->ce() = 1;
	#if LOG_QUEUE
	if (trace_) {
		double now = Scheduler::instance().clock();
		for(int i=0; i<queue_num_; i++) {
			int qlen = queues[i].byteLength();
			if (qlen > qmax_[i]) {
			qmax_[i] = qlen;
			}
			qavg_[i] += MIN((now-last_sample_) / (double)LOG_GRAN, 1.0) * qlen;
		}
		last_sample_ = now;
	}
	#endif
}

Packet *DWRR::deque(void)
{
	PacketDWRR *headNode = NULL;
	Packet *pkt = NULL;
	hdr_flags* hf = NULL;
	int pktSize = 0;
	double round_time_sample = 0;
	double now = Scheduler::instance().clock();

	credit_timer_.force_cancel();
  	updateTokenBucket();

	/*At least one queue is active, activeList is not empty */
	if (TotalByteLength() > 0)
	{
		#if CREDIT_QUEUE_STRICT_PRIORITY
		/* Queue 0 is Strict Priority Queue (for credit) */
		if (queues[0].length() > 0) {
			pkt = queues[0].head();
			assert(pkt);
			int pkt_size = hdr_cmn::access(pkt)->size();
			if (tokens_ >= pkt_size) {
				pkt = queues[0].deque();
				double rate = 1. / (now - credit_last_deque_);

				if (debug_thpt_estimate_ > 0) {
					debug_thpt_estimate_ = debug_thpt_estimate_ * 0.75 + rate * 0.25;
				} else {
					debug_thpt_estimate_ = rate;
				}

				if (now > credit_last_deque_)
				{
					// log_lst_.push_back(std::pair<double, double>(rate, debug_thpt_estimate_));
					// printf("[%.8p] Rate : %lf Hz = %lf GbpsEquiv (smoothed %lf Hz, %lf GbpsEquiv)\n", this, rate, 1538. * rate / 1000000000 * 8, debug_thpt_estimate_, 1538. * debug_thpt_estimate_ / 1000000000 * 8);

				}
				credit_last_deque_ = now;

				tokens_ -= pkt_size;

                if (enable_shared_buffer_ && broadcom_node_ingress_) {
                    broadcom_node_ingress_->RemoveFromIngressAdmission(pkt);
					broadcom_node_ingress_->RemoveFromEgressAdmission(pkt);
					// credit should never be unimportant
				}
				#if LOG_QUEUE
				// if (trace_) {
				// double now = Scheduler::instance().clock();
				// int qlen = credit_q_->byteLength() + q_->byteLength();
				// qavg_ += MIN((now-last_sample_) / (double)LOG_GRAN, 1.0) * qlen;
				// last_sample_ = now;
				// }
				if (trace_) {
					double now = Scheduler::instance().clock();
					for(int i=0; i < queue_num_; i++) {
						int qlen = queues[i].byteLength();
						qavg_[i] += MIN((now-last_sample_) / (double)LOG_GRAN, 1.0) * qlen;
					}
					last_sample_ = now;
				}
				#endif
				return pkt;
			} else {
    			double delay = (pkt_size - tokens_) / token_refresh_rate_;
				if(credit_timer_.status() == TIMER_PENDING) abort();
				assert(credit_timer_.status() != TIMER_PENDING);
				credit_timer_.resched(delay);
				// printf("Resch credit timer\n");
				pkt = NULL; // reset pkt
			}
		}
		#endif

		if (strict_high_priority_) {
            for (int i = 1; i < queue_num_; i++) {
				if (queues[i].length() == 0)
                    continue;
                pkt = queues[i].deque();
                break;
            }
        } else {

			/* Queue 1 to 7 is DWRR (or 0 to 7 if credit queue is not strict) */
			/* We must go through all actives queues and select a packet to dequeue */
			while (1) {
				#if CREDIT_QUEUE_STRICT_PRIORITY
				assert(activeList != &queues[0]);
				#endif
				headNode = activeList->next;	//Get head node from activeList
				if (!headNode) {
					break;
				//	fprintf (stderr,"no active flow\n");
				}
				

				/* if headNode is not empty */
				if (headNode->length() > 0)
				{
					/* headNode has not been served yet in this round */
					if (headNode->current == false)
					{
						headNode->deficitCounter += headNode->quantum;
						headNode->current = true;
					}

					pktSize = hdr_cmn::access(headNode->head())->size();
					/* if we have enough quantum to dequeue the head packet */
					#if !(CREDIT_QUEUE_STRICT_PRIORITY)
					if (pktSize <= headNode->deficitCounter && ((headNode != &queues[0]) || (tokens_ >= pktSize)))
					#else
					if (pktSize <= headNode->deficitCounter)
					#endif
					{
						pkt = headNode->deque();
						headNode->deficitCounter -= pktSize;
						
						if (this == debugNode) {
							// printf("OK %d %d %d\n", (headNode - &queues[0]), pktSize, headNode->deficitCounter);
						}
						hf = hdr_flags::access(pkt);

						if (headNode == &queues[0]) {
							tokens_ -= pktSize;
						}

						/* Dequeue ECN marking */
						
						/* Don't mark here. red.cc will mark */
						// if (deque_marking_ > 0 && MarkingECN(headNode->id) > 0 && hf->ect())
						// 	hf->ce() = 1;

						// printf("Dequeue %s ECN=%d (MarkingScheme %d), Q(%p) Qid=%ld, Qlen=%d, Thresh=%lf\n", headNode-&queues[1] ? "DCTCP" : "XPASS", (int)hf->ce(), marking_scheme_, headNode, headNode-&queues[0], headNode->byteLength(), headNode->thresh);
						/* After dequeue, headNode becomes empty. In such case, we should delete this queue from activeList. */
						if (headNode->length() == 0)
						{
							round_time_sample = now + pktSize * 8 / link_capacity_ - headNode->start_time ;
							round_time = round_time * estimate_round_alpha_ + round_time_sample * (1 - estimate_round_alpha_);

							if (debug_ && marking_scheme_ == MQ_MARKING)
								printf("%.9f queue: %d sample round time: %.9f round time: %.9f\n", now, headNode->id, round_time_sample, round_time);

							headNode = RemoveHeadList(activeList);
							headNode->start_time = now + pktSize * 8 / link_capacity_;
							headNode->deficitCounter = 0;
							headNode->active = false;
							headNode->current = false;
						}
						break;
					}
					/* If we don't have enough quantum to dequeue the head packet and the queue is not empty */
					else
					{
						if (this == debugNode) {
							// printf("Fail %d %d %d\n", (headNode - &queues[0]), pktSize, headNode->deficitCounter);
						}
						/* If credit queue could not be scheduled due to lack of tokens */
						// printf("pktSize = %d\n", pktSize);
						if (headNode == &queues[0] && tokens_ < pktSize) {
							if (credit_timer_.status() != TIMER_IDLE) {
								/* Only credit packet is remaining in the entire queue, but cannot dequeue due to lack of token */
								break;
							}
							double delay = (pktSize - tokens_) / token_refresh_rate_;
							assert(credit_timer_.status() == TIMER_IDLE);
							credit_timer_.resched(delay);
							// printf("Resch credit timer\n");
						}
						headNode = RemoveHeadList(activeList);
						headNode->current = false;
						round_time_sample = now - headNode->start_time;
						round_time = round_time * estimate_round_alpha_ + round_time_sample * (1-estimate_round_alpha_);

						if (debug_ && marking_scheme_ == MQ_MARKING)
							printf("%.9f queue: %d sample round time: %.9f round time: %.9f\n", now, headNode->id, round_time_sample, round_time);

						headNode->start_time = now;	//Reset start time
						InsertTailList(activeList, headNode);
					}
				}
			}
		}
	}

	if (TotalByteLength() == 0)
		last_idle_time = now;

	trace_qlen();
	trace_total_qlen();
	
	if (pkt) {
		hdr_cmn* cmnh = hdr_cmn::access(pkt);
		if(isPacketUnscheduled(pkt)) {
			assert(unscheduled_queue_len_ >= cmnh->size());
			unscheduled_queue_len_ -= cmnh->size();
		}
        bool unimportant = false;
        if (isPacketSelectiveDropTarget(flexpass_queue_scheme_, cmnh)) {
            unimportant = true;
            if (!enable_shared_buffer_  || !broadcom_node_ingress_) {
                assert(selective_packet_queue_len_ >= cmnh->size());
                selective_packet_queue_len_ -= cmnh->size();
			}
		}

        if (enable_shared_buffer_ && broadcom_node_ingress_) {
            broadcom_node_ingress_->RemoveFromIngressAdmission(pkt);
            broadcom_node_ingress_->RemoveFromEgressAdmission(pkt);
			if (unimportant) {
                broadcom_node_ingress_->RemoveFromIngressTLT(pkt);
                broadcom_node_ingress_->RemoveFromEgressTLT(pkt);
            }
        }
    }

	
	#if LOG_QUEUE
	if (trace_) {
		double now = Scheduler::instance().clock();
		for(int i=0; i < queue_num_; i++) {
			int qlen = queues[i].byteLength();
			qavg_[i] += MIN((now-last_sample_) / (double)LOG_GRAN, 1.0) * qlen;
		}
		last_sample_ = now;
	}
	#endif
    return pkt;
}

/* routine to write total qlen records */
void DWRR::trace_total_qlen()
{
	if (total_qlen_tchan_)
	{
		char wrk[500] = {0};
		int n;
		double t = Scheduler::instance().clock();
		sprintf(wrk, "%g, %d", t,TotalByteLength());
		n = strlen(wrk);
		wrk[n] = '\n';
		wrk[n+1] = 0;
		(void)Tcl_Write(total_qlen_tchan_, wrk, n+1);
	}
}

/* routine to write per-queue qlen records */
void DWRR::trace_qlen()
{
	if (qlen_tchan_)
	{
		char wrk[500] = {0};
		int n;
		double t = Scheduler::instance().clock();
		sprintf(wrk, "%g", t);
		n = strlen(wrk);
		wrk[n] = 0;
		(void)Tcl_Write(qlen_tchan_, wrk, n);

		for (int i = 0; i < queue_num_; i++)
		{
			sprintf(wrk, ", %d",queues[i].byteLength());
			n = strlen(wrk);
			wrk[n] = 0;
			(void)Tcl_Write(qlen_tchan_, wrk, n);
		}
		(void)Tcl_Write(qlen_tchan_, "\n", 1);
	}
}

void DWRR::expire() {
  Packet *p;
  double now = Scheduler::instance().clock();

  if (!blocked_) {
    p = deque();
    if (p) {
      utilUpdate(last_change_, now, blocked_);
      last_change_ = now;
      blocked_ = 1;
      target_->recv(p, &qh_);
    }
  }
}

void DWRR::updateTokenBucket() {
  double now = Scheduler::instance().clock();
  double elapsed_time = now - token_bucket_clock_;
  int64_t new_tokens;

  if (elapsed_time <= 0.0) {
    return;
  }

  new_tokens = (int64_t)(elapsed_time * token_refresh_rate_);
  tokens_ += new_tokens;
  tokens_ = min(tokens_, max_tokens_);
  
  token_bucket_clock_ += new_tokens / token_refresh_rate_;
}

void DwrrXpassCreditTimer::expire (Event *) {
  a_->expire();
}


#if LOG_QUEUE
void DWRR::logQueue() {
  double now = Scheduler::instance().clock();
  FILE *flog;
  for(int i=0;i<queue_num_; i++) {
	int qlen = queues[i].byteLength();
  	qavg_[i] += MIN((now-last_sample_) / (double)LOG_GRAN, 1.0) * qlen;
  }
  
  if (now >= LOG_GRAN) {
    char fname[1024];
    sprintf(fname,"outputs/queue_exp%d_%d.tr", exp_id_, qidx_);
    flog = fopen(fname, "a");
	fprintf(flog, "%lf ", now);
    for (int i=0; i<queue_num_; i++) {
		fprintf(flog, "%lf %d ", qavg_[i], qmax_[i]);
	}
    fprintf(flog, "\n");
    fclose(flog);
  }

  last_log_ = now;
  last_sample_ = now;


    for (int i=0; i<queue_num_; i++) {
  		qavg_[i] = 0.0;
  		qmax_[i] = 0;
	}
  if (trace_) {
    LogTimer.resched(LOG_GRAN);
  }
}

void DwrrEXPQueueLogTimer::expire(Event *) {
  a_->logQueue();
}

#endif
