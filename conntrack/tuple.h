#pragma once

#include "conntrack/common.h"

namespace xlb::conntrack {

struct [[gnu::packed]] Tuple2 {
  Tuple2(const be32_t &_ip, const be16_t &_port) : ip(_ip), port(_port) {}

  Tuple2() = default;
  ~Tuple2() = default;

  be32_t ip;
  be16_t port;

  friend std::ostream &operator<<(std::ostream &os, const Tuple2 &tuple) {
    os << "[ip: " << headers::ToIpv4Address(tuple.ip)
       << " port: " << tuple.port.value() << "]";
    return os;
  }
};

static_assert(sizeof(Tuple2) == 6);

struct [[gnu::packed]] Tuple4 {
  Tuple4(const Tuple2 &_src, const Tuple2 &_dst) : src(_src), dst(_dst) {}

  Tuple4() = default;
  ~Tuple4() = default;

  Tuple2 src;
  Tuple2 dst;

  friend std::ostream &operator<<(std::ostream &os, const Tuple4 &tuple) {
    os << "[src: " << tuple.src << " dst: " << tuple.dst << "]";
    return os;
  }
};

static_assert(sizeof(Tuple4) == 12);

}  // namespace xlb::conntrack

namespace std {

template <>
struct hash<xlb::conntrack::Tuple2> {
  size_t operator()(const xlb::conntrack::Tuple2 &key) const {
    return hash_combine(hash<xlb::be32_t>{}(key.ip),
                        hash<xlb::be16_t>{}(key.port));
  }
};

template <>
struct equal_to<xlb::conntrack::Tuple2> {
  bool operator()(const xlb::conntrack::Tuple2 &lhs,
                  const xlb::conntrack::Tuple2 &rhs) const {
    return lhs.ip == rhs.ip && lhs.port == rhs.port;
  }
};

template <>
struct hash<xlb::conntrack::Tuple4> {
  size_t operator()(const xlb::conntrack::Tuple4 &key) const {
    return hash_combine(hash<xlb::conntrack::Tuple2>{}(key.dst),
                        hash<xlb::conntrack::Tuple2>{}(key.src));
  }
};

template <>
struct equal_to<xlb::conntrack::Tuple4> {
  bool operator()(const xlb::conntrack::Tuple4 &lhs,
                  const xlb::conntrack::Tuple4 &rhs) const {
    return equal_to<xlb::conntrack::Tuple2>{}(lhs.src, rhs.src) &&
           equal_to<xlb::conntrack::Tuple2>{}(lhs.dst, rhs.dst);
  }
};

}  // namespace std

namespace xlb {

inline bool operator==(const conntrack::Tuple2 &lhs,
                       const conntrack::Tuple2 &rhs) {
  return std::equal_to<conntrack::Tuple2>{}(lhs, rhs);
}

inline bool operator!=(const conntrack::Tuple2 &lhs,
                       const conntrack::Tuple2 &rhs) {
  return !std::equal_to<conntrack::Tuple2>{}(lhs, rhs);
}

inline bool operator==(const conntrack::Tuple4 &lhs,
                       const conntrack::Tuple4 &rhs) {
  return std::equal_to<conntrack::Tuple4>{}(lhs, rhs);
}

inline bool operator!=(const conntrack::Tuple4 &lhs,
                       const conntrack::Tuple4 &rhs) {
  return !std::equal_to<conntrack::Tuple4>{}(lhs, rhs);
}

}  // namespace xlb

