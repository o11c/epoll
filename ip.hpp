// Copyright 2012 Ben Longbons
#ifndef IP_HPP
#define IP_HPP

#include <cstdint>
#include <netinet/in.h>

namespace net
{

struct IPv4
{
    uint8_t data[4];

    operator in_addr() const;
};

struct IPv6
{
    uint8_t data[16];

    operator in6_addr() const;
};

constexpr IPv4 ipv4_any = IPv4{{0,0,0,0}};
constexpr IPv4 ipv4_loopback = IPv4{{127,0,0,1}};
constexpr IPv6 ipv6_any = IPv6{{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}};
constexpr IPv6 ipv6_loopback = IPv6{{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1}};

inline
IPv4::operator in_addr() const
{
    in_addr out;
    memcpy(&out, data, 4);
    return out;
}

inline
IPv6::operator in6_addr() const
{
    in6_addr out;
    memcpy(&out, data, 16);
    return out;
}

}

#endif //IP_HPP
