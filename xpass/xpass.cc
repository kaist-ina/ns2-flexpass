#include "random.h"
#include "xpass.h"
#include "../tcp/template.h"

int hdr_xpass::offset_;
static class XPassHeaderClass: public PacketHeaderClass {
public:
  XPassHeaderClass(): PacketHeaderClass("PacketHeader/XPass", sizeof(hdr_xpass)) {
    bind_offset (&hdr_xpass::offset_);
  }
} class_xpass_hdr;

static class XPassClass: public TclClass {
public:
  XPassClass(): TclClass("Agent/XPass") {}
  TclObject* create(int, const char*const*) {
    return (new XPassAgent());
  }
} class_xpass;

void SendCreditTimer::expire(Event *) {
  a_->send_credit();
}

void CreditStopTimer::expire(Event *) {
  a_->send_credit_stop();
}

void SenderRetransmitTimer::expire(Event *) {
  a_->handle_sender_retransmit();
}

void ReceiverRetransmitTimer::expire(Event *) {
  a_->handle_receiver_retransmit();
}

void FCTTimer::expire(Event *) {
  a_->handle_fct();
}

void XPassAgent::delay_bind_init_all() {
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
  Agent::delay_bind_init_all();
}

int XPassAgent::delay_bind_dispatch(const char *varName, const char *localName,
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
  if (delay_bind(varName, localName, "exp_id_", &exp_id_, tracer)) {
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
  return Agent::delay_bind_dispatch(varName, localName, tracer);
}

void XPassAgent::init() {
  w_ = w_init_;
#if CFC_ALG == CFC_BIC
  bic_target_rate_ = base_credit_rate_/2;
#endif
  last_credit_rate_update_ = now();
}

static struct stat_ {
  int64_t tx_total{0};
  double max_credit_interval{0.0};
  int max_credit_interval_fid{0};
} stat;

int XPassAgent::command(int argc, const char*const* argv) {
  if (argc == 2) {
    if (strcmp(argv[1], "listen") == 0) {
      listen();
      return TCL_OK;
    } else if (strcmp(argv[1], "stop") == 0) {
      //on_transmission_ = false;
      return TCL_OK;
    } else if (strcmp(argv[1], "print-stat") == 0) {
      printf("tx_total\t%ld\n", stat.tx_total);
      printf("max_credit_interval_ms\t%lf\n", stat.max_credit_interval*1000);
      printf("max_credit_interval_fid\t%d\n", stat.max_credit_interval_fid);
      return TCL_OK;
    }
  } else if (argc == 3) {
    if (strcmp(argv[1], "advance-bytes") == 0) {
      if (credit_recv_state_ == XPASS_RECV_CLOSED) {
        advance_bytes(atol(argv[2]));
        return TCL_OK;
      } else {
        return TCL_ERROR;
      }
    }
  }
  return Agent::command(argc, argv);
}

void XPassAgent::recv(Packet* pkt, Handler*) {
  hdr_cmn *cmnh = hdr_cmn::access(pkt);

  switch (cmnh->ptype()) {
    case PT_XPASS_CREDIT_REQUEST:
      recv_credit_request(pkt);
      break;
    case PT_XPASS_CREDIT:
      recv_credit(pkt);
      break;
    case PT_XPASS_DATA:
      recv_data(pkt);
      break;
    case PT_XPASS_CREDIT_STOP:
      recv_credit_stop(pkt);
      break;
    case PT_XPASS_NACK:
      recv_nack(pkt);
      break;
    default:
      break;
  }
  Packet::free(pkt);
}

void XPassAgent::recv_credit_request(Packet *pkt) {
  hdr_xpass *xph = hdr_xpass::access(pkt);
  double lalpha = alpha_;
  switch (credit_send_state_) {
    case XPASS_SEND_CLOSE_WAIT:
      fct_timer_.force_cancel();
      init();
    //  assert(xph->sendbuffer_ > 0);
#if AIR
      if (xph->sendbuffer_ < 40) {
        lalpha = alpha_ * xph->sendbuffer_ / 40.0;
      }
#endif
      cur_credit_rate_ = (int)(lalpha * max_credit_rate_);
      cur_credit_rate_tr_ = cur_credit_rate_;
      // need to start to send credits.
      send_credit();
        
      // XPASS_SEND_CLOSED -> XPASS_SEND_CREDIT_REQUEST_RECEIVED
      credit_send_state_ = XPASS_SEND_CREDIT_SENDING;     
      break;
    case XPASS_SEND_CLOSED:
      init();
#if AIR
      if (xph->sendbuffer_ < 40) {
        lalpha = alpha_ * xph->sendbuffer_ / 40.0;
      } 
#endif
      cur_credit_rate_ = (int)(lalpha * max_credit_rate_);
      cur_credit_rate_tr_ = cur_credit_rate_;
      fst_ = xph->credit_sent_time();
      // need to start to send credits.
      send_credit();
        
      // XPASS_SEND_CLOSED -> XPASS_SEND_CREDIT_REQUEST_RECEIVED
      credit_send_state_ = XPASS_SEND_CREDIT_SENDING;     
      break;
  }
}

void XPassAgent::recv_credit(Packet *pkt) {
  credit_recved_rtt_++;
  switch (credit_recv_state_) {
    case XPASS_RECV_CREDIT_REQUEST_SENT:
      sender_retransmit_timer_.force_cancel();
      credit_recv_state_ = XPASS_RECV_CREDIT_RECEIVING;
      // first sender RTT.
      rtt_ = now() - rtt_;
      last_credit_recv_update_ = now();
    case XPASS_RECV_CREDIT_RECEIVING:
      // send data
      if (datalen_remaining() > 0) {
        send(construct_data(pkt), 0);
      }
 
      if (datalen_remaining() == 0) {
        if (credit_stop_timer_.status() != TIMER_IDLE) {
          fprintf(stderr, "Error: CreditStopTimer seems to be scheduled more than once.\n");
          exit(1);
        }
        // Because ns2 does not allow sending two consecutive packets, 
        // credit_stop_timer_ schedules CREDIT_STOP packet with no delay.
        credit_stop_timer_.sched(0);
      }
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
    case XPASS_RECV_CREDIT_STOP_SENT:
      if (datalen_remaining() > 0) {
        send(construct_data(pkt), 0);
      } else {
        credit_wasted_++;
      }
      credit_recved_++;
      break;
    case XPASS_RECV_CLOSE_WAIT:
      // accumulate credit count to check if credit stop has been delivered
      credit_wasted_++;
      break;
    case XPASS_RECV_CLOSED:
      credit_wasted_++;
      break;
  }
}

void XPassAgent::recv_data(Packet *pkt) {
  hdr_xpass *xph = hdr_xpass::access(pkt);
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

  process_ack(pkt);
  update_rtt(pkt);

  if (credit_send_state_ == XPASS_SEND_CLOSE_WAIT) {
    fct_ = now() - fst_;
    fct_timer_.resched(default_credit_stop_timeout_ * 5);
  }
}

void XPassAgent::recv_nack(Packet *pkt) {
  hdr_tcp *tcph = hdr_tcp::access(pkt);
  switch (credit_recv_state_) {
    case XPASS_RECV_CREDIT_STOP_SENT:
    case XPASS_RECV_CLOSE_WAIT:
    case XPASS_RECV_CLOSED:
      t_seqno_ = tcph->ackno();
      send(construct_credit_request(), 0);
      credit_recv_state_ = XPASS_RECV_CREDIT_REQUEST_SENT;
      sender_retransmit_timer_.resched(retransmit_timeout_);
      break;
    case XPASS_RECV_CREDIT_REQUEST_SENT:
    case XPASS_RECV_CREDIT_RECEIVING:
      // set t_seqno_ for retransmission
      t_seqno_ = tcph->ackno();
  }
}

static double max_delay = 0.0;

void XPassAgent::recv_credit_stop(Packet *pkt) {
  fct_ = now() - fst_;
  fct_timer_.resched(default_credit_stop_timeout_ * 5);
  send_credit_timer_.force_cancel();
  credit_send_state_ = XPASS_SEND_CLOSE_WAIT;
}

void XPassAgent::handle_fct() {
  char foname[40];
  assert(exp_id_);
  sprintf(foname, "outputs/fct_%d.out", exp_id_);

  FILE *fct_out = fopen(foname, "a");

  fprintf(fct_out, "%d,%ld,%.10lf,xpass\n", fid_, recv_next_-1, fct_);
  fclose(fct_out);
  credit_send_state_ = XPASS_SEND_CLOSED;

  stat.tx_total += recv_next_ - 1;
  stat.max_credit_interval = max_delay;
}

void XPassAgent::handle_sender_retransmit() {
  switch (credit_recv_state_) {
    case XPASS_RECV_CREDIT_REQUEST_SENT:
      send(construct_credit_request(), 0);
      sender_retransmit_timer_.resched(retransmit_timeout_);
      break;
    case XPASS_RECV_CREDIT_STOP_SENT:
      if (datalen_remaining() > 0) {
        credit_recv_state_ = XPASS_RECV_CREDIT_REQUEST_SENT;
        send(construct_credit_request(), 0);
        sender_retransmit_timer_.resched(retransmit_timeout_);
      } else {
        credit_recv_state_ = XPASS_RECV_CLOSE_WAIT;
        credit_recved_ = 0;
        sender_retransmit_timer_.resched((rtt_ > 0) ? rtt_ : default_credit_stop_timeout_); 
      }
      break;
    case XPASS_RECV_CLOSE_WAIT:
      if (credit_recved_ == 0) {
        FILE *waste_out = fopen("outputs/waste.out","a");

        credit_recv_state_ = XPASS_RECV_CLOSED;
        sender_retransmit_timer_.force_cancel();
        fprintf(waste_out, "%d,%ld,%d\n", fid_, curseq_-1, credit_wasted_);
        fclose(waste_out); 
        return;
      }
      // retransmit credit_stop
      send_credit_stop();
      break;
    case XPASS_RECV_CLOSED:
      fprintf(stderr, "Sender Retransmit triggered while connection is closed.");
      exit(1);
  }
}

void XPassAgent::handle_receiver_retransmit() {
  if (wait_retransmission_) {
    send(construct_nack(recv_next_), 0);
    receiver_retransmit_timer_.resched(retransmit_timeout_);
  }
}

Packet* XPassAgent::construct_credit_request() {
  Packet *p = allocpkt();
  if (!p) {
    fprintf(stderr, "ERROR: allockpkt() failed\n");
    exit(1);
  }

  hdr_tcp *tcph = hdr_tcp::access(p);
  hdr_cmn *cmnh = hdr_cmn::access(p);
  hdr_xpass *xph = hdr_xpass::access(p);

  tcph->seqno() = t_seqno_;
  tcph->ackno() = recv_next_;
  tcph->hlen() = xpass_hdr_size_;

  cmnh->size() = min_ethernet_size_;
  cmnh->ptype() = PT_XPASS_CREDIT_REQUEST;
  cmnh->tos() = TOS_XPASS_DATA;

  xph->credit_seq() = 0;
  xph->credit_sent_time_ = now();
  assert(pkt_remaining() > 0);
#if AIR
  xph->sendbuffer_ = pkt_remaining();
#endif

  // to measure rtt between credit request and first credit
  // for sender.
  rtt_ = now();

  return p;
}

Packet* XPassAgent::construct_credit_stop() {
  Packet *p = allocpkt();
  if (!p) {
    fprintf(stderr, "ERROR: allockpkt() failed\n");
    exit(1);
  }
  hdr_tcp *tcph = hdr_tcp::access(p);
  hdr_cmn *cmnh = hdr_cmn::access(p);
  hdr_xpass *xph = hdr_xpass::access(p);

  tcph->seqno() = t_seqno_;
  tcph->ackno() = recv_next_;
  tcph->hlen() = xpass_hdr_size_;
  
  cmnh->size() = min_ethernet_size_;
  cmnh->ptype() = PT_XPASS_CREDIT_STOP;
  cmnh->tos() = TOS_XPASS_DATA;

  xph->credit_seq() = 0;

  return p;
}

Packet* XPassAgent::construct_credit() {
  Packet *p = allocpkt();
  if (!p) {
    fprintf(stderr, "ERROR: allockpkt() failed\n");
    exit(1);
  }
  hdr_tcp *tcph = hdr_tcp::access(p);
  hdr_cmn *cmnh = hdr_cmn::access(p);
  hdr_xpass *xph = hdr_xpass::access(p);
  int credit_size = min_credit_size_;

  if (min_credit_size_ < max_credit_size_) {
    // variable credit size
    credit_size += Random::integer(max_credit_size_ - min_credit_size_ + 1);
  } else {
    // static credit size
    if (min_credit_size_ != max_credit_size_){
      fprintf(stderr, "ERROR: min_credit_size_ should be less than or equal to max_credit_size_\n");
      exit(1);
    }
  }

  tcph->seqno() = t_seqno_;
  tcph->ackno() = recv_next_;
  tcph->hlen() = credit_size;

  cmnh->size() = credit_size;
  cmnh->ptype() = PT_XPASS_CREDIT;

  xph->credit_sent_time() = now();
  xph->credit_seq() = c_seqno_;
  xph->fid_ = fid_;
  
  c_seqno_ = max(1, c_seqno_ + 1);

  return p;
}

Packet* XPassAgent::construct_data(Packet *credit) {
  Packet *p = allocpkt();
  if (!p) {
    fprintf(stderr, "ERROR: allockpkt() failed\n");
    exit(1);
  }
  hdr_tcp *tcph = hdr_tcp::access(p);
  hdr_cmn *cmnh = hdr_cmn::access(p);
  hdr_xpass *xph = hdr_xpass::access(p);
  hdr_xpass *credit_xph = hdr_xpass::access(credit);
  int datalen = (int)min(max_segment(),
                         datalen_remaining());

  if (datalen <= 0) {
    fprintf(stderr, "ERROR: datapacket has length of less than zero\n");
    exit(1);
  }
  tcph->seqno() = t_seqno_;
  tcph->ackno() = recv_next_;
  tcph->hlen() = xpass_hdr_size_;

  cmnh->size() = max(min_ethernet_size_, xpass_hdr_size_ + datalen);
  cmnh->ptype() = PT_XPASS_DATA;
  cmnh->tos() = TOS_XPASS_DATA;

  xph->credit_sent_time() = credit_xph->credit_sent_time();
  xph->credit_seq() = credit_xph->credit_seq();
  
  t_seqno_ += datalen;

  return p;
}

Packet* XPassAgent::construct_nack(seq_t seq_no) {
  Packet *p = allocpkt();
  if (!p) {
    fprintf(stderr, "ERROR: allockpkt() failed\n");
    exit(1);
  }
  hdr_tcp *tcph = hdr_tcp::access(p);
  hdr_cmn *cmnh = hdr_cmn::access(p);

  tcph->ackno() = seq_no;
  tcph->hlen() = xpass_hdr_size_; 

  cmnh->size() = min_ethernet_size_;
  cmnh->ptype() = PT_XPASS_NACK;

  return p;
}

void XPassAgent::send_credit() {
  double avg_credit_size = (min_credit_size_ + max_credit_size_)/2.0;
  double delay;

  credit_feedback_control();

  // send credit.
  send(construct_credit(), 0);

  // calculate delay for next credit transmission.
  delay = avg_credit_size / cur_credit_rate_;
  // add jitter
  if (max_jitter_ > min_jitter_) {
    double jitter = Random::uniform();
    jitter = jitter * (max_jitter_ - min_jitter_) + min_jitter_;
    // jitter is in the range between min_jitter_ and max_jitter_
    delay = delay*(1+jitter);
  }else if (max_jitter_ < min_jitter_) {
    fprintf(stderr, "ERROR: max_jitter_ should be larger than min_jitter_");
    exit(1);
  }

  max_delay = delay > max_delay ? delay : max_delay;
  send_credit_timer_.resched(delay);
}

void XPassAgent::send_credit_stop() {
  send(construct_credit_stop(), 0);
  // set on timer
  sender_retransmit_timer_.resched(rtt_ > 0 ? (2. * rtt_) : default_credit_stop_timeout_);    
  credit_recv_state_ = XPASS_RECV_CREDIT_STOP_SENT; //Later changes to XPASS_RECV_CLOSE_WAIT -> XPASS_RECV_CLOSED
}

void XPassAgent::advance_bytes(seq_t nb) {
  if(credit_recv_state_ != XPASS_RECV_CLOSED) {
    fprintf(stderr, "ERROR: tried to advance_bytes without XPASS_RECV_CLOSED\n");
  }
  if (nb <= 0) {
    fprintf(stderr, "ERROR: advanced bytes are less than or equal to zero\n");
  }

  // advance bytes
  curseq_ += nb;

  // send credit request
  send(construct_credit_request(), 0);
  sender_retransmit_timer_.sched(retransmit_timeout_);

  // XPASS_RECV_CLOSED -> XPASS_RECV_CREDIT_REQUEST_SENT
  credit_recv_state_ = XPASS_RECV_CREDIT_REQUEST_SENT;
}

void XPassAgent::process_ack(Packet *pkt) {
  hdr_cmn *cmnh = hdr_cmn::access(pkt);
  hdr_tcp *tcph = hdr_tcp::access(pkt);
  int datalen = cmnh->size() - tcph->hlen();
  if (datalen < 0) {
    fprintf(stderr, "ERROR: negative length packet has been detected.\n");
    exit(1);
  }
  if (tcph->seqno() > recv_next_) {
//    printf("[%d] %lf: data loss detected. (expected = %ld, received = %ld)\n",
//           fid_, now(), recv_next_, tcph->seqno());
    if (!wait_retransmission_) {
      wait_retransmission_ = true;
      send(construct_nack(recv_next_), 0);
      receiver_retransmit_timer_.resched(retransmit_timeout_);
    }
  } else if (tcph->seqno() == recv_next_) {
    if (wait_retransmission_) {
      wait_retransmission_ = false;
      receiver_retransmit_timer_.force_cancel();
    }
    recv_next_ += datalen;
  }
}

void XPassAgent::update_rtt(Packet *pkt) {
  hdr_xpass *xph = hdr_xpass::access(pkt);

  double rtt = now() - xph->credit_sent_time();
  if (rtt_ > 0.0) {
    rtt_ = 0.8*rtt_ + 0.2*rtt;
  }else {
    rtt_ = rtt;
  }
}

void XPassAgent::credit_feedback_control() {
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
  double loss_rate = credit_dropped_/(double)credit_total_;
  int min_rate = (int)(avg_credit_size() / rtt_);

#if CFC_ALG == CFC_ORIG
  double target_loss = (1.0 - cur_credit_rate_/(double)max_credit_rate_) * target_loss_scaling_;

  if (loss_rate > target_loss) {
    // congestion has been detected!
   if (loss_rate >= 1.0) {
      cur_credit_rate_ = (int)(avg_credit_size() / rtt_);
    } else {
      cur_credit_rate_ = (int)(avg_credit_size()*(credit_total_ - credit_dropped_)
                         / (now() - last_credit_rate_update_)
                         * (1.0+target_loss));
    }
    if (cur_credit_rate_ > old_rate) {
      cur_credit_rate_ = old_rate;
    }

    w_ = max(w_/2.0, min_w_);
    can_increase_w_ = false;
   // printf("[CFC] CGST CCR=%10d => %10d, LR=%10lf, TLR=%10lf\n", old_rate, cur_credit_rate_, loss_rate, target_loss);
  }else {
    // there is no congestion.
    if (can_increase_w_) {
      w_ = min(w_ + 0.05, 0.5);
    }else {
      can_increase_w_ = true;
    }

    if (cur_credit_rate_ < max_credit_rate_) {
      cur_credit_rate_ = (int)(w_*max_credit_rate_ + (1-w_)*cur_credit_rate_);
    }
    //printf("[CFC] ---- CCR=%10d => %10d, LR=%10lf, TLR=%10lf\n", old_rate, cur_credit_rate_, loss_rate, target_loss);
  }
#elif CFC_ALG == CFC_BIC
  double target_loss;
  int data_received_rate;

  if (cur_credit_rate_ >= base_credit_rate_) {
    target_loss = target_loss_scaling_;
  } else {
    target_loss = (1.0 - cur_credit_rate_/(double)base_credit_rate_) * target_loss_scaling_;
  }

  if (loss_rate > target_loss) {
    if (loss_rate >= 1.0) {
      data_received_rate = (int)(avg_credit_size() / rtt_);
    } else {
      data_received_rate = (int)(avg_credit_size()*(credit_total_ - credit_dropped_)
          / (now() - last_credit_rate_update_)*(1.0 + target_loss));
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
    if (bic_target_rate_ - cur_credit_rate_ <= 0.05*bic_target_rate_) {
      if (cur_credit_rate_ < bic_target_rate_) {
        cur_credit_rate_ = bic_target_rate_;
      } else {
        cur_credit_rate_ = cur_credit_rate_ + (cur_credit_rate_ - bic_target_rate_)
                                             *(1.0 + bic_beta_);
      }
    } else {
      cur_credit_rate_ = (cur_credit_rate_ + bic_target_rate_)/2;
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

void
XPassAgent::traceVar(TracedVar* v) 
{
	if (!channel_)
		return;

	double curtime;
	Scheduler& s = Scheduler::instance();
	const size_t TCP_WRK_SIZE = 128;
  char wrk[TCP_WRK_SIZE];

	curtime = &s ? s.clock() : 0;
	// XXX comparing addresses is faster than comparing names
/*	if (v == &cwnd_)
		snprintf(wrk, TCP_WRK_SIZE,
			 "%-8.7f %-2d %-2d %-2d %-2d %s %-6.3f\n",
			 curtime, addr(), port(), daddr(), dport(),
			 v->name(), double(*((TracedDouble*) v))); 
 	else if (v == &t_rtt_)
		snprintf(wrk, TCP_WRK_SIZE,
			 "%-8.7f %-2d %-2d %-2d %-2d %s %-6.3f\n",
			 curtime, addr(), port(), daddr(), dport(),
			 v->name(), int(*((TracedInt*) v))*tcp_tick_); 
	else if (v == &t_srtt_)
		snprintf(wrk, TCP_WRK_SIZE,
			 "%-8.5f %-2d %-2d %-2d %-2d %s %-6.3f\n",
			 curtime, addr(), port(), daddr(), dport(),
			 v->name(), 
			 (int(*((TracedInt*) v)) >> T_SRTT_BITS)*tcp_tick_); 
	else if (v == &t_rttvar_)
		snprintf(wrk, TCP_WRK_SIZE,
			 "%-8.5f %-2d %-2d %-2d %-2d %s %-6.3f\n",
			 curtime, addr(), port(), daddr(), dport(),
			 v->name(), 
			 int(*((TracedInt*) v))*tcp_tick_/4.0); 
	else*/
		snprintf(wrk, TCP_WRK_SIZE,
			 "%-8.7f %-2d %-2d %-2d %-2d %s %d\n",
			 curtime, addr(), port(), daddr(), dport(),
			 v->name(), int(*((TracedInt*) v))); 

	(void)Tcl_Write(channel_, wrk, -1);
}

void
XPassAgent::trace(TracedVar* v) 
{
		traceVar(v);
}


