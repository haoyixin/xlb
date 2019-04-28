#include "modules/tcp_inc.h"
#include "modules/ether_out.h"

#include "conntrack/conn.h"
#include "conntrack/table.h"

namespace xlb::modules {

using conntrack::Conn;
using conntrack::Tuple2;
using conntrack::Tuple4;
using conntrack::VirtSvc;

using conntrack::get_conntrack_index;
using conntrack::tcp_bit_set::TCP_SYN_SET;

using conntrack::tcp_conntrack;
using conntrack::tcp_conntrack::TCP_CONNTRACK_ESTABLISHED;
using conntrack::tcp_conntrack::TCP_CONNTRACK_MAX;
using conntrack::tcp_conntrack::TCP_CONNTRACK_SYN_RECV;

using conntrack::ip_conntrack_dir::IP_CT_DIR_ORIGINAL;
using conntrack::ip_conntrack_dir::IP_CT_DIR_REPLY;

namespace {

constexpr uint64_t ol_flags = PKT_TX_IPV4 | PKT_TX_IP_CKSUM | PKT_TX_TCP_CKSUM;

void make_response_rst(Ipv4 *ipv4, Tcp *tcp) {
  if (tcp->flags & Tcp::kAck) {
    tcp->seq_num = tcp->ack_num;
    tcp->ack_num = be32_t(0);
    tcp->flags = Tcp::kRst;
  } else {
    if (tcp->flags & Tcp::kSyn) {
      tcp->ack_num = tcp->seq_num + be32_t(1);
    } else {
      tcp->ack_num = tcp->seq_num;
    }

    tcp->flags = (Tcp::kRst | Tcp::kAck);
    tcp->seq_num = be32_t(0);
  }

  // TODO: remove options ......

  std::swap(tcp->src_port, tcp->dst_port);
  std::swap(ipv4->src, ipv4->dst);
}

bool add_ttm_option(Tcp *hdr, Tuple2 &cli) {
  // TODO: ......
  return true;
}

}  // namespace

void TcpInc::InitInTrivial() {
  STABLE_INIT();
  Exec::RegisterTrivial();

  RegisterTask<TS("exec_sync")>(
      [](Context *) -> Result { return {.packets = Exec::Sync()}; });
}

void TcpInc::InitInSlave(uint16_t) {
  STABLE_INIT();
  CTABLE_INIT();
  Exec::RegisterSlave();

  RegisterTask<TS("stable_sync")>(
      [](Context *) -> Result { return {.packets = STABLE.Sync()}; });

  RegisterTask<TS("ctable_sync")>(
      [](Context *) -> Result { return {.packets = CTABLE.Sync()}; });

  RegisterTask<TS("exec_sync")>(
      [](Context *) -> Result { return {.packets = Exec::Sync()}; });
}

template <>
void TcpInc::Process<PMD>(Context *ctx, Packet *packet) {
  auto *ip_hdr = packet->head_data<Ipv4 *>(packet->l2_len());
  auto *tcp_hdr = packet->head_data<Tcp *>(packet->l2_len() + packet->l3_len());

  if (unlikely(!packet->rx_l4_cksum_good())) {
    ctx->Drop(packet);
    W_DVLOG(1) << "invalid tcp checksum";
    return;
  }

  W_DVLOG(3) << "tcp packet from: " << ToIpv4Address(ip_hdr->src)
             << " port: " << tcp_hdr->src_port.value()
             << " to: " << ToIpv4Address(ip_hdr->dst)
             << " port: " << tcp_hdr->dst_port.value();

  auto send = [&]() {
    packet->set_l4_len(tcp_hdr->offset * 4);
    packet->set_ol_flags(ol_flags);

    ip_hdr->checksum = 0;
    tcp_hdr->checksum = 0;
    tcp_hdr->checksum = rte_ipv4_phdr_cksum(
        reinterpret_cast<struct ipv4_hdr *>(ip_hdr), ol_flags);

    Handle<EtherOut, PMD>(ctx, packet);
  };

  auto set = get_conntrack_index(tcp_hdr);
  Tuple4 tuple{{ip_hdr->src, tcp_hdr->src_port},
               {ip_hdr->dst, tcp_hdr->dst_port}};

  VirtSvc::Ptr vs;
  Conn *conn;

  //  std::pair<bool, tcp_conntrack> trans;

  switch (set) {
    case TCP_SYN_SET:
      if (!(vs = STABLE.FindVs(tuple.dst))) {
        make_response_rst(ip_hdr, tcp_hdr);
        send();
        return;
      }
      if (!(conn = CTABLE.Get(vs, tuple.src))) {
        make_response_rst(ip_hdr, tcp_hdr);
        send();
        return;
      }
      break;

    default:
      if (!(conn = CTABLE.Find(tuple))) {
        if (tcp_hdr->flags & Tcp::kRst) {
          ctx->Drop(packet);
        } else {
          make_response_rst(ip_hdr, tcp_hdr);
          send();
        }

        return;
      }
      break;
  }

  auto dir = conn->direction(tuple);

  // TODO: reset invalid ......
  if (conn->UpdateState(set, dir).first &&
      !add_ttm_option(tcp_hdr, tuple.src)) {
    make_response_rst(ip_hdr, tcp_hdr);
    send();
  }

  auto real = conn->real();
  auto virt = conn->virt();

  if (dir == IP_CT_DIR_ORIGINAL) {
    ip_hdr->src = conn->local().ip;
    ip_hdr->dst = real->tuple().ip;
    tcp_hdr->src_port = conn->local().port;
    tcp_hdr->dst_port = real->tuple().port;

    real->IncrPacketsIn(1);
    real->IncrBytesIn(packet->data_len());
    virt->IncrPacketsIn(1);
    virt->IncrBytesIn(packet->data_len());
  } else {
    ip_hdr->src = virt->tuple().ip;
    ip_hdr->dst = conn->client().ip;
    tcp_hdr->src_port = virt->tuple().port;
    tcp_hdr->dst_port = conn->client().port;

    real->IncrPacketsOut(1);
    real->IncrBytesOut(packet->data_len());
    virt->IncrPacketsOut(1);
    virt->IncrBytesOut(packet->data_len());
  }

  send();
}

}  // namespace xlb::modules
