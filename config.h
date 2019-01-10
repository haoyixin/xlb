#ifndef XLB_CONFIG_H
#define XLB_CONFIG_H

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

    int mtu;
    int socket;
    // TODO: offload & vlan
  };

  struct Mem {
    int hugepage;
    int channel;
    int packet_pool;
  };

  std::string rpc_ip_port;
  std::vector<int> worker_cores;

  // TODO: support multi numa node
  Nic nic;
  Mem mem;

  static void Load();

private:
  void validate();
};

#define CONFIG utils::Singleton<Config>::Get()

} // namespace xlb

VISITABLE_STRUCT(xlb::Config::Nic, name, pci_address, local_ips, netmask,
                 gateway, mtu);
VISITABLE_STRUCT(xlb::Config::Mem, hugepage, channel, packet_pool);
VISITABLE_STRUCT(xlb::Config, rpc_ip_port, worker_cores, nic, mem);

#endif // XLB_CONFIG_H
