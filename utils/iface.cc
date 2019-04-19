#include "utils/iface.h"

namespace xlb::utils {

namespace {

bool do_command(const std::string &ifname,
                const std::function<int(int, ifreq *)> &&func) {
  int sock_fd = socket(PF_INET, SOCK_DGRAM, 0);
  if (sock_fd < 0) return false;

  struct ifreq ifr = {};
  snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s", ifname.c_str());

  bool success = (func(sock_fd, &ifr) == 0);
  close(sock_fd);

  return success;
}

}  // namespace

bool SetHwAddr(const std::string &ifname, const uint8_t *addr) {
  if (ifname.empty() || ifname == "lo") return false;

  return do_command(ifname, [&addr](int fd, ifreq *ifr) -> int {
    ifr->ifr_hwaddr.sa_family = 1;
    memcpy(ifr->ifr_hwaddr.sa_data, addr, 6);
    return ioctl(fd, SIOCSIFHWADDR, ifr);
  });
}

/*

bool SetIpAddr(const std::string &ifname, const std::string &addr) {
  if (ifname.empty() || ifname == "lo") return false;

  DLOG(INFO) << "Set iface: " << ifname << " ip address to: " << addr;

  return do_command(ifname, [&addr](int fd, ifreq *ifr) -> int {
    struct sockaddr_in sin = {};
    sin.sin_family = AF_INET;
    sin.sin_port = 0;

    inet_pton(AF_INET, addr.c_str(), &(sin.sin_addr.s_addr));
    memcpy(&ifr->ifr_addr, &sin, sizeof(struct sockaddr));
    return ioctl(fd, SIOCSIFADDR, ifr);
  });
}

bool SetNetmask(const std::string &ifname, const std::string &netmask) {
  if (ifname.empty() || ifname == "lo") return false;

  return do_command(ifname, [&netmask](int fd, ifreq *ifr) -> int {
    struct sockaddr_in sin = {};
    sin.sin_family = AF_INET;

    inet_pton(AF_INET, netmask.c_str(), &(sin.sin_addr));
    memcpy(&ifr->ifr_netmask, &sin, sizeof(struct sockaddr));
    return ioctl(fd, SIOCSIFNETMASK, ifr);
  });
}

bool SetUp(const std::string &ifname) {
  if (ifname.empty() || ifname == "lo") return false;

  return do_command(ifname, [](int fd, ifreq *ifr) -> int {
    ifr->ifr_flags |= IFF_UP;
    return ioctl(fd, SIOCSIFFLAGS, ifr);
  });
}

 */

}  // namespace xlb::utils
