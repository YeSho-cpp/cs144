#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"
#include "ethernet_header.hh"

#include <iostream>

// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check_lab5`.

// You will need to add private members to the class declaration in `network_interface.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address) {
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
         << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();
    if (_cache.find(next_hop_ip) != _cache.end()){ // 目的以太网地址已知
       EthernetFrame frame= warp_Ethernet(1,_ethernet_address,_cache[next_hop_ip].mac_addr,_ip_address.ipv4_numeric(),next_hop_ip,dgram,0);
        _frames_out.push(frame);// 发送
    }else{ // 以太网未知要发送ARP请求
        // 先查看是否已经发送过且时间未超过
        if (_arp_queue.find(next_hop_ip)!=_arp_queue.end()&&_arp_queue[next_hop_ip].ttl<=_FLOOD_TIME) return; // 就不需要再发送了
        EthernetFrame frame=warp_Ethernet(0,_ethernet_address,ETHERNET_BROADCAST,_ip_address.ipv4_numeric(),next_hop_ip,std::nullopt,0);
        _frames_out.push(frame);// 发送
        _arp_queue.insert({next_hop_ip, MAC_TTL{ETHERNET_BROADCAST, dgram, 0}});
    }


}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    // 检查以太网帧的目的地址
    if (frame.header().dst != _ethernet_address && frame.header().dst != ETHERNET_BROADCAST)
        return std::nullopt;  // 忽略不需要的帧
        // 先判断帧的类型
    if (frame.header().type == EthernetHeader::TYPE_IPv4) {
        InternetDatagram dgram;
        // IPV4 需要解析成InternetDatagram
        if (dgram.parse(frame.payload()) == ParseResult::NoError) {
            return dgram;  // 返回解析成功的结果
        }
    } else if (frame.header().type == EthernetHeader::TYPE_ARP) {
        ARPMessage message;
        // 解析成ARPMessage
        if (message.parse(frame.payload()) == ParseResult::NoError) {
            // 解析成功就记住这对映射30秒
            _cache.insert({message.sender_ip_address, MAC_TTL{message.sender_ethernet_address, std::nullopt, 0}});
            if (message.opcode == ARPMessage::OPCODE_REQUEST &&
                message.target_ip_address == _ip_address.ipv4_numeric())  // 是ARP请求且请求的是自己
            {
                EthernetFrame 
            sframe=warp_Ethernet(0,_ethernet_address,message.sender_ethernet_address,_ip_address.ipv4_numeric(),message.sender_ip_address,std::nullopt,1);
                _frames_out.push(sframe);
            } else if (message.opcode == ARPMessage::OPCODE_REPLY &&
                       message.target_ip_address == _ip_address.ipv4_numeric()&&_arp_queue.find(message.sender_ip_address)!=_arp_queue.end()) { // 是ARP回复，且发送ip地址是之前广播的ipi地址
                send_datagram(_arp_queue[message.sender_ip_address].dgram.value(),
                              Address::from_ipv4_numeric(message.sender_ip_address));
                _arp_queue.erase(message.sender_ip_address);              // 删除队列中的数据报
                
            }
        }
    }
    return std::nullopt;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) {
    // 将那些过期的映射去掉
    for (auto iter=_cache.begin();iter!=_cache.end();)
    {
       iter->second.ttl+=ms_since_last_tick;
       if (iter->second.ttl>_CACHE_TIME)
       {
            iter=_cache.erase(iter);
       }
       else ++iter;
       
    }
    // 检查ARP队列，如果超过5秒了再次发送这个ARP
    for (auto iter = _arp_queue.begin(); iter != _arp_queue.end(); ++iter) {
        iter->second.ttl += ms_since_last_tick;  // 更新时间
        if (iter->second.ttl > _FLOOD_TIME) {
            send_datagram(iter->second.dgram.value(), Address::from_ipv4_numeric(iter->first));
            iter->second.ttl=0; // 发送完后时间要清零
        }
    }
}


EthernetFrame NetworkInterface::warp_Ethernet(uint16_t type,EthernetAddress src,EthernetAddress dst,uint32_t s_ip,uint32_t d_ip,std::optional<InternetDatagram>dgram,uint16_t arp_type){
    EthernetFrame frame;
    EthernetHeader header;
    header.src =src;
    header.dst=dst;
    if (type==1)
    {
        header.type = EthernetHeader::TYPE_IPv4;
        frame.header() = header;
        frame.payload() = dgram.value().serialize();
        return frame;
    }
    else{
        ARPMessage message;
        header.type = EthernetHeader::TYPE_ARP;
        message.opcode=arp_type?ARPMessage::OPCODE_REPLY:ARPMessage::OPCODE_REQUEST;
        message.sender_ip_address=s_ip;
        message.target_ip_address=d_ip;
        message.sender_ethernet_address=src;
        message.target_ethernet_address=(dst==ETHERNET_BROADCAST)?BROADCAST_ARP:dst;
        frame.header() = header;
        frame.payload()=message.serialize();
        return frame;
    }
}