#include "config.h"

#define CONFIGURU_IMPLEMENTATION 1
#include "3rdparty/configuru.hpp"

#include <glog/logging.h>
#include <iostream>
#include <rte_pci.h>
#include <utils/numa.h>

#include "headers/ether.h"
#include "opts.h"

namespace xlb {

#define _FAILED LOG(FATAL) << "Failed to validate config: "

#define _NON_EMPTY(_FIELD)                                                     \
  if (_FIELD.empty()) {                                                        \
    _FAILED << "'" #_FIELD "'"                                                 \
               " must be non-empty.";                                          \
  }

Config Config::all_;

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
  _NON_EMPTY(worker_cores);
  _NON_EMPTY(grpc_url);

  _NON_EMPTY(nic.local_ips);
  _NON_EMPTY(nic.name);
  _NON_EMPTY(nic.pci_address);
  _NON_EMPTY(nic.mac_address);
  _NON_EMPTY(nic.gateway);
  _NON_EMPTY(nic.netmask);

  auto mac_addr = headers::Ethernet::Address(nic.mac_address);

  if (mac_addr.IsZero() || mac_addr.IsBroadcast())
    _FAILED << "'nic.mac_address' is invalid";

  if (worker_cores.size() > 32)
    _FAILED << "size of 'worker_cores' cannot exceed 32";

  for (auto& w : worker_cores)
    if (!utils::IsValidCore(w))
      _FAILED << "core(" << w << ") in 'worker_cores' is invalid";

  if (nic.local_ips.size() % worker_cores.size() != 0)
    _FAILED << "number of 'local_ip' must be a multiple of 'worker_cores'";

  struct rte_pci_addr pci = {0};
  if (eal_parse_pci_DomBDF(nic.pci_address.c_str(), &pci) != 0 &&
      eal_parse_pci_BDF(nic.pci_address.c_str(), &pci) != 0)
    _FAILED << "'nic.pci_address' is invalid";
}

} // namespace xlb
