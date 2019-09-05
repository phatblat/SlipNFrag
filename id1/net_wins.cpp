/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// net_wins.c

#include "quakedef.h"
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "Ws2_32.lib")

extern cvar_t hostname;

#define MAXHOSTNAMELEN		256

static int net_acceptsocket = -1;		// socket for fielding new connections
static int net_controlsocket;
static int net_broadcastsocket = 0;
static struct qsockaddr broadcastaddr;

static in6_addr myAddr;
static bool myAddr_initialized = false;

qboolean	winsock_lib_initialized;

#include "net_wins.h"

int winsock_initialized = 0;
WSADATA		winsockdata;

int WINS_OpenIPV4Socket(int port);

//=============================================================================

void WINS_GetLocalAddress()
{
	struct hostent	*local = NULL;
	char			buff[MAXHOSTNAMELEN];
	unsigned long	addr;

	if (myAddr_initialized)
	{
		return;
	}

	if (gethostname(buff, MAXHOSTNAMELEN) == SOCKET_ERROR)
		return;

	addrinfo hints{ };
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_protocol = IPPROTO_UDP;

	addrinfo* results = nullptr;
	auto err = getaddrinfo(buff, "0", &hints, &results);
	if (err < 0)
	{
		return;
	}

	addrinfo* result;
	for (result = results; result != nullptr; result = result->ai_next)
	{
		if (result->ai_family == AF_INET6)
		{
			auto is_local = true;
			for (auto i = 0; i < 16; i++)
			{
				if (((sockaddr_in6*)result->ai_addr)->sin6_addr.u.Byte[i] != in6addr_loopback.u.Byte[i])
				{
					is_local = false;
					break;
				}
			}
			if (!is_local)
			{
				myAddr = ((sockaddr_in6*)result->ai_addr)->sin6_addr;
				myAddr_initialized = true;
				inet_ntop(AF_INET6, &myAddr, my_tcpip_address, NET_NAMELEN);
				break;
			}
		}
	}

	if (result == nullptr)
	{
		for (result = results; result != nullptr; result = result->ai_next)
		{
			if (result->ai_family == AF_INET)
			{
				auto addr = ntohl(((sockaddr_in*)result->ai_addr)->sin_addr.s_addr);
				myAddr = in6addr_v4mappedprefix;
				myAddr.u.Byte[12] = addr >> 24;
				myAddr.u.Byte[13] = (addr >> 16) & 255;
				myAddr.u.Byte[14] = (addr >> 8) & 255;
				myAddr.u.Byte[15] = addr & 255;
				myAddr_initialized = true;
				sprintf(my_tcpip_address, "%d.%d.%d.%d", (addr >> 24) & 0xff, (addr >> 16) & 0xff, (addr >> 8) & 0xff, addr & 0xff);
				break;
			}
		}
	}

	freeaddrinfo(results);
}


int WINS_Init (void)
{
	int		i;
	char	buff[MAXHOSTNAMELEN + 1];
	char	*p;
	int		r;
	WORD	wVersionRequested;

	winsock_lib_initialized = true;

	if (COM_CheckParm ("-noudp"))
		return -1;

	if (winsock_initialized == 0)
	{
		wVersionRequested = MAKEWORD(2, 2); 

		r = WSAStartup (wVersionRequested, &winsockdata);

		if (r)
		{
			Con_SafePrintf ("Winsock initialization failed.\n");
			return -1;
		}
	}
	winsock_initialized++;

	// determine my name
	if (gethostname(buff, MAXHOSTNAMELEN) == SOCKET_ERROR)
	{
		Con_DPrintf ("Winsock TCP/IP Initialization failed.\n");
		if (--winsock_initialized == 0)
			WSACleanup ();
		return -1;
	}

	// if the quake hostname isn't set, set it to the machine name
	if (Q_strcmp(hostname.string.c_str(), "UNNAMED") == 0)
	{
		// see if it's a text IP address (well, close enough)
		for (p = buff; *p; p++)
			if ((*p < '0' || *p > '9') && *p != '.')
				break;

		// if it is a real name, strip off the domain; we only want the host
		if (*p)
		{
			for (i = 0; i < 15; i++)
				if (buff[i] == '.')
					break;
			buff[i] = 0;
		}
		Cvar_Set ("hostname", buff);
	}

	i = COM_CheckParm ("-ip");
	if (i)
	{
		if (i < com_argc-1)
		{
			auto err = inet_pton(AF_INET6, com_argv[i+1], &myAddr);
			if (err < 0)
			{
				unsigned long addr = 0;
				err = inet_pton(AF_INET, com_argv[i+1], &addr);
				if (err < 0)
				{
					Sys_Error("%s is not a valid IP address", com_argv[i + 1]);
				}
				myAddr = in6addr_v4mappedprefix;
				myAddr.u.Byte[12] = addr >> 24;
				myAddr.u.Byte[13] = (addr >> 16) & 255;
				myAddr.u.Byte[14] = (addr >> 8) & 255;
				myAddr.u.Byte[15] = addr & 255;
				myAddr_initialized = true;
			}
			else
			{
				myAddr_initialized = true;
			}
			strcpy(my_tcpip_address, com_argv[i+1]);
		}
		else
		{
			Sys_Error ("NET_Init: you must specify an IP address after -ip");
		}
	}
	else
	{
		myAddr = in6addr_any;
		strcpy(my_tcpip_address, "in6addr_any");
	}

	if ((net_controlsocket = WINS_OpenIPV4Socket (0)) == -1)
	{
		Con_Printf("WINS_Init: Unable to open control socket\n");
		if (--winsock_initialized == 0)
			WSACleanup ();
		return -1;
	}

	broadcastaddr.data.resize(sizeof(sockaddr_in));
	auto address = (sockaddr_in*)broadcastaddr.data.data();
	address->sin_family = AF_INET;
	address->sin_addr.s_addr = INADDR_BROADCAST;
	address->sin_port = htons((unsigned short)net_hostport);

	Con_Printf("Winsock TCP/IP Initialized\n");
	tcpipAvailable = true;

	return net_controlsocket;
}

//=============================================================================

void WINS_Shutdown (void)
{
	WINS_Listen (false);
	WINS_CloseSocket (net_controlsocket);
	if (--winsock_initialized == 0)
		WSACleanup ();
}

//=============================================================================

void WINS_Listen (qboolean state)
{
	// enable listening
	if (state)
	{
		if (net_acceptsocket != -1)
			return;
		WINS_GetLocalAddress();
		if ((net_acceptsocket = WINS_OpenSocket (net_hostport)) == -1)
			Sys_Error ("WINS_Listen: Unable to open accept socket\n");
		return;
	}

	// disable listening
	if (net_acceptsocket == -1)
		return;
	WINS_CloseSocket (net_acceptsocket);
	net_acceptsocket = -1;
}

//=============================================================================

int WINS_OpenSocket (int port)
{
	int newsocket;
	qsockaddr addr;
	u_long _true = 1;
	int off = 0;

	if ((newsocket = socket (PF_INET6, SOCK_DGRAM, IPPROTO_UDP)) == -1)
		return -1;

	if (setsockopt(newsocket, IPPROTO_IPV6, IPV6_V6ONLY, (char*)&off, sizeof(off)) == -1)
		goto ErrorReturn;

	if (ioctlsocket (newsocket, FIONBIO, &_true) == -1)
		goto ErrorReturn;

	addr.data.resize(sizeof(sockaddr_in6));
	auto address = (sockaddr_in6*)addr.data.data();
	address->sin6_family = AF_INET6;
	address->sin6_addr = in6addr_any;
	address->sin6_port = htons((unsigned short)port);
	if( bind (newsocket, (const sockaddr*)address, (int)addr.data.size()) == 0)
		return newsocket;

	Sys_Error ("Unable to bind to %s", WINS_AddrToString(&addr));
ErrorReturn:
	closesocket (newsocket);
	return -1;
}

//=============================================================================

int WINS_OpenIPV4Socket(int port)
{
	int newsocket;
	qsockaddr addr;
	u_long _true = 1;

	if ((newsocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
		return -1;

	if (ioctlsocket(newsocket, FIONBIO, &_true) == -1)
		goto ErrorReturn;

	addr.data.resize(sizeof(sockaddr_in));
	auto address = (sockaddr_in*)addr.data.data();
	address->sin_family = AF_INET;
	address->sin_addr.s_addr = INADDR_ANY;
	address->sin_port = htons((unsigned short)port);
	if (bind(newsocket, (const sockaddr*)address, (int)addr.data.size()) == 0)
		return newsocket;

	Sys_Error("Unable to bind to %s", WINS_AddrToString(&addr));
ErrorReturn:
	closesocket(newsocket);
	return -1;
}

//=============================================================================

int WINS_CloseSocket (int socket)
{
	if (socket == net_broadcastsocket)
		net_broadcastsocket = 0;
	return closesocket (socket);
}


//=============================================================================
/*
============
PartialIPAddress

this lets you type only as much of the net address as required, using
the local network components to fill in the rest
============
*/
static int PartialIPAddress (char *in, struct qsockaddr *hostaddr)
{
	char buff[256];
	char *b;
	int addr;
	int num;
	int mask;
	int run;
	int port;
	
	for (auto i = 0; i < 12; i++)
	{
		if (myAddr.u.Byte[i] != in6addr_v4mappedprefix.u.Byte[i])
		{
			return -1;
		}
	}

	buff[0] = '.';
	b = buff;
	strcpy(buff+1, in);
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

	auto ipv4addr = (unsigned long)(myAddr.u.Byte[12] << 24) | (unsigned long)((myAddr.u.Byte[13] << 16) & 255) | (unsigned long)((myAddr.u.Byte[14] << 8) & 255) | (unsigned long)(myAddr.u.Byte[15] & 255);
	auto fulladdr = (ipv4addr & htonl(mask)) | htonl(addr);

	hostaddr->data.clear();
	hostaddr->data.resize(sizeof(sockaddr_in6));
	auto address = (sockaddr_in6*)hostaddr->data.data();
	address->sin6_family = AF_INET6;
	address->sin6_port = htons((short)port);
	address->sin6_addr = in6addr_v4mappedprefix;
	address->sin6_addr.u.Byte[12] = fulladdr >> 24;
	address->sin6_addr.u.Byte[13] = (fulladdr >> 16) & 255;
	address->sin6_addr.u.Byte[14] = (fulladdr >> 8) & 255;
	address->sin6_addr.u.Byte[15] = fulladdr & 255;

	return 0;
}
//=============================================================================

int WINS_Connect (int socket, struct qsockaddr *addr)
{
	return 0;
}

//=============================================================================

int WINS_CheckNewConnections (void)
{
	char buf[4096];

	if (net_acceptsocket == -1)
		return -1;

	if (recvfrom (net_acceptsocket, buf, sizeof(buf), MSG_PEEK, NULL, NULL) > 0)
	{
		return net_acceptsocket;
	}
	return -1;
}

//=============================================================================

int WINS_Read (int socket, std::vector<byte>& buf, struct qsockaddr *addr)
{
	u_long available = 0;
	auto err = ioctlsocket(socket, FIONREAD, &available);
	if (err < 0)
	{
		return err;
	}
	if (available == 0)
	{
		return 0;
	}
	auto len = (int)available;
	buf.resize(len);
	int addrlen;
	if (socket == net_controlsocket)
	{
		addrlen = (int)sizeof(sockaddr_in);
	}
	else
	{
		addrlen = (int)sizeof(sockaddr_in6);
	}
	addr->data.resize(addrlen);
	recvfrom(socket, (char*)buf.data(), len, 0, (sockaddr*)addr->data.data(), &addrlen);
	return len;
}

//=============================================================================

int WINS_MakeSocketBroadcastCapable (int socket)
{
	int	i = 1;

	// make this socket broadcast capable
	if (setsockopt(socket, SOL_SOCKET, SO_BROADCAST, (char *)&i, sizeof(i)) < 0)
		return -1;
	net_broadcastsocket = socket;

	return 0;
}

//=============================================================================

int WINS_Broadcast (int socket, byte *buf, int len)
{
	int ret;

	if (socket != net_broadcastsocket)
	{
		if (net_broadcastsocket != 0)
			Sys_Error("Attempted to use multiple broadcasts sockets\n");
		WINS_GetLocalAddress();
		ret = WINS_MakeSocketBroadcastCapable (socket);
		if (ret == -1)
		{
			Con_Printf("Unable to make socket broadcast capable\n");
			return ret;
		}
	}

	return WINS_Write (socket, buf, len, &broadcastaddr);
}

//=============================================================================

int WINS_Write (int socket, byte *buf, int len, struct qsockaddr *addr)
{
	int ret;
	ret = sendto (socket, (char*)buf, len, 0, (sockaddr*)addr->data.data(), (int)addr->data.size());
	if (ret == -1)
	{
		auto err = WSAGetLastError();
		if (err == WSAEWOULDBLOCK)
			return 0;
	}
	return ret;
}

//=============================================================================

char *WINS_AddrToString (struct qsockaddr *addr)
{
	static char buffer[64];

	if (addr->data.size() == sizeof(sockaddr_in6))
	{
		auto address = (sockaddr_in6*)addr->data.data();
		auto haddr = address->sin6_addr;

		sprintf(buffer, "[%x:%x:%x:%x:%x:%x:%x:%x]:%d", haddr.u.Word[0], haddr.u.Word[1], haddr.u.Word[2], haddr.u.Word[3], haddr.u.Word[4], haddr.u.Word[5], haddr.u.Word[6], haddr.u.Word[7], ntohs(address->sin6_port));
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

int WINS_StringToAddr (char *string, struct qsockaddr *addr)
{
	int ha1 = 0, ha2 = 0, ha3 = 0, ha4 = 0, hp = 0;

	sscanf(string, "%d.%d.%d.%d:%d", &ha1, &ha2, &ha3, &ha4, &hp);

	addr->data.clear();
	addr->data.resize(sizeof(sockaddr_in6));
	auto address = (sockaddr_in6*)addr->data.data();
	address->sin6_family = AF_INET6;
	address->sin6_addr = in6addr_v4mappedprefix;
	address->sin6_addr.u.Byte[12] = ha1;
	address->sin6_addr.u.Byte[13] = ha2;
	address->sin6_addr.u.Byte[14] = ha3;
	address->sin6_addr.u.Byte[15] = ha4;
	address->sin6_port = htons((unsigned short)hp);
	return 0;
}

//=============================================================================

int WINS_GetSocketAddr (int socket, struct qsockaddr *addr)
{
	int addrlen;
	if (socket == net_controlsocket)
	{
		addrlen = (int)sizeof(sockaddr_in);
	}
	else
	{
		addrlen = (int)sizeof(sockaddr_in6);
	}
	addr->data.resize(addrlen);
	auto err = getsockname(socket, (sockaddr*)addr->data.data(), &addrlen);
	if (err < 0)
	{
		return err;
	}
	if (socket == net_controlsocket)
	{
		auto address = (sockaddr_in*)addr->data.data();
		auto a = address->sin_addr.s_addr;
		if (a == 0 || a == inet_addr("127.0.0.1"))
		{
			auto ipv4addr = (unsigned long)(myAddr.u.Byte[12] << 24) | (unsigned long)((myAddr.u.Byte[13] << 16) & 255) | (unsigned long)((myAddr.u.Byte[14] << 8) & 255) | (unsigned long)(myAddr.u.Byte[15] & 255);
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
			if (is_local && a.u.Byte[i] != in6addr_loopback.u.Byte[i])
			{
				is_local = false;
			}
			if (is_none && a.u.Byte[i] != 0)
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
			address->sin6_addr = myAddr;
		}
	}
	return 0;
}

//=============================================================================

int WINS_GetNameFromAddr (struct qsockaddr *addr, std::string& name)
{
	struct hostent *hostentry;

	hostentry = gethostbyaddr ((char *)&((struct sockaddr_in *)addr->data.data())->sin_addr, sizeof(struct in_addr), AF_INET);
	if (hostentry)
	{
		name = hostentry->h_name;
		return 0;
	}

	name = WINS_AddrToString (addr);
	return 0;
}

//=============================================================================

int WINS_GetAddrFromName(const char *name, struct qsockaddr *addr)
{
	if (name[0] >= '0' && name[0] <= '9')
		return PartialIPAddress ((char*)name, addr);
	
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
				address->sin6_port = ipv4address->sin_port;
				address->sin6_addr = in6addr_v4mappedprefix;
				address->sin6_addr.u.Byte[12] = ipv4address->sin_addr.S_un.S_un_b.s_b1;
				address->sin6_addr.u.Byte[13] = ipv4address->sin_addr.S_un.S_un_b.s_b2;
				address->sin6_addr.u.Byte[14] = ipv4address->sin_addr.S_un.S_un_b.s_b3;
				address->sin6_addr.u.Byte[15] = ipv4address->sin_addr.S_un.S_un_b.s_b4;
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

int WINS_AddrCompare (struct qsockaddr *addr1, struct qsockaddr *addr2)
{
	auto address1 = (sockaddr_in*)addr1->data.data();
	auto address2 = (sockaddr_in*)addr2->data.data();
	
	if (address1->sin_family != address2->sin_family)
		return -1;

	if (address1->sin_addr.s_addr != address2->sin_addr.s_addr)
		return -1;

	if (address1->sin_port != address2->sin_port)
		return 1;

	return 0;
}

//=============================================================================

int WINS_GetSocketPort (struct qsockaddr *addr)
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


int WINS_SetSocketPort (struct qsockaddr *addr, int port)
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

int WINS_MaxMessageSize(struct qsockaddr* sock)
{
	return 65507;
}

//=============================================================================

int WINS_MaxUnreliableMessageSize(struct qsockaddr* sock)
{
	return 65507;
}
