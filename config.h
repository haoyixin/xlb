#pragma once

#include <string>
#include <vector>

#include "3rdparty/visit_struct.hpp"

#include "utils/singleton.h"

namespace xlb {

struct Config {
public:
  struct Nic {
    std::string name;
    std::string pci_address;
    // TODO: mac_address

    std::vector<std::string> local_ips;
    std::string netmask;
    std::string gateway;

    uint16_t mtu;
    int socket;
    // TODO: offload & vlan
  };

  struct Mem {
    size_t hugepage;
    size_t channel;
    size_t packet_pool;
  };

  std::string rpc_ip_port;
  std::vector<uint16_t> worker_cores;

  // TODO: support multi numa node
  Nic nic;
  Mem mem;

  static void Load();

private:
  void validate();
};

#define CONFIG utils::Singleton<Config>::instance()

} // namespace xlb

VISITABLE_STRUCT(xlb::Config::Nic, name, pci_address, local_ips, netmask,
                 gateway, mtu);
VISITABLE_STRUCT(xlb::Config::Mem, hugepage, channel, packet_pool);
VISITABLE_STRUCT(xlb::Config, rpc_ip_port, worker_cores, nic, mem);
