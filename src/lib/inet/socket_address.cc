// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/lib/inet/socket_address.h"

#include <endian.h>

#include <sstream>

namespace inet {

// static
const SocketAddress SocketAddress::kInvalid;

SocketAddress::SocketAddress() { std::memset(&v6_, 0, sizeof(v6_)); }

SocketAddress::SocketAddress(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3, IpPort port) {
  std::memset(&v4_, 0, sizeof(v4_));
  v4_.sin_family = AF_INET;
  v4_.sin_port = port.as_in_port_t();
  uint8_t* bytes = reinterpret_cast<uint8_t*>(&v4_.sin_addr);
  bytes[0] = b0;
  bytes[1] = b1;
  bytes[2] = b2;
  bytes[3] = b3;
}

SocketAddress::SocketAddress(in_addr_t addr, IpPort port) {
  std::memset(&v4_, 0, sizeof(v4_));
  v4_.sin_family = AF_INET;
  v4_.sin_port = port.as_in_port_t();
  v4_.sin_addr.s_addr = addr;
}

SocketAddress::SocketAddress(const sockaddr_in& addr) {
  FX_DCHECK(addr.sin_family == AF_INET);
  v4_ = addr;
}

SocketAddress::SocketAddress(uint16_t w0, uint16_t w1, uint16_t w2, uint16_t w3, uint16_t w4,
                             uint16_t w5, uint16_t w6, uint16_t w7, IpPort port,
                             uint32_t scope_id) {
  std::memset(&v6_, 0, sizeof(v6_));
  v6_.sin6_family = AF_INET6;
  v6_.sin6_port = port.as_in_port_t();
  v6_.sin6_scope_id = scope_id;
  uint16_t* words = v6_.sin6_addr.s6_addr16;
  words[0] = htobe16(w0);
  words[1] = htobe16(w1);
  words[2] = htobe16(w2);
  words[3] = htobe16(w3);
  words[4] = htobe16(w4);
  words[5] = htobe16(w5);
  words[6] = htobe16(w6);
  words[7] = htobe16(w7);
}

SocketAddress::SocketAddress(uint16_t w0, uint16_t w7, IpPort port, uint32_t scope_id) {
  std::memset(&v6_, 0, sizeof(v6_));
  v6_.sin6_family = AF_INET6;
  v6_.sin6_port = port.as_in_port_t();
  v6_.sin6_scope_id = scope_id;
  uint16_t* words = v6_.sin6_addr.s6_addr16;
  words[0] = htobe16(w0);
  words[7] = htobe16(w7);
}

SocketAddress::SocketAddress(const in6_addr& addr, IpPort port, uint32_t scope_id) {
  std::memset(&v6_, 0, sizeof(v6_));
  v6_.sin6_family = AF_INET6;
  v6_.sin6_port = port.as_in_port_t();
  v6_.sin6_scope_id = scope_id;
  v6_.sin6_addr = addr;
}

SocketAddress::SocketAddress(const sockaddr_in6& addr) {
  FX_DCHECK(addr.sin6_family == AF_INET6);
  v6_ = addr;
}

void SocketAddress::Build(const IpAddress& addr, IpPort port, uint32_t scope_id) {
  if (!addr.is_valid()) {
    std::memset(&v6_, 0, sizeof(v6_));
    return;
  }

  if (addr.is_v4()) {
    std::memset(&v4_, 0, sizeof(v4_));
    v4_.sin_family = AF_INET;
    v4_.sin_port = port.as_in_port_t();
    v4_.sin_addr = addr.as_in_addr();
  } else {
    std::memset(&v6_, 0, sizeof(v6_));
    v6_.sin6_family = AF_INET6;
    v6_.sin6_port = port.as_in_port_t();
    v6_.sin6_addr = addr.as_in6_addr();
    v6_.sin6_scope_id = scope_id;
  }
}

SocketAddress::SocketAddress(const IpAddress& addr, IpPort port, uint32_t scope_id) {
  Build(addr, port, scope_id);
}

SocketAddress::SocketAddress(const sockaddr_storage& addr) {
  FX_DCHECK(addr.ss_family == AF_INET || addr.ss_family == AF_INET6);
  if (addr.ss_family == AF_INET) {
    v4_ = *reinterpret_cast<const sockaddr_in*>(&addr);
  } else {
    v6_ = *reinterpret_cast<const sockaddr_in6*>(&addr);
  }
}

SocketAddress::SocketAddress(const fuchsia::net::SocketAddress& addr) : SocketAddress() {
  switch (addr.Which()) {
    case fuchsia::net::SocketAddress::kIpv4:
      Build(IpAddress(addr.ipv4().address), IpPort::From_uint16_t(addr.ipv4().port), 0);
      break;
    case fuchsia::net::SocketAddress::kIpv6:
      Build(IpAddress(addr.ipv6().address), IpPort::From_uint16_t(addr.ipv6().port),
            static_cast<uint32_t>(addr.ipv6().zone_index));
      break;
    case fuchsia::net::SocketAddress::Invalid:
      break;
  }
}

SocketAddress::SocketAddress(const fuchsia::net::Ipv4SocketAddress& addr)
    : SocketAddress(IpAddress(addr.address), IpPort::From_uint16_t(addr.port)) {}

SocketAddress::SocketAddress(const fuchsia::net::Ipv6SocketAddress& addr) {
  Build(IpAddress(addr.address), IpPort::From_uint16_t(addr.port),
        static_cast<uint32_t>(addr.zone_index));
}

std::string SocketAddress::ToString() const {
  std::ostringstream os;
  os << *this;
  return os.str();
}

std::ostream& operator<<(std::ostream& os, const SocketAddress& value) {
  if (!value.is_valid()) {
    return os << "<invalid>";
  }

  os << value.address() << ":" << value.port();

  if (value.is_v6() && value.scope_id() != 0) {
    os << "(" << value.scope_id() << ")";
  }

  return os;
}

SocketAddress::operator fuchsia::net::Ipv4SocketAddress() const {
  FX_DCHECK(is_v4());
  return fuchsia::net::Ipv4SocketAddress{
      .address = static_cast<fuchsia::net::Ipv4Address>(address()), .port = port().as_uint16_t()};
}

SocketAddress::operator fuchsia::net::Ipv6SocketAddress() const {
  FX_DCHECK(is_v6());
  return fuchsia::net::Ipv6SocketAddress{
      .address = static_cast<fuchsia::net::Ipv6Address>(address()),
      .port = port().as_uint16_t(),
      .zone_index = scope_id()};
}

SocketAddress::operator fuchsia::net::SocketAddress() const {
  if (is_v4()) {
    return fuchsia::net::SocketAddress::WithIpv4(
        static_cast<fuchsia::net::Ipv4SocketAddress>(*this));
  }

  return fuchsia::net::SocketAddress::WithIpv6(static_cast<fuchsia::net::Ipv6SocketAddress>(*this));
}

}  // namespace inet
