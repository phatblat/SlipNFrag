// net_sock.c

#include "quakedef.h"
#include "net_sock.h"

//=============================================================================
/*
 ============
 SOCK_PartialIPAddress
 
 this lets you type only as much of the net address as required, using
 the local network components to fill in the rest
 ============
 */
int SOCK_PartialIPAddress (const char *input, in6_addr& local, struct qsockaddr *hostaddr)
{
    char buff[256];
    char *b;
    int addr;
    int num;
    int mask;
    int run;
    int port;
    
    for (auto i = 0; i < 10; i++)
    {
        if (local.s6_addr[i] != 0)
        {
            return -1;
        }
    }
    if (local.s6_addr[10] != 255)
    {
        return -1;
    }
    if (local.s6_addr[11] != 255)
    {
        return -1;
    }
    
    buff[0] = '.';
    b = buff;
    strcpy(buff+1, input);
    if (buff[1] == '.')
        b++;
    
    addr = 0;
    mask=-1;
    while (*b == '.')
    {
        b++;
        num = 0;
        run = 0;
        while (!( *b < '0' || *b > '9'))
        {
            num = num*10 + *b++ - '0';
            if (++run > 3)
                return -1;
        }
        if ((*b < '0' || *b > '9') && *b != '.' && *b != ':' && *b != 0)
            return -1;
        if (num < 0 || num > 255)
            return -1;
        mask<<=8;
        addr = (addr<<8) + num;
    }
    
    if (*b++ == ':')
        port = Q_atoi(b);
    else
        port = net_hostport;
    
    auto ipv4addr = (uint32_t)(local.s6_addr[12] << 24) | (uint32_t)((local.s6_addr[13] << 16) & 255) | (uint32_t)((local.s6_addr[14] << 8) & 255) | (uint32_t)(local.s6_addr[15] & 255);
    auto fulladdr = (ipv4addr & mask) | addr;
    
    hostaddr->data.clear();
    hostaddr->data.resize(sizeof(sockaddr_in6));
    auto address = (sockaddr_in6*)hostaddr->data.data();
    address->sin6_family = AF_INET6;
    address->sin6_port = htons((short)port);
    address->sin6_addr.s6_addr[10] = 255;
    address->sin6_addr.s6_addr[11] = 255;
    address->sin6_addr.s6_addr[12] = fulladdr >> 24;
    address->sin6_addr.s6_addr[13] = (fulladdr >> 16) & 255;
    address->sin6_addr.s6_addr[14] = (fulladdr >> 8) & 255;
    address->sin6_addr.s6_addr[15] = fulladdr & 255;
    
    return 0;
}

//=============================================================================

int SOCK_Connect (int socket, struct qsockaddr *addr)
{
    return 0;
}

//=============================================================================

int SOCK_GetSocketAddr (int socket, int controlsocket, in6_addr& local, struct qsockaddr *addr)
{
    socklen_t addrlen;
    if (socket == controlsocket)
    {
        addrlen = (socklen_t)sizeof(sockaddr_in);
    }
    else
    {
        addrlen = (socklen_t)sizeof(sockaddr_in6);
    }
    addr->data.resize(addrlen);
    auto err = getsockname(socket, (sockaddr*)addr->data.data(), &addrlen);
    if (err < 0)
    {
        return err;
    }
    if (socket == controlsocket)
    {
        auto address = (sockaddr_in*)addr->data.data();
        auto a = address->sin_addr.s_addr;
        if (a == 0 || a == inet_addr("127.0.0.1"))
        {
            auto ipv4addr = (uint32_t)(local.s6_addr[12] << 24) | (uint32_t)((local.s6_addr[13] << 16) & 255) | (uint32_t)((local.s6_addr[14] << 8) & 255) | (uint32_t)(local.s6_addr[15] & 255);
            address->sin_addr.s_addr = htonl(ipv4addr);
        }
    }
    else
    {
        auto address = (sockaddr_in6*)addr->data.data();
        auto a = address->sin6_addr;
        auto is_local = true;
        auto is_none = true;
        for (auto i = 0; i < 16; i++)
        {
            if (is_local && a.s6_addr[i] != in6addr_loopback.s6_addr[i])
            {
                is_local = false;
            }
            if (is_none && a.s6_addr[i] != 0)
            {
                is_none = false;
            }
            if (!is_local && !is_none)
            {
                break;
            }
        }
        if (is_none || is_local)
        {
            address->sin6_addr = local;
        }
    }
    return 0;
}

//=============================================================================

char *SOCK_AddrToString (struct qsockaddr *addr)
{
    static char buffer[64];
    
    if (addr->data.size() == sizeof(sockaddr_in6))
    {
        auto address = (sockaddr_in6*)addr->data.data();
        auto haddr = address->sin6_addr;
        
        sprintf(buffer, "[%x:%x:%x:%x:%x:%x:%x:%x]:%d", haddr.s6_addr[0], haddr.s6_addr[1], haddr.s6_addr[2], haddr.s6_addr[3], haddr.s6_addr[4], haddr.s6_addr[5], haddr.s6_addr[6], haddr.s6_addr[7], ntohs(address->sin6_port));
    }
    else
    {
        auto address = (sockaddr_in*)addr->data.data();
        auto haddr = ntohl(address->sin_addr.s_addr);
        sprintf(buffer, "%d.%d.%d.%d:%d", (haddr >> 24) & 0xff, (haddr >> 16) & 0xff, (haddr >> 8) & 0xff, haddr & 0xff, ntohs(address->sin_port));
    }
    return buffer;
}

//=============================================================================

int SOCK_StringToAddr (char *string, struct qsockaddr *addr)
{
    int ha1 = 0, ha2 = 0, ha3 = 0, ha4 = 0, hp = 0;
    
    sscanf(string, "%d.%d.%d.%d:%d", &ha1, &ha2, &ha3, &ha4, &hp);
    
    addr->data.clear();
    addr->data.resize(sizeof(sockaddr_in6));
    auto address = (sockaddr_in6*)addr->data.data();
    address->sin6_family = AF_INET6;
    address->sin6_addr.s6_addr[10] = 255;
    address->sin6_addr.s6_addr[11] = 255;
    address->sin6_addr.s6_addr[12] = ha1;
    address->sin6_addr.s6_addr[13] = ha2;
    address->sin6_addr.s6_addr[14] = ha3;
    address->sin6_addr.s6_addr[15] = ha4;
    address->sin6_port = htons((unsigned short)hp);
    return 0;
}

//=============================================================================

int SOCK_GetNameFromAddr (struct qsockaddr *addr, std::string& name)
{
    char hostname[NI_MAXHOST];
    auto err = getnameinfo((const sockaddr*)addr->data.data(), (socklen_t)addr->data.size(), hostname, NI_MAXHOST, nullptr, 0, 0);
    if (err == 0)
    {
        name = hostname;
        return 0;
    }
    name = SOCK_AddrToString (addr);
    return 0;
}

//=============================================================================

int SOCK_GetAddrFromName(const char *name, in6_addr& local, struct qsockaddr *addr)
{
    if (name[0] >= '0' && name[0] <= '9')
        return SOCK_PartialIPAddress ((char*)name, local, addr);
    
    addrinfo hints { };
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;
    
    addrinfo* results = nullptr;
    auto err = getaddrinfo(name, std::to_string(net_hostport).c_str(), &hints, &results);
    if (err < 0)
    {
        return -1;
    }
    
    addrinfo* result;
    for (result = results; result != nullptr; result = result->ai_next)
    {
        if (result->ai_family == AF_INET6)
        {
            addr->data.resize(sizeof(sockaddr_in6));
            memcpy(addr->data.data(), result->ai_addr, addr->data.size());
            break;
        }
    }
    
    if (result == nullptr)
    {
        for (result = results; result != nullptr; result = result->ai_next)
        {
            if (result->ai_family == AF_INET)
            {
                addr->data.clear();
                addr->data.resize(sizeof(sockaddr_in6));
                auto address = (sockaddr_in6*)addr->data.data();
                address->sin6_family = AF_INET6;
                auto ipv4address = (sockaddr_in*)result->ai_addr;
                auto ipv4addressnumber = htonl(ipv4address->sin_addr.s_addr);
                address->sin6_port = ipv4address->sin_port;
                address->sin6_addr.s6_addr[10] = 255;
                address->sin6_addr.s6_addr[11] = 255;
                address->sin6_addr.s6_addr[12] = ipv4addressnumber >> 24;
                address->sin6_addr.s6_addr[13] = (ipv4addressnumber >> 16) & 255;
                address->sin6_addr.s6_addr[14] = (ipv4addressnumber >> 8) & 255;
                address->sin6_addr.s6_addr[15] = ipv4addressnumber & 255;
                break;
            }
        }
    }
    
    freeaddrinfo(results);
    
    if (results == nullptr)
    {
        return -1;
    }
    
    return 0;
}

//=============================================================================

int SOCK_AddrCompare (struct qsockaddr *addr1, struct qsockaddr *addr2)
{
    auto size1 = addr1->data.size();
    auto size2 = addr2->data.size();
    
    if (size1 != size2)
        return -1;
    
    if (size1 == sizeof(sockaddr_in6))
    {
        auto address1 = (sockaddr_in6*)addr1->data.data();
        auto address2 = (sockaddr_in6*)addr2->data.data();
        
        if (address1->sin6_family != address2->sin6_family)
            return -1;
        
        for (auto i = 0; i < 16; i++)
        {
            if (address1->sin6_addr.s6_addr[i] != address2->sin6_addr.s6_addr[i])
            {
                return -1;
            }
        }
        
        if (address1->sin6_port != address2->sin6_port)
            return 1;
    }
    else
    {
        auto address1 = (sockaddr_in*)addr1->data.data();
        auto address2 = (sockaddr_in*)addr2->data.data();
        
        if (address1->sin_family != address2->sin_family)
            return -1;
        
        if (address1->sin_addr.s_addr != address2->sin_addr.s_addr)
            return -1;
        
        if (address1->sin_port != address2->sin_port)
            return 1;
    }
    
    return 0;
}

//=============================================================================

int SOCK_GetSocketPort (struct qsockaddr *addr)
{
    if (addr->data.size() == sizeof(sockaddr_in6))
    {
        return ntohs(((struct sockaddr_in6*)addr->data.data())->sin6_port);
    }
    else
    {
        return ntohs(((struct sockaddr_in*)addr->data.data())->sin_port);
    }
}

int SOCK_SetSocketPort (struct qsockaddr *addr, int port)
{
    if (addr->data.size() == sizeof(sockaddr_in6))
    {
        ((struct sockaddr_in6*)addr->data.data())->sin6_port = htons((unsigned short)port);
    }
    else
    {
        ((struct sockaddr_in*)addr->data.data())->sin_port = htons((unsigned short)port);
    }
    return 0;
}

//=============================================================================

int SOCK_MaxMessageSize(struct qsockaddr* sock)
{
    return 65507;
}

//=============================================================================

int SOCK_MaxUnreliableMessageSize(struct qsockaddr* sock)
{
    return 65507;
}
