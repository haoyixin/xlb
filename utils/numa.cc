#include "utils/numa.h"

#include <experimental/filesystem>
#include <utility>
#include <fstream>
#include <regex>

#include <glog/logging.h>

#include "utils/boost.h"
#include "utils/format.h"
#include "utils/singleton.h"

namespace fs = std::experimental::filesystem;

namespace xlb {
namespace utils {

namespace {

class directory_iterator {
private:
  fs::directory_iterator i_;
  fs::directory_iterator e_;
  std::regex exp_;
  std::pair<std::string, fs::path> idx_;

  bool eval_(fs::directory_entry const &entry) {
    std::string filename(entry.path().filename().string());
    std::smatch what;
    if (!std::regex_search(filename, what, exp_)) {
      return false;
    }
    idx_ = std::make_pair(what[1], entry.path());
    return true;
  }

public:
  typedef std::input_iterator_tag iterator_category;
  typedef const std::pair<std::string, fs::path> value_type;
  typedef std::ptrdiff_t difference_type;
  typedef value_type *pointer;
  typedef value_type &reference;

  directory_iterator() : i_(), e_(), exp_(), idx_() {}

  directory_iterator(fs::path const &dir, std::string const &exp)
      : i_(dir), e_(), exp_(exp), idx_() {
    while (i_ != e_ && !eval_(*i_)) {
      ++i_;
    }
  }

  bool operator==(directory_iterator const &other) { return i_ == other.i_; }

  bool operator!=(directory_iterator const &other) { return i_ != other.i_; }

  directory_iterator &operator++() {
    do {
      ++i_;
    } while (i_ != e_ && !eval_(*i_));
    return *this;
  }

  directory_iterator operator++(int) {
    directory_iterator tmp(*this);
    ++*this;
    return tmp;
  }

  reference operator*() const { return idx_; }

  pointer operator->() const { return &idx_; }
};

std::set<uint32_t> ids_from_line(const std::string &content) {
  std::set<uint32_t> ids;
  std::vector<std::string> v1;
  split(v1, content, is_any_of(","));
  for (std::string entry : v1) {
    trim(entry);
    std::vector<std::string> v2;
    split(v2, entry, is_any_of("-"));
    CHECK(!v2.empty());
    if (1 == v2.size()) {
      // only one ID
      ids.insert(std::stoul(v2[0]));
    } else {
      // range of IDs
      uint32_t first = std::stoul(*v2.begin());
      uint32_t last = std::stoul(*v2.rbegin());
      for (uint32_t i = first; i <= last; ++i) {
        ids.insert(i);
      }
    }
  }
  return ids;
}

std::vector<uint32_t> distance_from_line(const std::string &content) {
  std::vector<uint32_t> distance;
  std::vector<std::string> v1;
  split(v1, content, is_any_of(" "));
  for (std::string entry : v1) {
    trim(entry);
    CHECK(!entry.empty());
    distance.push_back(std::stoul(entry));
  }
  return distance;
}

std::set<std::string> pcis_from_node(uint32_t node) {
  std::set<std::string> pcis;
  directory_iterator end;
  fs::path pci_path{Format("/sys/bus/pci/devices/")};

  for (directory_iterator iter{pci_path, "^(.+)$"}; iter != end; ++iter) {
    std::string content;
    std::getline(std::ifstream{iter->second / "numa_node"}, content);

    int pci_node = std::stoi(content);
    if ((pci_node < 0 ? 0 : pci_node) == node)
      pcis.insert(iter->first);
  }

  return pcis;
}

std::vector<Node> create_topology() {
  std::vector<Node> topo;
  // 1. parse list of CPUs which are online
  std::ifstream fs_online{fs::path("/sys/devices/system/cpu/online")};
  std::string content;
  std::getline(fs_online, content);
  std::set<uint32_t> cpus = ids_from_line(content);

  // parsing cpus failed
  CHECK(!cpus.empty());

  std::map<uint32_t, Node> map;
  // iterate list of logical cpus
  for (uint32_t cpu_id : cpus) {
    fs::path cpu_path{Format("/sys/devices/system/cpu/cpu%d/", cpu_id)};
    CHECK(fs::exists(cpu_path));
    // 2. to witch NUMA node the CPU belongs to
    directory_iterator e;
    for (directory_iterator i{cpu_path, "^node([0-9]+)$"}; i != e; ++i) {
      uint32_t node_id = std::stoul(i->first);
      map[node_id].id = node_id;
      map[node_id].logical_cpus.insert(cpu_id);
      // assigned to only one NUMA node
      break;
    }
  }

  if (map.empty()) {
    // maybe /sys/devices/system/cpu/cpu[0-9]/node[0-9] was not defined
    // put all CPUs to NUMA node 0
    map[0].id = 0;
    for (uint32_t cpu_id : cpus) {
      map[0].logical_cpus.insert(cpu_id);
    }
  }

  for (auto entry : map) {
    entry.second.pci_devices = pcis_from_node(entry.second.id);

    // NUMA-node distance
    fs::path distance_path{
        Format("/sys/devices/system/node/node%d/distance", entry.second.id)};
    if (fs::exists(distance_path)) {
      std::ifstream fs_distance{distance_path};
      std::string content;
      std::getline(fs_distance, content);
      entry.second.distance = distance_from_line(content);
      topo.push_back(entry.second);
    } else {
      // fake NUMA distance
      entry.second.distance.push_back(10);
      topo.push_back(entry.second);
    }
  }

  return topo;
}

class topology : public TopologyBase {
public:
  topology() : TopologyBase(create_topology()) {}

  Node *find(std::function<bool(Node &)> func) {
    auto iter = find_if(static_cast<TopologyBase &>(*this), func);
    return iter != this->end() ? &*iter : nullptr;
  }
};

topology &topology_singleton() { return Singleton<topology>::Get(); }

#define _RETURN_ID_OPT_BY_FIELD_HAS_VALUE(_FIELD, _VALUE)                      \
  do {                                                                         \
    auto node = topology_singleton().find(                                     \
        [&](auto &node) { return any_of_equal(node._FIELD, _VALUE); });        \
    return node != nullptr ? std::make_optional(node->id) : std::nullopt;      \
  } while (0)

} // namespace

TopologyBase &Topology() {
  return static_cast<TopologyBase &>(topology_singleton());
}

std::optional<uint32_t> CoreSocketId(uint32_t core_id) {
  _RETURN_ID_OPT_BY_FIELD_HAS_VALUE(logical_cpus, core_id);
}

std::optional<uint32_t> PciSocketId(const std::string &pci_addr) {
  _RETURN_ID_OPT_BY_FIELD_HAS_VALUE(pci_devices, pci_addr);
}

} // namespace utils
} // namespace xlb
