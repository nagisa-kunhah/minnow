#include "router.hh"

#include <iostream>
#include <limits>

using namespace std;

// route_prefix: The "up-to-32-bit" IPv4 address prefix to match the datagram's
// destination address against prefix_length: For this route to be applicable,
// how many high-order (most-significant) bits of
//    the route_prefix will need to match the corresponding bits of the
//    datagram's destination address?
// next_hop: The IP address of the next hop. Will be empty if the network is
// directly attached to the router (in
//    which case, the next hop address should be the datagram's final
//    destination).
// interface_num: The index of the interface to send the datagram out on.
void Router::add_route(const uint32_t route_prefix, const uint8_t prefix_length,
                       const optional<Address> next_hop,
                       const size_t interface_num) {
  cerr << "DEBUG: adding route "
       << Address::from_ipv4_numeric(route_prefix).ip() << "/"
       << static_cast<int>(prefix_length) << " => "
       << (next_hop.has_value() ? next_hop->ip() : "(direct)")
       << " on interface " << interface_num << "\n";
  routes_.push_back(
      make_tuple(route_prefix, prefix_length, next_hop, interface_num));
  // Your code here.
}

// Go through all the interfaces, and route every incoming datagram to its
// proper outgoing interface.
void Router::route() {
  // Your code here.
  for (auto& interface : _interfaces) {
    auto& datagrams = interface->datagrams_received();
    while (!datagrams.empty()) {
      routeOneDatagrams(datagrams.front());
      datagrams.pop();
    }
  }
}

void Router::routeOneDatagrams(InternetDatagram& datagram) {
  int mxPrefixLength = -1;
  size_t choicedIdx = routes_.size();
  for (size_t i = 0; i < routes_.size(); i++) {
    auto isMatch = [](uint32_t dstIp, uint32_t nowIp,
                      uint8_t prefixLength) -> bool {
      if (prefixLength == 0) {
        return true;
      }
      uint8_t needRight = 32 - prefixLength;
      return (dstIp >> (needRight) == nowIp >> (needRight));
    };
    auto [route_prefix, prefix_length, next_hop, interface_num] = routes_[i];
    if (isMatch(datagram.header.dst, route_prefix, prefix_length)) {
      if (mxPrefixLength < prefix_length) {
        mxPrefixLength = prefix_length;
        choicedIdx = i;
      }
    }
  }
  if (choicedIdx == routes_.size()) {
    // drop
    return;
  }
  auto [choicedRoutePrefix, choicedPrefixLen, choicedNextHop,
        choicedInterfaceNum] = routes_[choicedIdx];
  if (datagram.header.ttl <= 1) {
    return;
  }
  datagram.header.ttl--;
  if (choicedNextHop.has_value()) {
    interface(choicedInterfaceNum)
        ->send_datagram(datagram, choicedNextHop.value());
  } else {
    interface(choicedIdx)
        ->send_datagram(datagram,
                        Address::from_ipv4_numeric(datagram.header.dst));
  }
}
