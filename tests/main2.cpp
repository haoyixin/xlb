
//#include <rte_eal.h>
#include "rte_eal.h"
#include <utils/format.h>

#include "module.h"
#include "opts.h"
#include "packet.h"
#include "packet_pool.h"
#include "port.h"
#include "trash/task.h"
#include "worker.h"

#include "config.h"

#include "utils/allocator.h"
#include "utils/cuckoo_map.h"
#include "utils/endian.h"
#include "utils/random.h"
#include "utils/time.h"

#include "headers/ether.h"

#include "ports/pmd.h"

#include "glog/logging.h"

#include "headers/arp.h"

#include "headers/ip.h"

#include "utils/range.h"

#include "utils/simd.h"

#include "headers/tcp.h"

#include "utils/lock_less_queue.h"

//#include "ports/pmd.h"

#include "utils/thread_local.h"

#include "control.h"

static std::atomic<int> _i;

DECLARE_PORT(PMD);

int main() {

  //  char *argv[] = {"-c", "0xff", "-n", "2", "-m", "256", "--no-shconf"};
  FLAGS_alsologtostderr = 1;

  //  rte_eal_init(7, argv);
  google::InitGoogleLogging("xlb");

  xlb::Config::Load();

  xlb::PacketPool::CreatePools();

  PORT_INIT(PMD);

  xlb::PacketBatch batch{};
  int cnt;
  xlb::Packet *pkt;
  int i;

  xlb::headers::Ethernet *eth;
  xlb::headers::Ipv4 *ip;

  for (uint64_t i : xlb::utils::range(0)) {
    batch.clear();
    batch.set_cnt(PORTS_PMD->RecvPackets(i % 3, batch.pkts(), 32));
    for (auto &p : batch) {
      eth = p.head_data<xlb::headers::Ethernet *>();
      if (eth->ether_type.value() == xlb::headers::Ethernet::Type::kIpv4) {
        ip = p.head_data<xlb::headers::Ipv4 *>(sizeof(xlb::headers::Ethernet));
        std::cout << "q: " << i % 3 << " l2-s: " << eth->src_addr.ToString()
                  << " l3-s: " << xlb::headers::ToIpv4Address(ip->src)
                  << std::endl;
      }
    }
  }

  return 0;
}