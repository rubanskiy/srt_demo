/*****************************************************************************
 * SRT - Secure, Reliable, Transport
 * Copyright (c) 2017 Haivision Systems Inc.
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; If not, see <http://www.gnu.org/licenses/>
 * 
 *****************************************************************************/

/*****************************************************************************
written by
   Haivision Systems Inc.
 *****************************************************************************/

#ifndef INC__NETINET_ANY_H
#define INC__NETINET_ANY_H

#include <cstring>
#include "platform_sys.h"

// This is a smart structure that this moron who has designed BSD sockets
// should have defined in the first place.

struct sockaddr_any
{
    union
    {
        sockaddr_in sin;
        sockaddr_in6 sin6;
        sockaddr sa;
    };
    socklen_t len;

    sockaddr_any(int domain = AF_INET)
    {
        memset(this, 0, sizeof *this);
        sa.sa_family = domain;
        len = size();
    }

    socklen_t size() const
    {
        switch (sa.sa_family)
        {
        case AF_INET: return static_cast<socklen_t>(sizeof sin);
        case AF_INET6: return static_cast<socklen_t>(sizeof sin6);

        default: return 0; // fallback, impossible
        }
    }

    int family() const { return sa.sa_family; }

    // port is in exactly the same location in both sin and sin6
    // and has the same size. This is actually yet another common
    // field, just not mentioned in the sockaddr structure.
    uint16_t& r_port() { return sin.sin_port; }
    uint16_t r_port() const { return sin.sin_port; }
    int hport() const { return ntohs(sin.sin_port); }

    void hport(int value)
    {
        // Port is fortunately located at the same position
        // in both sockaddr_in and sockaddr_in6 and has the
        // same size.
        sin.sin_port = htons(value);
    }

    sockaddr* operator&() { return &sa; }

    template <int> struct TypeMap;

    template <int af_domain>
    typename TypeMap<af_domain>::type& get();

    struct Equal
    {
        bool operator()(const sockaddr_any& c1, const sockaddr_any& c2)
        {
            return memcmp(&c1, &c2, sizeof(c1)) == 0;
        }
    };

    struct EqualAddress
    {
        bool operator()(const sockaddr_any& c1, const sockaddr_any& c2)
        {
            if ( c1.sa.sa_family == AF_INET )
            {
                return c1.sin.sin_addr.s_addr == c2.sin.sin_addr.s_addr;
            }

            if ( c1.sa.sa_family == AF_INET6 )
            {
                return memcmp(&c1.sin6.sin6_addr, &c2.sin6.sin6_addr, sizeof (in6_addr)) == 0;
            }

            return false;
        }

    };

    bool equal_address(const sockaddr_any& rhs) const
    {
        return EqualAddress()(*this, rhs);
    }

    struct Less
    {
        bool operator()(const sockaddr_any& c1, const sockaddr_any& c2)
        {
            return memcmp(&c1, &c2, sizeof(c1)) < 0;
        }
    };
    
};

template<> struct sockaddr_any::TypeMap<AF_INET> { typedef sockaddr_in type; };
template<> struct sockaddr_any::TypeMap<AF_INET6> { typedef sockaddr_in6 type; };

template <>
inline sockaddr_any::TypeMap<AF_INET>::type& sockaddr_any::get<AF_INET>() { return sin; }
template <>
inline sockaddr_any::TypeMap<AF_INET6>::type& sockaddr_any::get<AF_INET6>() { return sin6; }

#endif
