#include "config.h"

#define CONFIGURU_IMPLEMENTATION 1

#include "3rdparty/configuru.hpp"

#include <boost/algorithm/cxx11/any_of.hpp>
#include <boost/fiber/numa/topology.hpp>

#include <gflags/gflags.h>
#include <glog/logging.h>

#include <rte_pci.h>

#include "utils/boost.h"
#include "utils/endian.h"
#include "utils/numa.h"
#include "utils/common.h"

#include "headers/ether.h"
#include "headers/ip.h"

DEFINE_string(config, "../config.json", "Path of config file.");

namespace xlb {

namespace {

void error_reporter(std::string str) {
  LOG(FATAL) << "Failed to parse config: " << str;
}

} // namespace

void Config::Load() {
  configuru::deserialize(&CONFIG,
                         configuru::parse_file(FLAGS_config, configuru::JSON),
                         error_reporter);
  CONFIG.validate();
}

void Config::validate() {
  CHECK_GE(master_core, 0);
  CHECK(utils::CoreSocketId(master_core));

  // TODO: more detail log
  CHECK(!slave_cores.empty());
  CHECK_LE(slave_cores.size(), 32);

  auto socket = utils::PciSocketId(nic.pci_address);
  CHECK(socket.has_value());
  nic.socket = socket.value();

  for (auto &w : slave_cores) {
    auto socket_id = utils::CoreSocketId(w);
    CHECK(socket_id.has_value());
    CHECK_EQ(nic.socket, socket_id.value());
  }

  // TODO: validate legality
  CHECK_NE(rpc_ip_port, "");

  CHECK_NE(nic.name, "");
  CHECK_NE(nic.pci_address, "");
//  CHECK_NE(nic.netmask, "");

  CHECK_GT(nic.mtu, 0);
  CHECK_LE(nic.mtu, UINT16_MAX);

  CHECK_NE(kni.ip_address, "");
  CHECK_NE(kni.netmask, "");
  CHECK_NE(kni.gateway, "");
  CHECK_GT(kni.ring_size, 64);

  CHECK_GT(mem.hugepage, 0);
  CHECK_GT(mem.channel, 0);

  mem.packet_pool = align_ceil_pow2(mem.packet_pool);

  CHECK_GT(mem.packet_pool, 512 * slave_cores.size());
//  CHECK_EQ(mem.packet_pool % 2, 0);

  CHECK(!nic.local_ips.empty());
  std::sort(nic.local_ips.begin(), nic.local_ips.end());

  CHECK(!nic.local_ips.empty());
  CHECK_EQ(nic.local_ips.size() % slave_cores.size(), 0);
  CHECK(std::unique(nic.local_ips.begin(), nic.local_ips.end()) ==
        nic.local_ips.end());

  utils::be32_t _dummy;

  for (auto &ip : nic.local_ips)
    CHECK(headers::ParseIpv4Address(ip, &_dummy));

  CHECK(headers::ParseIpv4Address(kni.ip_address, &_dummy));
  CHECK(headers::ParseIpv4Address(kni.netmask, &_dummy));
  CHECK(headers::ParseIpv4Address(kni.gateway, &_dummy));
}

} // namespace xlb
