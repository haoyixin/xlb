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
    //    std::string netmask;
    //    std::string gateway;

    uint16_t mtu;
    int socket;
    // TODO: offload & vlan
  };

  struct Mem {
    size_t hugepage;
    size_t channel;
    size_t packet_pool;
  };

  struct Kni {
    std::string ip_address;
    std::string netmask;
    std::string gateway;
    size_t ring_size;
  };

  std::string rpc_ip_port;
  std::vector<uint16_t> slave_cores;
  uint8_t master_core;

  // TODO: support multi numa node
  Nic nic;
  Mem mem;
  Kni kni;

  static void Load();

 private:
  void validate();
};

#define CONFIG utils::Singleton<Config>::instance()

}  // namespace xlb

VISITABLE_STRUCT(xlb::Config::Nic, name, pci_address, local_ips, mtu);
VISITABLE_STRUCT(xlb::Config::Mem, hugepage, channel, packet_pool);
VISITABLE_STRUCT(xlb::Config::Kni, ip_address, netmask, gateway, ring_size);
VISITABLE_STRUCT(xlb::Config, rpc_ip_port, slave_cores, master_core, nic, mem,
                 kni);
