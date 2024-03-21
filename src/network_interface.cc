#include "network_interface.hh"

#include <iostream>

#include "arp_message.hh"
#include "exception.hh"

using namespace std;

ARPMessage make_arp(const uint16_t opcode,
                    const EthernetAddress sender_ethernet_address,
                    const uint32_t& sender_ip_address,
                    const EthernetAddress target_ethernet_address,
                    const uint32_t& target_ip_address) {
  ARPMessage arp;
  arp.opcode = opcode;
  arp.sender_ethernet_address = sender_ethernet_address;
  arp.sender_ip_address = sender_ip_address;
  arp.target_ethernet_address = target_ethernet_address;
  arp.target_ip_address = target_ip_address;
  return arp;
}

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of
//! the interface \param[in] ip_address IP (what ARP calls "protocol") address
//! of the interface
NetworkInterface::NetworkInterface(string_view name,
                                   shared_ptr<OutputPort> port,
                                   const EthernetAddress& ethernet_address,
                                   const Address& ip_address)
    : name_(name),
      port_(notnull("OutputPort", move(port))),
      ethernet_address_(ethernet_address),
      ip_address_(ip_address) {
  cerr << "DEBUG: Network interface has Ethernet address "
       << to_string(ethernet_address) << " and IP address " << ip_address.ip()
       << "\n";
}
//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically
//! a router or default gateway, but may also be another host if directly
//! connected to the same network as the destination) Note: the Address type can
//! be converted to a uint32_t (raw 32-bit IP address) by using the
//! Address::ipv4_numeric() method.
void NetworkInterface::send_datagram(const InternetDatagram& dgram,
                                     const Address& next_hop) {
  // Your code here.
  (void)dgram;
  (void)next_hop;
  auto it = ARPCacheMap_.find(next_hop.ipv4_numeric());
  if (it == ARPCacheMap_.end()) {
    if (!lastARPTime_.count(next_hop.ipv4_numeric()) ||
        timestamp_ - lastARPTime_[next_hop.ipv4_numeric()] >= ARPRetryMS) {
      EthernetFrame broadcastMsg;
      broadcastMsg.header.dst = ETHERNET_BROADCAST;
      broadcastMsg.header.src = ethernet_address_;
      broadcastMsg.header.type = EthernetHeader::TYPE_ARP;
      broadcastMsg.payload = serialize(
          make_arp(ARPMessage::OPCODE_REQUEST, ethernet_address_,
                   ip_address_.ipv4_numeric(), {}, next_hop.ipv4_numeric()));
      lastARPTime_[next_hop.ipv4_numeric()] = timestamp_;
      transmit(broadcastMsg);
      blockedData_[next_hop.ipv4_numeric()].push_back(dgram);
    }
    return;
  }
  EthernetFrame msg;
  tuple<uint32_t, EthernetAddress, size_t> node = *(it->second);
  msg.header.dst = std::get<1>(node);
  msg.header.src = ethernet_address_;
  msg.header.type = EthernetHeader::TYPE_IPv4;
  msg.payload = serialize(dgram);
  transmit(msg);
}

//! \param[in] frame the incoming Ethernet frame
void NetworkInterface::recv_frame(const EthernetFrame& frame) {
  // Your code here.
  (void)frame;
  if (frame.header.dst != ETHERNET_BROADCAST &&
      frame.header.dst != ethernet_address_) {
    return;
  }
  if (frame.header.type == EthernetHeader::TYPE_IPv4) {
    InternetDatagram msg;
    bool res = parse(msg, frame.payload);
    if (res) {
      datagrams_received_.push(msg);
    }
  }
  if (frame.header.type == EthernetHeader::TYPE_ARP) {
    ARPMessage arpmessage;
    bool res = parse(arpmessage, frame.payload);
    if (res) {
      if (ARPCacheMap_.count(arpmessage.sender_ip_address)) {
        removeCache(arpmessage.sender_ip_address);
      }
      ARPCache_.push_back(make_tuple(arpmessage.sender_ip_address,
                                     arpmessage.sender_ethernet_address,
                                     timestamp_));
      ARPCacheMap_[arpmessage.sender_ip_address] = --ARPCache_.end();
      processAQueue(arpmessage.sender_ip_address);
      if (frame.header.dst == ETHERNET_BROADCAST &&
          arpmessage.target_ip_address == ip_address_.ipv4_numeric()) {
        // ask for our ip address
        EthernetFrame reply;
        reply.header.dst = arpmessage.sender_ethernet_address;
        reply.header.src = ethernet_address_;
        reply.header.type = EthernetHeader::TYPE_ARP;
        reply.payload = serialize(make_arp(
            ARPMessage::OPCODE_REPLY, ethernet_address_,
            ip_address_.ipv4_numeric(), arpmessage.sender_ethernet_address,
            arpmessage.sender_ip_address));
        transmit(reply);
      }
    }
  }
}

void NetworkInterface::processAQueue(uint32_t arriveIP) {
  if (!blockedData_.count(arriveIP)) {
    return;
  }
  for (InternetDatagram dgram : blockedData_[arriveIP]) {
    assert(ARPCacheMap_.count(arriveIP));
    auto it = ARPCacheMap_.find(arriveIP);
    assert(it != ARPCacheMap_.end());
    EthernetFrame msg;
    tuple<uint32_t, EthernetAddress, size_t> node = *(it->second);
    msg.header.dst = std::get<1>(node);
    msg.header.src = ethernet_address_;
    msg.header.type = EthernetHeader::TYPE_IPv4;
    msg.payload = serialize(dgram);
    transmit(msg);
  }
  blockedData_.erase(arriveIP);
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call
//! to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) {
  // Your code here.
  (void)ms_since_last_tick;
  timestamp_ += ms_since_last_tick;
  while (needRemoveARP()) {
    tuple<uint32_t, EthernetAddress, size_t> node = ARPCache_.front();
    uint32_t ipv4Number = std::get<0>(node);
    removeCache(ipv4Number);
  }
}

void NetworkInterface::removeCache(uint32_t ipAddress) {
  auto removeIt = ARPCacheMap_[ipAddress];
  ARPCache_.erase(removeIt);
  ARPCacheMap_.erase(ipAddress);
}

bool NetworkInterface::needRemoveARP() const {
  if (ARPCache_.empty()) return false;
  tuple<uint32_t, EthernetAddress, size_t> node = ARPCache_.front();
  size_t lastTimestmp = std::get<2>(node);
  return timestamp_ - lastTimestmp >= ARPRemenberMs;
}
