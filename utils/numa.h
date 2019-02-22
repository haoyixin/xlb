#pragma once

#include <set>
#include <string>
#include <vector>
#include <optional>

namespace xlb::utils {

struct Node {
  uint32_t id;
  std::set<uint32_t> logical_cpus;
  std::set<std::string> pci_devices;
  std::vector<uint32_t> distance;
};

using TopologyBase = std::vector<Node>;

inline bool operator<(Node const &lhs, Node const &rhs) noexcept {
  return lhs.id < rhs.id;
}

TopologyBase &Topology();

std::optional<uint32_t> CoreSocketId(uint32_t core_id);
std::optional<uint32_t> PciSocketId(const std::string &pci_addr);

}  // namespace xlb::utils
