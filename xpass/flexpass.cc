#include "flexpass.h"
#include <cmath>
#include <csignal>
#include "../tcp/template.h"
#include "random.h"

#define DEBUG_FLEXPASS 0
#if DEBUG_FLEXPASS
#define NS_LOG(f_, ...)                                                                                                     \
    printf(("[%8.4lf][%3d] (%s:%d) %s " f_ "\n"), (now() * 1000) - 100, fid_, __FILE__, __LINE__, __func__, ##__VA_ARGS__); \
    fflush(stdout)
#define NS_ASSERT(f)                            \
    if (!(f)) {                                 \
        NS_LOG("Assertion \"" #f "\" failed."); \
        raise(SIGINT);                          \
    }
#define FLOW_DEBUG(msg_, ...)                                         \
    {                                                                 \
        if (fid_ == target_fid_) FLOW_DEBUG_ALL(msg_, ##__VA_ARGS__); \
    }
#define FLOW_DEBUG_COLOR(color, msg_, ...)                                         \
    {                                                                              \
        if (fid_ == target_fid_) FLOW_DEBUG_ALL_COLOR(color, msg_, ##__VA_ARGS__); \
    }
#define FLOW_DEBUG_ALL(msg_, ...) FLOW_DEBUG_ALL_COLOR("[0m", msg_, ##__VA_ARGS__);
#define FLOW_DEBUG_ALL_COLOR(color, msg_, ...) printf("[%8.4lf][%3d] (%s:%.4d %p) \033" color msg_ "\033[0m\n", (now() * 1000) - 100, fid_, __FILE__, __LINE__, this, ##__VA_ARGS__)
#define FLOW_DEBUG_INT(time)                                                                                \
    {                                                                                                       \
        if (fid_ == target_fid_ && (round((now() - 0.1) * 10000000) == round((time)*10000))) raise(SIGINT); \
    }

#else
#define NS_DEBUG(f_, ...)
#define NS_ASSERT(f)
#define FLOW_DEBUG(msg, ...)
#define FLOW_DEBUG_ALL(msg_, ...)
#define FLOW_DEBUG_COLOR(color, msg_, ...)
#define FLOW_DEBUG_ALL_COLOR(color, msg_, ...)
#define FLOW_DEBUG_INT(time)
#endif

#define COLOR_RCV "[0;32m"
#define COLOR_SND "[0;36m"
#define COLOR_WARN "[0;31m"
#define PRINTF_COLOR(color, msg_, ...) printf("[%8.4lf][%3d] (%s:%.4d) \033" color msg_ "\033[0m\n", (now() * 1000) - 100, fid_, __FILE__, __LINE__, ##__VA_ARGS__)

#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define MIN(a, b) (((a) > (b)) ? (b) : (a))

int hdr_flexpass::offset_;
static const int target_fid_ = 1;

static struct stat_ {
    int64_t tx_total_proactive{0};
    int64_t tx_total_reactive{0};
    int64_t tx_total_recovery{0};
	int64_t rx_total_bytes{0};
	int64_t rx_needed_bytes{0};
	int64_t rx_total_reduntant_bytes{0};
	double max_credit_interval{0.0};
	int max_credit_interval_fid{0};
} stat;

struct per_flow_stat {
    int64_t tx_total_proactive{0};
    int64_t tx_total_reactive{0};
    int64_t tx_total_recovery{0};
    int64_t credit_wasted{0};
};

static std::unordered_map<int, struct per_flow_stat> g_p_per_flow_stat;

static class FlexPassHeaderClass : public PacketHeaderClass {
   public:
    FlexPassHeaderClass() : PacketHeaderClass("PacketHeader/FLEXPASS", sizeof(hdr_flexpass)) {
        bind_offset(&hdr_flexpass::offset_);
    }
} class_FlexPasshdr;

static class FlexPassClass : public TclClass {
   public:
    FlexPassClass() : TclClass("Agent/TCP/FullTcp/FlexPass") {}
    TclObject *create(int, const char *const *) {
        return (new FlexPassAgent());
    }
} class_flexpass;

void FlexPassSendCreditTimer::expire(Event *) {
    a_->send_credit();
}

void FlexPassCreditStopTimer::expire(Event *) {
    a_->send_credit_stop();
}

void FlexPassSenderRetransmitTimer::expire(Event *) {
    a_->handle_sender_retransmit();
}

void FlexPassReceiverRetransmitTimer::expire(Event *) {
    a_->handle_receiver_retransmit();
}

void FlexPassFCTTimer::expire(Event *) {
    a_->handle_fct();
}

void FlexPassAllocateTxTimer::expire(Event *) {
    a_->allocate_tx_bytes(xpass_, 1);
}

#if AEOLUS
void FlexPassRestartFirstRttSendTimer::expire(Event *) {
    // a_->send(a_->construct_credit_request(0), 0);
}
#endif

void FlexPassAgent::delay_bind_init_all() {
    delay_bind_init_one("max_credit_rate_");
    delay_bind_init_one("base_credit_rate_");
    delay_bind_init_one("alpha_");
    delay_bind_init_one("target_loss_scaling_");
    delay_bind_init_one("min_credit_size_");
    delay_bind_init_one("max_credit_size_");
    delay_bind_init_one("min_ethernet_size_");
    delay_bind_init_one("max_ethernet_size_");
    delay_bind_init_one("xpass_hdr_size_");
    delay_bind_init_one("w_init_");
    delay_bind_init_one("min_w_");
    delay_bind_init_one("retransmit_timeout_");
    delay_bind_init_one("default_credit_stop_timeout_");
    delay_bind_init_one("min_jitter_");
    delay_bind_init_one("max_jitter_");
    delay_bind_init_one("exp_id_");
#if CFC_ALG == CFC_BIC
    delay_bind_init_one("bic_s_min_");
    delay_bind_init_one("bic_s_max_");
    delay_bind_init_one("bic_beta_");
#endif
    delay_bind_init_one("cur_credit_rate_tr_");

#if AEOLUS
    /* Aeolus */
    delay_bind_init_one("max_link_rate_");  //max link rate
    delay_bind_init_one("base_rtt_");       // base rtt
    delay_bind_init_one("unscheduled_burst_period_");
    delay_bind_init_one("aeolus_firstrtt_burst_");
#endif

    delay_bind_init_one("flexpass_beta_");
    delay_bind_init_one("flexpass_beta_min_");
    delay_bind_init_one("enable_ack_");
    delay_bind_init_one("flexpass_xpass_prioritized_bytes_");
    delay_bind_init_one("debug_flowsize_");

    delay_bind_init_one("rc3_mode_");
    delay_bind_init_one("new_allocation_logic_");
    delay_bind_init_one("reordering_measure_in_rc3_");
    delay_bind_init_one("static_allocation_");

    SackFullTcpAgent::delay_bind_init_all();
}

int FlexPassAgent::delay_bind_dispatch(const char *varName, const char *localName,
                                   TclObject *tracer) {
    if (delay_bind(varName, localName, "max_credit_rate_", &max_credit_rate_,
                   tracer)) {
        return TCL_OK;
    }
    if (delay_bind(varName, localName, "base_credit_rate_", &base_credit_rate_,
                   tracer)) {
        return TCL_OK;
    }
    if (delay_bind(varName, localName, "alpha_", &alpha_, tracer)) {
        return TCL_OK;
    }
    if (delay_bind(varName, localName, "target_loss_scaling_", &target_loss_scaling_,
                   tracer)) {
        return TCL_OK;
    }
    if (delay_bind(varName, localName, "min_credit_size_", &min_credit_size_,
                   tracer)) {
        return TCL_OK;
    }
    if (delay_bind(varName, localName, "max_credit_size_", &max_credit_size_,
                   tracer)) {
        return TCL_OK;
    }
    if (delay_bind(varName, localName, "min_ethernet_size_", &min_ethernet_size_,
                   tracer)) {
        return TCL_OK;
    }
    if (delay_bind(varName, localName, "max_ethernet_size_", &max_ethernet_size_,
                   tracer)) {
        return TCL_OK;
    }
    if (delay_bind(varName, localName, "xpass_hdr_size_", &xpass_hdr_size_,
                   tracer)) {
        return TCL_OK;
    }
    if (delay_bind(varName, localName, "w_init_", &w_init_, tracer)) {
        return TCL_OK;
    }
    if (delay_bind(varName, localName, "min_w_", &min_w_, tracer)) {
        return TCL_OK;
    }
    if (delay_bind(varName, localName, "retransmit_timeout_", &retransmit_timeout_,
                   tracer)) {
        return TCL_OK;
    }
    if (delay_bind(varName, localName, "default_credit_stop_timeout_", &default_credit_stop_timeout_,
                   tracer)) {
        return TCL_OK;
    }
    if (delay_bind(varName, localName, "max_jitter_", &max_jitter_, tracer)) {
        return TCL_OK;
    }
    if (delay_bind(varName, localName, "min_jitter_", &min_jitter_, tracer)) {
        return TCL_OK;
    }
#if CFC_ALG == CFC_BIC
    if (delay_bind(varName, localName, "bic_s_min_", &bic_s_min_, tracer)) {
        return TCL_OK;
    }
    if (delay_bind(varName, localName, "bic_s_max_", &bic_s_max_, tracer)) {
        return TCL_OK;
    }
    if (delay_bind(varName, localName, "bic_beta_", &bic_beta_, tracer)) {
        return TCL_OK;
    }
#endif
    if (delay_bind(varName, localName, "cur_credit_rate_tr_", &cur_credit_rate_tr_, tracer)) {
        return TCL_OK;
    }

#if AEOLUS
    /* Aeolus */
    if (delay_bind(varName, localName, "max_link_rate_", &max_link_rate_,
                   tracer)) {
        return TCL_OK;
    }
    if (delay_bind(varName, localName, "base_rtt_", &base_rtt_,
                   tracer)) {
        return TCL_OK;
    }
    if (delay_bind(varName, localName, "unscheduled_burst_period_", &unscheduled_burst_period_,
                   tracer)) {
        return TCL_OK;
    }
#endif

    if (delay_bind(varName, localName, "flexpass_beta_", &flexpass_beta_, tracer)) {
        return TCL_OK;
    }
    if (delay_bind(varName, localName, "flexpass_beta_min_", &flexpass_beta_min_, tracer)) {
        return TCL_OK;
    }
    if (delay_bind(varName, localName, "enable_ack_", &enable_ack_, tracer)) {
        return TCL_OK;
    }
    if (delay_bind(varName, localName, "flexpass_xpass_prioritized_bytes_", &flexpass_xpass_prioritized_bytes_, tracer)) {
        return TCL_OK;
    }

    if (delay_bind(varName, localName, "debug_flowsize_", &debug_flowsize_, tracer)) {
        return TCL_OK;
    }

    if (delay_bind(varName, localName, "rc3_mode_", &rc3_mode_, tracer)) {
        return TCL_OK;
    }
    
    if (delay_bind(varName, localName, "new_allocation_logic_", &new_allocation_logic_, tracer)) {
        return TCL_OK;
    }

    if (delay_bind(varName, localName, "reordering_measure_in_rc3_", &reordering_measure_in_rc3_, tracer)) {
        return TCL_OK;
    }

    if (delay_bind(varName, localName, "static_allocation_", &static_allocation_, tracer)) {
        return TCL_OK;
    }

    return SackFullTcpAgent::delay_bind_dispatch(varName, localName, tracer);
}

void FlexPassAgent::init() {
    w_ = w_init_;
#if CFC_ALG == CFC_BIC
    bic_target_rate_ = base_credit_rate_ / 2;
#endif
    last_credit_rate_update_ = now();
}

int FlexPassAgent::command(int argc, const char *const *argv) {
    if (argc == 2) {
        if (strcmp(argv[1], "listen") == 0) {
            listen();
            SackFullTcpAgent::listen();
            return TCL_OK;
        } else if (strcmp(argv[1], "stop") == 0) {
            //on_transmission_ = false;
            return TCL_OK;
        } else if (strcmp(argv[1], "print-stat") == 0) {
            printf("tx_total_proactive\t%ld\n", stat.tx_total_proactive);
            printf("tx_total_reactive\t%ld\n", stat.tx_total_reactive);
            printf("tx_total_recovery\t%ld\n", stat.tx_total_recovery);
            printf("rx_total_bytes\t%ld\n", stat.rx_total_bytes);
            printf("rx_needed_bytes\t%ld\n", stat.rx_needed_bytes);
            printf("rx_total_reduntant_bytes\t%ld\n", stat.rx_total_reduntant_bytes);
            printf("max_credit_interval_ms\t%lf\n", stat.max_credit_interval * 1000);
            printf("max_credit_interval_fid\t%d\n", stat.max_credit_interval_fid);
            return TCL_OK;
        }
    } else if (argc == 3) {
        if (strcmp(argv[1], "advance-bytes") == 0) {
            if (credit_recv_state_ == FLEXPASS_RECV_CLOSED) {
                advance_bytes(atol(argv[2]));
                return TCL_OK;
            } else {
                return TCL_ERROR;
            }
        }
    }
    return Agent::command(argc, argv);
}

FILE *fo_debug_ackno = NULL;

void FlexPassAgent::recv(Packet *pkt, Handler *h) {
    hdr_cmn *cmnh = hdr_cmn::access(pkt);

    switch (cmnh->ptype()) {
    case PT_XPASS_CREDIT_REQUEST:
        recv_credit_request(pkt);
        Packet::free(pkt);
        break;
    case PT_XPASS_CREDIT:
        recv_credit(pkt);
        Packet::free(pkt);
        break;
    case PT_XPASS_DATA:
        recv_data(pkt);
        Packet::free(pkt);
        break;
    case PT_XPASS_CREDIT_STOP:
        recv_credit_stop(pkt);
        Packet::free(pkt);
        break;
    case PT_XPASS_NACK:
        recv_nack(pkt);
        Packet::free(pkt);
        break;
    case PT_XPASS_DATA_ACK:
        recv_ack(pkt);
        Packet::free(pkt);
        break;
#if AEOLUS
    /* Aeolus */
    case PT_XPASS_RETRANSMITTED_DATA:
        recv_retransmitted_data(pkt);
        break;
    case PT_XPASS_FIRSTRTT_DATA:
        recv_firstrtt_data(pkt);
        break;
    case PT_XPASS_DATA_ACK:
        recv_data_ack(pkt);
        break;
    case PT_XPASS_PROBE:
        recv_probe(pkt);
        break;
    case PT_XPASS_PROBE_ACK:
        recv_probe_ack(pkt);
        break;
#endif
    default: {
        hdr_tcp *tcph = hdr_tcp::access(pkt);
        bool syn = !!(tcph->flags() & TH_SYN);
        bool fin = !!(tcph->flags() & TH_FIN);
        bool ack = !!(tcph->flags() & TH_ACK);
        int datalen = cmnh->size() - tcph->hlen();
	    stat.rx_total_bytes += datalen;

        if (ack) {
            if (tcph->ackno() > highest_reactive_ack_)
                highest_reactive_ack_ = tcph->ackno();
        }
        if (syn && ack) {
            double rtt = now() - syn_sent_time_;

            FLOW_DEBUG_COLOR(COLOR_SND, "RECV Reactive  SYNACK! Ack=%ld", tcph->ackno());
            allocate_tx_bytes(false, 0);
        } else if (ack && !datalen) {
            auto it = packet_tcp_sent_timestamp_.find(tcph->ackno());
            if (it != packet_tcp_sent_timestamp_.end()) {
                if (it->second >= 0) {
                    double rtt = now() - it->second;
                    FLOW_DEBUG_COLOR(COLOR_SND, "RECV Reactive  ACK %ld, RTT=%lfus", tcph->ackno(), rtt * 1000000);
                }
                packet_tcp_sent_timestamp_.erase(it);
            } else {
                if (fin) {
                    FLOW_DEBUG_COLOR(COLOR_SND, "RECV Reactive  FIN %ld", tcph->ackno());
                } else {
                    FLOW_DEBUG_COLOR(COLOR_SND, "RECV Reactive  ACK %ld", tcph->ackno());
                }
            }
            if (SackFullTcpAgent::highest_ack_ < tcph->ackno())
                allocate_tx_bytes(false, 0, tcph->ackno());
            else
                allocate_tx_bytes(false, 0);
        }
        bool deliver = true;
        if (!syn && datalen) {
            hdr_flexpass *flexpassh = hdr_flexpass::access(pkt);
            dontcare_skipped_bytes = flexpassh->stat_rc3_bytes_total_recovery_;
            if (SackFullTcpAgent::rcv_nxt_ < flexpassh->dontcare_seq()) {
                FLOW_DEBUG_COLOR(COLOR_RCV, "RECV Reactive  Manipulate Reactive TCP stack rcv_nxt_ from %ld. skipping %ld bytes", SackFullTcpAgent::rcv_nxt_, dontcare_skipped_bytes);
                SackFullTcpAgent::rcv_nxt_ = flexpassh->dontcare_seq();
                SackFullTcpAgent::last_ack_sent_ = flexpassh->dontcare_seq();
            }
            FLOW_DEBUG_COLOR(COLOR_RCV, "RECV Reactive  SEQ %ld - %ld, dontcare_seq %ld, acc_rec_bytes %ld", tcph->seqno(), tcph->seqno() + datalen, flexpassh->dontcare_seq(), dontcare_skipped_bytes);
            stat_reordering_detection(flexpassh->original_flow_seq(), datalen);
        }
        if (!syn && ack && !datalen) {
            for (int i = 0; i < tcph->sa_length(); i++) {
                if (tcph->ackno() < SackFullTcpAgent::highest_ack_ && SackFullTcpAgent::highest_ack_ >= tcph->sa_right(i)) {
                    deliver = false;
                }
            }
        }
        if (deliver)
            SackFullTcpAgent::recv(pkt, h);
        if (datalen && total_bytes_ && (recv_next_ - 1 + SackFullTcpAgent::rcv_nxt_ - 1 - dontcare_skipped_bytes >= total_bytes_)) {
            fct_ = now() - fst_;
            write_fct();
        }
    } break;
    }
}

void FlexPassAgent::report_fct() {
    // intentionally leave this as blank: FCT handler for TCP subflow
    if (fct_reported_) {
        fprintf(stderr, "INVALID FCT RESULT!!\n");
        abort();
    }
}

void FlexPassAgent::recv_credit_request(Packet *pkt) {
    hdr_flexpass *xph = hdr_flexpass::access(pkt);
    total_bytes_ = xph->total_len();
    double lalpha = alpha_;
    switch (credit_send_state_) {
    case FLEXPASS_SEND_CLOSE_WAIT:
        fct_timer_.force_cancel();
        init();
#if AIR
        if (xph->sendbuffer_ < 40) {
            lalpha = alpha_ * xph->sendbuffer_ / 40.0;
        }
#endif
        cur_credit_rate_ = (int)(lalpha * max_credit_rate_);
        cur_credit_rate_tr_ = cur_credit_rate_;
        // need to start to send credits.
        send_credit();

        // FLEXPASS_SEND_CLOSED -> FLEXPASS_SEND_CREDIT_REQUEST_RECEIVED
        credit_send_state_ = FLEXPASS_SEND_CREDIT_SENDING;
        break;
    case FLEXPASS_SEND_CLOSED:
#if AEOLUS
        init_for_credit_transmit();  //init(); /* Aeolus */
#else
        init();
#endif
#if AIR
        if (xph->sendbuffer_ < 40) {
            lalpha = alpha_ * xph->sendbuffer_ / 40.0;
        }
#endif
        cur_credit_rate_ = (int)(lalpha * max_credit_rate_);
        cur_credit_rate_tr_ = cur_credit_rate_;
        fst_ = xph->credit_sent_time();
        FLOW_DEBUG_COLOR(COLOR_SND, "RECV Proactive Credit Request at %lf ms (FST)", fst_ * 1000);
#if AEOLUS
        pkt_number_to_receive = (int)xph->credit_seq(); /* Aeolus */
        credit_request_id_++;
#endif
        // need to start to send credits.
        send_credit();

        // FLEXPASS_SEND_CLOSED -> FLEXPASS_SEND_CREDIT_REQUEST_RECEIVED
        credit_send_state_ = FLEXPASS_SEND_CREDIT_SENDING;
        break;
    default:
        /* To silence compiler warning */
        break;
    }
}

int FlexPassAgent::build_options(hdr_tcp *tcph) {
    int total = FullTcpAgent::build_options(tcph);

    if (!rq_.empty()) {
        int nblk = rq_.gensack(&tcph->sa_left(0), -max_sack_blocks_); /* populate max_sax_blocks_ SACK blocks from rear */
        tcph->sa_length() = nblk;
        total += (nblk * sack_block_size_) + sack_option_size_;
    } else {
        tcph->sa_length() = 0;
    }
    return (total);
}

void FlexPassAgent::recv_credit(Packet *pkt) {
#if AEOLUS
    /* Aeolus */
    hdr_flexpass *xph_crd = hdr_flexpass::access(pkt);
    if (sender_credit_request_id_ != xph_crd->credit_req_id() && sender_flow_state_ == SENDER_WITHIN_FIRST_RTT) {
        // if (last_firstrtt_burst_ == 0 && sender_flow_state_ == SENDER_WITHIN_FIRST_RTT){
        sender_credit_request_id_ = xph_crd->credit_req_id();
        sender_flow_state_ = SENDER_AFTER_FIRST_RTT;
        NS_LOG("Stopping FirstRTT");
        firstrtt_send_timer_.force_cancel();
        send(construct_probe(), 0);
    }
#endif

    FLOW_DEBUG_COLOR(COLOR_SND, "RECV Proactive Credit!");
    credit_recved_rtt_++;
    switch (credit_recv_state_) {
    case FLEXPASS_RECV_CREDIT_REQUEST_SENT:
        sender_retransmit_timer_.force_cancel();
        credit_recv_state_ = FLEXPASS_RECV_CREDIT_RECEIVING;
        // first sender RTT.
        rtt_ = now() - rtt_;
        last_credit_recv_update_ = now();
        if (credit_request_sent_time_ >= 0) {
            double rtt = now() - credit_request_sent_time_;
            flexpass_xpass_rtt_ = rtt;
            flexpass_rtt_min_ = flexpass_rtt_min_ < 0 ? rtt : MIN(flexpass_rtt_min_, rtt);
        }
        credit_request_sent_time_ = 0;
    case FLEXPASS_RECV_CREDIT_RECEIVING:
        // send data
#if AEOLUS
        /* Aeolus */
        if (lost_pkt_num > 0) {
            seq_t lost_pkt_seqno = get_first_lost_pkt_id() * (max_ethernet_size_ - xpass_hdr_size_) + 1;
            if (lost_pkt_seqno > 0) {
                send(construct_retransmitted_data(lost_pkt_seqno, pkt), 0);
                lost_pkt_num--;
                pkt_acked_[first_lost_pkt_id] = 1;
                ack_counter_firstrtt_pkt++;
            }
        } else {
            if (datalen_remaining() > 0) {
                flow_finished_with_firstrtt_pkt = 0;
                send(construct_data(pkt), 0);
            } else if (datalen_remaining() == 0 && ack_counter_firstrtt_pkt < sent_counter_firstrtt_pkt) {
                int first_unacked_pkt_id = get_first_unacked_pkt_id();
                seq_t unacked_pkt_seqno = first_unacked_pkt_id * (max_ethernet_size_ - xpass_hdr_size_) + 1;
                if (unacked_pkt_seqno > 0) {
                    send(construct_retransmitted_data(unacked_pkt_seqno, pkt), 0);
                    pkt_acked_[first_unacked_pkt_id] = 1;
                    ack_counter_firstrtt_pkt++;
                    last_processed_ack_pkt_id = first_unacked_pkt_id;
                }
            }
        }
        if (datalen_remaining() == 0 && sent_counter_firstrtt_pkt == ack_counter_firstrtt_pkt && already_sent_credit_stop == 0) {
            if (credit_stop_timer_.status() != TIMER_IDLE) {
                fprintf(stderr, "Error: CreditStopTimer seems to be scheduled more than once.\n");
                exit(1);
            }
            // Because ns2 does not allow sending two consecutive packets,
            // credit_stop_timer_ schedules CREDIT_STOP packet with no delay.
            credit_stop_timer_.sched(0);
            already_sent_credit_stop = 1;
        }
#else
        allocate_tx_bytes(true);
        if (datalen_remaining_xpass() > 0) {
            Packet *pkt_xmit = construct_data(pkt);
            hdr_tcp *tcph = hdr_tcp::access(pkt_xmit);

            send(pkt_xmit, 0);
        } else {
            credit_wasted_++;
        }
        // FLOW_DEBUG_INT(2.2734);
        if (datalen_remaining() == 0 || t_seqno_ > total_bytes_ || SackFullTcpAgent::highest_ack_ - rc3_bytes_total_recovery_ > total_bytes_) {
            if (SackFullTcpAgent::highest_ack_ != SackFullTcpAgent::curseq_ + 1 && t_seqno_ <= total_bytes_) {
                FLOW_DEBUG_COLOR(COLOR_WARN, "Mismatch: highest_ack=%ld, curseq_+1=%ld", SackFullTcpAgent::highest_ack_, SackFullTcpAgent::curseq_ + 1);
                printf("Mismatch\n");
            }
            if (credit_stop_timer_.status() != TIMER_IDLE) {
                fprintf(stderr, "Error: CreditStopTimer seems to be scheduled more than once.\n");
                exit(1);
            }
            credit_stop_sent_count_++;
            // printf("[%2.8lf: %p] Requesting Credit stop #%d\n", now(), this, credit_stop_sent_count_);
            // Because ns2 does not allow sending two consecutive packets,
            // credit_stop_timer_ schedules CREDIT_STOP packet with no delay.
            FLOW_DEBUG_COLOR(COLOR_SND, "SEND =========================");
            FLOW_DEBUG_COLOR(COLOR_SND, "SEND Terminating transmission:");
            FLOW_DEBUG_COLOR(COLOR_SND, "SEND total_bytes=%ld", total_bytes_);
            FLOW_DEBUG_COLOR(COLOR_SND, "SEND PRO Allocation=%ld", curseq_ - 1);
            FLOW_DEBUG_COLOR(COLOR_SND, "SEND REA Allocation=%ld", SackFullTcpAgent::curseq_);
            FLOW_DEBUG_COLOR(COLOR_SND, "SEND REA highest_ack_=%ld", SackFullTcpAgent::highest_ack_);
            FLOW_DEBUG_COLOR(COLOR_SND, "SEND REA highest_ack_(reactive)=%ld", highest_reactive_ack_);
            FLOW_DEBUG_COLOR(COLOR_SND, "SEND REA total_recovery=%ld", rc3_bytes_total_recovery_);
            FLOW_DEBUG_COLOR(COLOR_SND, "SEND REA remaining_recovery=%ld", rc3_bytes_needs_recovery_);
            FLOW_DEBUG_COLOR(COLOR_SND, "SEND PRO Alloc + REA Alloc - REA recov =%ld", curseq_ - 1 + SackFullTcpAgent::curseq_ - rc3_bytes_total_recovery_);
            FLOW_DEBUG_COLOR(COLOR_SND, "SEND =========================");

            FLOW_DEBUG_INT(7.0700);
            credit_stop_timer_.sched(0);
        }
#endif

#if ECS
        else if (now() - last_credit_recv_update_ >= rtt_) {
            if (credit_recved_rtt_ >= (2 * pkt_remaining())) {
                // Early credit stop
                if (credit_stop_timer_.status() != TIMER_IDLE) {
                    fprintf(stderr, "Error: CreditStopTimer seems to be scheduled more than once.\n");
                    exit(1);
                }
                // Because ns2 does not allow sending two consecutive packets,
                // credit_stop_timer_ schedules CREDIT_STOP packet with no delay.
                credit_stop_timer_.sched(0);
            }
            credit_recved_rtt_ = 0;
            last_credit_recv_update_ = now();
        }
#endif
        break;
    case FLEXPASS_RECV_CREDIT_STOP_SENT:
#if AEOLUS
        if (lost_pkt_num > 0) {
            seq_t lost_pkt_seqno = get_first_lost_pkt_id() * (max_ethernet_size_ - xpass_hdr_size_) + 1;
            if (lost_pkt_seqno > 0) {
                send(construct_retransmitted_data(lost_pkt_seqno, pkt), 0);
                lost_pkt_num--;
                pkt_acked_[first_lost_pkt_id] = 1;
                ack_counter_firstrtt_pkt++;
            }
        } else {
            if (datalen_remaining() > 0) {
                send(construct_data(pkt), 0);
            } else {
                credit_wasted_++;
            }
        }
        credit_recved_++;
#else
        if (datalen_remaining_xpass() > 0) {
            Packet *pkt_xmit = construct_data(pkt);
            hdr_tcp *tcph = hdr_tcp::access(pkt_xmit);
            if (packet_sent_timestamp_.find(tcph->seqno()) == packet_sent_timestamp_.end())
                packet_sent_timestamp_[tcph->seqno()] = now();
            else
                packet_sent_timestamp_[tcph->seqno()] = -1;
            send(pkt_xmit, 0);
        } else {
            credit_wasted_++;
        }
        credit_recved_++;
#endif
        break;
    case FLEXPASS_RECV_CLOSE_WAIT:
        // accumulate credit count to check if credit stop has been delivered
        credit_wasted_++;
        break;
    case FLEXPASS_RECV_CLOSED:
        credit_wasted_++;
        break;
    default:
        /* To silence compiler warning */
        break;
    }
}

void FlexPassAgent::recv_data(Packet *pkt) {
    hdr_cmn *cmnh = hdr_cmn::access(pkt);
    hdr_tcp *tcph = hdr_tcp::access(pkt);
    hdr_flexpass *xph = hdr_flexpass::access(pkt);
    int datalen = cmnh->size() - tcph->hlen();
    stat.rx_total_bytes += datalen;
    // distance between expected sequence number and actual sequence number.
    int distance = xph->credit_seq() - c_recv_next_;

    if (distance < 0) {
        // credit packet reordering or credit sequence number overflow happend.
        fprintf(stderr, "ERROR: Credit Sequence number is reverted.\n");
        exit(1);
    }
    credit_total_ += (distance + 1);
    credit_dropped_ += distance;

    c_recv_next_ = xph->credit_seq() + 1;

    dontcare_skipped_bytes = xph->stat_rc3_bytes_total_recovery_;
    if (SackFullTcpAgent::rcv_nxt_ < xph->dontcare_seq()) {
        FLOW_DEBUG_COLOR(COLOR_RCV, "RECV Reactive  Manipulate Reactive TCP stack rcv_nxt_ from %ld. skipping %ld bytes", SackFullTcpAgent::rcv_nxt_, dontcare_skipped_bytes);
        SackFullTcpAgent::rcv_nxt_ = xph->dontcare_seq();
        SackFullTcpAgent::last_ack_sent_ = xph->dontcare_seq();
    }

    FLOW_DEBUG_COLOR(COLOR_RCV, "RECV Proactive SEQ %ld - %ld, dontcare_seq %ld, acc_rec_bytes %ld", tcph->seqno(), tcph->seqno() + datalen, xph->dontcare_seq(), dontcare_skipped_bytes);

    fct_ = now() - fst_;

    process_ack(pkt);
    update_rtt(pkt);

    // printf("Received SEQ #%ld (Proactive) \n", xph->original_flow_seq());
    stat_reordering_detection(xph->original_flow_seq(), datalen);

#if AEOLUS
        /* Aeolus */
        pkt_number_received++;
    if (pkt_number_received == pkt_number_to_receive) {
        output_fct();
    }
#endif

    if (credit_send_state_ == FLEXPASS_SEND_CLOSE_WAIT) {
        fct_ = now() - fst_;
        fct_timer_.resched(default_credit_stop_timeout_ * 5);
    }
}

void FlexPassAgent::recv_nack(Packet *pkt) {
    hdr_tcp *tcph = hdr_tcp::access(pkt);
    switch (credit_recv_state_) {
    case FLEXPASS_RECV_CREDIT_STOP_SENT:
    case FLEXPASS_RECV_CLOSE_WAIT:
    case FLEXPASS_RECV_CLOSED:
        t_seqno_ = tcph->ackno();
#if AEOLUS
        send(construct_credit_request(byte_num_to_send - t_seqno_ + 1), 0);
#else
        send(construct_credit_request(), 0);
#endif
        credit_recv_state_ = FLEXPASS_RECV_CREDIT_REQUEST_SENT;
        sender_retransmit_timer_.resched(retransmit_timeout_);
        break;
    case FLEXPASS_RECV_CREDIT_REQUEST_SENT:
    case FLEXPASS_RECV_CREDIT_RECEIVING:
        // set t_seqno_ for retransmission
        t_seqno_ = tcph->ackno();
        break;
    default:
        /* To silence compiler warning */
        break;
    }
}

void FlexPassAgent::recv_ack(Packet *pkt) {
    hdr_tcp *tcph = hdr_tcp::access(pkt);
    hdr_cmn *cmnh = hdr_cmn::access(pkt);

    switch (credit_recv_state_) {
    case FLEXPASS_RECV_CREDIT_STOP_SENT:
    case FLEXPASS_RECV_CLOSE_WAIT:
    case FLEXPASS_RECV_CLOSED:
    case FLEXPASS_RECV_CREDIT_REQUEST_SENT:
    case FLEXPASS_RECV_CREDIT_RECEIVING: {
        auto it = packet_sent_timestamp_.find(tcph->seqno());
        if (it != packet_sent_timestamp_.end()) {
            if (it->second >= 0) {
                double rtt = now() - it->second;
            }
            packet_sent_timestamp_.erase(it);
        } else {
            
        }
        break;
    }
    default:
        /* To silence compiler warning */
        break;
    }
}

void FlexPassAgent::recv_credit_stop(Packet *pkt) {
    //   fct_ = now() - fst_;
    fct_timer_.resched(default_credit_stop_timeout_ * 5);
    send_credit_timer_.force_cancel();
    credit_send_state_ = FLEXPASS_SEND_CLOSE_WAIT;
    hdr_flexpass *flexpassh = hdr_flexpass::access(pkt);
    total_bytes_xpass_plus_rc3_ = flexpassh->credit_seq();
    dontcare_skipped_bytes = flexpassh->stat_rc3_bytes_total_recovery_;
    FLOW_DEBUG_COLOR(COLOR_RCV, "RECV Proactive Credit stop. skipping %ld bytes", dontcare_skipped_bytes);
}

static double max_delay = 0.0;
static int max_delay_fid = 0;

void FlexPassAgent::write_fct() {
    if (fct_reported_) {
        return;
    } else {
		stat.rx_needed_bytes += total_bytes_;
	}
	char foname[40];
    assert(exp_id_);
    sprintf(foname, "outputs/fct_%d.out", exp_id_);
    fct_reported_ = true;
    FILE *fct_out = fopen(foname, "a");
    
    per_flow_stat pfs = { 0, };
    if (g_p_per_flow_stat.find(fid_) != g_p_per_flow_stat.end())
        pfs = g_p_per_flow_stat[fid_];
    fprintf(fct_out, "%d,%ld,%.10lf,flexpass,%ld,%ld,%ld,%d,%ld,%ld\n",
            fid_, total_bytes_, fct_, stat_max_reordering_bytes_,
            recv_next_ - 1, SackFullTcpAgent::rcv_nxt_ - 1, dontcare_skipped_bytes, g_p_per_flow_stat[fid_].credit_wasted);

    fclose(fct_out);

    FLOW_DEBUG_COLOR(COLOR_RCV, "RECV Finish with FCT %lf ms. Proactive %ld B, Reactive %ld B (Delegated to Proactive %ld B), Total %ld B.",
                     fct_ * 1000, recv_next_ - 1, SackFullTcpAgent::rcv_nxt_ - 1 - dontcare_skipped_bytes, (seq_t)dontcare_skipped_bytes, recv_next_ - 1 + SackFullTcpAgent::rcv_nxt_ - 1 - dontcare_skipped_bytes);
    stat.tx_total_proactive += recv_next_ - 1;
    stat.tx_total_reactive += MAX(SackFullTcpAgent::rcv_nxt_ - 1 - dontcare_skipped_bytes, 0);
    stat.tx_total_recovery += dontcare_skipped_bytes;
    if (g_p_per_flow_stat.find(fid_) == g_p_per_flow_stat.end())
        g_p_per_flow_stat[fid_] = {0,};
    g_p_per_flow_stat[fid_].tx_total_proactive = recv_next_ - 1;
    g_p_per_flow_stat[fid_].tx_total_reactive = MAX(SackFullTcpAgent::rcv_nxt_ - 1 - dontcare_skipped_bytes, 0);
    g_p_per_flow_stat[fid_].tx_total_recovery = dontcare_skipped_bytes;
    stat.max_credit_interval = max_delay;
    stat.max_credit_interval_fid = max_delay_fid;
}

bool FlexPassAgent::is_recv_complete() {
    if (rc3_mode_) {
        return (recv_next_ - 1 >= total_bytes_xpass_plus_rc3_) && (recv_next_ - 1 + MAX(SackFullTcpAgent::rcv_nxt_ - 1 - dontcare_skipped_bytes, 0) >= total_bytes_);
    } else {
        return (recv_next_ - 1 + SackFullTcpAgent::rcv_nxt_ - 1 >= total_bytes_);
    }
}

void FlexPassAgent::handle_fct() {
    /* Although credit_stop has been received, must postpone this until TCP layer has received all data */
    FLOW_DEBUG_COLOR(COLOR_RCV, "RECV =========================");
    FLOW_DEBUG_COLOR(COLOR_RCV, "RECV Verifying transmission:");
    FLOW_DEBUG_COLOR(COLOR_RCV, "RECV Expected Total Size=%ld", total_bytes_);
    FLOW_DEBUG_COLOR(COLOR_RCV, "RECV PRO Allocation=%ld", recv_next_ - 1);
    FLOW_DEBUG_COLOR(COLOR_RCV, "RECV REA Allocation=%ld", SackFullTcpAgent::rcv_nxt_ - 1);
    FLOW_DEBUG_COLOR(COLOR_RCV, "RECV REA skipped bytes=%ld", dontcare_skipped_bytes);
    FLOW_DEBUG_COLOR(COLOR_RCV, "RECV PRO Alloc + REA Alloc - REA recov =%ld", recv_next_ - 1 + SackFullTcpAgent::rcv_nxt_ - 1 - dontcare_skipped_bytes);
    FLOW_DEBUG_COLOR(COLOR_RCV, "RECV =========================");
    if (!is_recv_complete()) {
        // rq_.dumplist();
        PRINTF_COLOR(COLOR_WARN, "Something is wrong: Expected %ld, Got %ld", total_bytes_, recv_next_ - 1 + SackFullTcpAgent::rcv_nxt_ - 1 - dontcare_skipped_bytes);
        NS_ASSERT(0);
    } else {
        write_fct();
    }
    credit_send_state_ = FLEXPASS_SEND_CLOSED;
}

void FlexPassAgent::handle_sender_retransmit() {
    switch (credit_recv_state_) {
    case FLEXPASS_RECV_CREDIT_REQUEST_SENT:
#if AEOLUS
        send(construct_credit_request(0), 0);
#else
        send(construct_credit_request(), 0);
#endif
        credit_request_sent_time_ = now();
        sender_retransmit_timer_.resched(retransmit_timeout_);
        break;
    case FLEXPASS_RECV_CREDIT_STOP_SENT:
        if (datalen_remaining_xpass() > 0) {
            credit_recv_state_ = FLEXPASS_RECV_CREDIT_REQUEST_SENT;
#if AEOLUS
            send(construct_credit_request(0), 0);
#else
            send(construct_credit_request(), 0);
#endif
            sender_retransmit_timer_.resched(retransmit_timeout_);
        } else {
            credit_recv_state_ = FLEXPASS_RECV_CLOSE_WAIT;
            credit_recved_ = 0;
            sender_retransmit_timer_.resched((rtt_ > 0) ? rtt_ : default_credit_stop_timeout_);
        }
        break;
    case FLEXPASS_RECV_CLOSE_WAIT:
        if (credit_recved_ == 0) {
            char foname[40];
            sprintf(foname, "outputs/waste_%d.out", exp_id_);

            FILE *waste_out = fopen(foname, "a");

            credit_recv_state_ = FLEXPASS_RECV_CLOSED;
            sender_retransmit_timer_.force_cancel();
            fprintf(waste_out, "%d,%ld,%d\n", fid_, curseq_ - 1, credit_wasted_);
            fclose(waste_out);
            per_flow_stat pfs = {
                0,
            };
            if (g_p_per_flow_stat.find(fid_) != g_p_per_flow_stat.end())
                pfs = g_p_per_flow_stat[fid_];
            g_p_per_flow_stat[fid_].credit_wasted = credit_wasted_;
            return;
        }
        // retransmit credit_stop
        send_credit_stop();
        break;
    case FLEXPASS_RECV_CLOSED:
        fprintf(stderr, "Sender Retransmit triggered while connection is closed.");
        exit(1);
        break;
    default:
        /* To silence compiler warning */
        break;
    }
}

void FlexPassAgent::handle_receiver_retransmit() {
    if (wait_retransmission_) {
        send(construct_nack(recv_next_, 0), 0);  // set triggered_seq_no to 0, so that sender don't have to calculate RTT with retx NACK
        receiver_retransmit_timer_.resched(retransmit_timeout_);
    }
}

#if AEOLUS
Packet *FlexPassAgent::construct_credit_request(seq_t nb) {
#else
Packet *FlexPassAgent::construct_credit_request() {
#endif
    Packet *p = allocpkt();
    if (!p) {
        fprintf(stderr, "ERROR: allockpkt() failed\n");
        exit(1);
    }

    hdr_tcp *tcph = hdr_tcp::access(p);
    hdr_cmn *cmnh = hdr_cmn::access(p);
    hdr_flexpass *xph = hdr_flexpass::access(p);

    tcph->seqno() = t_seqno_;
    tcph->ackno() = recv_next_;
    tcph->hlen() = xpass_hdr_size_;

    cmnh->size() = min_ethernet_size_;
    cmnh->ptype() = PT_XPASS_CREDIT_REQUEST;
    cmnh->tos() = TOS_FLEXPASS_PROACTIVE;

#if AEOLUS
    /* Aeolus */
    // xph->credit_seq() = 0;
    if (nb % (max_ethernet_size_ - xpass_hdr_size_) == 0) {
        xph->credit_seq() = nb / (max_ethernet_size_ - xpass_hdr_size_);
    } else {
        xph->credit_seq() = nb / (max_ethernet_size_ - xpass_hdr_size_) + 1;
    }
#else
    xph->credit_seq() = 0;
#endif

    xph->credit_sent_time_ = now();
    assert(pkt_remaining() > 0);
#if AIR
    xph->sendbuffer_ = pkt_remaining();
#endif
    xph->total_len() = total_bytes_;
    // to measure rtt between credit request and first credit
    // for sender.
    rtt_ = now();

    return p;
}

Packet *FlexPassAgent::construct_credit_stop() {
    Packet *p = allocpkt();
    if (!p) {
        fprintf(stderr, "ERROR: allockpkt() failed\n");
        exit(1);
    }
    hdr_tcp *tcph = hdr_tcp::access(p);
    hdr_cmn *cmnh = hdr_cmn::access(p);
    hdr_flexpass *xph = hdr_flexpass::access(p);

    tcph->seqno() = t_seqno_;
    tcph->ackno() = recv_next_;
    tcph->hlen() = xpass_hdr_size_;

    cmnh->size() = min_ethernet_size_;
    cmnh->ptype() = PT_XPASS_CREDIT_STOP;
    cmnh->tos() = TOS_FLEXPASS_PROACTIVE;

    xph->credit_seq() = stat_bytes_sent_xpass_ + stat_bytes_sent_rc3_;  // for xpass integrity check
    xph->dontcare_seq() = SackFullTcpAgent::highest_ack_;               // reactive highest ack
    xph->stat_rc3_bytes_total_recovery_ = rc3_bytes_total_recovery_;    // reactive total recovery

    return p;
}

Packet *FlexPassAgent::construct_credit() {
    Packet *p = allocpkt();
    if (!p) {
        fprintf(stderr, "ERROR: allockpkt() failed\n");
        exit(1);
    }
    hdr_tcp *tcph = hdr_tcp::access(p);
    hdr_cmn *cmnh = hdr_cmn::access(p);
    hdr_flexpass *xph = hdr_flexpass::access(p);
    int credit_size = min_credit_size_;

    if (min_credit_size_ < max_credit_size_) {
        // variable credit size
        credit_size += Random::integer(max_credit_size_ - min_credit_size_ + 1);
    } else {
        // static credit size
        if (min_credit_size_ != max_credit_size_) {
            fprintf(stderr, "ERROR: min_credit_size_ should be less than or equal to max_credit_size_\n");
            exit(1);
        }
    }

    tcph->seqno() = t_seqno_;
    tcph->ackno() = recv_next_;
    tcph->hlen() = credit_size;

    cmnh->size() = credit_size;
    cmnh->ptype() = PT_XPASS_CREDIT;
    cmnh->tos() = TOS_FLEXPASS_CREDIT;
    xph->credit_sent_time() = now();
    xph->credit_seq() = c_seqno_;
    xph->fid_ = fid_;

#if AEOLUS
    xph->credit_req_id() = credit_request_id_;
#endif

    c_seqno_ = max(1, c_seqno_ + 1);

    return p;
}

Packet *FlexPassAgent::construct_data(Packet *credit) {
    Packet *p = allocpkt();
    if (!p) {
        fprintf(stderr, "ERROR: allockpkt() failed\n");
        exit(1);
    }
    hdr_tcp *tcph = hdr_tcp::access(p);
    hdr_cmn *cmnh = hdr_cmn::access(p);
    hdr_flexpass *xph = hdr_flexpass::access(p);
    hdr_flexpass *credit_xph = hdr_flexpass::access(credit);
    int datalen = (int)min(max_segment(),
                           datalen_remaining_xpass());

    if (datalen <= 0) {
        fprintf(stderr, "ERROR: datapacket has length of less than zero %d\n", datalen);
        exit(1);
    }
    tcph->seqno() = t_seqno_;
    tcph->ackno() = recv_next_;
    tcph->hlen() = xpass_hdr_size_;

    cmnh->size() = max(min_ethernet_size_, xpass_hdr_size_ + datalen);
    cmnh->ptype() = PT_XPASS_DATA;
    cmnh->tos() = TOS_FLEXPASS_PROACTIVE;

    xph->credit_sent_time() = credit_xph->credit_sent_time();
    xph->credit_seq() = credit_xph->credit_seq();
    xph->payload_len() = datalen;
    xph->dontcare_seq() = SackFullTcpAgent::highest_ack_; // reactive highest ack
    xph->stat_rc3_bytes_total_recovery_ = rc3_bytes_total_recovery_; // reactive total recovery
    xph->original_flow_seq() = stat_next_proactive_orig_seq_;
    stat_next_proactive_orig_seq_ += datalen;

    FLOW_DEBUG_COLOR(COLOR_SND, "SEND Proactive SEQ %ld-%ld", t_seqno_, t_seqno_ + datalen);

    t_seqno_ += datalen;

#if AEOLUS
    if (t_seqno_ - last_firstrtt_burst_ >= unscheduled_burst_period_ && datalen_remaining() > 0) {
        NS_LOG("Resetting Unscheduled Burst at t_seqno_=%u", t_seqno_);
        NS_ASSERT(lost_pkt_num == 0);
        for (int i = 0; i < max_pktno_firstrtt; i++) {
            pkt_acked_[i] = -1;
            pkt_payload_size_[i] = 0;
        }
        last_processed_ack_pkt_id = -1;
        lost_pkt_num = 0;
        first_lost_pkt_id = -1;
        last_sent_pkt_seqno = -1;

        flow_finished_with_firstrtt_pkt = -1;
        sent_counter_firstrtt_pkt = 0;
        ack_counter_firstrtt_pkt = 0;

        probe_frequent = 4;
        probe_counter = 0;
        last_firstrtt_burst_ = t_seqno_ - 1;
        sender_flow_state_ = SENDER_WITHIN_FIRST_RTT;
        firstrtt_send_timer_.resched(firstrtt_transmit_interval);
        restart_firstrtt_timer_.resched(0);
    }
#endif

    return p;
}

Packet *FlexPassAgent::construct_nack(seq_t seq_no, seq_t triggered_seq_no) {
    Packet *p = allocpkt();
    if (!p) {
        fprintf(stderr, "ERROR: allockpkt() failed\n");
        exit(1);
    }
    hdr_tcp *tcph = hdr_tcp::access(p);
    hdr_cmn *cmnh = hdr_cmn::access(p);

    tcph->seqno() = triggered_seq_no;  // actually not relevant with actual seqno. To notify sender which packet triggered NACK
    tcph->ackno() = seq_no;
    tcph->hlen() = xpass_hdr_size_;

    cmnh->size() = min_ethernet_size_;
    cmnh->ptype() = PT_XPASS_NACK;
    cmnh->tos() = TOS_FLEXPASS_PROACTIVE;

    return p;
}

Packet *FlexPassAgent::construct_ack(seq_t seq_no, int datalen) {
    Packet *p = allocpkt();
    if (!p) {
        fprintf(stderr, "ERROR: allockpkt() failed\n");
        exit(1);
    }
    hdr_tcp *tcph = hdr_tcp::access(p);
    hdr_cmn *cmnh = hdr_cmn::access(p);

    tcph->seqno() = seq_no;
    tcph->ackno() = seq_no + datalen;
    tcph->hlen() = xpass_hdr_size_;

    cmnh->size() = min_ethernet_size_;
    cmnh->ptype() = PT_XPASS_DATA_ACK;
    cmnh->tos() = TOS_FLEXPASS_PROACTIVE;

    return p;
}

void FlexPassAgent::send_credit() {
    double avg_credit_size = (min_credit_size_ + max_credit_size_) / 2.0;
    double delay;
    // FLOW_DEBUG_INT(5.4551);

    credit_feedback_control();

    // send credit.
    send(construct_credit(), 0);

    // calculate delay for next credit transmission.
    delay = avg_credit_size / ((double)cur_credit_rate_ * (double)flexpass_beta_);
    // add jitter
    if (max_jitter_ > min_jitter_) {
        double jitter = Random::uniform();
        jitter = jitter * (max_jitter_ - min_jitter_) + min_jitter_;
        // jitter is in the range between min_jitter_ and max_jitter_
        delay = delay * (1 + jitter);
    } else if (max_jitter_ < min_jitter_) {
        fprintf(stderr, "ERROR: max_jitter_ should be larger than min_jitter_");
        exit(1);
    }
    delay = MAX(delay, 0);
    send_credit_timer_.resched(delay);
    if (delay > max_delay) {
        max_delay = delay;
        max_delay_fid = fid_;
    }
    FLOW_DEBUG_COLOR(COLOR_RCV, "SEND Proactive Credit, credit rate is %ld, will send credit after %lfms", cur_credit_rate_, delay * 1000);
}

void FlexPassAgent::send_credit_stop() {
    send(construct_credit_stop(), 0);

    FLOW_DEBUG_COLOR(COLOR_SND, "SEND Proactive Credit Stop");
    // set on timer
    sender_retransmit_timer_.resched(rtt_ > 0 ? (2. * rtt_) : default_credit_stop_timeout_);
    credit_recv_state_ = FLEXPASS_RECV_CREDIT_STOP_SENT;  //Later changes to FLEXPASS_RECV_CLOSE_WAIT -> FLEXPASS_RECV_CLOSED
}

void FlexPassAgent::sendpacket(seq_t seqno, seq_t ackno, int pflags,
                           int datalen, int reason, Packet *p) {
    if (!p) p = allocpkt();
    hdr_tcp *tcph = hdr_tcp::access(p);
    hdr_flags *fh = hdr_flags::access(p);
    hdr_flexpass *flexpassh = hdr_flexpass::access(p);

    /* build basic header w/options */

    tcph->seqno() = seqno;
    tcph->ackno() = ackno;
    tcph->flags() = pflags;
    tcph->reason() |= reason;  // make tcph->reason look like ns1 pkt->flags?
    tcph->sa_length() = 0;     // may be increased by build_options()
    tcph->hlen() = tcpip_base_hdr_size_;
    tcph->hlen() += build_options(tcph);

    /*
   * Explicit Congestion Notification (ECN) related:
   * Bits in header:
   * 	ECT (EC Capable Transport),
   * 	ECNECHO (ECHO of ECN Notification generated at router),
   * 	CWR (Congestion Window Reduced from RFC 2481)
   * States in TCP:
   *	ecn_: I am supposed to do ECN if my peer does
   *	ect_: I am doing ECN (ecn_ should be T and peer does ECN)
   */

    if (datalen > 0 && ecn_) {
        // set ect on data packets
        fh->ect() = ect_;  // on after mutual agreement on ECT
    } else if (ecn_ && ecn_syn_ && ecn_syn_next_ && (pflags & TH_SYN) && (pflags & TH_ACK)) {
        // set ect on syn/ack packet, if syn packet was negotiating ECT
        fh->ect() = ect_;
    } else {
        /* Set ect() to 0.  -M. Weigle 1/19/05 */
        fh->ect() = 0;
    }

    if (dctcp_)
        fh->ect() = ect_;

    if (ecn_ && ect_ && recent_ce_) {
        // This is needed here for the ACK in a SYN, SYN/ACK, ACK
        // sequence.
        pflags |= TH_ECE;
    }
    // fill in CWR and ECE bits which don't actually sit in
    // the tcp_flags but in hdr_flags
    if (pflags & TH_ECE) {
        fh->ecnecho() = 1;
    } else {
        fh->ecnecho() = 0;
    }
    if (pflags & TH_CWR) {
        fh->cong_action() = 1;
    } else {
        /* Set cong_action() to 0  -M. Weigle 1/19/05 */
        fh->cong_action() = 0;
    }

    /* actual size is data length plus header length */

    hdr_cmn *ch = hdr_cmn::access(p);
    ch->size() = datalen + tcph->hlen();

    if (datalen <= 0)
        ++nackpack_;
    else {
        ++ndatapack_;
        ndatabytes_ += datalen;
        last_send_time_ = now();  // time of last data
    }
    if (reason == REASON_TIMEOUT || reason == REASON_DUPACK || reason == REASON_SACK) {
        ++nrexmitpack_;
        nrexmitbytes_ += datalen;
    }

    ch->tos() = TOS_FLEXPASS_REACTIVE;
    last_ack_sent_ = ackno;

    if (datalen) {
        if (packet_tcp_sent_timestamp_.find(seqno + datalen) == packet_tcp_sent_timestamp_.end())
            packet_tcp_sent_timestamp_[seqno + datalen] = now();
        else
            packet_tcp_sent_timestamp_[seqno + datalen] = -1;
    }

    flexpassh->original_flow_seq() = stat_next_reactive_orig_seq_;
    stat_next_reactive_orig_seq_ += datalen;

    advance_packet(p);

    return;
}

void FlexPassAgent::advance_packet(Packet *p) {
    hdr_cmn *cmnh = hdr_cmn::access(p);
    hdr_tcp *tcph = hdr_tcp::access(p);
    hdr_flexpass *xph = hdr_flexpass::access(p);

    bool syn = (tcph->flags() & TH_SYN) ? 1 : 0;
    bool fin = (tcph->flags() & TH_FIN) ? 1 : 0;
    bool ack = (tcph->flags() & TH_ACK) ? 1 : 0;
    int datalen = cmnh->size() - tcph->hlen();

    if (syn && !ack) {
        credit_request_sent_time_ = now();
    }

    xph->dontcare_seq() = highest_ack_;
    xph->stat_rc3_bytes_total_recovery_ = rc3_bytes_total_recovery_;
    FLOW_DEBUG_INT(0.2910);
    if (datalen) {
        FLOW_DEBUG_COLOR(COLOR_SND, "SEND Reactive  SEQ %ld-%ld\t ACK %ld", tcph->seqno(), tcph->seqno() + datalen, tcph->ackno());
    } else if (fin) {
        FLOW_DEBUG_COLOR(COLOR_RCV, "SEND Reactive  FIN %ld", tcph->ackno());
    } else {
        if (syn && ack) {
            FLOW_DEBUG_COLOR(COLOR_RCV, "SEND Reactive SYNACK %ld", tcph->ackno());
        } else if (syn) {
            FLOW_DEBUG_COLOR(COLOR_SND, "SEND Reactive SYN %ld", tcph->ackno());
        } else {
            if (total_bytes_send_) {
                FLOW_DEBUG_COLOR(COLOR_SND, "SEND Reactive ACK %ld", tcph->ackno());
                if (tcph->ackno() == 1 && !reactive_subflow_ready_) {
                    reactive_subflow_ready_ = true;
                    FLOW_DEBUG_COLOR(COLOR_SND, "Beginning reactive subflow!!");
                    allocate_tx_bytes(false);
                }
            } else {
                FLOW_DEBUG_COLOR(COLOR_RCV, "SEND Reactive ACK %ld", tcph->ackno());
            }


            
        }
    }

    try_send(p, 0);
}

void FlexPassAgent::try_send(Packet *p, Handler *h) {
    if (p->uid_ > 0) {
        printf("Chance of error!\n");
    }
    send(p, h);
}

void FlexPassAgent::advance_bytes(seq_t nb) {
    if (credit_recv_state_ != FLEXPASS_RECV_CLOSED) {
        fprintf(stderr, "ERROR: tried to advance_bytes without FLEXPASS_RECV_CLOSED\n");
    }
    if (nb <= 0) {
        fprintf(stderr, "ERROR: advanced bytes are less than or equal to zero\n");
    }

#if AEOLUS
    /* Aeolus */
    byte_num_to_send = nb;
#endif

    // advance bytes
    pending_bytes_ += nb;
    total_bytes_ += nb;
    total_bytes_send_ += nb;
   
    if (rc3_mode_)
        rc3_tcp_snd_nxt += nb;

    if (static_allocation_) {
        // fprintf(stderr, "Warning: static_allocation must be used only for motivation purpose!\n");
        seq_t reactive = pending_bytes_ / 2;
        seq_t proactive = pending_bytes_ - reactive;
        pending_bytes_ = 0;
        curseq_ += proactive;
        SackFullTcpAgent::advance_bytes(reactive);
    }

    // send credit request
#if AEOLUS
    send(construct_credit_request(nb), 0);
#else
    send(construct_credit_request(), 0);
#endif
    credit_request_sent_time_ = now();
    sender_retransmit_timer_.sched(retransmit_timeout_);

#if AEOLUS
    /* Aeolus */
    if (firstrtt_init_done == 0) {
        init_for_firstrtt_transmit();
        firstrtt_init_done = 1;
    }
    firstrtt_send_timer_.sched((double)0.0);
#endif

    // FLEXPASS_RECV_CLOSED -> FLEXPASS_RECV_CREDIT_REQUEST_SENT
    credit_recv_state_ = FLEXPASS_RECV_CREDIT_REQUEST_SENT;
    SackFullTcpAgent::advance_bytes(0);  // establish connection

    flexpass_xpass_prioritized_bytes_left_ = flexpass_xpass_prioritized_bytes_;
    allocate_tx_bytes(false);
}

void FlexPassAgent::recover_lost_tcp_block(bool timeout, seq_t maxseg, bool force) {
    seq_t blk_begin, blk_end;
    seq_t tcp_allocation = SackFullTcpAgent::curseq_;  //MIN(SackFullTcpAgent::curseq_, SackFullTcpAgent::maxseq_ - 1);

    // if (highest_ack_ < 0)
    //   return;
    if (!tcp_allocation)
        return;

    if (!datalen_remaining())
        return;

    bool emit_fake_pkt = true;
    bool found = false;
    if (highest_ack_ >= 0 && sq_.total())
        found = sq_.findnextblock(highest_ack_, &blk_begin, &blk_end);

    // make fake ACK and deliver it to TCP layer
    Packet *p = allocpkt();
    if (!p) {
        fprintf(stderr, "ERROR: allockpkt() failed\n");
        exit(1);
    }
    hdr_tcp *tcph = hdr_tcp::access(p);
    hdr_cmn *cmnh = hdr_cmn::access(p);

    tcph->seqno() = SackFullTcpAgent::rcv_nxt_;
    tcph->hlen() = xpass_hdr_size_;
    tcph->flags() = TH_ACK;

    cmnh->size() = tcph->hlen();
    cmnh->ptype() = PT_ACK;
    if (reordering_measure_in_rc3_) {
        stat_recovery_head_ = total_bytes_ - highest_ack_;
    } else {
        stat_recovery_head_ = highest_ack_;
    }
    
    if (timeout) {
        int bytes_recovery = SackFullTcpAgent::maxseq_ - 1 - highest_reactive_ack_ + 1; // + bytes_sacked_after_seq;
        NS_ASSERT(highest_reactive_ack_ <= SackFullTcpAgent::maxseq_);
        if (bytes_recovery) {
            FLOW_DEBUG_INT(24.2004);

            seq_t t = rc3_bytes_needs_recovery_;
            rc3_bytes_needs_recovery_ += bytes_recovery;
            rc3_bytes_total_recovery_ += bytes_recovery;
            // if timeout have happened before in a row, it must not recover twice. For that, update highest_reactive_ack_
            FLOW_DEBUG_COLOR(COLOR_SND, "ALOC Detected Timeout. Last Reactive ACK was %ld, Reactive maxseq_ was %ld, so we recover %ld bytes. needs_recovery_ =%ld->%ld", \
                             highest_reactive_ack_, SackFullTcpAgent::maxseq_, bytes_recovery, t, rc3_bytes_needs_recovery_);

            seq_t t1 = highest_reactive_ack_;
            highest_reactive_ack_ += bytes_recovery;
            FLOW_DEBUG_COLOR(COLOR_WARN, "Highest_reactive_ack will be updated %ld -> %ld. Problem?", t1, highest_reactive_ack_);
            tcph->ackno() = SackFullTcpAgent::maxseq_;
        } else {
            emit_fake_pkt = false;
        }

    } else {
        if (found) {
            NS_ASSERT(blk_begin > highest_ack_);
            seq_t t = rc3_bytes_needs_recovery_;
            rc3_bytes_needs_recovery_ += blk_begin - highest_ack_;
            rc3_bytes_total_recovery_ += blk_begin - highest_ack_;

            // if loss have happened before in a row, it must not recover twice. For that, update highest_reactive_ack_
            seq_t t1 = highest_reactive_ack_;
            highest_reactive_ack_ += blk_begin - highest_ack_;
            FLOW_DEBUG_COLOR(COLOR_SND, "ALOC Performing recovery from SACK, now rc3_bytes_needs_recovery_=%ld -> %ld (maxseg). ack=%lu", t, rc3_bytes_needs_recovery_, blk_end)
            FLOW_DEBUG_COLOR(COLOR_WARN, "Highest_reactive_ack will be updated %ld -> %ld. Problem?", t1, highest_reactive_ack_);
            tcph->ackno() = blk_end;
        } else if (force) {
            // check "unacked" segments now
            if (maxseg) {
                seq_t pkt = MIN(tcp_allocation - SackFullTcpAgent::highest_ack_ + 1, maxseg);
                if(pkt) {
                    rc3_bytes_needs_recovery_ += pkt;
                    rc3_bytes_total_recovery_ += pkt;
                    tcph->ackno() = tcp_allocation + 1 - (tcp_allocation - SackFullTcpAgent::highest_ack_ + 1 - pkt);
                    FLOW_DEBUG_COLOR(COLOR_SND, "ALOC Performing recovery from unacked data, now rc3_bytes_needs_recovery_=%u (maxseg)", rc3_bytes_needs_recovery_);
                } else {
                    FLOW_DEBUG_COLOR(COLOR_SND, "ALOC Try to perform recovery, but no more unacked data! datalen_remaining = %ld", datalen_remaining());
                    emit_fake_pkt = false;
                }
                
            } else {
                rc3_bytes_needs_recovery_ += tcp_allocation - SackFullTcpAgent::highest_ack_ + 1;
                rc3_bytes_total_recovery_ += tcp_allocation - SackFullTcpAgent::highest_ack_ + 1;
                tcph->ackno() = tcp_allocation + 1;

                FLOW_DEBUG_COLOR(COLOR_SND, "RECV Reactive  Fake ACK! Increasing rc3_bytes_needs_recovery_=%u (no maxseg)", rc3_bytes_needs_recovery_);
            }
        } else {
            emit_fake_pkt = false;
        }
    }
    NS_ASSERT(rc3_bytes_total_recovery_ <= tcp_allocation);

    if (emit_fake_pkt) {
        SackFullTcpAgent::recv(p, NULL);
    } else {
        Packet::free(p);
    }
    datalen_remaining();
}
void FlexPassAgent::allocate_tx_bytes(bool is_credit_available) {
    allocate_tx_bytes(is_credit_available, 0);
}

void FlexPassAgent::allocate_tx_bytes(bool is_credit_available, int step) {
    allocate_tx_bytes(is_credit_available, step, SackFullTcpAgent::highest_ack_);
}

void FlexPassAgent::allocate_tx_bytes(bool is_credit_available, int step, seq_t new_highest_ack) {
    typedef enum { NOT_ALLOC, PROACTIVE, REACTIVE } policy_t;
    policy_t policy = NOT_ALLOC;
    // FLOW_DEBUG_INT(1.0829);

    if (total_bytes_ == 0) return;

    seq_t reactive_buffered_bytes = get_reactive_buffered_bytes(new_highest_ack);
    seq_t unacked_bytes = SackFullTcpAgent::curseq_ - SackFullTcpAgent::iss_;
    seq_t cwnd = SackFullTcpAgent::window() * SackFullTcpAgent::maxseg_;
    seq_t nb = 0;
    seq_t estimated_orig_seq = 0;

    if (is_credit_available) {
        if (new_allocation_logic_) {
            if (rc3_bytes_needs_recovery_ == 0) {
                recover_lost_tcp_block(false, max_segment());
            }
            if (rc3_bytes_needs_recovery_) {
                nb = MIN(max_segment(), rc3_bytes_needs_recovery_);
                rc3_bytes_needs_recovery_ -= nb;
                estimated_orig_seq = stat_recovery_head_;
                stat_recovery_head_ += nb;
            } else if (pending_bytes_) {
                nb = MIN(max_segment(), pending_bytes_);
                pending_bytes_ -= nb;
                estimated_orig_seq = stat_pending_head_;
                stat_pending_head_ += nb;
            } else if (unacked_bytes) {
                // force set "unacked" packet into "recovery" state for the ease of the implementation
                recover_lost_tcp_block(false, max_segment(), true);
                nb = MIN(max_segment(), rc3_bytes_needs_recovery_);
                rc3_bytes_needs_recovery_ -= nb;
                if (reordering_measure_in_rc3_) {
                    stat_unacked_head_ -= nb;
                    estimated_orig_seq = stat_unacked_head_;
                } else {
                    estimated_orig_seq = stat_unacked_head_;
                    stat_unacked_head_ += nb;
                }
            }
        } else {
            if (rc3_bytes_needs_recovery_ == 0) {
                recover_lost_tcp_block(false, max_segment(), true);
            }
            if (pending_bytes_) {
                nb = MIN(max_segment(), pending_bytes_);
                pending_bytes_ -= nb;
            } else if (rc3_bytes_needs_recovery_) {
                nb = MIN(max_segment(), rc3_bytes_needs_recovery_);
                rc3_bytes_needs_recovery_ -= nb;
            }
        }

        if (nb) {
            stat_bytes_sent_xpass_ += nb;
            policy = PROACTIVE;
        }
    } else {
        seq_t total_allocated = curseq_ - iss_ + SackFullTcpAgent::curseq_ - SackFullTcpAgent::iss_ - rc3_bytes_total_recovery_;
        if (reactive_subflow_ready_ && total_allocated < total_bytes_) {
            seq_t t_nb = MIN(MIN(maxseg_, total_bytes_ - total_allocated), pending_bytes_);
            if (t_nb && reactive_buffered_bytes + t_nb <= cwnd) {
                nb = t_nb;
                pending_bytes_ -= nb;
                rc3_tcp_snd_nxt -= nb;
                stat_bytes_sent_tcp_ += nb;
                policy = REACTIVE;
                if (reordering_measure_in_rc3_) {
                    if (stat_unacked_head_ == 0)
                        stat_unacked_head_ = total_bytes_;
                    stat_unacked_head_ -= nb;
                    estimated_orig_seq = stat_unacked_head_;
                } else {
                    estimated_orig_seq = stat_pending_head_;
                    stat_unacked_head_ = stat_pending_head_;
                    stat_pending_head_ += nb;
                }
            }
        }
    }

    NS_ASSERT(nb >= 0);
    NS_ASSERT(pending_bytes_ >= 0);

    if (policy == PROACTIVE) {
        stat_next_proactive_orig_seq_ = estimated_orig_seq;
        curseq_ += nb;
        FLOW_DEBUG_COLOR(COLOR_SND, "ALOC Allocating %ld bytes to Proactive Stack", nb);
    } else if (policy == REACTIVE) {
        stat_next_reactive_orig_seq_ = estimated_orig_seq;
        SackFullTcpAgent::advance_bytes(nb);
        seq_t t = reactive_buffered_bytes;
        reactive_buffered_bytes = get_reactive_buffered_bytes();
        cwnd = SackFullTcpAgent::window() * maxseg_;
        FLOW_DEBUG_COLOR(COLOR_SND, "ALOC Allocating %ld bytes to Reactive  Stack. reactive curseq_=%ld, pending_bytes = %ld, Reactive Buffer %ld->%ld, cwnd %d", nb, SackFullTcpAgent::curseq_, pending_bytes_, t, reactive_buffered_bytes, cwnd);
    } else {
        if (!step)
            FLOW_DEBUG_COLOR(COLOR_SND, "ALOC Not allocating anywhre, since there is no credit, cwnd=%d, buffered_bytes=%ld", cwnd, reactive_buffered_bytes);
        if (is_credit_available) FLOW_DEBUG_COLOR(COLOR_WARN, "ALOC Wasting Credit Here! datalen_remaining=%ld", datalen_remaining());
    }

    if (policy != NOT_ALLOC) {
        allocate_tx_bytes(policy == PROACTIVE ? false : is_credit_available, 1);
    }
}
#if 0
void FlexPassAgent::allocate_tx_bytes(bool is_credit_available, int step, seq_t new_highest_ack) {
    NS_ASSERT(rc3_mode_);
    typedef enum { NOT_ALLOC,
                   PROACTIVE,
                   REACTIVE } policy_t;
    // FLOW_DEBUG_INT(0.0636);
    // if (!is_credit_available) return;
    policy_t policy = NOT_ALLOC;

    if (pending_bytes_ == 0) {
        if (total_bytes_ == 0) return;
        if (rc3_bytes_needs_recovery_ == 0 && is_credit_available) {
            recover_lost_tcp_block(false, max_segment());
        }
        if (rc3_bytes_needs_recovery_ == 0) {
            if (is_credit_available) FLOW_DEBUG_COLOR(COLOR_WARN, "Wasting Credit Here!");
            return;
        }
    }

    seq_t reactive_buffered_bytes = get_reactive_buffered_bytes(new_highest_ack);
    int cwnd = SackFullTcpAgent::window() * maxseg_;
    seq_t nb = 0;

    if (is_credit_available) {
        if (pending_bytes_) {
            nb = MIN(max_segment(), pending_bytes_);
            pending_bytes_ -= nb;
            rc3_xpass_snd_nxt += nb;
            stat_bytes_sent_xpass_ += nb;
        } else if (rc3_bytes_needs_recovery_) {
            nb = MIN(max_segment(), rc3_bytes_needs_recovery_);
            rc3_bytes_needs_recovery_ -= nb;
            stat_bytes_sent_rc3_ += nb;
            // printf("Performing Recovery of %ld bytes at XPASS!\n", nb);
        } else {
            NS_ASSERT(pending_bytes_ || rc3_bytes_needs_recovery_);
        }
        policy = PROACTIVE;
    } else {
        if (rc3_tcp_snd_nxt > rc3_xpass_snd_nxt) {
            seq_t t_nb = MIN(maxseg_, rc3_tcp_snd_nxt - rc3_xpass_snd_nxt);
            if (reactive_buffered_bytes + t_nb <= cwnd) {
                nb = t_nb;
                pending_bytes_ -= nb;
                rc3_tcp_snd_nxt -= nb;
                stat_bytes_sent_tcp_ += nb;
                policy = REACTIVE;
            }
        }
    }

    if (policy == PROACTIVE) {
        curseq_ += nb;
        FLOW_DEBUG_COLOR(COLOR_SND, "ALOC: Allocating %ld bytes to Proactive Stack", nb);
    } else if (policy == REACTIVE) {
        // FLOW_DEBUG_INT(30.6997);
        SackFullTcpAgent::advance_bytes(nb);
        seq_t t = reactive_buffered_bytes;
        reactive_buffered_bytes = get_reactive_buffered_bytes();
        cwnd = SackFullTcpAgent::window() * maxseg_;
        // FLOW_DEBUG_INT(30.6997);
        FLOW_DEBUG_COLOR(COLOR_SND, "ALOC: Allocating %ld bytes to Reactive  Stack. Reactive Buffer %ld->%ld, cwnd %d", nb, t, reactive_buffered_bytes, cwnd);
    } else {
        if (!step)
            FLOW_DEBUG_COLOR(COLOR_SND, "ALOC: Not allocating anywhere");
        if (is_credit_available) FLOW_DEBUG_COLOR(COLOR_WARN, "ALOC: Wasting Credit Here!");
    }

    if (policy != NOT_ALLOC) {
        allocate_tx_bytes(policy == PROACTIVE ? false : is_credit_available, 1);
    }
}
#endif

void FlexPassAgent::dupack_action() {
    FLOW_DEBUG_COLOR(COLOR_WARN, "Reactive  TCP DupAck!!");
    FLOW_DEBUG_INT(0.2908);
    if (rc3_mode_) {
        if (curseq_ > total_bytes_) {
            FLOW_DEBUG_COLOR(COLOR_SND, "Never mind. All the reactive segments were sent using proactive");
            return;
        }
        recover_lost_tcp_block(false, max_segment());
        int prev_window = SackFullTcpAgent::window();
        int prev_ssthresh = SackFullTcpAgent::ssthresh_;
        // do slowdown
        if (SackFullTcpAgent::dupack_action_no_snd(false)) {
            send_much(0, REASON_DUPACK, maxburst_);
        }

        FLOW_DEBUG_COLOR(COLOR_SND, "Reactive  TCP Slowdown: window %d->%d, ssthresh %d->%d", prev_window, SackFullTcpAgent::window(), prev_ssthresh, int(SackFullTcpAgent::ssthresh_));
    } else {
        SackFullTcpAgent::dupack_action();
    }
}

void FlexPassAgent::timeout_action() {
    FLOW_DEBUG_COLOR(COLOR_WARN, "Reactive  TCP Timeout!!");
    if (!reactive_subflow_ready_) {
        SackFullTcpAgent::timeout_action();
        return;
    }
    if (rc3_mode_) {
        if (curseq_ > total_bytes_) {
            FLOW_DEBUG_COLOR(COLOR_SND, "Never mind. All the reactive segments were sent using proactive");
            return;
        }
        FLOW_DEBUG_INT(11.8291);
        recover_lost_tcp_block(true, 0);
        // SackFullTcpAgent::reset_rtx_timer(1);
        int prev_window = SackFullTcpAgent::window();
        int prev_ssthresh = SackFullTcpAgent::ssthresh_;
        seq_t prev_seqno = SackFullTcpAgent::t_seqno_;
        // do slowdown
        SackFullTcpAgent::timeout_action();

        // use previous t_seqno (prevent side effect of SackFullTcpAgent::timeout_action)
        SackFullTcpAgent::t_seqno_ = prev_seqno;

        // unarm ack timer (prevent side effect of SackFullTcpAgent::timeout_action)
        SackFullTcpAgent::cancel_rtx_timer();

        NS_ASSERT(prev_seqno == SackFullTcpAgent::t_seqno_);

        allocate_tx_bytes(false, 0);

        FLOW_DEBUG_COLOR(COLOR_SND, "Reactive  TCP Slowdown: window %d->%d, ssthresh %d->%d", prev_window, SackFullTcpAgent::window(), prev_ssthresh, int(SackFullTcpAgent::ssthresh_));
    } else {
        SackFullTcpAgent::timeout_action();
    }
}

void FlexPassAgent::process_ack(Packet *pkt) {
    hdr_cmn *cmnh = hdr_cmn::access(pkt);
    hdr_tcp *tcph = hdr_tcp::access(pkt);
    hdr_flexpass *xph = hdr_flexpass::access(pkt);
    int datalen = xph->payload_len();

#if AEOLUS
    /* Aeolus */
    if (credit_induced_data_received == 0) {
        credit_induced_data_received = 1;
        recv_next_ = tcph->seqno();
    }

#endif
    if (datalen < 0) {
        fprintf(stderr, "ERROR: negative length packet has been detected.\n");
        exit(1);
    }
    if (tcph->seqno() > recv_next_) {
        printf("[%d] %lf: data loss detected. (expected = %ld, received = %ld)\n",
               fid_, now(), recv_next_, tcph->seqno());
        /* recovery with NACK */
        if (!wait_retransmission_) {
            wait_retransmission_ = true;
            send(construct_nack(recv_next_, tcph->seqno()), 0);
            receiver_retransmit_timer_.resched(retransmit_timeout_);
        }
    } else if (tcph->seqno() == recv_next_) {
        if (wait_retransmission_) {
            wait_retransmission_ = false;
            receiver_retransmit_timer_.force_cancel();
        }
        //send(construct_ack(tcph->seqno(), datalen), 0);
        recv_next_ += datalen;
    } else {
		stat.rx_total_reduntant_bytes += datalen;
	}
}

void FlexPassAgent::update_rtt(Packet *pkt) {
    hdr_flexpass *xph = hdr_flexpass::access(pkt);

    double rtt = now() - xph->credit_sent_time();
    if (rtt_ > 0.0) {
        rtt_ = 0.8 * rtt_ + 0.2 * rtt;
    } else {
        rtt_ = rtt;
    }
}

void FlexPassAgent::credit_feedback_control() {
    if (rtt_ <= 0.0) {
        return;
    }
    if ((now() - last_credit_rate_update_) < rtt_) {
        return;
    }
    if (credit_total_ == 0) {
        return;
    }

    int old_rate = cur_credit_rate_;
    double loss_rate = credit_dropped_ / (double)credit_total_;
    int min_rate = (int)(avg_credit_size() / rtt_);

#if CFC_ALG == CFC_ORIG
    double target_loss = (1.0 - cur_credit_rate_ / (double)max_credit_rate_) * target_loss_scaling_;

    if (loss_rate > target_loss) {
        // congestion has been detected!
        if (loss_rate >= 1.0) {
            cur_credit_rate_ = (int)(avg_credit_size() / rtt_);
        } else {
            cur_credit_rate_ = (int)(avg_credit_size() * (credit_total_ - credit_dropped_) / (now() - last_credit_rate_update_) * (1.0 + target_loss));
        }
        if (cur_credit_rate_ > old_rate) {
            cur_credit_rate_ = old_rate;
        }

        w_ = max(w_ / 2.0, min_w_);
        can_increase_w_ = false;
    } else {
        // there is no congestion.
        if (can_increase_w_) {
            w_ = min(w_ + 0.05, 0.5);
        } else {
            can_increase_w_ = true;
        }

        if (cur_credit_rate_ < max_credit_rate_) {
            cur_credit_rate_ = (int)(w_ * max_credit_rate_ + (1 - w_) * cur_credit_rate_);
        }
    }

    FLOW_DEBUG_COLOR(COLOR_RCV, "RATE: Current credit rate=%ld, Max credit rate=%ld, Slowdown=%.2lfX",
                     cur_credit_rate_, max_credit_rate_, (double)max_credit_rate_ / cur_credit_rate_);
#elif CFC_ALG == CFC_BIC
    double target_loss;
    int data_received_rate;

    if (cur_credit_rate_ >= base_credit_rate_) {
        target_loss = target_loss_scaling_;
    } else {
        target_loss = (1.0 - cur_credit_rate_ / (double)base_credit_rate_) * target_loss_scaling_;
    }

    if (loss_rate > target_loss) {
        if (loss_rate >= 1.0) {
            data_received_rate = (int)(avg_credit_size() / rtt_);
        } else {
            data_received_rate = (int)(avg_credit_size() * (credit_total_ - credit_dropped_) / (now() - last_credit_rate_update_) * (1.0 + target_loss));
        }
        bic_target_rate_ = cur_credit_rate_;
        if (cur_credit_rate_ > data_received_rate)
            cur_credit_rate_ = data_received_rate;

        if (old_rate - cur_credit_rate_ < bic_s_min_) {
            cur_credit_rate_ = old_rate - bic_s_min_;
        } else if (old_rate - cur_credit_rate_ > bic_s_max_) {
            cur_credit_rate_ = old_rate - bic_s_max_;
        }
    } else {
        if (bic_target_rate_ - cur_credit_rate_ <= 0.05 * bic_target_rate_) {
            if (cur_credit_rate_ < bic_target_rate_) {
                cur_credit_rate_ = bic_target_rate_;
            } else {
                cur_credit_rate_ = cur_credit_rate_ + (cur_credit_rate_ - bic_target_rate_) * (1.0 + bic_beta_);
            }
        } else {
            cur_credit_rate_ = (cur_credit_rate_ + bic_target_rate_) / 2;
        }
        if (cur_credit_rate_ - old_rate < bic_s_min_) {
            cur_credit_rate_ = old_rate + bic_s_min_;
        } else if (cur_credit_rate_ - old_rate > bic_s_max_) {
            cur_credit_rate_ = old_rate + bic_s_max_;
        }
    }
#endif

    if (cur_credit_rate_ > max_credit_rate_) {
        cur_credit_rate_ = max_credit_rate_;
    }
    if (cur_credit_rate_ < min_rate) {
        cur_credit_rate_ = min_rate;
    }
    cur_credit_rate_tr_ = cur_credit_rate_;
    credit_total_ = 0;
    credit_dropped_ = 0;
    last_credit_rate_update_ = now();
}

void FlexPassAgent::traceVar(TracedVar *v) {
    if (!channel_)
        return;

    double curtime;
    Scheduler &s = Scheduler::instance();
    const size_t TCP_WRK_SIZE = 128;
    char wrk[TCP_WRK_SIZE];

    curtime = &s ? s.clock() : 0;
    snprintf(wrk, TCP_WRK_SIZE,
             "%-8.7f %-2d %-2d %-2d %-2d %s %d\n",
             curtime, addr(), port(), daddr(), dport(),
             v->name(), int(*((TracedInt *)v)));

    (void)Tcl_Write(channel_, wrk, -1);
}

void FlexPassAgent::trace(TracedVar *v) {
    traceVar(v);
}

#if AEOLUS
/* Aeolus */
void FlexPassFirstRttSendTimer::expire(Event *) {
    a_->send_firstrtt_data();
}

void FlexPassRetransmitTimer::expire(Event *) {
    a_->handle_retransmit();
}

void FlexPassAgent::init_for_credit_transmit() {
    w_ = w_init_;
    cur_credit_rate_ = (int)(alpha_ * max_credit_rate_);
    last_credit_rate_update_ = now();

    credit_induced_data_received = 0;

    pkt_number_received = 0;
    rtt_ = base_rtt_;

    max_pktno_firstrtt = 2 * max_link_rate_ * rtt_ / (8 * max_ethernet_size_);
    pkt_received_ = new int[max_pktno_firstrtt];
    for (int i = 0; i < max_pktno_firstrtt; i++) {
        pkt_received_[i] = -1;
    }
}

void FlexPassAgent::init_for_firstrtt_transmit() {
    rtt_ = base_rtt_;
    firstrtt_transmit_interval = max_ethernet_size_ * 8 / max_link_rate_;

    max_pktno_firstrtt = 2 * max_link_rate_ * rtt_ / (8 * max_ethernet_size_);

    pkt_acked_ = new int[max_pktno_firstrtt];
    pkt_payload_size_ = new int[max_pktno_firstrtt];
    for (int i = 0; i < max_pktno_firstrtt; i++) {
        pkt_acked_[i] = -1;
        pkt_payload_size_[i] = 0;
    }
    last_processed_ack_pkt_id = -1;
    lost_pkt_num = 0;
    first_lost_pkt_id = -1;
    last_sent_pkt_seqno = -1;

    flow_finished_with_firstrtt_pkt = -1;
    sent_counter_firstrtt_pkt = 0;
    ack_counter_firstrtt_pkt = 0;

    probe_frequent = 4;
    probe_counter = 0;
    last_firstrtt_burst_ = t_seqno_ - 1;
}

int FlexPassAgent::get_first_lost_pkt_id() {
    first_lost_pkt_id++;

    while (last_processed_ack_pkt_id != -1 && first_lost_pkt_id <= last_processed_ack_pkt_id && pkt_acked_[first_lost_pkt_id] == 1) {
        first_lost_pkt_id++;
    }
    if (first_lost_pkt_id > last_processed_ack_pkt_id) {
        fprintf(stdout, "Error: cannot find lost packet. last_processed_ack_pkt_id = %d, first_lost_pkt_id = %d\n", last_processed_ack_pkt_id, first_lost_pkt_id);
        printf("lost packet number =%d\n", lost_pkt_num);
        for (int i = 0; i <= last_processed_ack_pkt_id; i++) {
            printf("pkt_acked_[%d] = %d   ", i, pkt_acked_[i]);
        }
        printf("\n");
        return -1;
    }
    if (pkt_acked_[first_lost_pkt_id] != 0) {
        fprintf(stderr, "Error: packet acknowledgement state is not correct.\n");
        return -1;
    }
    return first_lost_pkt_id;
}

int FlexPassAgent::get_first_unacked_pkt_id() {
    int last_processed_ack_pkt_seqno = last_processed_ack_pkt_id * (max_ethernet_size_ - xpass_hdr_size_) + 1;
    if (last_processed_ack_pkt_seqno < last_sent_pkt_seqno) {
        return last_processed_ack_pkt_id + 1;
    } else {
        return -1;
    }
}

void FlexPassAgent::recv_retransmitted_data(Packet *pkt) {
    hdr_cmn *cmnh = hdr_cmn::access(pkt);
    hdr_tcp *tcph = hdr_tcp::access(pkt);
    int pkt_id = tcph->seqno() / (max_ethernet_size_ - xpass_hdr_size_);
    int datalen = cmnh->size() - tcph->hlen();

    if (pkt_received_[pkt_id] == -1) {
        pkt_received_[pkt_id] = 1;
        pkt_number_received++;
        if (credit_induced_data_received == 0) {
            recv_next_ += datalen;
        }
        if (pkt_number_received == pkt_number_to_receive) {
            output_fct();
        }
    }

    hdr_flexpass *xph = hdr_flexpass::access(pkt);
    // distance between expected sequence number and actual sequence number.
    int distance = xph->credit_seq() - c_recv_next_;

    if (distance < 0) {
        // credit packet reordering or credit sequence number overflow happend.
        fprintf(stderr, "ERROR: Credit Sequence number is reverted.\n");
        exit(1);
    }

    credit_total_ += (distance + 1);
    credit_dropped_ += distance;

    c_recv_next_ = xph->credit_seq() + 1;
    update_rtt(pkt);
}

void FlexPassAgent::recv_firstrtt_data(Packet *rev_pkt) {
    hdr_cmn *cmnh = hdr_cmn::access(rev_pkt);
    hdr_tcp *tcph = hdr_tcp::access(rev_pkt);
    hdr_flexpass *xph = hdr_flexpass::access(rev_pkt);
    int pkt_id = (tcph->seqno() - xph->firstrtt_init_seq()) / (max_ethernet_size_ - xpass_hdr_size_);
    int datalen = cmnh->size() - tcph->hlen();

    NS_LOG("Received FRB %d", pkt_id);

    if (pkt_received_[pkt_id] == -1) {
        pkt_received_[pkt_id] = 1;
        pkt_number_received++;
        if (credit_induced_data_received == 0) {
            recv_next_ += datalen;
        }
        if (pkt_number_received == pkt_number_to_receive) {
            output_fct();
        }
    }

    send(construct_data_ack(rev_pkt), 0);
}

void FlexPassAgent::recv_data_ack(Packet *rev_pkt) {
    hdr_tcp *tcph_rev_pkt = hdr_tcp::access(rev_pkt);
    hdr_flexpass *flexpass_rev_pkt = hdr_flexpass::access(rev_pkt);
    seq_t temp_seqno = tcph_rev_pkt->ackno();

    int temp_pkt_id = (temp_seqno - flexpass_rev_pkt->firstrtt_init_seq()) / (max_ethernet_size_ - xpass_hdr_size_);

    NS_LOG("Received FRB Ack %d", temp_pkt_id);
    for (int i = last_processed_ack_pkt_id + 1; i < temp_pkt_id; i++)
        if (pkt_acked_[i] == -1) {
            pkt_acked_[i] = 0;
            lost_pkt_num++;
        }
    if (last_processed_ack_pkt_id < temp_pkt_id) {
        pkt_acked_[temp_pkt_id] = 1;
        ack_counter_firstrtt_pkt++;
        last_processed_ack_pkt_id = temp_pkt_id;
    }

    if (flow_finished_with_firstrtt_pkt == 1 && sent_counter_firstrtt_pkt == ack_counter_firstrtt_pkt && lost_pkt_num == 0 && already_sent_credit_stop == 0) {
        // sender_calculated_fct_ = now() - fst_ - base_rtt_/2;
        credit_stop_timer_.sched(0);
        already_sent_credit_stop = 1;
    }
}

void FlexPassAgent::recv_probe(Packet *rev_pkt) {
    send(construct_probe_ack(rev_pkt), 0);
}

void FlexPassAgent::recv_probe_ack(Packet *rev_pkt) {
    hdr_tcp *tcph_rev_pkt = hdr_tcp::access(rev_pkt);
    seq_t temp_seqno = tcph_rev_pkt->ackno();

    int temp_pkt_id = temp_seqno / (max_ethernet_size_ - xpass_hdr_size_);

    for (int i = last_processed_ack_pkt_id + 1; i < temp_pkt_id; i++)
        if (pkt_acked_[i] == -1) {
            pkt_acked_[i] = 0;
            lost_pkt_num++;
        }
    if (pkt_acked_[temp_pkt_id] == -1) {
        pkt_acked_[temp_pkt_id] = 0;
        lost_pkt_num++;
    }
    if (last_processed_ack_pkt_id < temp_pkt_id) {
        last_processed_ack_pkt_id = temp_pkt_id;
    }
}

void FlexPassAgent::handle_retransmit() {
    switch (credit_recv_state_) {
    case FLEXPASS_RECV_CREDIT_REQUEST_SENT:
        send(construct_credit_request(byte_num_to_send), 0);
        retransmit_timer_.resched(retransmit_timeout_);
        break;
    default:
        /* To silence compiler */
        break;
    }
}

Packet *FlexPassAgent::construct_firstrtt_data() {
    Packet *p = allocpkt();
    fflush(stdout);
    if (!p) {
        fprintf(stderr, "ERROR: allockpkt() failed in construct_firstrtt_data()\n");
        exit(1);
    }
    hdr_tcp *tcph = hdr_tcp::access(p);
    hdr_cmn *cmnh = hdr_cmn::access(p);
    hdr_flexpass *xph = hdr_flexpass::access(p);
    int datalen = (int)min(max_ethernet_size_ - xpass_hdr_size_,
                           datalen_remaining());

    int pkt_id = (t_seqno_ - last_firstrtt_burst_) / (max_ethernet_size_ - xpass_hdr_size_);
    pkt_payload_size_[pkt_id] = datalen;
    NS_ASSERT(pkt_id < max_pktno_firstrtt);
    NS_LOG("Send firstrtt burst!! pktid=%d", pkt_id);
    if (datalen <= 0) {
        fprintf(stderr, "ERROR: datapacket has length of less than zero\n");
        exit(1);
    }

    tcph->seqno() = t_seqno_;
    tcph->ackno() = recv_next_;
    tcph->hlen() = xpass_hdr_size_;

    cmnh->size() = max(min_ethernet_size_, xpass_hdr_size_ + datalen);
    cmnh->ptype() = PT_XPASS_FIRSTRTT_DATA;
    xph->firstrtt_init_seq_ = last_firstrtt_burst_;

    last_sent_pkt_seqno = t_seqno_;
    t_seqno_ += datalen;

    return p;
}

Packet *FlexPassAgent::construct_retransmitted_data(seq_t seqno, Packet *credit) {
    Packet *p = allocpkt();
    if (!p) {
        fprintf(stderr, "ERROR: allockpkt() failed in construct_retransmitted_data()\n");
        exit(1);
    }
    hdr_tcp *tcph = hdr_tcp::access(p);
    hdr_cmn *cmnh = hdr_cmn::access(p);
    hdr_flexpass *xph = hdr_flexpass::access(p);
    hdr_flexpass *credit_xph = hdr_flexpass::access(credit);

    int pkt_id = seqno / (max_ethernet_size_ - xpass_hdr_size_);
    int datalen = pkt_payload_size_[pkt_id];

    if (datalen <= 0) {
        fprintf(stderr, "ERROR: datapacket has length of less than zero\n");
        exit(1);
    }

    tcph->seqno() = seqno;
    tcph->ackno() = recv_next_;
    tcph->hlen() = xpass_hdr_size_;

    cmnh->size() = max(min_ethernet_size_, xpass_hdr_size_ + datalen);
    cmnh->ptype() = PT_XPASS_RETRANSMITTED_DATA;

    xph->credit_sent_time() = credit_xph->credit_sent_time();
    xph->credit_seq() = credit_xph->credit_seq();

    // t_seqno_ += datalen;

    return p;
}

Packet *FlexPassAgent::construct_data_ack(Packet *rev_pkt) {
    Packet *p = allocpkt();
    if (!p) {
        fprintf(stderr, "ERROR: allockpkt() failed in construct_data_ack()\n");
        exit(1);
    }
    hdr_tcp *tcph = hdr_tcp::access(p);
    hdr_tcp *tcph_rev_pkt = hdr_tcp::access(rev_pkt);
    hdr_cmn *cmnh = hdr_cmn::access(p);
    hdr_flexpass *xph = hdr_flexpass::access(p);
    hdr_flexpass *xph_rev_pkt = hdr_flexpass::access(rev_pkt);

    tcph->seqno() = (seq_t)0;
    tcph->ackno() = tcph_rev_pkt->seqno();
    tcph->hlen() = xpass_hdr_size_;

    cmnh->size() = min_ethernet_size_;
    cmnh->ptype() = PT_XPASS_DATA_ACK;

    xph->firstrtt_init_seq() = xph_rev_pkt->firstrtt_init_seq();
    return p;
}

void FlexPassAgent::output_fct() {
    // send_credit_timer_.force_cancel();
    FILE *fct_out = fopen("outputs/fct.out", "a");
    fprintf(fct_out, "%d,%ld,%.10lf\n", fid_, recv_next_ - 1, now() - fst_);
    fclose(fct_out);
}

Packet *FlexPassAgent::construct_probe() {
    Packet *p = allocpkt();
    if (!p) {
        fprintf(stderr, "ERROR: allockpkt() failed in construct_probe()\n");
        exit(1);
    }
    hdr_tcp *tcph = hdr_tcp::access(p);
    hdr_cmn *cmnh = hdr_cmn::access(p);

    tcph->seqno() = last_sent_pkt_seqno;
    tcph->ackno() = 0;
    tcph->hlen() = xpass_hdr_size_;

    cmnh->size() = min_ethernet_size_;
    cmnh->ptype() = PT_XPASS_PROBE;

    return p;
}

Packet *FlexPassAgent::construct_probe_ack(Packet *rev_pkt) {
    Packet *p = allocpkt();
    if (!p) {
        fprintf(stderr, "ERROR: allockpkt() failed in construct_probe_ack()\n");
        exit(1);
    }
    hdr_tcp *tcph = hdr_tcp::access(p);
    hdr_tcp *tcph_rev_pkt = hdr_tcp::access(rev_pkt);
    hdr_cmn *cmnh = hdr_cmn::access(p);

    tcph->seqno() = (seq_t)0;
    tcph->ackno() = tcph_rev_pkt->seqno();
    tcph->hlen() = xpass_hdr_size_;

    cmnh->size() = min_ethernet_size_;
    cmnh->ptype() = PT_XPASS_PROBE_ACK;

    return p;
}

void FlexPassAgent::send_firstrtt_data() {
    /* Aeolus First RTT burst switch */
    if (!aeolus_firstrtt_burst_)
        return;

    NS_LOG("Sending FirstRTT data");
    // send data
    if (datalen_remaining() > 0 && sent_counter_firstrtt_pkt < max_pktno_firstrtt) {
        send(construct_firstrtt_data(), 0);
        sent_counter_firstrtt_pkt++;
        if (datalen_remaining() == 0) {
            flow_finished_with_firstrtt_pkt = 1;
        }
        if (probe_counter == probe_frequent - 1) {
            send(construct_probe(), 0);
        }
        probe_counter = (probe_counter + 1) % probe_frequent;
    }
    if (sender_flow_state_ == SENDER_WITHIN_FIRST_RTT && datalen_remaining() > 0 && sent_counter_firstrtt_pkt < max_pktno_firstrtt) {
        firstrtt_send_timer_.resched(firstrtt_transmit_interval);
    }
}
#endif