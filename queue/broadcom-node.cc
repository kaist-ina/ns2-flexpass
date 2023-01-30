#include <broadcom-node.h>
#include <object.h>
#include <simulator.h>
#include <iostream>
#include "delay.h"
#include "classifier.h"
#include "trace.h"

static FILE* fp = nullptr;


#define MTU 1538
#define UPDATE_MAX_PARAM()                                                                                                                                                        \
	{                                                                                                                                                                             \
		stat_max_usedEgressPortBytes[port] = m_usedEgressPortBytes[port] > stat_max_usedEgressPortBytes[port] ? m_usedEgressPortBytes[port] : stat_max_usedEgressPortBytes[port]; \
		stat_max_bytesEgressTltUip[port] = m_bytesEgressTltUip[port] > stat_max_bytesEgressTltUip[port] ? m_bytesEgressTltUip[port] : stat_max_bytesEgressTltUip[port];           \
                                                                                                                                                                                  \
		for (int i = 0; i < qCnt; i++) {                                                                                                                                          \
			if (m_usedEgressQSharedBytes[port][i] + m_usedEgressQMinBytes[port][i] > stat_max_totalEgressBytes[port][i]) {                                                        \
				stat_max_totalEgressBytes[port][i] = m_usedEgressQSharedBytes[port][i] + m_usedEgressQMinBytes[port][i];                                                          \
			}                                                                                                                                                                     \
		}                                                                                                                                                                         \
		if (stat_max_q1_total < m_usedEgressQMinBytes[port][1] + m_usedEgressQSharedBytes[port][1]) {                                                                                                                    \
			stat_max_q1_total = m_usedEgressQMinBytes[port][1] + m_usedEgressQSharedBytes[port][1];                                                                                                                      \
			stat_max_q1_uip = m_bytesEgressTltUip[port]; /* Assumption here is that all uip goes to Queue 1 */                                                                                                                          \
			stat_max_q1_port = port;                                                                                                                                              \
		}                                                                                                                                                                         \
	                                                                                                                                                                         \
		if (stat_max_q2_total < m_usedEgressQMinBytes[port][2] + m_usedEgressQSharedBytes[port][2]) {                                                                                                                    \
			stat_max_q2_total = m_usedEgressQMinBytes[port][2] + m_usedEgressQSharedBytes[port][2];                                                                                                                      \
			stat_max_q2_port = port;                                                                                                                                              \
		}\
    }

static class BroadcomNodeClass : public TclClass
{
	public:
		BroadcomNodeClass() : TclClass("Queue/BroadcomNode") { }
		TclObject* create(int argc, const char*const* argv)
		{
			return (new BroadcomNode);
		}
} class_broadcom_node;

static struct {
  	uint64_t bytesDropped = 0;
	uint64_t packetsDropped = 0;
	bool print_stat = true;
} stat;


BroadcomNode::BroadcomNode () : TclObject() {
    for (int i = 0; i < MAX_PORT_COUNT*2; i++) {
        port_queue_map_[i] = NULL;
    }
    if (fp == nullptr) {
		fp = fopen("queue_len_mng.csv", "w");
	}

	m_maxBufferBytes = 0;
    bind("max_buffer_bytes_", &m_maxBufferBytes);
    bind("port_count_", &m_pCnt);
    bind("selective_dropping_threshold_", &m_maxBytes_TltUip);
   
    if (!m_maxBufferBytes) {
        std::cerr << "Total Shared buffer should be larger than 0 byte." << std::endl;
        abort();
    }
    if (m_pCnt > MAX_PORT_COUNT) {
        std::cerr << "Number of ports per switch should be less than " << MAX_PORT_COUNT << std::endl;
        abort();
    }

    m_usedTotalBytes = 0;
    m_totaluipbytes = 0;

    for (uint32_t i = 0; i < pCntMax; i++) // port 0 is not used
    {
        m_usedIngressPortBytes[i] = 0;
        m_usedEgressPortBytes[i] = 0;
        m_bytesIngressTltUip[i] = 0;
        m_bytesEgressTltUip[i] = 0;
        stat_max_bytesEgressTltUip[i] = 0;
        stat_max_usedEgressPortBytes[i] = 0;
        for (uint32_t j = 0; j < qCnt; j++)
        {
            m_usedIngressPGBytes[i][j] = 0;
            m_usedIngressPGHeadroomBytes[i][j] = 0;
            m_usedEgressQMinBytes[i][j] = 0;
            m_usedEgressQSharedBytes[i][j] = 0;
            m_pause_remote[i][j] = false;
        }
    }
    for (int i = 0; i < 4; i++)
    {
        m_usedIngressSPBytes[i] = 0;
        m_usedEgressSPBytes[i] = 0;
    }
    //ingress params
    m_buffer_cell_limit_sp = 4000 * MTU; //ingress sp buffer threshold
    //m_buffer_cell_limit_sp_shared=4000*MTU; //ingress sp buffer shared threshold, nonshare -> share
    m_pg_min_cell = MTU; //ingress pg guarantee
    m_port_min_cell = MTU; //ingress port guarantee
    m_pg_shared_limit_cell = 20 * MTU; //max buffer for an ingress pg
    m_port_max_shared_cell = 4800 * MTU; //max buffer for an ingress port
    m_pg_hdrm_limit = 103000; //2*10us*40Gbps+2*1.5kB //106 * MTU; //ingress pg headroom
    m_port_max_pkt_size = 100 * MTU; //ingress global headroom
    m_buffer_cell_limit_sp = m_maxBufferBytes - (m_pCnt)*m_pg_hdrm_limit - (m_pCnt)*std::max(qCnt * m_pg_min_cell, m_port_min_cell);  //12000 * MTU; //ingress sp buffer threshold
    //still needs reset limits..
    m_port_min_cell_off = 4700 * MTU;
    m_pg_shared_limit_cell_off = m_pg_shared_limit_cell - 2 * MTU;
    //egress params
    m_op_buffer_shared_limit_cell = m_maxBufferBytes - (m_pCnt)*std::max(qCnt * m_pg_min_cell, m_port_min_cell);  //m_maxBufferBytes; //per egress sp limit
    m_op_uc_port_config_cell = m_maxBufferBytes; //per egress port limit
    m_q_min_cell = 1 + MTU;
    m_op_uc_port_config1_cell = m_maxBufferBytes;
    //qcn
    m_pg_qcn_threshold = 60 * MTU;
    m_pg_qcn_threshold_max = 60 * MTU;
    m_pg_qcn_maxp = 0.1;
    //dynamic threshold
    m_dynamicth = false;
    //pfc and dctcp
    m_enable_pfc_on_dctcp = 1;
    m_dctcp_threshold = 40 * MTU;
    m_dctcp_threshold_max = 400 * MTU;
    // these are set as attribute
    m_pg_shared_alpha_cell = 0;
    m_pg_shared_alpha_cell_egress = 0;
    m_port_shared_alpha_cell = 128;   //not used for now. not sure whether this is used on switches
    m_pg_shared_alpha_cell_off_diff = 16;
    m_port_shared_alpha_cell_off_diff = 16;
    m_log_start = 2.1;
    m_log_end = 2.2;
    m_log_step = 0.00001;

    // ns3 attributes
    m_dctcp_threshold = 40 * MTU;
    m_dctcp_threshold_max = 400 * MTU;
    m_PFCenabled = true;
    m_pg_shared_alpha_cell = 1. / 128.;
    m_pg_shared_alpha_cell_egress = 1. / 4.;
}
bool BroadcomNode::CheckIngressAdmission(int port, uint32_t qIndex, uint32_t psize) {
    if (port < 0) return true;
    assert(m_pg_shared_alpha_cell > 0);

    if (m_usedTotalBytes + psize > m_maxBufferBytes)
    {
        stat.bytesDropped += psize;
        stat.packetsDropped += 1;
        return false;
    }
    if (m_usedIngressPGBytes[port][qIndex] + psize > m_pg_min_cell && m_usedIngressPortBytes[port] + psize > m_port_min_cell) // exceed guaranteed, use share buffer
    {
        if (m_usedIngressSPBytes[GetIngressSP(port, qIndex)] > m_buffer_cell_limit_sp) // check if headroom is already being used
        {
            if (m_usedIngressPGHeadroomBytes[port][qIndex] + psize > m_pg_hdrm_limit) // exceed headroom space
            {
                stat.bytesDropped += psize;
                stat.packetsDropped += 1;
                return false;
            }
        }
    }
    return true;
}

bool BroadcomNode::CheckEgressAdmission(int port, uint32_t qIndex, uint32_t psize) {
    if (port < 0) return true;
    assert(m_pg_shared_alpha_cell_egress > 0);

    //PFC OFF Nothing
    bool threshold = true;
    if (m_usedEgressSPBytes[GetEgressSP(port, qIndex)] + psize > m_op_buffer_shared_limit_cell)  //exceed the sp limit
    {
        stat.bytesDropped += psize;
        stat.packetsDropped += 1;
        return false;
    }
    if (m_usedEgressPortBytes[port] + psize > m_op_uc_port_config_cell)	//exceed the port limit
    {
        stat.bytesDropped += psize;
        stat.packetsDropped += 1;
        return false;
    }
    if (m_usedEgressQSharedBytes[port][qIndex] + psize > m_op_uc_port_config1_cell) //exceed the queue limit
    {
        stat.bytesDropped += psize;
        stat.packetsDropped += 1;
        return false;
    }
    //PFC ON return true here
    if (m_PFCenabled) {
        return true;
    }
    if ((double)m_usedEgressQSharedBytes[port][qIndex] + psize  > m_pg_shared_alpha_cell_egress*((double)m_op_buffer_shared_limit_cell - m_usedEgressSPBytes[GetEgressSP(port, qIndex)])) {
        threshold = false;
        stat.bytesDropped += psize;
        stat.packetsDropped += 1;
        //drop because it exceeds threshold
    }
    return threshold;
}

void BroadcomNode::UpdateIngressAdmission(int port, uint32_t qIndex, uint32_t psize, Packet *p){

    packet_ingress_map_[p] = std::pair<int, uint32_t>(port, qIndex);
    if (port < 0) return;
    m_usedTotalBytes += psize; //count total buffer usage
    m_usedIngressSPBytes[GetIngressSP(port, qIndex)] += psize;
    m_usedIngressPortBytes[port] += psize;
    m_usedIngressPGBytes[port][qIndex] += psize;
    if (m_usedIngressSPBytes[GetIngressSP(port, qIndex)] > m_buffer_cell_limit_sp)	//begin to use headroom buffer
    {
        m_usedIngressPGHeadroomBytes[port][qIndex] += psize;
    }
}
void BroadcomNode::UpdateEgressAdmission(int port, uint32_t qIndex, uint32_t psize, Packet* p) {

    packet_egress_map_[p] = std::pair<int, uint32_t>(port, qIndex);
    if (port < 0) return;
    if (m_usedEgressQMinBytes[port][qIndex] + psize < m_q_min_cell)  //guaranteed
    {
        m_usedEgressQMinBytes[port][qIndex] += psize;
        m_usedEgressPortBytes[port] = m_usedEgressPortBytes[port] + psize;
        UPDATE_MAX_PARAM();
#if EGRESS_STAT
        if (printQueueLen)
            fprintf(stderr, "%.8lf\tEGRESS_QUEUE\t%d\t%u\t%u\t%u\n", Simulator::Now().GetSeconds(), switchId, port,
                    std::max(m_usedEgressQMinBytes[port][qIndex], m_usedEgressPortBytes[port]),
                    m_bytesEgressTltUip[port]);
#endif
        return;
    } else {
        /*
            2 case
            First, when there is left space in q_min_cell, and we should use remaining space in q_min_cell and add rest to the shared_pool
            Second, just adding to shared pool
            */
        if (m_usedEgressQMinBytes[port][qIndex] != m_q_min_cell) {
            m_usedEgressQSharedBytes[port][qIndex] = m_usedEgressQSharedBytes[port][qIndex] + psize + m_usedEgressQMinBytes[port][qIndex] - m_q_min_cell;
            m_usedEgressPortBytes[port] = m_usedEgressPortBytes[port] + psize;  //+ m_usedEgressQMinBytes[port][qIndex] - m_q_min_cell ;
            m_usedEgressSPBytes[GetEgressSP(port, qIndex)] = m_usedEgressSPBytes[GetEgressSP(port, qIndex)] + psize + m_usedEgressQMinBytes[port][qIndex] - m_q_min_cell;
            m_usedEgressQMinBytes[port][qIndex] = m_q_min_cell;
        } else {
            m_usedEgressQSharedBytes[port][qIndex] += psize;
            m_usedEgressPortBytes[port] += psize;
            m_usedEgressSPBytes[GetEgressSP(port, qIndex)] += psize;
        }
        UPDATE_MAX_PARAM();
#if EGRESS_STAT
        if (printQueueLen)
            fprintf(stderr, "%.8lf\tEGRESS_QUEUE\t%d\t%u\t%u\t%u\n", Simulator::Now().GetSeconds(), switchId, port,
                    std::max(m_usedEgressQMinBytes[port][qIndex], m_usedEgressPortBytes[port]),
                    m_bytesEgressTltUip[port]);
#endif
    }
}

void BroadcomNode::RemoveFromIngressAdmission(Packet *p) {
    hdr_cmn* cmnh = hdr_cmn::access(p);
    int pktSize = cmnh->size();
    auto it = packet_ingress_map_.find(p);
    if (it == packet_ingress_map_.end()) {
        fprintf(stderr, "Missing packet priority information");
        abort();
    }
    RemoveFromIngressAdmission(it->second.first, it->second.second, pktSize);
}

void BroadcomNode::RemoveFromEgressAdmission(Packet* p) {
    hdr_cmn* cmnh = hdr_cmn::access(p);
    int pktSize = cmnh->size();
    auto it = packet_egress_map_.find(p);
    if (it == packet_egress_map_.end()) {
        fprintf(stderr, "Missing packet priority information");
        abort();
    }
    RemoveFromEgressAdmission(it->second.first, it->second.second, pktSize);
}
void BroadcomNode::RemoveFromIngressAdmission(int port, uint32_t qIndex, uint32_t psize) {
    if (port < 0) return;
    if (m_usedTotalBytes < psize) {
        m_usedTotalBytes = psize;
        std::cerr << "Warning : Illegal Remove" << std::endl;
    }
    if (m_usedIngressSPBytes[GetIngressSP(port, qIndex)] < psize) {
        m_usedIngressSPBytes[GetIngressSP(port, qIndex)] = psize;
        std::cerr << "Warning : Illegal Remove" << std::endl;
    }
    if (m_usedIngressSPBytes[GetIngressSP(port, qIndex)] < psize) {
        m_usedIngressSPBytes[GetIngressSP(port, qIndex)] = psize;
        std::cerr << "Warning : Illegal Remove" << std::endl;
    }
    if (m_usedIngressPortBytes[port] < psize) {
        m_usedIngressPortBytes[port] = psize;
        std::cerr << "Warning : Illegal Remove" << std::endl;
    }
    if (m_usedIngressPGBytes[port][qIndex] < psize) {
        m_usedIngressPGBytes[port][qIndex] = psize;
        std::cerr << "Warning : Illegal Remove" << std::endl;
    }
    m_usedTotalBytes -= psize;
    m_usedIngressSPBytes[GetIngressSP(port, qIndex)] -= psize;
    m_usedIngressPortBytes[port] -= psize;
    m_usedIngressPGBytes[port][qIndex] -= psize;
    if ((double)m_usedIngressPGHeadroomBytes[port][qIndex] - psize > 0)
        m_usedIngressPGHeadroomBytes[port][qIndex] -= psize;
    else
        m_usedIngressPGHeadroomBytes[port][qIndex] = 0;

#if EGRESS_STAT
    if (printQueueLen)
        fprintf(stderr, "%.8lf\tEGRESS_QUEUE\t%d\t%u\t%u\t%u\n", Simulator::Now().GetSeconds(), switchId, port,
                std::max(m_usedEgressQMinBytes[port][qIndex], m_usedEgressPortBytes[port]),
                m_bytesEgressTltUip[port]);
#endif
}
void BroadcomNode::RemoveFromEgressAdmission(int port, uint32_t qIndex, uint32_t psize) {
    if (port < 0) return;
    if (m_usedEgressQMinBytes[port][qIndex] < m_q_min_cell)  //guaranteed
    {
        if (m_usedEgressQMinBytes[port][qIndex] < psize) {
            std::cerr << "STOP overflow\n";
        }
        m_usedEgressQMinBytes[port][qIndex] -= psize;
        m_usedEgressPortBytes[port] -= psize;
        UPDATE_MAX_PARAM();
        return;
    } else {
        /*
				2 case
				First, when packet was using both qminbytes and qsharedbytes we should substract from each one
				Second, just subtracting shared pool
				*/

        //first case
        if (m_usedEgressQMinBytes[port][qIndex] == m_q_min_cell && m_usedEgressQSharedBytes[port][qIndex] < psize) {
            m_usedEgressQMinBytes[port][qIndex] = m_usedEgressQMinBytes[port][qIndex] + m_usedEgressQSharedBytes[port][qIndex] - psize;
            m_usedEgressSPBytes[GetEgressSP(port, qIndex)] = m_usedEgressSPBytes[GetEgressSP(port, qIndex)] - m_usedEgressQSharedBytes[port][qIndex];
            m_usedEgressQSharedBytes[port][qIndex] = 0;
            if (m_usedEgressPortBytes[port] < psize) {
                std::cerr << "STOP overflow\n";
            }
            m_usedEgressPortBytes[port] -= psize;

        } else {
            if (m_usedEgressQSharedBytes[port][qIndex] < psize || m_usedEgressPortBytes[port] < psize || m_usedEgressSPBytes[GetEgressSP(port, qIndex)] < psize) {
                std::cerr << "STOP overflow\n";
            }
            m_usedEgressQSharedBytes[port][qIndex] -= psize;
            m_usedEgressPortBytes[port] -= psize;
            m_usedEgressSPBytes[GetEgressSP(port, qIndex)] -= psize;
        }
        UPDATE_MAX_PARAM();
    }
}

void BroadcomNode::GetPauseClasses(uint32_t port, uint32_t qIndex, bool pClasses[]) {
    if (port >= pCntMax) {
        std::cerr << "ERROR: port is " << port << std::endl;
    }
    if (m_dynamicth) {
        for (uint32_t i = 0; i < qCnt; i++) {
            pClasses[i] = false;
            if (m_usedIngressPGBytes[port][i] <= m_pg_min_cell + m_port_min_cell)
                continue;
            if (i == 1 && !m_enable_pfc_on_dctcp)  //dctcp
                continue;

            //std::cerr << "BCM : Used=" << m_usedIngressPGBytes[port][i] << ", thresh=" << m_pg_shared_alpha_cell*((double)m_buffer_cell_limit_sp - m_usedIngressSPBytes[GetIngressSP(port, qIndex)]) + m_pg_min_cell+m_port_min_cell << std::endl;

            if ((double)m_usedIngressPGBytes[port][i] - m_pg_min_cell - m_port_min_cell > m_pg_shared_alpha_cell * ((double)m_buffer_cell_limit_sp - m_usedIngressSPBytes[GetIngressSP(port, qIndex)]) || m_usedIngressPGHeadroomBytes[port][qIndex] != 0) {
                pClasses[i] = true;
            }
        }
    } else {
        if (m_usedIngressPortBytes[port] > m_port_max_shared_cell)  //pause the whole port
        {
            for (uint32_t i = 0; i < qCnt; i++) {
                if (i == 1 && !m_enable_pfc_on_dctcp)  //dctcp
                    pClasses[i] = false;

                pClasses[i] = true;
            }
            return;
        } else {
            for (uint32_t i = 0; i < qCnt; i++) {
                pClasses[i] = false;
            }
        }
        if (m_usedIngressPGBytes[port][qIndex] > m_pg_shared_limit_cell) {
            if (qIndex == 1 && !m_enable_pfc_on_dctcp)
                return;

            pClasses[qIndex] = true;
        }
    }
    return;
}

bool BroadcomNode::GetResumeClasses(uint32_t port, uint32_t qIndex) {
    if (m_dynamicth) {
        if ((double)m_usedIngressPGBytes[port][qIndex] - m_pg_min_cell - m_port_min_cell < m_pg_shared_alpha_cell * ((double)m_buffer_cell_limit_sp - m_usedIngressSPBytes[GetIngressSP(port, qIndex)] - m_pg_shared_alpha_cell_off_diff) && m_usedIngressPGHeadroomBytes[port][qIndex] == 0) {
            return true;
        }
    } else {
        if (m_usedIngressPGBytes[port][qIndex] < m_pg_shared_limit_cell_off && m_usedIngressPortBytes[port] < m_port_min_cell_off) {
            return true;
        }
    }
    return false;
}

uint32_t BroadcomNode::GetIngressSP(uint32_t port, uint32_t pgIndex) const {
    // if (pgIndex == 1)
    //     return 1;
    // else
        return 0;
}

uint32_t BroadcomNode::GetEgressSP(uint32_t port, uint32_t qIndex) const {
    // if (qIndex == 1)
    //     return 1;
    // else
        return 0;
}

bool BroadcomNode::ShouldSendCN(uint32_t indev, uint32_t ifindex, uint32_t qIndex) {
    if (qIndex == qCnt - 1)
        return false;
    if (qIndex == 1)  //dctcp
    {
        if (m_usedEgressQSharedBytes[ifindex][qIndex] > m_dctcp_threshold_max) {
            return true;
        } else {
            if (m_usedEgressQSharedBytes[ifindex][qIndex] > m_dctcp_threshold && m_dctcp_threshold != m_dctcp_threshold_max) {
                double p = 1.0 * (m_usedEgressQSharedBytes[ifindex][qIndex] - m_dctcp_threshold) / (m_dctcp_threshold_max - m_dctcp_threshold);
                if (m_uniform_random_var.uniform() < p)
                    return true;
            }
        }
        return false;
    } else {
        if (m_usedEgressQSharedBytes[ifindex][qIndex] > m_pg_qcn_threshold_max) {
            return true;
        } else if (m_usedEgressQSharedBytes[ifindex][qIndex] > m_pg_qcn_threshold && m_pg_qcn_threshold != m_pg_qcn_threshold_max) {
            double p = 1.0 * (m_usedEgressQSharedBytes[ifindex][qIndex] - m_pg_qcn_threshold) / (m_pg_qcn_threshold_max - m_pg_qcn_threshold) * m_pg_qcn_maxp;
            if (m_uniform_random_var.uniform() < p)
                return true;
        }
        return false;
    }
}

void BroadcomNode::SetBroadcomParams(
    uint32_t buffer_cell_limit_sp,         //ingress sp buffer threshold p.120
    uint32_t buffer_cell_limit_sp_shared,  //ingress sp buffer shared threshold p.120, nonshare -> share
    uint32_t pg_min_cell,                  //ingress pg guarantee p.121					---1
    uint32_t port_min_cell,                //ingress port guarantee						---2
    uint32_t pg_shared_limit_cell,         //max buffer for an ingress pg			---3	PAUSE
    uint32_t port_max_shared_cell,         //max buffer for an ingress port		---4	PAUSE
    uint32_t pg_hdrm_limit,                //ingress pg headroom
    uint32_t port_max_pkt_size,            //ingress global headroom
    uint32_t q_min_cell,                   //egress queue guaranteed buffer
    uint32_t op_uc_port_config1_cell,      //egress queue threshold
    uint32_t op_uc_port_config_cell,       //egress port threshold
    uint32_t op_buffer_shared_limit_cell,  //egress sp threshold
    uint32_t q_shared_alpha_cell,
    uint32_t port_share_alpha_cell,
    uint32_t pg_qcn_threshold) {
    m_buffer_cell_limit_sp = buffer_cell_limit_sp;
    m_buffer_cell_limit_sp_shared = buffer_cell_limit_sp_shared;
    m_pg_min_cell = pg_min_cell;
    m_port_min_cell = port_min_cell;
    m_pg_shared_limit_cell = pg_shared_limit_cell;
    m_port_max_shared_cell = port_max_shared_cell;
    m_pg_hdrm_limit = pg_hdrm_limit;
    m_port_max_pkt_size = port_max_pkt_size;
    m_q_min_cell = q_min_cell;
    m_op_uc_port_config1_cell = op_uc_port_config1_cell;
    m_op_uc_port_config_cell = op_uc_port_config_cell;
    m_op_buffer_shared_limit_cell = op_buffer_shared_limit_cell;
    m_pg_shared_alpha_cell = q_shared_alpha_cell;
    m_port_shared_alpha_cell = port_share_alpha_cell;
    m_pg_qcn_threshold = pg_qcn_threshold;
}

uint32_t BroadcomNode::GetUsedBufferTotal() {
    return m_usedTotalBytes;
}

void BroadcomNode::SetDynamicThreshold() {
    m_dynamicth = true;
    m_pg_shared_limit_cell = m_maxBufferBytes;  //using dynamic threshold, we don't respect the static thresholds anymore
    m_port_max_shared_cell = m_maxBufferBytes;
    return;
}

void BroadcomNode::SetMarkingThreshold(uint32_t kmin, uint32_t kmax, double pmax) {
    m_pg_qcn_threshold = kmin * 1030;
    m_pg_qcn_threshold_max = kmax * 1030;
    m_pg_qcn_maxp = pmax;
}

void BroadcomNode::SetTCPMarkingThreshold(uint32_t kmin, uint32_t kmax) {
    m_dctcp_threshold = kmin * 1500;
    m_dctcp_threshold_max = kmax * 1500;
}

int BroadcomNode::FindDestinationPort(nsaddr_t target_ip) const {
    auto it = routing_table_.find(target_ip);
    if (it == routing_table_.end())
        return -1;
    return it->second;
}

int BroadcomNode::FindDestinationPort(Connector *connector) const {
    if (!connector)
        return -1;
    
    if (!port_count)
        return -1;

    for (int i = 0; i < MAX_PORT_COUNT*2 ; i++) {
        if (port_queue_map_[i] == connector) {
            return i;
        }
    }
    return -1;
}

void BroadcomNode::RegisterSwitchingTable(nsaddr_t target_ip, int port) {
    routing_table_[target_ip] = port;
}

int BroadcomNode::RegisterPort(Queue *queue, bool ingress) {
    if (ingress) {
        if (ingress_port_count >= m_pCnt) {
            fprintf(stderr, "Port count per BroadcomNode can't exceed %d (Configured in ns-defualt.tcl).\n", m_pCnt);
            abort();
        }
        ingress_port_count++;
    }
    port_queue_map_[port_count] = queue;
    return port_count++;
}


int BroadcomNode::EstimateNextHop(Connector* current, Packet* p) const {
    if (!p || !current) return -1;
    const char* cmd_cls[] = {"self", "isClassifier"};
    const char* cmd_que[] = {"self", "isQueue"};
    const char* cmd_tra[] = {"self", "isTrace"};
    const char* cmd_del[] = {"self", "isDelay"};
    const char* cmd_con[] = {"self", "isConnector"};
    const char* cmd_agn[] = {"self", "isAgent"};

    TclObject* iter = current;
    // printf("Find start: ");
    int limit = 100;
    while (iter != NULL) {
        limit--;
        if (!limit) {
            break;
        }
        if (iter->command(2, cmd_cls) == (TCL_OK)) {
            // printf("%p(Classifier) -> ", iter);
            Classifier* cls = reinterpret_cast<Classifier*>(iter);
            iter = cls->find(p);
        } else if (iter->command(2, cmd_tra) == (TCL_OK)) {
            // printf("%p(Trace) -> ", iter);
            Trace* tra = reinterpret_cast<Trace*>(iter);
            iter = tra->target();
        } else if (iter->command(2, cmd_del) == (TCL_OK)) {
            // printf("%p(LinkDelay) -> ", iter);
            LinkDelay* ldl = reinterpret_cast<LinkDelay*>(iter);
            iter = ldl->target();
        } else if (iter->command(2, cmd_que) == (TCL_OK)) {
            if (iter == current) {
                // printf("%p(Queue) -> ", iter);
                Queue* que = reinterpret_cast<Queue*>(iter);
                iter = que->target();
            } else {
                // printf("%p(Queue) : Found!\n", iter);
                return FindDestinationPort(reinterpret_cast<Connector*>(iter));
            }
        } else if (iter->command(2, cmd_agn) == (TCL_OK)) {
            // printf("%p(Agent) -> ", iter);
            break;
            // Agent* agn = reinterpret_cast<Agent*>(iter);
            // iter = agn->target();
        } else if (iter->command(2, cmd_con) == (TCL_OK)) {
            // printf("%p(Connector) -> ", iter);
            Connector* con = reinterpret_cast<Connector*>(iter);
            iter = con->target();
        } else {
            break;
        }
    }

    printf("undefined!\n");
    return -1;
}

int BroadcomNode::command(int argc, const char* const* argv) {
    if (argc == 2) {
        if (strcmp(argv[1], "print-egress-stat") == 0) {
            uint32_t max_val1 = 0, max_val2[qCnt] = {0, };
            unsigned max_id1, max_id2;
            for (unsigned i = 0; i < pCntMax; i++) {
                if (stat_max_bytesEgressTltUip[i] > max_val1) {
                    max_val1 = stat_max_bytesEgressTltUip[i];
                    max_id1 = i;
                }

                for (unsigned q = 0; q < qCnt; q++) {
                    if (stat_max_totalEgressBytes[i][q] > max_val2[q]) {
                        max_val2[q] = stat_max_totalEgressBytes[i][q];
                        max_id2 = i;
                    }
                }
            }

            printf("Max: Port %u Egress : uimp %u bytes\n", max_id1, max_val1);
            for(unsigned q = 0; q < qCnt; q++) {
                printf("Max: Port %u Egress : Queue %u: %u bytes\n", max_id2, q, max_val2[q]);
            }

			return (TCL_OK);
        } else if (strcmp(argv[1], "print-egress-stat-q1") == 0) {
            // only count q1 this time
			printf("Q1Max: Port %u Egress : Total %u, Reactive(UIP) %u, Proactive %u\n", stat_max_q1_port, stat_max_q1_total, stat_max_q1_uip, stat_max_q1_total - stat_max_q1_uip);
			printf("Q2Max: Port %u Egress : Total %u\n", stat_max_q2_port, stat_max_q2_total);
			return (TCL_OK);
        }
    }
    return TclObject::command(argc, argv);
}

bool BroadcomNode::CheckIngressTLT(int port, uint32_t qIndex, uint32_t psize) {
    return true;
}

bool BroadcomNode::CheckEgressTLT(int port, uint32_t qIndex, uint32_t psize) {
    // if m_maxBytes_TltUip is 0, regard TLT is disabled
    if (m_maxBytes_TltUip && m_bytesEgressTltUip[port] + psize >= m_maxBytes_TltUip) {
        return false;
    }
    return true;
}

void BroadcomNode::UpdateIngressTLT(int port, uint32_t qIndex, uint32_t psize, Packet* p) {
    packet_uimp_ingress_map_[p] = std::pair<int, uint32_t>(port, qIndex);
    m_bytesIngressTltUip[port] += psize;
    m_totaluipbytes += psize;
    return;
}
void BroadcomNode::UpdateEgressTLT(int port, uint32_t qIndex, uint32_t psize, Packet* p) {
    packet_uimp_egress_map_[p] = std::pair<int, uint32_t>(port, qIndex);
    m_bytesEgressTltUip[port] += psize;
    UPDATE_MAX_PARAM();
    return;
}

void BroadcomNode::RemoveFromIngressTLT(Packet* p) {
    hdr_cmn* cmnh = hdr_cmn::access(p);
    int pktSize = cmnh->size();
    auto it = packet_uimp_ingress_map_.find(p);
    if (it == packet_uimp_ingress_map_.end()) {
        fprintf(stderr, "Missing packet priority information");
        abort();
    }
    RemoveFromIngressTLT(it->second.first, it->second.second, pktSize);
}

void BroadcomNode::RemoveFromEgressTLT(Packet* p) {
    hdr_cmn* cmnh = hdr_cmn::access(p);
    int pktSize = cmnh->size();
    auto it = packet_uimp_egress_map_.find(p);
    if (it == packet_uimp_egress_map_.end()) {
        fprintf(stderr, "Missing packet priority information");
        abort();
    }
    RemoveFromEgressTLT(it->second.first, it->second.second, pktSize);
}

void BroadcomNode::RemoveFromIngressTLT(int port, uint32_t qIndex, uint32_t psize) {
    m_bytesIngressTltUip[port] -= psize;
    m_totaluipbytes -= psize;
}

void BroadcomNode::RemoveFromEgressTLT(int port, uint32_t qIndex, uint32_t psize) {
    m_bytesEgressTltUip[port] -= psize;
    UPDATE_MAX_PARAM();
}
