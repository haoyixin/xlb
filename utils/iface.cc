#include "utils/iface.h"

#include <arpa/inet.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <string>

#include "headers/ip.h"

namespace xlb::utils {

bool SetHwAddr(const std::string &ifname, const headers::Ethernet::Address &addr) {
  if (ifname.empty() || ifname == "lo" || addr.IsBroadcast() ||
      addr.IsBroadcast())
    return false;

  int sock_fd = socket(PF_INET, SOCK_DGRAM, 0);
  if (sock_fd < 0)
    return false;

  struct ifreq ifr = {};
  snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s", ifname.c_str());
  ifr.ifr_hwaddr.sa_family = 1;
  memcpy(ifr.ifr_hwaddr.sa_data, addr.bytes, headers::Ethernet::Address::kSize);

  int ret = ioctl(sock_fd, SIOCSIFHWADDR, &ifr);
  close(sock_fd);

  return ret == 0;
}

bool SetIpAddr(const std::string &ifname, const headers::be32_t &addr) {
  if (ifname.empty() || ifname == "lo")
    return false;

  int sock_fd = socket(PF_INET, SOCK_DGRAM, 0);
  if (sock_fd < 0)
    return false;

  struct ifreq ifr = {};
  struct sockaddr_in sin = {};
  sin.sin_family = AF_INET;
  inet_pton(AF_INET, headers::ToIpv4Address(addr).c_str(), &(sin.sin_addr));
  memcpy(&ifr.ifr_addr, &sin, sizeof(struct sockaddr));

  int ret = ioctl(sock_fd, SIOCSIFADDR, &ifr);
  close(sock_fd);

  return ret == 0;
}

}  // namespace xlb::utils
