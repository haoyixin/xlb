#pragma once

#include "common.h"

#include "headers/ip.h"

namespace xlb {

struct [[gnu::packed]] Tuple2 {
  be32_t ip;
  be16_t port;

  friend std::ostream &operator<<(std::ostream &os, const Tuple2 &tuple) {
    os << "ip: " << headers::ToIpv4Address(tuple.ip)
       << " port: " << tuple.port.value();
    return os;
  }
};

static_assert(sizeof(Tuple2) == 6);

struct [[gnu::packed]] Tuple4 {
  Tuple2 src;
  Tuple2 dst;

  friend std::ostream &operator<<(std::ostream &os, const Tuple4 &tuple) {
    os << "src: " << tuple.src << " dst: " << tuple.dst;
    return os;
  }
};

static_assert(sizeof(Tuple4) == 12);

}  // namespace xlb

namespace std {

template <>
struct hash<xlb::Tuple2> {
  size_t operator()(const xlb::Tuple2 &key) const {
    return hash_combine(hash<xlb::be32_t>{}(key.ip),
                        hash<xlb::be16_t>{}(key.port));
  }
};

template <>
struct equal_to<xlb::Tuple2> {
  bool operator()(const xlb::Tuple2 &lhs, const xlb::Tuple2 &rhs) const {
    return lhs.ip == rhs.ip && lhs.port == rhs.port;
  }
};

template <>
struct hash<xlb::Tuple4> {
  size_t operator()(const xlb::Tuple4 &key) const {
    return hash_combine(hash<xlb::Tuple2>{}(key.dst),
                        hash<xlb::Tuple2>{}(key.src));
  }
};

template <>
struct equal_to<xlb::Tuple4> {
  bool operator()(const xlb::Tuple4 &lhs, const xlb::Tuple4 &rhs) const {
    return equal_to<xlb::Tuple2>{}(lhs.src, rhs.src) &&
           equal_to<xlb::Tuple2>{}(lhs.dst, rhs.dst);
  }
};

}  // namespace std

namespace xlb {

inline bool operator==(const xlb::Tuple2 &lhs, const xlb::Tuple2 &rhs) {
  return std::equal_to<Tuple2>{}(lhs, rhs);
}

inline bool operator!=(const xlb::Tuple2 &lhs, const xlb::Tuple2 &rhs) {
  return !std::equal_to<Tuple2>{}(lhs, rhs);
}

inline bool operator==(const xlb::Tuple4 &lhs, const xlb::Tuple4 &rhs) {
  return std::equal_to<Tuple4>{}(lhs, rhs);
}

inline bool operator!=(const xlb::Tuple4 &lhs, const xlb::Tuple4 &rhs) {
  return !std::equal_to<Tuple4>{}(lhs, rhs);
}

}  // namespace xlb
