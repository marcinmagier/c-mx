
#ifndef __MX_SOCKET_H_
#define __MX_SOCKET_H_


#include <stddef.h>
#include <stdbool.h>
#include <sys/socket.h>



int socket_open(int family, int type);
int socket_close(int sock);

static inline int socket_create_inet_stream(void) {
    return socket_open(AF_INET, SOCK_STREAM);
}
static inline int socket_create_inet_datagram(void) {
    return socket_open(AF_INET, SOCK_DGRAM);
}
static inline int socket_create_unix_stream(void) {
    return socket_open(AF_UNIX, SOCK_STREAM);
}


static inline bool socket_is_valid(int sock) {
    return (sock >= 0) ? true : false;
}

int socket_get_addr_family(int sock);

int socket_set_reuse_addr(int sock, int on);
int socket_set_reuse_port(int sock, int on);
int socket_set_tcp_no_delay(int sock, int on);
int socket_set_non_blocking(int sock, int on);
int socket_set_timeout(int sock, unsigned int timeout);
int socket_set_ttl(int sock, unsigned char value);

//int socket_connect(const char *addr, unsigned int port);
int socket_connect_inet(int sock_family, const char *addr, unsigned int port);
int socket_connect_unix(const char *path);

int socket_bind_inet(int sock, struct sockaddr *addr, unsigned short port);

int socket_listen(const char *addr, unsigned int port);
int socket_listen_inet(int sock, const char *addr, unsigned int port);
int socket_listen_unix(int sock, const char *path);

int socket_send(int sock, const void *buffer, size_t len);
int socket_receive(int sock, void *buffer, size_t len);

int socket_sendto(int sock, struct sockaddr *addr, unsigned short port, void *data, size_t len);

int socket_send_msg(int sock, const void *buffer, size_t len);
int socket_receive_msg(int sock, void *buffer, size_t len);
int socket_receive_msg_len(int sock);



#endif /* __MX_SOCKET_H_ */
