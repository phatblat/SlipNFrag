// net_sock.h

#ifdef _WIN32

#else

#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

#endif

int  SOCK_PartialIPAddress (const char *input, in6_addr& local, struct qsockaddr *hostaddr);
int  SOCK_Read (int socket, std::vector<byte>& buf, struct qsockaddr *addr);
int  SOCK_ReadIPv4 (int socket, std::vector<byte>& buf, struct qsockaddr *addr);
int  SOCK_Connect (int socket, struct qsockaddr *addr);
char *SOCK_AddrToString (struct qsockaddr *addr);
int  SOCK_StringToAddr (char *string, struct qsockaddr *addr);
int  SOCK_GetSocketAddr (int socket, int controlsocket, in6_addr& local, struct qsockaddr *addr);
int  SOCK_GetNameFromAddr (struct qsockaddr *addr, std::string& name);
int  SOCK_GetAddrFromName (const char *name, in6_addr& local, struct qsockaddr *addr);
int  SOCK_AddrCompare (struct qsockaddr *addr1, struct qsockaddr *addr2);
int  SOCK_GetSocketPort (struct qsockaddr *addr);
int  SOCK_SetSocketPort (struct qsockaddr *addr, int port);
int  SOCK_MaxMessageSize(struct qsockaddr *addr);
int  SOCK_MaxUnreliableMessageSize(struct qsockaddr *addr);
