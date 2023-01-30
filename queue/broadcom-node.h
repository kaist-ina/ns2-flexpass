#ifndef _QUEUE_BROADCOM_NODE_H_
#define _QUEUE_BROADCOM_NODE_H_
#include "connector.h"
#include "queue.h"
#include "random.h"
#include "packet.h"
#include <unordered_map>
class BroadcomNode : public TclObject {
private:
    static const int MAX_PORT_COUNT = 32; // rx max cnt
    std::unordered_map<nsaddr_t, int> routing_table_;
    int port_count{0};
    unsigned ingress_port_count{0};
    Queue* port_queue_map_[MAX_PORT_COUNT*2]; // count both rx and tx
    static const unsigned pCntMax = MAX_PORT_COUNT*2;
    static const unsigned qCnt = 8;	// Number of queues/priorities used


public:
    BroadcomNode();
    bool CheckIngressAdmission(int port, uint32_t qIndex, uint32_t psize);
    bool CheckEgressAdmission(int port, uint32_t qIndex, uint32_t psize);

    void UpdateIngressAdmission(int port, uint32_t qIndex, uint32_t psize, Packet *p);
    void UpdateEgressAdmission(int port, uint32_t qIndex, uint32_t psize, Packet* p);
    void RemoveFromIngressAdmission(int port, uint32_t qIndex, uint32_t psize);
    void RemoveFromEgressAdmission(int port, uint32_t qIndex, uint32_t psize);
    void RemoveFromIngressAdmission(Packet *p);
    void RemoveFromEgressAdmission(Packet *p);

    bool CheckIngressTLT(int port, uint32_t qIndex, uint32_t psize);
    bool CheckEgressTLT(int port, uint32_t qIndex, uint32_t psize);
    void UpdateIngressTLT(int port, uint32_t qIndex, uint32_t psize, Packet* p);
    void UpdateEgressTLT(int port, uint32_t qIndex, uint32_t psize, Packet* p);
    void RemoveFromIngressTLT(int port, uint32_t qIndex, uint32_t psize);
    void RemoveFromEgressTLT(int port, uint32_t qIndex, uint32_t psize);
    void RemoveFromIngressTLT(Packet* p);
    void RemoveFromEgressTLT(Packet *p);

    int FindDestinationPort(nsaddr_t target_ip) const;
    int FindDestinationPort(Connector* connector) const;

    void RegisterSwitchingTable(nsaddr_t target_ip, int port);
    int command(int argc, const char* const* argv);

    int RegisterPort(Queue* queue, bool ingress);
    int EstimateNextHop(Connector* current, Packet* p) const;
    void GetPauseClasses(uint32_t port, uint32_t qIndex, bool pClasses[]);
    bool GetResumeClasses(uint32_t port, uint32_t qIndex);

    bool ShouldSendCN(uint32_t indev, uint32_t ifindex, uint32_t qIndex);
    void SetBroadcomParams(
        uint32_t buffer_cell_limit_sp,         //ingress sp buffer threshold p.120
        uint32_t buffer_cell_limit_sp_shared,  //ingress sp buffer shared threshold, nonshare -> share
        uint32_t pg_min_cell,                  //ingress pg guarantee
        uint32_t port_min_cell,                //ingress port guarantee
        uint32_t pg_shared_limit_cell,         //max buffer for an ingress pg
        uint32_t port_max_shared_cell,         //max buffer for an ingress port
        uint32_t pg_hdrm_limit,                //ingress pg headroom
        uint32_t port_max_pkt_size,            //ingress global headroom
        uint32_t q_min_cell,                   //egress queue guaranteed buffer
        uint32_t op_uc_port_config1_cell,      //egress queue threshold
        uint32_t op_uc_port_config_cell,       //egress port threshold
        uint32_t op_buffer_shared_limit_cell,  //egress sp threshold
        uint32_t q_shared_alpha_cell,
        uint32_t port_share_alpha_cell,
        uint32_t pg_qcn_threshold);
    uint32_t GetUsedBufferTotal();
    void SetDynamicThreshold();
    void SetMarkingThreshold(uint32_t kmin, uint32_t kmax, double pmax);
    void SetTCPMarkingThreshold(uint32_t kmin, uint32_t kmax);

   protected:
    uint32_t GetIngressSP(uint32_t port, uint32_t pgIndex) const;
    uint32_t GetEgressSP(uint32_t port, uint32_t qIndex) const ;

   private:
    std::unordered_map<Packet*, std::pair<int, uint32_t>> packet_ingress_map_;  // port, priority
    std::unordered_map<Packet*, std::pair<int, uint32_t>> packet_egress_map_;   // port, priority
    std::unordered_map<Packet*, std::pair<int, uint32_t>> packet_uimp_ingress_map_;  // port, priority
    std::unordered_map<Packet*, std::pair<int, uint32_t>> packet_uimp_egress_map_;  // port, priority
    bool m_PFCenabled;
    uint32_t m_pCnt;
    uint32_t m_maxBufferBytes;
    uint32_t m_usedTotalBytes;

    uint32_t m_usedIngressPGBytes[pCntMax][qCnt];
    uint32_t m_usedIngressPortBytes[pCntMax];
    uint32_t m_usedIngressSPBytes[4];
    uint32_t m_usedIngressPGHeadroomBytes[pCntMax][qCnt];
    uint32_t m_maxBytes_TltUip;
    uint32_t m_bytesInQueueTltUip[pCntMax];
    uint32_t m_bytesInQueueTltUipTotal;
    uint32_t m_bytesIngressTltUip[pCntMax];
    uint32_t m_bytesEgressTltUip[pCntMax];
    uint32_t m_totaluipbytes;


    uint32_t m_usedEgressQMinBytes[pCntMax][qCnt];
    uint32_t m_usedEgressQSharedBytes[pCntMax][qCnt];
    uint32_t m_usedEgressPortBytes[pCntMax];
    uint32_t m_usedEgressSPBytes[4];

    bool m_pause_remote[pCntMax][qCnt];

    //ingress params
    uint32_t m_buffer_cell_limit_sp; //ingress sp buffer threshold p.120
    uint32_t m_buffer_cell_limit_sp_shared; //ingress sp buffer shared threshold, nonshare -> share
    uint32_t m_pg_min_cell; //ingress pg guarantee
    uint32_t m_port_min_cell; //ingress port guarantee
    uint32_t m_pg_shared_limit_cell; //max buffer for an ingress pg
    uint32_t m_port_max_shared_cell; //max buffer for an ingress port
    uint32_t m_pg_hdrm_limit; //ingress pg headroom
    uint32_t m_port_max_pkt_size; //ingress global headroom
    //still needs reset limits..
    uint32_t m_port_min_cell_off; // PAUSE off threshold
    uint32_t m_pg_shared_limit_cell_off;
    uint32_t m_global_hdrm_limit;


    //egress params
    uint32_t m_q_min_cell;	//egress queue guaranteed buffer
    uint32_t m_op_uc_port_config1_cell; //egress queue threshold
    uint32_t m_op_uc_port_config_cell; //egress port threshold
    uint32_t m_op_buffer_shared_limit_cell; //egress sp threshold

    //dynamic threshold
    double m_pg_shared_alpha_cell;
    double m_pg_shared_alpha_cell_egress;
    double m_pg_shared_alpha_cell_off_diff;
    double m_port_shared_alpha_cell;
    double m_port_shared_alpha_cell_off_diff;
    bool m_dynamicth;

    //QCN threshold
    uint32_t m_pg_qcn_threshold;
    uint32_t m_pg_qcn_threshold_max;
    double m_pg_qcn_maxp;

    double m_log_start;
    double m_log_end;
    double m_log_step;

    //dctcp
    uint32_t m_dctcp_threshold;
    uint32_t m_dctcp_threshold_max;
    bool m_enable_pfc_on_dctcp;

    uint32_t stat_max_usedEgressPortBytes [pCntMax] {0,};
    uint32_t stat_max_bytesEgressTltUip  [pCntMax] {0,};

    uint32_t stat_max_totalEgressBytes[pCntMax][qCnt] {0, };
	uint32_t stat_max_q1_total {0};
    uint32_t stat_max_q1_uip {0};
	uint32_t stat_max_q1_port {0};
	uint32_t stat_max_q2_total {0};
	uint32_t stat_max_q2_port {0};

	Random m_uniform_random_var;
};

#endif