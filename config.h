#ifndef XLB_CONFIG_H
#define XLB_CONFIG_H

#include "3rdparty/visit_struct.hpp"

#include <string>
#include <vector>

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

  std::string grpc_url;
  std::vector<int> worker_cores;

  int hugepage;
  int packet_pool;

  // TODO: only support one numa node now
  Nic nic;

  static const Config &All() { return all_; }

  static void Load();

private:
  void validate();

  static void error_reporter(std::string str);
  static Config all_;
};

#define CONFIG Config::All()

} // namespace xlb

VISITABLE_STRUCT(xlb::Config::Nic, name, pci_address, local_ips, netmask,
                 gateway, mtu);
VISITABLE_STRUCT(xlb::Config, grpc_url, worker_cores, hugepage, packet_pool,
                 nic);

#endif // XLB_CONFIG_H
