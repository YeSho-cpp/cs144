#include "router.hh"
#include "address.hh"

#include <cstdint>
#include <iostream>
#include <optional>

using namespace std;

// Dummy implementation of an IP router

// Given an incoming Internet datagram, the router decides
// (1) which interface to send it out on, and
// (2) what next hop address to send it to.

// For Lab 6, please replace with a real implementation that passes the
// automated checks run by `make check_lab6`.

// You will need to add private members to the class declaration in `router.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

//! \param[in] route_prefix The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
//! \param[in] prefix_length For this route to be applicable, how many high-order (most-significant) bits of the route_prefix will need to match the corresponding bits of the datagram's destination address?
//! \param[in] next_hop The IP address of the next hop. Will be empty if the network is directly attached to the router (in which case, the next hop address should be the datagram's final destination).
//! \param[in] interface_num The index of the interface to send the datagram out on.
void Router::add_route(const uint32_t route_prefix,
                       const uint8_t prefix_length,
                       const optional<Address> next_hop,
                       const size_t interface_num) {
    cerr << "DEBUG: adding route " << Address::from_ipv4_numeric(route_prefix).ip() << "/" << int(prefix_length)
         << " => " << (next_hop.has_value() ? next_hop->ip() : "(direct)") << " on interface " << interface_num << "\n";

    // Your code here.

    // todo 将这个路由信息存起来
    Out_Stats stas;
    stas.interface_num=interface_num;
    stas.next_hop=next_hop;
    _routTable.insert({createKey(route_prefix, prefix_length),stas});
}

//! \param[in] dgram The datagram to be routed
void  Router::route_one_datagram(InternetDatagram &dgram) {
    // todo 将一个数据报路由到下一跳
    // 1.搜索路由表找到最合适的目的地址
    optional<Address> next_hop=std::nullopt;
    uint8_t max_len=0;
    size_t interface_num=0;
    for (auto it=_routTable.begin();it!=_routTable.end();++it) {
        auto [route_prefix,prefix_length]=unpack_key(it->first);
        auto subnetMask=to_subnetMask(prefix_length); //获得子网掩码
        if ((dgram.header().dst&subnetMask)==(route_prefix&subnetMask)&&prefix_length>=max_len) {
            next_hop=(it->second.next_hop.has_value())?it->second.next_hop:Address::from_ipv4_numeric(dgram.header().dst);
            max_len=prefix_length;
            interface_num=it->second.interface_num;
        }
    }
    // 2.找不到删除这个数据包
    if (!next_hop.has_value()||(dgram.header().ttl--<=1)) return;

    // 3. 否则就发送

    interface(interface_num).send_datagram(dgram,next_hop.value());
}

void Router::route() {
    // Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
    for (auto &interface : _interfaces) {
        auto &queue = interface.datagrams_out();
        while (not queue.empty()) {
            route_one_datagram(queue.front());
            queue.pop();
        }
    }
}

// 转换子网掩码
uint32_t Router::to_subnetMask(uint8_t prefix_length){
    return (prefix_length==0)?0:(~0U<<(32-prefix_length));
}

uint64_t Router::createKey(uint32_t route_prefix,uint8_t prefix_length){
    uint64_t key=route_prefix;
    key<<=32;
    key|=prefix_length;
    return key;
}

std::tuple<uint32_t, uint8_t> Router::unpack_key(uint64_t key) {
    uint32_t ipAddress = key >> 32;          // 从高32位提取IP地址
    uint8_t prefixLength = key & 0xFFFFFFFF;  // 从低32位提取前缀长度
    return std::make_tuple(ipAddress, prefixLength);
}

