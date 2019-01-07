#include "config.h"

#define CONFIGURU_IMPLEMENTATION 1

#include "3rdparty/configuru.hpp"

#include "utils/boost.h"
#include "utils/endian.h"
#include "utils/numa.h"

#include "headers/ether.h"
#include "headers/ip.h"
#include "opts.h"

#include <boost/algorithm/cxx11/any_of.hpp>
#include <boost/fiber/numa/topology.hpp>
#include <glog/logging.h>
#include <rte_pci.h>

namespace al = boost::algorithm;

namespace xlb {

void Config::error_reporter(std::string str) {
  LOG(FATAL) << "Failed to parse config: " << str;
}

void Config::Load() {
  configuru::deserialize(&all_,
                         configuru::parse_file(FLAGS_config, configuru::JSON),
                         error_reporter);
  all_.validate();
}

void Config::validate() {
  // TODO: more detail info
  CHECK(!worker_cores.empty());
  CHECK_LE(worker_cores.size(), 32);

  //  auto topo = utils::Topology();

  auto socket = utils::PciSocketId(nic.pci_address);
  CHECK(socket.has_value());
  nic.socket = socket.value();

  for (auto &w : worker_cores) {
    auto socket_id = utils::CoreSocketId(w);
    CHECK(socket_id.has_value());
    CHECK_EQ(nic.socket, socket_id.value());
  }

  // TODO: validate legality
  CHECK_NE(rpc_ip_port, "");

  CHECK_NE(nic.name, "");
  CHECK_NE(nic.pci_address, "");
  CHECK_NE(nic.gateway, "");
  CHECK_NE(nic.netmask, "");

  CHECK_GT(nic.mtu, 0);
  CHECK_LE(nic.mtu, UINT16_MAX);

  CHECK_GT(hugepage, 0);
  CHECK_GT(packet_pool, 0);
  CHECK_EQ(packet_pool % 2, 0);

  CHECK(!nic.local_ips.empty());
  std::sort(nic.local_ips.begin(), nic.local_ips.end());

  CHECK(!nic.local_ips.empty());
  CHECK_EQ(nic.local_ips.size() % worker_cores.size(), 0);
  CHECK(std::unique(nic.local_ips.begin(), nic.local_ips.end()) ==
        nic.local_ips.end());

  utils::be32_t _dummy;

  for (auto &ip : nic.local_ips)
    CHECK(headers::ParseIpv4Address(ip, &_dummy));
}

} // namespace xlb
