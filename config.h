#pragma once

#include <string>
#include <vector>
#include <unordered_map>

#include "3rdparty/visit_struct.hpp"

#include "headers/ether.h"
#include "utils/singleton.h"
#include "utils/endian.h"

namespace xlb {

struct Config {
 public:
  struct Nic {
    std::string name;
    std::string pci_address;
    // Set when pmd is initialized
    headers::Ethernet::Address mac_address;
    std::vector<std::string> local_ips;
    uint16_t mtu;
    // Set when verifying configuration
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

  struct Svc {
    size_t max_virtual_service;
    size_t max_real_service;
    size_t max_real_per_virtual;
    size_t max_conn;
  };

  std::string rpc_ip_port;
  std::vector<uint16_t> slave_cores;
  uint8_t master_core;
  std::unordered_multimap<uint16_t, utils::be32_t> slave_local_ips;

  // TODO: support multi numa node
  Nic nic;
  Mem mem;
  Kni kni;
  Svc svc;

  static void Load();

 private:
  void validate();
};

#define CONFIG (utils::UnsafeSingleton<Config>::instance())

}  // namespace xlb

VISITABLE_STRUCT(xlb::Config::Nic, name, pci_address, local_ips, mtu);
VISITABLE_STRUCT(xlb::Config::Mem, hugepage, channel, packet_pool);
VISITABLE_STRUCT(xlb::Config::Kni, ip_address, netmask, gateway, ring_size);
VISITABLE_STRUCT(xlb::Config::Svc, max_virtual_service, max_real_service,
                 max_real_per_virtual, max_conn);
VISITABLE_STRUCT(xlb::Config, rpc_ip_port, slave_cores, master_core, nic, mem,
                 kni, svc);
